// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_TEST_STAKING_TESTS_H
#define BLOCKNET_TEST_STAKING_TESTS_H

#include <test/test_bitcoin.h>

#define protected public // for overridding protected fields in CChainParams
#include <chainparams.h>
#undef protected
#include <governance/governance.h>
#include <index/txindex.h>
#include <kernel.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <rpc/server.h>
#include <stakemgr.h>
#include <timedata.h>
#include <univalue.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

static UniValue CallRPC2(const std::string & strMethod, const UniValue & params) {
    JSONRPCRequest request;
    request.strMethod = strMethod;
    request.params = params;
    request.fHelp = false;
    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(request);
        return result;
    }
    catch (const UniValue& objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}

static void AddKey(CWallet & wallet, const CKey & key) {
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

static void rescanWallet(CWallet *w) {
    WalletRescanReserver reserver(w);
    reserver.reserve();
    w->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
}

static void removeGovernanceDBFiles() {
    try {
        fs::remove_all(GetDataDir() / "indexes" / "governance");
    } catch (...) {}
}

/**
 * Proof-of-Stake test chain.
 */
struct TestChainPoSData {
    std::string coinbase;
    std::vector<CBlock> blocks;
    int64_t mockTimeAfter;
    explicit TestChainPoSData() {
        CKey key;
        key.MakeNewKey(true);
        coinbase = EncodeSecret(key);
    }
};
extern std::map<std::string, std::shared_ptr<TestChainPoSData>> g_CachedTestChainPoS;
struct TestChainPoS : public TestingSetup {

    explicit TestChainPoS(const bool & init = true) : TestingSetup(CBaseChainParams::REGTEST) {
        if (init)
            Init("default"); // use default cached chain
    }

    /**
     * A cached version of the named chain will be used if available. First time a name
     * is used to create a sample chain, the chain is cached for use in other unit tests.
     * Specifying an empty string will not cache the chain.
     * @param name
     * @return
     */
    std::shared_ptr<TestChainPoSData> Init(const std::string & name="") {
        gov::Governance::instance().reset();

        if (!name.empty() && g_CachedTestChainPoS.count(name) && !g_CachedTestChainPoS[name]->blocks.empty()) {
            auto cachedChain = g_CachedTestChainPoS[name];
            coinbaseKey = DecodeSecret(cachedChain->coinbase);
            for (const auto & block : cachedChain->blocks) {
                auto blockptr = std::make_shared<const CBlock>(block);
                SetMockTime(blockptr->GetBlockTime());
                ProcessNewBlock(Params(), blockptr, true, nullptr);
                m_coinbase_txns.push_back(blockptr->vtx[0]);
            }
            SetMockTime(cachedChain->mockTimeAfter);

            ReloadWallet();

            // Turn on index for staking
            g_txindex = MakeUnique<TxIndex>(1 << 20, true);
            g_txindex->Start();
            g_txindex->Sync();

            // Stake some blocks
            StakeBlocks(5);
            return cachedChain;
        }

        auto cachedChain = std::make_shared<TestChainPoSData>();

        // set coin maturity to something small to help staking tests
        coinbaseKey.MakeNewKey(true);
        cachedChain->coinbase = EncodeSecret(coinbaseKey);
        CBasicKeyStore keystore; // temp used to spend coinbases
        keystore.AddKey(coinbaseKey);
        CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        for (int i = 0; i < Params().GetConsensus().lastPOWBlock; ++i) {
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing);
            std::vector<CMutableTransaction> txs;
            if (i > Params().GetConsensus().coinMaturity) {
                int j = i - Params().GetConsensus().coinMaturity;
                auto tx = m_coinbase_txns[j];
                CMutableTransaction mtx;
                mtx.vin.resize(1);
                mtx.vin[0] = CTxIn(COutPoint(tx->GetHash(), 0));
                mtx.vout.resize(2);
                for (int k = 0; k < (int)mtx.vout.size(); ++k) {
                    mtx.vout[k].scriptPubKey = scriptPubKey;
                    mtx.vout[k].nValue = (tx->GetValueOut()/mtx.vout.size()) - CENT;
                }
                const CTransaction txConst(mtx);
                SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[0]);
                ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, mtx.vout[0].nValue, SIGHASH_ALL), tx->vout[0].scriptPubKey, sigdata);
                UpdateInput(mtx.vin[0], sigdata);
                txs.push_back(mtx);
            }
            CBlock b = CreateAndProcessBlock(txs, scriptPubKey);
            cachedChain->blocks.push_back(b);
            m_coinbase_txns.push_back(b.vtx[0]);
        }
        cachedChain->mockTimeAfter = GetAdjustedTime();

        if (!name.empty())
            g_CachedTestChainPoS[name] = cachedChain;

        ReloadWallet();

        // Turn on index for staking
        g_txindex = MakeUnique<TxIndex>(1 << 20, true);
        g_txindex->Start();
        g_txindex->Sync();

        // Stake some blocks
        StakeBlocks(5);
        return cachedChain;
    }

    void ReloadWallet() {
        if (wallet) {
            UnregisterValidationInterface(wallet.get());
            RemoveWallet(wallet);
            wallet.reset();
        }

        bool firstRun;
        wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
        AddWallet(wallet); // add wallet to global mgr
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        {
            WalletRescanReserver reserver(wallet.get());
            reserver.reserve();
            wallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
        }
        wallet->SetBroadcastTransactions(true);
        RegisterValidationInterface(wallet.get());
    }

    CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey) {
        const CChainParams& chainparams = Params();
        std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
        CBlock& block = pblocktemplate->block;

        // Replace mempool-selected txns with just coinbase plus passed-in txns:
        block.vtx.resize(1);
        for (const CMutableTransaction& tx : txns)
            block.vtx.push_back(MakeTransactionRef(tx));
        // IncrementExtraNonce creates a valid coinbase and merkleRoot
        {
            LOCK(cs_main);
            unsigned int extraNonce = 0;
            IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);
        }

        while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
        ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

        return block;
    }

    void StakeBlocks(const int blockCount) {
        int tries{0};
        const int currentBlockHeight = chainActive.Height();
        while (chainActive.Height() < currentBlockHeight + blockCount) {
            CBlockIndex *pindex = nullptr;
            try {
                {
                    LOCK(cs_main);
                    pindex = chainActive.Tip();
                }
                std::vector<std::shared_ptr<CWallet>> wallets{wallet};
                if (pindex && staker.Update(wallets, pindex, Params().GetConsensus(), true) && staker.TryStake(pindex, Params())) {
                    LOCK(cs_main);
                    auto lc = wallet->chain().lock();
                    WalletRescanReserver reserver(wallet.get());
                    reserver.reserve();
                    wallet->ScanForWalletTransactions(lc->getBlockHash(chainActive.Height()), {}, reserver, true);
                }
            } catch (std::exception & e) {
                LogPrintf("Staker ran into an exception: %s\n", e.what());
                throw e;
            } catch (...) {
                throw std::runtime_error("Staker unknown error");
            }
            if (++tries > 1000)
                throw std::runtime_error("Staker failed to find stake");
            auto stime = staker.LastUpdateTime();
            if (stime == 0)
                stime = GetAdjustedTime();
            SetMockTime(stime + Params().GetConsensus().PoSFutureBlockTimeLimit(pindex->GetBlockTime()));
        }
    }

    StakeMgr::StakeCoin FindStake() {
        const int currentBlockHeight = chainActive.Height();
        CBlockIndex *tip = nullptr;
        {
            LOCK(cs_main);
            tip = chainActive.Tip();
        }
        int tries{0};
        const CChainParams & params = Params();
        while (true) {
            try {
                std::vector<std::shared_ptr<CWallet>> wallets{wallet};
                if (staker.Update(wallets, tip, params.GetConsensus(), true)) {
                    std::vector<StakeMgr::StakeCoin> nextStakes;
                    if (!staker.NextStake(nextStakes, tip, params))
                        continue;
                    for (const auto & ns : nextStakes) {
                        auto blocktemplate = BlockAssembler(params).CreateNewBlockPoS(*ns.coin, ns.hashBlock, ns.time, ns.blockTime, ns.wallet.get(), true);
                        uint256 haspos;
                        if (CheckProofOfStake(blocktemplate->block, tip, haspos, params.GetConsensus()))
                            return ns;
                    }
                }
            } catch (std::exception & e) {
                LogPrintf("Staker ran into an exception: %s\n", e.what());
            } catch (...) { }
            if (++tries > 1000)
                throw std::runtime_error("Staker failed to find stake");
            auto stime = staker.LastUpdateTime();
            if (stime == 0)
                stime = GetAdjustedTime();
            SetMockTime(stime + params.GetConsensus().PoSFutureBlockTimeLimit(tip->GetBlockTime()));
        }
    }

    ~TestChainPoS() {
        if (g_txindex) {
            g_txindex->Stop();
            g_txindex.reset();
        }
        if (wallet) {
            UnregisterValidationInterface(wallet.get());
            RemoveWallet(wallet);
            wallet.reset();
        }
        m_coinbase_txns.clear();
        locked_chain.reset();
        chain.reset();
        // clean governance db
        removeGovernanceDBFiles();
        gov::Governance::instance().reset();
    };

    std::unique_ptr<interfaces::Chain> chain = interfaces::MakeChain();
    std::unique_ptr<interfaces::Chain::Lock> locked_chain = chain->assumeLocked();  // Temporary. Removed in upcoming lock cleanup
    std::shared_ptr<CWallet> wallet;
    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
    std::vector<CTransactionRef> m_coinbase_txns; // For convenience, coinbase transactions
    StakeMgr staker;
};

#endif //BLOCKNET_TEST_STAKING_TESTS_H