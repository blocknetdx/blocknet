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
#include <memory>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class Exchange
{
    class Impl;

public:
    static Exchange & instance();

protected:
    Exchange();
    ~Exchange();

public:
    bool init();

    bool isEnabled();
    bool isStarted();

    bool haveConnectedWallet(const std::string & walletName);
    std::vector<std::string> connectedWallets() const;

    bool checkUtxoItems(const uint256 & txid,
                        const std::vector<wallet::UtxoEntry> & items);
    bool txOutIsLocked(const wallet::UtxoEntry & entry) const;

    bool createTransaction(const uint256                        & id,
                           const std::vector<unsigned char>     & sourceAddr,
                           const std::string                    & sourceCurrency,
                           const uint64_t                       & sourceAmount,
                           const std::vector<unsigned char>     & destAddr,
                           const std::string                    & destCurrency,
                           const uint64_t                       & destAmount,
                           const std::vector<wallet::UtxoEntry> & items,
                           const uint32_t                       & timestamp,
                           bool                                 & isCreated);

    bool acceptTransaction(const uint256                        & id,
                           const std::vector<unsigned char>     & sourceAddr,
                           const std::string                    & sourceCurrency,
                           const uint64_t                       & sourceAmount,
                           const std::vector<unsigned char>     & destAddr,
                           const std::string                    & destCurrency,
                           const uint64_t                       & destAmount,
                           const std::vector<wallet::UtxoEntry> & items);

    bool deletePendingTransactions(const uint256 & id);
    bool deleteTransaction(const uint256 & id);

    bool updateTransactionWhenHoldApplyReceived(const TransactionPtr & tx,
                                                const std::vector<unsigned char> & from);
    bool updateTransactionWhenInitializedReceived(const TransactionPtr & tx,
                                                  const std::vector<unsigned char> & from,
                                                  const uint256 & datatxid,
                                                  const std::vector<unsigned char> & pk);
    bool updateTransactionWhenCreatedReceived(const TransactionPtr & tx,
                                              const std::vector<unsigned char> & from,
                                              const std::string & binTxId,
                                              const std::vector<unsigned char> & innerScript);
    bool updateTransactionWhenConfirmedReceived(const TransactionPtr & tx,
                                                const std::vector<unsigned char> & from);

    const TransactionPtr      transaction(const uint256 & hash);
    const TransactionPtr      pendingTransaction(const uint256 & hash);
    std::list<TransactionPtr> pendingTransactions() const;
    std::list<TransactionPtr> transactions() const;
    std::list<TransactionPtr> finishedTransactions() const;

private:
    std::unique_ptr<Impl> m_p;
};

} // namespace xbridge

#endif // XBRIDGEEXCHANGE_H
