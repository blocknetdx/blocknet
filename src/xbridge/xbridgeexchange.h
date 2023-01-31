// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEEXCHANGE_H
#define BLOCKNET_XBRIDGE_XBRIDGEEXCHANGE_H

#include <xbridge/xbridgepacket.h>
#include <xbridge/xbridgetransaction.h>
#include <xbridge/xbridgewallet.h>

#include <uint256.h>
#include <sync.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

extern bool ExchangeUtxos(const uint256 & txid, std::vector<wallet::UtxoEntry> & items);

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
     * @brief Load the running wallets.
     * @return
     */
    bool loadWallets(std::set<std::string> & wallets);

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

    // public-private keys (service node key pair)
    /**
     * @brief pubKey
     * @return service node public key
     */
    const std::vector<unsigned char> & pubKey() const;
    /**
     * @brief privKey
     * @return service node private key
     */
    const std::vector<unsigned char> & privKey() const;

    /**
     * @brief haveConnectedWallet
     * @param walletName -
     * @return true, if echange connected to wallet
     */
    bool haveConnectedWallet(const std::string & walletName);
    /**
     * @brief connectedWallets
     * @return  vector of connected wallets
     */
    std::vector<std::string> connectedWallets() const;

    bool checkUtxoItems(const uint256 & txid,
                        const std::vector<wallet::UtxoEntry> & items);
    bool getUtxoItems(const uint256 & txid,
                      std::vector<wallet::UtxoEntry> & items);

    /**
     * @brief createTransaction - create new partial order xbridge transaction
     * @param id - id of transaction
     * @param sourceAddr - source address
     * @param sourceCurrency - source currency
     * @param sourceAmount - source
     * @param destAddr - destination address
     * @param destCurrency - destination currency
     * @param destAmount - destination amount
     * @param timestamp - time of created
     * @param mpubkey -
     * @param items
     * @param blockHash
     * @param isCreated operation status
     * @param isPartialOrder
     * @param minFromAmount
     * @return true, if transaction created
     */
    bool createTransaction(const uint256                        & id,
                           const std::vector<unsigned char>     & sourceAddr,
                           const std::string                    & sourceCurrency,
                           const int64_t                       & sourceAmount,
                           const std::vector<unsigned char>     & destAddr,
                           const std::string                    & destCurrency,
                           const int64_t                       & destAmount,
                           const int64_t                       & timestamp,
                           const std::vector<unsigned char>     & mpubkey,
                           const std::vector<wallet::UtxoEntry> & items,
                           uint256                              & blockHash,
                           bool                                 & isCreated,
                           bool                                   isPartialOrder,
                           int64_t                               minFromAmount);

    /**
     * @brief acceptTransaction - accept xbridge transaction
     * @param id - if of transaction
     * @param sourceAddr - source address
     * @param sourceCurrency - source currency
     * @param sourceAmount - source amount
     * @param destAddr - destination address
     * @param destCurrency - destination currency
     * @param destAmount - destination amount
     * @param mpubkey
     * @param items
     * @param isPartialOrderAllowed
     * @return return true, if transaction accepted success
     */
    bool acceptTransaction(const uint256                        & id,
                           const std::vector<unsigned char>     & sourceAddr,
                           const std::string                    & sourceCurrency,
                           const int64_t                       & sourceAmount,
                           const std::vector<unsigned char>     & destAddr,
                           const std::string                    & destCurrency,
                           const int64_t                       & destAmount,
                           const std::vector<unsigned char>     & mpubkey,
                           const std::vector<wallet::UtxoEntry> & items,
                           bool                                   isPartialOrderAllowed);

    /**
     * @brief deletePendingTransaction - delete transaction, unlocked items
     * @param id - id of transaction
     * @return true, if transaction found and removed
     */
    bool deletePendingTransaction(const uint256 & id);
    /**
     * @brief deleteTransaction - delete transaction, unlock utxo items
     * @param id - id of transaction
     * @return true, if transaction found and removed
     */
    bool deleteTransaction(const uint256 & id);

    /**
     * @brief updateTransactionWhenHoldApplyReceived - increase transaction state counter
     * @param tx - pointer to transaction description
     * @param from
     * @return true, if all check success and new transaction state == trHold
     */
    bool updateTransactionWhenHoldApplyReceived(const TransactionPtr & tx,
                                                const std::vector<unsigned char> & from);
    /**
     * @brief updateTransactionWhenInitializedReceived -
     * @param tx
     * @param from
     * @param pk
     * @return true, if all checks success and new transaction state == trInizialized
     */
    bool updateTransactionWhenInitializedReceived(const TransactionPtr & tx,
                                                  const std::vector<unsigned char> & from,
                                                  const std::vector<unsigned char> & pk);
    /**
     * @brief updateTransactionWhenCreatedReceived - increase transaction state
     * @param tx
     * @param from
     * @param binTxId
     * @param innerScript
     * @return true, if all check success and new transaction state == trCreated
     */
    bool updateTransactionWhenCreatedReceived(const TransactionPtr & tx,
                                              const std::vector<unsigned char> & from,
                                              const std::string & binTxId);
    /**
     * @brief updateTransactionWhenConfirmedReceived
     * @param tx
     * @param from
     * @return true, if all checks success and new transaction state == trFinished
     */
    bool updateTransactionWhenConfirmedReceived(const TransactionPtr & tx,
                                                const std::vector<unsigned char> & from);

    /**
     * @brief transaction find transaction
     * @param hash - hash/id? of transaction
     * @return a pointer to the found transaction, or new instace of transaction
     */
    const TransactionPtr      transaction(const uint256 & hash);
    /**
     * @brief pendingTransaction - find pending transaction
     * @param hash - hash/id? of transaction
     * @return a pointer to the found transaction, or new instace of transaction
     */
    const TransactionPtr      pendingTransaction(const uint256 & hash);
    /**
     * @brief pendingTransactions
     * @return list of pending (open)
     */
    std::list<TransactionPtr> pendingTransactions() const;
    /**
     * @brief transactions
     * @return list of transactions
     */
    std::list<TransactionPtr> transactions() const;
    /**
     * @brief finishedTransactions
     * @return list of finished transactions
     */
    std::list<TransactionPtr> finishedTransactions() const;

    /**
     * @brief eraseExpiredTransactions - erase expired transaction
     * @return status of operation
     */
    size_t eraseExpiredTransactions();

    /**
     * @brief lockUtxos - locks the utxo's with the specified tx id
     * @param id - id of transaction
     * @return true, if utxo's were added
     */
    bool lockUtxos(const uint256 &id, const std::vector<wallet::UtxoEntry> &items);

    /**
     * @brief unlockUtxos - unlocks the utxo's with the specified tx id
     * @param id - id of transaction
     * @return true, if utxo's were removed
     */
    bool unlockUtxos(const uint256 & id);

    /**
     * @brief Update the transaction timestamp. Also removes expired transactions from pending data store.
     * @param tx - Transaction pointer
     * @return true if transaction timestamp was updated, otherwise false if it expired
     */
    bool updateTimestampOrRemoveExpired(const TransactionPtr & tx);

    /**
     * @brief Check if the maker's utxos are still valid and unspent. Return false if they are invalid, otherwise
     *        return true for all other cases. This check should only check every N seconds. See implementation for
     *        specific time.
     * @param tx
     * @return
     * @return
     */
    bool makerUtxosAreStillValid(const TransactionPtr & tx);

private:
    std::unique_ptr<Impl> m_p;
    mutable CCriticalSection m_lock;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEEXCHANGE_H
