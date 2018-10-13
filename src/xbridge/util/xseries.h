// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XSERIES_H
#define XSERIES_H

#include "chainparams.h"
#include "currencypair.h"
#include "xutil.h"
#include "xbridge/xbridgetransactiondescr.h"
#include "sync.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using boost::posix_time::ptime;
using boost::posix_time::time_period;
using boost::posix_time::time_duration;
using boost::posix_time::from_time_t;

/**
 * @brief validate and hold parameters used by dxGetOrderHistory() and others
 */
class xQuery {
    const ptime currentTime   = boost::posix_time::microsec_clock::universal_time();
    const ptime oneDayFromNow = currentTime + boost::gregorian::days{1};
public:
// types
    enum class WithTxids { Excluded, Included };
    enum class WithInverse { Excluded, Included };
    enum class Transform { None, Invert };
    class IntervalLimit {
        using value_type = int;
        value_type limit{max()};
    public:
        IntervalLimit() = default;
        IntervalLimit(value_type x) : limit(x < min() || x > max() ? 0 : x) {}
        value_type count() const { return limit; }
        bool is_valid() const { return min() <= limit && limit <= max(); }
        static inline constexpr value_type min() { return 1; }
        static inline constexpr value_type max() { return std::numeric_limits<value_type>::max(); }
    };
    class IntervalTimestamp {
        enum { Invalid, AtStart, AtEnd } where { AtStart };
    public:
        IntervalTimestamp() = default;
        IntervalTimestamp(const std::string& x)
            : where{x == "at_start" ? AtStart : x == "at_end" ? AtEnd : Invalid} {}
        bool is_valid() const { return where != Invalid; }
        bool at_start() const { return where == AtStart; }
    };
// variables
    ccy::Currency fromCurrency;
    ccy::Currency toCurrency;
    time_duration granularity{boost::date_time::not_a_date_time};
    time_period period{ptime{},ptime{}};
    WithTxids with_txids;
    WithInverse with_inverse;
    IntervalLimit interval_limit;
    IntervalTimestamp interval_timestamp;
    std::string reason;

// constructors
    xQuery() = delete;
    xQuery(const std::string& fromSymbol,
           const std::string& toSymbol,
           int g,
           int64_t start_time,
           int64_t end_time,
           WithTxids with_txids,
           WithInverse with_inverse,
           IntervalLimit limit,
           IntervalTimestamp interval_timestamp
    )
        : fromCurrency{fromSymbol,xbridge::TransactionDescr::COIN}
        , toCurrency{toSymbol,xbridge::TransactionDescr::COIN}
        , granularity{validate_granularity(g)}
        , period{get_start_time(start_time,granularity), get_end_time(end_time,granularity)}
        , with_txids{with_txids}
        , with_inverse{with_inverse}
        , interval_limit{limit}
        , interval_timestamp{interval_timestamp}
        , reason{not is_valid_granularity(granularity) ?
                 ("granularity=" +std::to_string(g)+" must be one of: "+supported_seconds_csv())
              : (period.begin() < earliestTime()) ? "Start time too early."
              : (period.is_null()) ? "Start time >= end time."
              : (period.end() > oneDayFromNow) ? "Start/end times are too large."
              : (not interval_limit.is_valid()) ?
                  ("interval_limit must be in range "+std::to_string(IntervalLimit::min())
                   +" to "+std::to_string(IntervalLimit::max())+".")
              : (not interval_timestamp.is_valid()) ?
                  "interval_timestamp not one of { at_start, at_end }."
              : ""}
    {}

// functions
    bool error() const { return not reason.empty(); }
    const std::string& what() const { return reason; }
    static inline ptime earliestTime() {
        return ptime{boost::gregorian::date{2018, 2, 25}};
    }
    static inline std::string supported_seconds_csv() {
        std::string str{}, s{};
        for(auto v : supported_seconds()) {
            str += s + std::to_string(v);
            s = ",";
        }
        return str;
    }
private:
    static inline constexpr std::array<int,6> supported_seconds() {
        return {{ 1*60, 5*60, 15*60, 1*60*60, 6*60*60, 24*60*60 }};
    }
    static inline time_duration validate_granularity(int val) {
        constexpr auto s = supported_seconds();
        const auto f = std::find(s.begin(), s.end(), val);
        return boost::posix_time::seconds{f == s.end() ? 0 : *f};
    }
    static inline bool is_valid_granularity(const time_duration& td) {
        constexpr auto s = supported_seconds();
        return s.end() != std::find(s.begin(), s.end(), td.total_seconds());
    }
    static inline ptime get_start_time(int64_t start_secs, time_duration period) {
        const int64_t psec = period.total_seconds();
        if (start_secs < 0 || psec < 1)
            return from_time_t(0);
        return from_time_t( (start_secs / psec) * psec );
    }
    static inline ptime get_end_time(int64_t end_secs, time_duration period) {
        const int64_t psec = period.total_seconds();
        if (end_secs < 0 || psec < 1)
            return from_time_t(0);
        return from_time_t(((end_secs + psec - 1) / psec) * psec);
    }

public:
    static inline time_duration min_granularity() {
        constexpr auto s = supported_seconds();
        const auto* m = std::min_element(s.begin(), s.end());
        return boost::posix_time::seconds{*m};
    }
};

/**
 * @brief Aggregate transactions for a time interval
 */
class xAggregate {
public:

    // types
    using price_t = double;

    // variables
    ptime timeEnd{};
    price_t open{0};
    price_t high{0};
    price_t low{0};
    price_t close{0};
    ccy::Asset fromVolume{};
    ccy::Asset toVolume{};
    std::vector<xid_t> orderIds{};

    // ctors
    xAggregate() = delete;
    xAggregate(ccy::Currency fromCurrency, ccy::Currency toCurrency)
    : fromVolume{fromCurrency}, toVolume{toCurrency}
    {}

    // helpers
    static inline price_t inverse(price_t v) {
        return v < std::numeric_limits<price_t>::epsilon() ? 0. : 1. / v;
    }

    // accessors
    xAggregate inverse() const;

    // mutators
    void update(const xAggregate& x, xQuery::WithTxids);
    void update(const CurrencyPair& x, xQuery::WithTxids);
};


/**
 * @brief Cache of open,high,low,close transaction aggregated series
 */
class xSeriesCache
{
private: // types
    using pairSymbol = std::string;

public:
    using xAggregateContainer = std::deque<xAggregate>;
    using xAggregateIterator = xAggregateContainer::iterator;
    class xRange {
    public:
        xAggregateIterator b;
        xAggregateIterator e;
        xAggregateIterator begin() const { return b; }
        xAggregateIterator end() const { return e; }
    };
    xSeriesCache() = default;
    std::vector<xAggregate> getChainXAggregateSeries(const xQuery&);
    std::vector<xAggregate> getXAggregateSeries(const xQuery&);
    xAggregateContainer& getXAggregateContainer(const pairSymbol&);
    template <class Iterator>
    xRange getXAggregateRange(const Iterator& begin,
                              const Iterator& end,
                              const time_period& period)
    {
        auto low = std::lower_bound(begin, end, period.begin(),
                                    [](const xAggregate& a, const ptime& b) {
                                        return a.timeEnd <= b; });
        auto up = std::upper_bound(low, end, period.end(),
                                   [](const ptime& period_end, const xAggregate& b) {
                                       return period_end <= b.timeEnd; });
        return {low, up};
    }

    void updateSeriesCache(const time_period&);

private:
    void updateXSeries(std::vector<xAggregate>& series,
                       const ccy::Currency& from,
                       const ccy::Currency& to,
                       const xQuery& q,
                       xQuery::Transform tf);
private:
    CCriticalSection m_xSeriesCacheUpdateLock;
    /**
     * The cache keeps data in intervals of the minimum of
     *    - the granularity of time in the blockchain (TargetSpacing), and
     *    - the minimum granularity supported in a query.
     * There may be gaps between intervals, so it is potentially sparse.
     */
    time_duration m_cache_granularity{
        std::min(xQuery::min_granularity(),
                 time_duration{boost::posix_time::seconds{Params().TargetSpacing()}})};
    time_period m_cache_period{ptime{},ptime{}};
    std::unordered_map<pairSymbol, xAggregateContainer> mSparseSeries;
};
#endif // XSERIES_H
