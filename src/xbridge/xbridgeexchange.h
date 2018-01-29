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
    /**
     * @brief instance - classical implementation of singletone
     * @return
     */
    static Exchange & instance();

protected:
    /**
     * @brief Exchange - default constructor, init private implementation
     */
    Exchange();
    ~Exchange();

public:
    /**
     * @brief init - init exchange Wallets from settings
     * @return
     */
    bool init();

    /**
     * @brief isEnabled
     * @return true, if list of exchange wallets not empty and set flag -enableexchange
     */
    bool isEnabled();
    /**
     * @brief isStarted
     * @return true, enabled and servicenode started
     */
    bool isStarted();

    /**
     * @brief haveConnectedWallet
     * @param walletName -
     * @return true, if echange connected to wallet
     */
    bool haveConnectedWallet(const std::string & walletName);
    /**
     * @brief connectedWallets
     * @return list of connected wallets
     */
    std::vector<std::string> connectedWallets() const;

    /**
     * @brief checkUtxoItems
     * @param txid
     * @param items
     * @return
     */
    bool checkUtxoItems(const uint256 & txid,
                        const std::vector<wallet::UtxoEntry> & items);
    /**
     * @brief txOutIsLocked
     * @param entry
     * @return
     */
    bool txOutIsLocked(const wallet::UtxoEntry & entry) const;

    /**
     * @brief createTransaction
     * @param id
     * @param sourceAddr
     * @param sourceCurrency
     * @param sourceAmount
     * @param destAddr
     * @param destCurrency
     * @param destAmount
     * @param items
     * @param timestamp
     * @param blockHash
     * @param isCreated
     * @return
     */
    bool createTransaction(const uint256                        & id,
                           const std::vector<unsigned char>     & sourceAddr,
                           const std::string                    & sourceCurrency,
                           const uint64_t                       & sourceAmount,
                           const std::vector<unsigned char>     & destAddr,
                           const std::string                    & destCurrency,
                           const uint64_t                       & destAmount,
                           const std::vector<wallet::UtxoEntry> & items,
                           const uint32_t                       & timestamp,
                           uint256                              & blockHash,
                           bool                                 & isCreated);

    /**
     * @brief acceptTransaction check transaction state, if trNew - do nothing,
     * if trJoined = send hold to client
     * @param id
     * @param sourceAddr
     * @param sourceCurrency
     * @param sourceAmount
     * @param destAddr
     * @param destCurrency
     * @param destAmount
     * @param items
     * @return
     */
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

    /**
     * @brief updateTransactionWhenHoldApplyReceived -
     * @param tx - transaction
     * @param from - source address
     * @return true, if after increase transaction state == Hold
     */
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
