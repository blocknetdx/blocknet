//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEEXCHANGE_H
#define XBRIDGEEXCHANGE_H

#include "uint256.h"
#include "xbridgetransaction.h"
#include "xbridgewallet.h"

#include <string>
#include <set>
#include <map>
#include <list>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

//*****************************************************************************
//*****************************************************************************
typedef std::pair<std::string, std::string> StringPair;

//*****************************************************************************
//*****************************************************************************
class XBridgeExchange
{
public:
    static XBridgeExchange & instance();

protected:
    XBridgeExchange();
    ~XBridgeExchange();

public:
    bool init();

    bool isEnabled();
    bool isStarted();

    bool haveConnectedWallet(const std::string & walletName);

    // std::vector<unsigned char> walletAddress(const std::string & walletName);

    bool createTransaction(const uint256     & id,
                           const std::string & sourceAddr,
                           const std::string & sourceCurrency,
                           const uint64_t    & sourceAmount,
                           const std::string & destAddr,
                           const std::string & destCurrency,
                           const uint64_t    & destAmount,
                           uint256           & pendingId,
                           bool              & isCreated);

    bool acceptTransaction(const uint256     & id,
                           const std::string & sourceAddr,
                           const std::string & sourceCurrency,
                           const uint64_t    & sourceAmount,
                           const std::string & destAddr,
                           const std::string & destCurrency,
                           const uint64_t    & destAmount,
                           uint256           & transactionId);

    bool deletePendingTransactions(const uint256 & id);
    bool deleteTransaction(const uint256 & id);

    bool updateTransactionWhenHoldApplyReceived(XBridgeTransactionPtr tx,
                                                const std::string & from);
    bool updateTransactionWhenInitializedReceived(XBridgeTransactionPtr tx,
                                                  const std::string & from,
                                                  const uint256 & datatxid,
                                                  const xbridge::CPubKey & pk);
    bool updateTransactionWhenCreatedReceived(XBridgeTransactionPtr tx,
                                              const std::string & from,
                                              const std::string & binTxId,
                                              const std::string & innerScript);
    bool updateTransactionWhenConfirmedReceived(XBridgeTransactionPtr tx,
                                                const std::string & from);

    bool updateTransaction(const uint256 & hash);

    const XBridgeTransactionPtr transaction(const uint256 & hash);
    const XBridgeTransactionPtr pendingTransaction(const uint256 & hash);
    std::list<XBridgeTransactionPtr> pendingTransactions() const;
    std::list<XBridgeTransactionPtr> transactions() const;
    std::list<XBridgeTransactionPtr> finishedTransactions() const;
    std::list<XBridgeTransactionPtr> transactionsHistory() const;
    void addToTransactionsHistory(const uint256 & id);

    std::vector<StringPair> listOfWallets() const;

private:
    std::list<XBridgeTransactionPtr> transactions(bool onlyFinished) const;

private:
    // connected wallets
    typedef std::map<std::string, WalletParam> WalletList;
    WalletList                               m_wallets;

    mutable boost::mutex                     m_pendingTransactionsLock;
    std::map<uint256, XBridgeTransactionPtr> m_pendingTransactions;

    mutable boost::mutex                     m_transactionsLock;
    std::map<uint256, XBridgeTransactionPtr> m_transactions;

    mutable boost::mutex                     m_transactionsHistoryLock;
    std::map<uint256, XBridgeTransactionPtr> m_transactionsHistory;

    mutable boost::mutex                     m_unconfirmedLock;
    std::map<std::string, uint256>           m_unconfirmed;

    // TODO use deque and limit size
    std::set<uint256>                        m_walletTransactions;

    mutable boost::mutex                     m_knownTxLock;
    std::set<uint256>                        m_knownTransactions;
};

#endif // XBRIDGEEXCHANGE_H
