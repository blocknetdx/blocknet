// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_TXINDEX_H
#define BITCOIN_INDEX_TXINDEX_H

#include <chain.h>
#include <index/base.h>
#include <txdb.h>

/**
 * TxIndex is used to look up transactions included in the blockchain by hash.
 * The index is written to a LevelDB database and records the filesystem
 * location of each transaction by transaction hash.
 */
class TxIndex final : public BaseIndex
{
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

protected:
    /// Override base class init to migrate from old database.
    bool Init() override;

    bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) override;

    BaseIndex::DB& GetDB() const override;

    const char* GetName() const override { return "txindex"; }

    void ThreadSync();

public:
    /// Constructs the index, which becomes available to be queried.
    explicit TxIndex(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    // Destructor is declared because this class contains a unique_ptr to an incomplete type.
    virtual ~TxIndex() override;

    /// Look up a transaction by hash.
    ///
    /// @param[in]   tx_hash  The hash of the transaction to be returned.
    /// @param[out]  block_hash  The hash of the block the transaction is found in.
    /// @param[out]  tx  The transaction itself.
    /// @return  true if transaction is found, false otherwise
    bool FindTx(const uint256& tx_hash, uint256& block_hash, CTransactionRef& tx) const;

    /// Override start command
    void Start() {
        if (!Init()) {
            FatalError("%s: %s failed to initialize", __func__, GetName());
            return;
        }
    }

    /// Sync up to the current tip.
    void Sync() {
        CBlockLocator locator;
        if (!GetDB().ReadBestBlock(locator)) {
            locator.SetNull();
        }

        {
            LOCK(cs_main);
            if (locator.IsNull()) {
                m_best_block_index = nullptr;
            } else {
                m_best_block_index = FindForkInGlobalIndex(chainActive, locator);
            }
            m_synced = m_best_block_index.load() == chainActive.Tip();
        }

        ThreadSync();
    }

    /// Connect block to the index
    void BlockConnectedSync(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                            const std::vector<CTransactionRef>& txn_conflicted) {
        BlockConnected(block, pindex, txn_conflicted);
    }

    /// Write block index
    void ChainStateFlushedSync(const CBlockLocator& locator) {
        ChainStateFlushed(locator);
    }

    /// Returns txindex best block index
    const CBlockIndex* BestBlockIndex() {
        return m_best_block_index;
    }

private:
    void writeBestBlock(const int height);
};

/// The global transaction index, used in GetTransaction. May be null.
extern std::unique_ptr<TxIndex> g_txindex;

#endif // BITCOIN_INDEX_TXINDEX_H
