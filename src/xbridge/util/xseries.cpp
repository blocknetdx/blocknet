// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

#include <assert.h>
#include "currencypair.h"
#include "xseries.h"
#include "xbridge/xbridgetransactiondescr.h"
#include "xbridge/xbridgeapp.h"

extern CurrencyPair TxOutToCurrencyPair(const CTxOut & txout, std::string& snode_pubkey);

//******************************************************************************
//******************************************************************************
namespace {
    // Helper functions to filter transactions in a query
    //
    template<class A, class B> bool is_equivalent_pair(const A& a, const B& b) {
        return (a.toCurrency == b.toCurrency && a.fromCurrency == b.fromCurrency) ||
               (a.toCurrency == b.fromCurrency && a.fromCurrency == b.toCurrency);
    }
    template<typename T> bool is_too_small(T amount) {
        return ::fabs(amount) < std::numeric_limits<double>::epsilon();
    }
    void transaction_filter(std::vector<CurrencyPair>& matches,
                            const xbridge::TransactionDescr& tr,
                            const xQuery& query)
    {
        if (not(tr.state == xbridge::TransactionDescr::trFinished)) return;
        if (not query.period.contains(tr.txtime)) return;
        if (not is_equivalent_pair(tr, query)) return;
        if (is_too_small(tr.fromAmount) || is_too_small(tr.toAmount)) return;
        matches.emplace_back(query.with_txids == xQuery::WithTxids::Included
                             ? tr.id.GetHex() : xid_t{},
                             tr.fromCurrency, tr.fromAmount,
                             tr.toCurrency,   tr.toAmount,
                             tr.txtime);
    }
    void updateXSeries(std::vector<xAggregate>& series,
                       const xSeriesCache::xRange& r,
                       const xQuery& q,
                       xQuery::Transform tf)
    {
        for (auto it=r.begin(); it != r.end(); ++it) {
            auto offset = it->timeEnd - q.period.begin();
            size_t idx = (offset.total_seconds() - 1) / q.granularity.total_seconds();
            series.at(idx).update(tf == xQuery::Transform::Invert ? it->inverse() : *it, q.with_txids);
        }
    }


    std::vector<CurrencyPair> get_tradingdata(time_period query)
    {
        LOCK(cs_main);

        std::vector<CurrencyPair> records;

        CBlockIndex * pindex = chainActive.Tip();
        auto ts = from_time_t(pindex->GetBlockTime());
        while (pindex->pprev != nullptr && query.end() < ts) {
            pindex = pindex->pprev;
            ts = from_time_t(pindex->GetBlockTime());
        }

        for (; pindex->pprev != nullptr && query.contains(ts);
             pindex = pindex->pprev, ts = from_time_t(pindex->GetBlockTime()))
        {
            CBlock block;
            if (not ReadBlockFromDisk(block, pindex))
                continue; // throw?
            for (const CTransaction & tx : block.vtx)
            {
                for (const CTxOut & out : tx.vout)
                {
                    std::string snode_pubkey{};
                    CurrencyPair p = TxOutToCurrencyPair(out, snode_pubkey);
                    if (p.tag == CurrencyPair::Tag::Valid) {
                        p.timeStamp = ts;
                        records.emplace_back(p);
                    }
                }
            }
        }
        return records;
    }

    ptime get_end_time(int64_t end_secs, time_duration period = boost::posix_time::seconds{60}) {
        const int64_t psec = period.total_seconds();
        if (end_secs < 0 || psec < 1)
            return from_time_t(0);
        return from_time_t(((end_secs + psec - 1) / psec) * psec);
    }
}

//******************************************************************************
//******************************************************************************
std::vector<xAggregate>
xSeriesCache::getChainXAggregateSeries(const xQuery& query)
{
    xQuery q{query};
    const size_t query_seconds = q.period.length().total_seconds();
    const size_t granularity_seconds = q.granularity.total_seconds();
    const size_t num_intervals = std::min(query_seconds / granularity_seconds,
                                          q.interval_limit.count);
    time_duration adjusted_duration = boost::posix_time::seconds{
        static_cast<long>(num_intervals * granularity_seconds)};
    q.period = time_period{q.period.end() - adjusted_duration, q.period.end()};
    std::vector<xAggregate> series{};
    series.resize(num_intervals,xAggregate{ptime{},0,0,0,0,0,0});

    auto t = q.period.begin();
    for (size_t i = 0; i < num_intervals; ++i) {
        t += q.granularity;
        series[i].timeEnd = t;
    }

    if (not m_cache_period.contains(q.period))
        updateSeriesCache(q.period);

    pairSymbol key = q.fromCurrency +"/"+ q.toCurrency;
    auto range = getXAggregateRange(key, q.period);
    updateXSeries(series, range, q, xQuery::Transform::None);
    if (q.with_inverse == xQuery::WithInverse::Included) {
        key = q.toCurrency +"/"+ q.fromCurrency;
        range = getXAggregateRange(key, q.period);
        updateXSeries(series, range, q, xQuery::Transform::Invert);
    }
    return series;
}

//******************************************************************************
//******************************************************************************
xSeriesCache::xAggregateContainer&
xSeriesCache::getXAggregateContainer(const pairSymbol& key)
{
    auto f = mSparseSeries.find(key);
    if (f == mSparseSeries.end()) {
        mSparseSeries[key] = {};
        f = mSparseSeries.find(key);
        assert(f != mSparseSeries.end());
    }
    return f->second;
}

//******************************************************************************
//******************************************************************************
xSeriesCache::xRange
xSeriesCache::getXAggregateRange(const pairSymbol& key, const time_period& period)
{
    auto& q = getXAggregateContainer(key);
    auto low = std::lower_bound(q.begin(), q.end(), period.begin(),
                                   [](const xAggregate& a, const ptime& b) {
                                       return a.end_time() <= b; });
    auto up = std::upper_bound(low, q.end(), period.end(),
                                [](const ptime& period_end, const xAggregate& b) {
                                   return period_end <= b.end_time(); });
    return {low, up};
}

//******************************************************************************
//******************************************************************************
void xSeriesCache::updateSeriesCache(const time_period& period)
{
    // TODO(amdn) cached aggregate series for blocks that are purged from the chain
    // need to be invalidated... this code invalidates on every query to
    // ensure results are up-to-date, cache can be enabled when invalidation
    // hook is in place
    boost::mutex::scoped_lock l(m_xSeriesCacheUpdateLock);
    std::vector<CurrencyPair> pairs = get_tradingdata(period);
    std::sort(pairs.begin(), pairs.end(), // ascending by updated time
              [](const CurrencyPair& a, const CurrencyPair& b) {
                  return a.timeStamp < b.timeStamp; });

    mSparseSeries.clear();
    for (const auto& p : pairs) {
        pairSymbol key = p.fromCurrency +"/"+ p.toCurrency;
        auto& q = getXAggregateContainer(key);
        if (q.empty() || q.back().timeEnd <= p.timeStamp) {
            q.emplace_back(xAggregate{});
            q.back().timeEnd = get_end_time(to_time_t(p.timeStamp));
        }
        q.back().update(p,xQuery::WithTxids::Included);
    }
    m_cache_period = period;
}

//******************************************************************************
//******************************************************************************
xAggregate xAggregate::inverse() const {
    xAggregate x;
    x.timeEnd = timeEnd;
    x.orderIds = orderIds;
    x.open = inverse(open);
    x.high  = inverse(high);
    x.low   = inverse(low);
    x.close = inverse(close);
    x.toVolume = fromVolume;
    x.fromVolume = toVolume;
    return x;
}

void xAggregate::update(const CurrencyPair& x, xQuery::WithTxids with_txids) {
    price_t price = x.toAmount == 0
        ? 0.
        : static_cast<double>(x.fromAmount) / static_cast<double>(x.toAmount);
    if (open == 0) {
        open = high = low = price;
    }
    high = std::max(high,price);
    low  = std::min(low,price);
    close = price;
    fromVolume += x.fromAmount;
    toVolume += x.toAmount;
    if (with_txids == xQuery::WithTxids::Included)
        orderIds.push_back(x.xid());
}

void xAggregate::update(const xAggregate& x, xQuery::WithTxids with_txids) {
    if (open == 0) {
        open = high = low = x.open;
    }
    high = std::max(high,x.high);
    low  = std::min(low,x.low);
    close = x.close;
    fromVolume += x.fromVolume;
    toVolume += x.toVolume;
    if (with_txids == xQuery::WithTxids::Included) {
        for (const auto& it : x.orderIds)
            orderIds.push_back(it);
    }
}

//******************************************************************************
//******************************************************************************
std::vector<xAggregate> xSeriesCache::getXAggregateSeries(const xQuery& query)
{
    auto& app = xbridge::App::instance();
    auto local_matches = app.history_matches(transaction_filter, query);

    std::sort(local_matches.begin(), local_matches.end(), // ascending by updated time
              [](const CurrencyPair& a, const CurrencyPair& b) {
                  return a.timeStamp < b.timeStamp; });

    //--Retrieve matching aggregate transactions from blockchain (cached)
    std::vector<xAggregate> series = getChainXAggregateSeries(query);

    //--Update aggregate from blockchain with transactions from local history
    auto it = series.begin();
    auto end = series.end();
    if (it != end) {
        for (const auto& x : local_matches) {
            for ( ; it != end && x.timeStamp < (it->timeEnd - query.granularity); ++it)
                ;;
            assert(it != end && x.timeStamp < it->timeEnd);
            it->update(x,query.with_txids);
        }
    }
    return series;
}
