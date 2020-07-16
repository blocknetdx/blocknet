// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgeexchange.h>

#include <xbridge/bitcoinrpcconnector.h>
#include <xbridge/util/logger.h>
#include <xbridge/util/settings.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbridgewalletconnector.h>

#include <chainparamsbase.h>
#include <coinvalidator.h>
#include <key.h>
#include <pubkey.h>
#include <servicenode/servicenodemgr.h>
#include <sync.h>

#include <algorithm>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

// from xbridgeapp.h
extern WalletConnectorPtr ConnectorByCurrency(const std::string & currency);

bool ExchangeUtxos(const uint256 & txid, std::vector<wallet::UtxoEntry> & items) {
    return Exchange::instance().getUtxoItems(txid, items);
}

//******************************************************************************
//******************************************************************************
class Exchange::Impl
{
    friend class Exchange;

protected:
    bool initKeyPair();

    std::list<TransactionPtr> transactions(bool onlyFinished) const;

protected:
    // connected wallets
    typedef std::map<std::string, WalletParam> WalletList;
    WalletList                                         m_wallets;
    mutable CCriticalSection                           m_walletsLock;

    mutable CCriticalSection                           m_pendingTransactionsLock;
    std::map<uint256, uint256>                         m_hashToIdMap;
    std::map<uint256, TransactionPtr>                  m_pendingTransactions;

    mutable CCriticalSection                           m_transactionsLock;
    std::map<uint256, TransactionPtr>                  m_transactions;

    // utxo records
    CCriticalSection                                   m_utxoLocker;
    std::set<wallet::UtxoEntry>                        m_utxoItems;
    std::map<uint256, std::vector<wallet::UtxoEntry> > m_utxoTxMap;

    std::vector<unsigned char>                         m_pubkey;
    std::vector<unsigned char>                         m_privkey;
};

//*****************************************************************************
//*****************************************************************************
Exchange::Exchange()
    : m_p(new Impl)
{
}

//*****************************************************************************
//*****************************************************************************
Exchange::~Exchange()
{
}

//*****************************************************************************
//*****************************************************************************
// static
Exchange & Exchange::instance()
{
    static Exchange e;
    return e;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::init()
{
    if (!settings().isExchangeEnabled())
    {
        // disabled
        return true;
    }

    if (!m_p->initKeyPair())
    {
        ERR() << "bad service node key pair " << __FUNCTION__;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::loadWallets(std::set<std::string> & wallets)
{
    LOCK(m_lock);
    auto & s = settings();
    for (std::set<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        std::string label      = s.get<std::string>(*i + ".Title");
        std::string address    = s.get<std::string>(*i + ".Address");
        std::string ip         = s.get<std::string>(*i + ".Ip");
        std::string port       = s.get<std::string>(*i + ".Port");
        std::string user       = s.get<std::string>(*i + ".Username");
        std::string passwd     = s.get<std::string>(*i + ".Password");
        uint64_t    minAmount  = s.get<uint64_t>(*i + ".MinimumAmount", 0);
        uint32_t    txVersion  = s.get<uint32_t>(*i + ".TxVersion", 1);
        std::string jsonver    = s.get<std::string>(*i + ".JSONVersion", "");
        std::string contenttype = s.get<std::string>(*i + ".ContentType", "");
        std::string cashAddrPf = s.get<std::string>(*i + ".CashAddrPrefix", "");

        if (user.empty() || passwd.empty())
            WARN() << *i << " \"" << label << "\"" << " has empty credentials";

        if (ip.empty() || port.empty()) {
            WARN() << *i << " \"" << label << "\"" << " failed to load config: Ip and Port is required";
            continue;
        }

        WalletParam & wp = m_p->m_wallets[*i];
        wp.currency   = *i;
        wp.title      = label;
        wp.m_ip       = ip;
        wp.m_port     = port;
        wp.m_user     = user;
        wp.m_passwd   = passwd;
        wp.dustAmount = minAmount;
        wp.txVersion  = txVersion;
        wp.jsonver    = jsonver;
        wp.contenttype = contenttype;
        wp.cashAddrPrefix = cashAddrPf;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::isEnabled()
{
    LOCK(m_lock);
    if (m_p) {
        LOCK(m_p->m_walletsLock);
        return ((m_p->m_wallets.size() > 0) && gArgs.GetBoolArg("-enableexchange", false));
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::isStarted()
{
    return isEnabled() && sn::ServiceNodeMgr::instance().hasActiveSn();
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::Impl::initKeyPair()
{
    if (!sn::ServiceNodeMgr::instance().hasActiveSn()) {
        ERR() << "service node key not set " << __FUNCTION__;
        return false;
    }

    const auto & key = sn::ServiceNodeMgr::instance().getActiveSn().key;
    if (!key.IsValid()) {
        ERR() << "invalid service node key " << __FUNCTION__;
        return false;
    }

    CPubKey pubkey = key.GetPubKey();
    if (!pubkey.IsCompressed())
        pubkey.Compress();

    m_pubkey  = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    m_privkey = std::vector<unsigned char>(key.begin(),    key.end());

    return true;
}

//*****************************************************************************
//*****************************************************************************
const std::vector<unsigned char> & Exchange::pubKey() const
{
    if (m_p->m_pubkey.empty() || m_p->m_pubkey.size() != 33)
    {
        if (!m_p->initKeyPair())
        {
            ERR() << "bad service node key pair " << __FUNCTION__;
        }
    }
    return m_p->m_pubkey;
}

//*****************************************************************************
//*****************************************************************************
const std::vector<unsigned char> & Exchange::privKey() const
{
    if (m_p->m_privkey.empty() || m_p->m_privkey.size() != 32)
    {
        if (!m_p->initKeyPair())
        {
            ERR() << "bad service node key pair " << __FUNCTION__;
        }
    }
    return m_p->m_privkey;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::haveConnectedWallet(const std::string & walletName)
{
    LOCK(m_p->m_walletsLock);
    return m_p->m_wallets.count(walletName) > 0;
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> Exchange::connectedWallets() const
{
    LOCK(m_p->m_walletsLock);
    std::vector<std::string> list;
    for (const auto & wallet : m_p->m_wallets)
    {
        list.push_back(wallet.first);
    }
    return list;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::checkUtxoItems(const uint256 & txid, const std::vector<wallet::UtxoEntry> & items)
{
    LOCK(m_p->m_utxoLocker);

    if (m_p->m_utxoTxMap.count(txid))
    {
        // transaction found
        return true;
    }

    // check
    for (const wallet::UtxoEntry & item : items)
    {
        if (m_p->m_utxoItems.count(item) || !CoinValidator::instance().IsCoinValid(item.txId)) // check not in bad funds
        {
            // duplicate items
            return false;
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::getUtxoItems(const uint256 & txid, std::vector<wallet::UtxoEntry> & items)
{
    LOCK(m_p->m_utxoLocker);

    if(txid.IsNull())
    {
        for(const wallet::UtxoEntry & entry : m_p->m_utxoItems)
            items.push_back(entry);

        return true;
    }

    if (m_p->m_utxoTxMap.count(txid))
    {
        for(const wallet::UtxoEntry & entry : m_p->m_utxoTxMap[txid])
            items.push_back(entry);

        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::createTransaction(const uint256                        & txid,
                                 const std::vector<unsigned char>     & sourceAddr,
                                 const std::string                    & sourceCurrency,
                                 const uint64_t                       & sourceAmount,
                                 const std::vector<unsigned char>     & destAddr,
                                 const std::string                    & destCurrency,
                                 const uint64_t                       & destAmount,
                                 const uint64_t                       & timestamp,
                                 const std::vector<unsigned char>     & mpubkey,
                                 const std::vector<wallet::UtxoEntry> & items,
                                 uint256                              & blockHash,
                                 bool                                 & isCreated,
                                 const bool                             isPartialOrder = false,
                                 const uint64_t                         minFromAmount = 0)
{
    DEBUG_TRACE();

    isCreated = false;

    if (!haveConnectedWallet(sourceCurrency) || !haveConnectedWallet(destCurrency))
    {
        xbridge::LogOrderMsg(txid.GetHex(), "no active wallet for order", __FUNCTION__);
        return false;
    }

    // check locked items
    if (!checkUtxoItems(txid, items))
    {
        // duplicate items
        xbridge::LogOrderMsg(txid.GetHex(), "utxo check failed, duplicates detected", __FUNCTION__);
        return false;
    }

    {
    LOCK(m_p->m_walletsLock);
    const WalletParam & wp  = m_p->m_wallets[sourceCurrency];
    const WalletParam & wp2 = m_p->m_wallets[destCurrency];

    // check amounts
    {
        if (wp.dustAmount && wp.dustAmount > sourceAmount)
        {
            xbridge::LogOrderMsg(txid.GetHex(), "rejected order because source currency amount less than minimum payment", __FUNCTION__);
            return false;
        }
        if (wp2.dustAmount && wp2.dustAmount > destAmount)
        {
            xbridge::LogOrderMsg(txid.GetHex(), "rejected order because dest currency amount less than minimum payment", __FUNCTION__);
            return false;
        }
    }
    }

    TransactionPtr tr(new xbridge::Transaction(txid,
                                               sourceAddr,
                                               sourceCurrency, sourceAmount,
                                               destAddr,
                                               destCurrency, destAmount,
                                               timestamp,
                                               blockHash,
                                               mpubkey,
                                               isPartialOrder,
                                               minFromAmount));
    if (!tr->isValid())
    {
        xbridge::LogOrderMsg(txid.GetHex(), "rejected order because it failed validity checks", __FUNCTION__);
        return false;
    }

    if(tr->isExpiredByBlockNumber())
    {
        xbridge::LogOrderMsg(txid.GetHex(), "rejected order because it has expired (by block number)", __FUNCTION__);
        return false;
    }

    {
        LOCK(m_p->m_pendingTransactionsLock);

        if (!m_p->m_pendingTransactions.count(txid))
        {
            // new transaction
            isCreated = true;
            m_p->m_pendingTransactions[txid] = tr;
        }
        else
        {
            m_p->m_pendingTransactions[txid]->m_lock.lock();

            // found, check if expired
            if (!m_p->m_pendingTransactions[txid]->isExpired())
            {
                m_p->m_pendingTransactions[txid]->updateTimestamp();

                m_p->m_pendingTransactions[txid]->m_lock.unlock();
            }
            else
            {
                m_p->m_pendingTransactions[txid]->m_lock.unlock();

                // if expired - delete old transaction
                m_p->m_pendingTransactions.erase(txid);

                // create new
                m_p->m_pendingTransactions[txid] = tr;
            }
        }
    }

    // add locked items
    lockUtxos(txid, items);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::acceptTransaction(const uint256                        & txid,
                                 const std::vector<unsigned char>     & sourceAddr,
                                 const std::string                    & sourceCurrency,
                                 const uint64_t                       & sourceAmount,
                                 const std::vector<unsigned char>     & destAddr,
                                 const std::string                    & destCurrency,
                                 const uint64_t                       & destAmount,
                                 const std::vector<unsigned char>     & mpubkey,
                                 const std::vector<wallet::UtxoEntry> & items,
                                 const bool                             isPartialOrderAllowed = false)
{
    DEBUG_TRACE();

    if (!haveConnectedWallet(sourceCurrency) || !haveConnectedWallet(destCurrency))
    {
        xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because of a bad connector " +
                                   (!haveConnectedWallet(sourceCurrency) ? sourceCurrency : destCurrency), __FUNCTION__);
        return false;
    }

    // check locked items
    if (!checkUtxoItems(txid, items))
    {
        xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because duplicate utxos were detected", __FUNCTION__);
        // duplicate items
        return false;
    }

    TransactionPtr tr(new xbridge::Transaction(txid,
                                               sourceAddr,
                                               sourceCurrency, sourceAmount,
                                               destAddr,
                                               destCurrency, destAmount,
                                               std::time(0), uint256(), mpubkey,
                                               isPartialOrderAllowed, 0));
    if (!tr->isValid())
    {
        xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because the transaction failed validity checks", __FUNCTION__);
        return false;
    }

    TransactionPtr tmp;

    {
        LOCK(m_p->m_pendingTransactionsLock);

        if (!m_p->m_pendingTransactions.count(txid))
        {
            xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because it was not found", __FUNCTION__);
            // no pending
            return false;
        }
        else
        {
            m_p->m_pendingTransactions[txid]->m_lock.lock();

            // found, check if expired
            if (m_p->m_pendingTransactions[txid]->isExpired())
            {
                m_p->m_pendingTransactions[txid]->m_lock.unlock();

                // if expired - delete old transaction
                m_p->m_pendingTransactions.erase(txid);
                xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because it has expired", __FUNCTION__);
                return false;
            }
            else
            {
                // try join with existing transaction
                if (!m_p->m_pendingTransactions[txid]->tryJoin(tr))
                {
                    xbridge::LogOrderMsg(txid.GetHex(), "rejecting order because counterparties have not joined", __FUNCTION__);
                    m_p->m_pendingTransactions[txid]->m_lock.unlock();
                    return false;
                }
                else
                {
                    xbridge::LogOrderMsg(tr->id().GetHex(), "joining counterparties on order", __FUNCTION__);
                    tmp = m_p->m_pendingTransactions[txid];
                }
            }

            m_p->m_pendingTransactions[txid]->m_lock.unlock();
        }
    }

    if (tmp)
    {
        // move to transactions
        {
            LOCK(m_p->m_transactionsLock);
            m_p->m_transactions[txid] = tmp;
        }
        {
            LOCK(m_p->m_pendingTransactionsLock);
            m_p->m_pendingTransactions.erase(txid);
        }
    }

    // add locked items
    lockUtxos(txid, items);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::deletePendingTransaction(const uint256 & id)
{
    LOCK(m_p->m_pendingTransactionsLock);

    xbridge::LogOrderMsg(id.GetHex(), "delete pending order and unlock utxos", __FUNCTION__);

    // if there are any locked utxo's for this txid, unlock them
    unlockUtxos(id);

    m_p->m_pendingTransactions.erase(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::deleteTransaction(const uint256 & txid)
{
    LOCK(m_p->m_transactionsLock);

    xbridge::LogOrderMsg(txid.GetHex(), "deleting order", __FUNCTION__);

    m_p->m_transactions.erase(txid);

    unlockUtxos(txid);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::updateTransactionWhenHoldApplyReceived(const TransactionPtr & tx,
                                                      const std::vector<unsigned char> & from)
{
    if (tx->increaseStateCounter(xbridge::Transaction::trJoined, from) == xbridge::Transaction::trHold)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::updateTransactionWhenInitializedReceived(const TransactionPtr &tx,
                                                        const std::vector<unsigned char> & from,
                                                        const std::vector<unsigned char> & pk)
{
    if (!tx->setKeys(from, pk))
    {
        // wtf?
        xbridge::LogOrderMsg(tx->id().GetHex(), "unknown sender address for order", __FUNCTION__);
        return false;
    }

    if (tx->increaseStateCounter(xbridge::Transaction::trHold, from) == xbridge::Transaction::trInitialized)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::updateTransactionWhenCreatedReceived(const TransactionPtr & tx,
                                                           const std::vector<unsigned char> & from,
                                                           const std::string & binTxId)
{
    if (!tx->setBinTxId(from, binTxId))
    {
        // wtf?
        xbridge::LogOrderMsg(tx->id().GetHex(), "unknown sender address for order", __FUNCTION__);
        return false;
    }

    if (tx->increaseStateCounter(xbridge::Transaction::trInitialized, from) == xbridge::Transaction::trCreated)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::updateTransactionWhenConfirmedReceived(const TransactionPtr & tx,
                                                             const std::vector<unsigned char> & from)
{
    // update transaction state
    if (tx->increaseStateCounter(xbridge::Transaction::trCreated, from) == xbridge::Transaction::trFinished)
    {
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
const TransactionPtr Exchange::transaction(const uint256 & hash)
{
    {
        LOCK(m_p->m_transactionsLock);

        if (m_p->m_transactions.count(hash))
        {
            return m_p->m_transactions[hash];
        }
    }

    return TransactionPtr(new xbridge::Transaction);
}

//*****************************************************************************
//*****************************************************************************
const TransactionPtr Exchange::pendingTransaction(const uint256 & hash)
{
    {
        LOCK(m_p->m_pendingTransactionsLock);

        if (m_p->m_pendingTransactions.count(hash))
        {
            return m_p->m_pendingTransactions[hash];
        }
    }

    // return XBridgeTransaction::trInvalid;
    return TransactionPtr(new xbridge::Transaction);
}

//*****************************************************************************
//*****************************************************************************
std::list<TransactionPtr> Exchange::pendingTransactions() const
{
    LOCK(m_p->m_pendingTransactionsLock);

    std::list<TransactionPtr> list;

    for (std::map<uint256, TransactionPtr>::const_iterator i = m_p->m_pendingTransactions.begin();
         i != m_p->m_pendingTransactions.end(); ++i)
    {
        list.push_back(i->second);
    }

    return list;
}

//*****************************************************************************
//*****************************************************************************
std::list<TransactionPtr> Exchange::Impl::transactions(bool onlyFinished) const
{
    LOCK(m_transactionsLock);

    std::list<TransactionPtr> list;

    for (const std::pair<uint256, TransactionPtr> & i : m_transactions)
    {
        if (!onlyFinished)
        {
            list.push_back(i.second);
        }
        else if (i.second->isExpired() ||
                 !i.second->isValid() ||
                 i.second->isFinished())
        {
            list.push_back(i.second);
        }
    }

    return list;
}

//*****************************************************************************
//*****************************************************************************
std::list<TransactionPtr> Exchange::transactions() const
{
    return m_p->transactions(false);
}

//*****************************************************************************
//*****************************************************************************
std::list<TransactionPtr> Exchange::finishedTransactions() const
{
    return m_p->transactions(true);
}

//*****************************************************************************
//*****************************************************************************
size_t Exchange::eraseExpiredTransactions()
{
    if (!isStarted())
    {
        return 0;
    }

    size_t result = 0;

    LOCK(m_p->m_pendingTransactionsLock);

    // Use non-hoisted iterator to prevent invalidation during erase
    for (auto it = m_p->m_pendingTransactions.cbegin(); it != m_p->m_pendingTransactions.cend(); )
    {
        TransactionPtr ptr = it->second;

        if (ptr->isExpiredByBlockNumber())
        {
            xbridge::LogOrderMsg(ptr->id().GetHex(), "order expired by block number", __FUNCTION__);
             m_p->m_pendingTransactions.erase(it++);
             unlockUtxos(ptr->id());
             ++result;
         }
        else if(ptr->isExpired())
        {
            xbridge::LogOrderMsg(ptr->id().GetHex(), "order expired by ttl", __FUNCTION__);
            m_p->m_pendingTransactions.erase(it++);
            unlockUtxos(ptr->id());
            ++result;
        } else {
            ++it;
        }
    }

    return result;
}

//******************************************************************************
//******************************************************************************
bool Exchange::lockUtxos(const uint256 &id, const std::vector<wallet::UtxoEntry> &items)
{
    if (items.empty())
    {
        return false;
    }

    LOCK(m_p->m_utxoLocker);
    // use set to prevent overwriting utxo's from 'A' or 'B' role
    std::set<wallet::UtxoEntry> utxoTxMapItems;
    for (const wallet::UtxoEntry & item : m_p->m_utxoTxMap[id])
    {
        utxoTxMapItems.insert(item);
    }

    for (const wallet::UtxoEntry & item : items)
    {
        m_p->m_utxoItems.insert(item);
        if (!utxoTxMapItems.count(item))
        {
            utxoTxMapItems.insert(item);
            m_p->m_utxoTxMap[id].push_back(item);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Exchange::unlockUtxos(const uint256 &id)
{
    LOCK(m_p->m_utxoLocker);

    if (!m_p->m_utxoTxMap.count(id))
    {
        return false;
    }

    for (const wallet::UtxoEntry & item : m_p->m_utxoTxMap[id])
    {
        m_p->m_utxoItems.erase(item);
    }

    m_p->m_utxoTxMap.erase(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::updateTimestampOrRemoveExpired(const TransactionPtr & tx)
{
    LOCK(m_p->m_pendingTransactionsLock);

    auto txid = tx->id();
    m_p->m_pendingTransactions[txid]->m_lock.lock();

    // found, check if expired
    if (!m_p->m_pendingTransactions[txid]->isExpired())
    {
        // return false if update is too soon
        if (m_p->m_pendingTransactions[txid]->updateTooSoon()) {
            m_p->m_pendingTransactions[txid]->m_lock.unlock();
            return false;
        }
        m_p->m_pendingTransactions[txid]->updateTimestamp();
        m_p->m_pendingTransactions[txid]->m_lock.unlock();
        return true;
    }
    else
    {
        m_p->m_pendingTransactions[txid]->m_lock.unlock();

        // if expired - delete old transaction
        m_p->m_pendingTransactions.erase(txid);
        return false;
    }
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::makerUtxosAreStillValid(const TransactionPtr & tx)
{
    auto current = boost::posix_time::microsec_clock::universal_time();
    if ((current - tx->utxoCheckTime()).total_seconds() < gArgs.GetArg("-orderinputscheck", 900))
        return true; // Only update at most every N seconds (default 15 minutes)
    tx->updateUtxoCheckTime(current);

    xbridge::LogOrderMsg(tx->id().GetHex(), "running automated maker utxo check on order", __FUNCTION__);

    WalletConnectorPtr makerConn = ConnectorByCurrency(tx->a_currency());
    if (!makerConn) // non-fatal just skip
        return true;

    auto & makerUtxos = tx->a_utxos();
    for (auto entry : makerUtxos) {
        if (!makerConn->getTxOut(entry)) {
            // Invalid utxos cancel order
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", tx->id().GetHex());
            log_obj.pushKV("utxo_txid", entry.txId);
            log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
            xbridge::LogOrderMsg(log_obj, "bad maker utxo in order", __FUNCTION__);
            return false;
        }
    }

    return true; // done
}

} // namespace xbridge
