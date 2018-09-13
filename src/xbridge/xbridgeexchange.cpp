//*****************************************************************************
//*****************************************************************************

#include "xbridgeexchange.h"
#include "xbridgeapp.h"
#include "util/logger.h"
#include "util/settings.h"
#include "util/xutil.h"
#include "bitcoinrpcconnector.h"
#include "activeservicenode.h"
#include "chainparamsbase.h"
#include "coinvalidator.h"

#include "key.h"
#include "pubkey.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

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

    mutable boost::mutex                               m_pendingTransactionsLock;
    std::map<uint256, uint256>                         m_hashToIdMap;
    std::map<uint256, TransactionPtr>                  m_pendingTransactions;

    mutable boost::mutex                               m_transactionsLock;
    std::map<uint256, TransactionPtr>                  m_transactions;

    // utxo records
    boost::mutex                                       m_utxoLocker;
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

    Settings & s = settings();

    std::vector<std::string> wallets = s.exchangeWallets();
    for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        std::string label      = s.get<std::string>(*i + ".Title");
        std::string address    = s.get<std::string>(*i + ".Address");
        std::string ip         = s.get<std::string>(*i + ".Ip");
        std::string port       = s.get<std::string>(*i + ".Port");
        std::string user       = s.get<std::string>(*i + ".Username");
        std::string passwd     = s.get<std::string>(*i + ".Password");
        uint64_t    minAmount  = s.get<uint64_t>(*i + ".MinimumAmount", 0);
        uint32_t    txVersion  = s.get<uint32_t>(*i + ".TxVersion", 1);


        if (/*address.empty() || */ip.empty() || port.empty() ||
                user.empty() || passwd.empty())
        {
            LOG() << "read wallet " << *i << " with empty parameters>";
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

        LOG() << "read wallet " << *i << " \"" << label << "\" address <" << address << ">";
    }

    if (isEnabled())
    {
        LOG() << "exchange enabled";
    }

    if (isStarted())
    {
        LOG() << "exchange started";
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::isEnabled()
{
    return ((m_p->m_wallets.size() > 0) && GetBoolArg("-servicenode", false));
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::isStarted()
{
    return (isEnabled() && (activeServicenode.status == ACTIVE_SERVICENODE_STARTED));
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::Impl::initKeyPair()
{
    std::string secret = GetArg("-servicenodeprivkey", "");
    if (secret.empty())
    {
        ERR() << "service node key not set " << __FUNCTION__;
        return false;
    }

    CBitcoinSecret vchSecret;
    if (!vchSecret.SetString(secret))
    {
        ERR() << "invalid service node key " << __FUNCTION__;
        return false;
    }

    CKey    key    = vchSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    if (!pubkey.IsCompressed())
    {
        pubkey.Compress();
    }

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
    return m_p->m_wallets.count(walletName) > 0;
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> Exchange::connectedWallets() const
{
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
    boost::mutex::scoped_lock l(m_p->m_utxoLocker);

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
    boost::mutex::scoped_lock l(m_p->m_utxoLocker);

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
                                 bool                                 & isCreated)
{
    DEBUG_TRACE();

    isCreated = false;

    if (!haveConnectedWallet(sourceCurrency) || !haveConnectedWallet(destCurrency))
    {
        LOG() << "no active wallet for transaction " << txid.ToString();
        return false;
    }

    // check locked items
    if (!checkUtxoItems(txid, items))
    {
        // duplicate items
        LOG() << "utxo check failed " << txid.ToString();
        return false;
    }

    const WalletParam & wp  = m_p->m_wallets[sourceCurrency];
    const WalletParam & wp2 = m_p->m_wallets[destCurrency];

    // check amounts
    {
        if (wp.dustAmount && wp.dustAmount > sourceAmount)
        {
            LOG() << "tx " <<  txid.ToString()
                  << " rejected because sourceAmount less than minimum payment";
            return false;
        }
        if (wp2.dustAmount && wp2.dustAmount > destAmount)
        {
            LOG() << "tx " << txid.ToString()
                  << " rejected because destAmount less than minimum payment";
            return false;
        }
    }

    TransactionPtr tr(new xbridge::Transaction(txid,
                                               sourceAddr,
                                               sourceCurrency, sourceAmount,
                                               destAddr,
                                               destCurrency, destAmount,
                                               timestamp,
                                               blockHash,
                                               mpubkey));
    if (!tr->isValid())
    {
        LOG() << "created tx " <<  txid.ToString()
              << " is not valid so rejected";
        return false;
    }

    if(tr->isExpiredByBlockNumber())
    {
        LOG() << "tx " <<  txid.ToString()
              << " is expired by block number so rejected";
        return false;
    }

    {
        boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

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
                                 const std::vector<wallet::UtxoEntry> & items)
{
    DEBUG_TRACE();

    if (!haveConnectedWallet(sourceCurrency) || !haveConnectedWallet(destCurrency))
    {
        LOG() << "no active wallet for transaction "
              << util::base64_encode(std::string((char *)txid.begin(), 32));
        return false;
    }

    // check locked items
    if (!checkUtxoItems(txid, items))
    {
        LOG() << "dx accept duplicate items " << __FUNCTION__;
        // duplicate items
        return false;
    }

    TransactionPtr tr(new xbridge::Transaction(txid,
                                               sourceAddr,
                                               sourceCurrency, sourceAmount,
                                               destAddr,
                                               destCurrency, destAmount,
                                               std::time(0), uint256(), mpubkey));
    if (!tr->isValid())
    {
        LOG() << "invalid transaction " << __FUNCTION__;
        return false;
    }

    TransactionPtr tmp;

    {
        boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

        if (!m_p->m_pendingTransactions.count(txid))
        {
            LOG() << "transaction not found " << __FUNCTION__;
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
                LOG() << "try accept expired transaction " << __FUNCTION__;
                return false;
            }
            else
            {
                // try join with existing transaction
                if (!m_p->m_pendingTransactions[txid]->tryJoin(tr))
                {
                    LOG() << "transaction not joined " << __FUNCTION__;
                    m_p->m_pendingTransactions[txid]->m_lock.unlock();
                    return false;
                }
                else
                {
                    LOG() << "transactions joined, id <" << tr->id().GetHex() << ">";
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
            boost::mutex::scoped_lock l(m_p->m_transactionsLock);
            m_p->m_transactions[txid] = tmp;
        }
        {
            boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);
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
    boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

    LOG() << "delete pending transaction <" << id.GetHex() << ">";

    // if there are any locked utxo's for this txid, unlock them
    unlockUtxos(id);

    m_p->m_pendingTransactions.erase(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Exchange::deleteTransaction(const uint256 & txid)
{
    boost::mutex::scoped_lock l(m_p->m_transactionsLock);

    LOG() << "delete transaction <" << txid.GetHex() << ">";

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
        LOG() << "unknown sender address for transaction, id <" << tx->id().GetHex() << ">";
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
                                                           const std::string & binTxId,
                                                           const std::vector<unsigned char> & innerScript)
{
    if (!tx->setBinTxId(from, binTxId, innerScript))
    {
        // wtf?
        LOG() << "unknown sender address for transaction, id <" << tx->id().GetHex() << ">";
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
        boost::mutex::scoped_lock l(m_p->m_transactionsLock);

        if (m_p->m_transactions.count(hash))
        {
            return m_p->m_transactions[hash];
        }
        else
        {
            // unknown transaction
            LOG() << "unknown transaction, id <" << hash.GetHex() << ">";
        }
    }

    return TransactionPtr(new xbridge::Transaction);
}

//*****************************************************************************
//*****************************************************************************
const TransactionPtr Exchange::pendingTransaction(const uint256 & hash)
{
    {
        boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

        if (m_p->m_pendingTransactions.count(hash))
        {
            return m_p->m_pendingTransactions[hash];
        }
        else
        {
            // unknown transaction
            LOG() << "unknown pending transaction, id <" << hash.GetHex() << ">";
        }
    }

    // return XBridgeTransaction::trInvalid;
    return TransactionPtr(new xbridge::Transaction);
}

//*****************************************************************************
//*****************************************************************************
std::list<TransactionPtr> Exchange::pendingTransactions() const
{
    boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

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
    boost::mutex::scoped_lock l(m_transactionsLock);

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

    boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

    // Use non-hoisted iterator to prevent invalidation during erase
    for (auto it = m_p->m_pendingTransactions.cbegin(); it != m_p->m_pendingTransactions.cend(); )
    {
        TransactionPtr ptr = it->second;

        boost::mutex::scoped_lock l1(ptr->m_lock);

        if (ptr->isExpired() || ptr->isExpiredByBlockNumber())
        {
            LOG() << __FUNCTION__ << std::endl << "order expired" << ptr;

            m_p->m_pendingTransactions.erase(it++);

            unlockUtxos(ptr->id());

            ++result;
        } else {
            ++it;
        }
    }

    if(result > 0)
        LOG() << "deleted " << result << "  expired transactions";

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

    boost::mutex::scoped_lock l(m_p->m_utxoLocker);
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
    boost::mutex::scoped_lock l(m_p->m_utxoLocker);

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
    boost::mutex::scoped_lock l(m_p->m_pendingTransactionsLock);

    auto txid = tx->id();
    m_p->m_pendingTransactions[txid]->m_lock.lock();

    // found, check if expired
    if (!m_p->m_pendingTransactions[txid]->isExpired())
    {
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

} // namespace xbridge
