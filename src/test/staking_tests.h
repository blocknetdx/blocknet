// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_STAKING_TESTS_H
#define BLOCKNET_STAKING_TESTS_H

#include <test/test_bitcoin.h>

#define protected public // for overridding protected fields in CChainParams
#include <chainparams.h>
#undef protected
#include <index/txindex.h>
#include <kernel.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <rpc/server.h>
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

/**
 * Proof-of-Stake test chain.
 */
struct TestChainPoS : public TestingSetup {
    explicit TestChainPoS(bool init = true) : TestingSetup(CBaseChainParams::REGTEST) {
        if (init)
            Init();
    }

    void Init() {
        // set coin maturity to something small to help staking tests
        coinbaseKey.MakeNewKey(true);
        CBasicKeyStore keystore; // temp used to spend coinbases
        keystore.AddKey(coinbaseKey);

        CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        for (int i = 0; i < Params().GetConsensus().lastPOWBlock; ++i) {
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing); // prevent difficulty from increasing too rapidly
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
            m_coinbase_txns.push_back(b.vtx[0]);
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

        // Turn on index for staking
        g_txindex = MakeUnique<TxIndex>(1 << 20, true);
        g_txindex->Start();
        g_txindex->Sync();

        // Stake some blocks
        StakeBlocks(5);
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

        CBlock result = block;
        return result;
    }

    void StakeBlocks(const int blockCount) {
        int tries{0};
        const int currentBlockHeight = chainActive.Height();
        while (chainActive.Height() < currentBlockHeight + blockCount) {
            try {
                CBlockIndex *pindex = nullptr;
                {
                    LOCK(cs_main);
                    pindex = chainActive.Tip();
                }
                std::vector<std::shared_ptr<CWallet>> wallets{wallet};
                if (pindex && staker.Update(wallets, pindex, Params().GetConsensus()) && staker.TryStake(pindex, Params())) {
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
            SetMockTime(staker.LastUpdateTime() + MAX_FUTURE_BLOCK_TIME_POS);
        }
    }

    ~TestChainPoS() {
        RemoveWallet(wallet);
        g_txindex->Stop();
        g_txindex.reset();
    };

    std::unique_ptr<interfaces::Chain> chain = interfaces::MakeChain();
    std::unique_ptr<interfaces::Chain::Lock> locked_chain = chain->assumeLocked();  // Temporary. Removed in upcoming lock cleanup
    std::shared_ptr<CWallet> wallet;
    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
    std::vector<CTransactionRef> m_coinbase_txns; // For convenience, coinbase transactions
    StakeMgr staker;
};

#endif //BLOCKNET_STAKING_TESTS_H