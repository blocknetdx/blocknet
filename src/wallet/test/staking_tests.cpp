// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_bitcoin.h>

#include <amount.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <index/txindex.h>
#include <kernel.h>
#include <miner.h>
#include <pow.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <txmempool.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

// Proof-of-Stake tests
BOOST_AUTO_TEST_SUITE(staking_tests)

static void AddKey(CWallet & wallet, const CKey & key) {
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

/**
 * Proof-of-Stake test chain.
 */
struct TestChainPoS : public TestingSetup {
    TestChainPoS() : TestingSetup(CBaseChainParams::REGTEST) {
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
                mtx.vout.resize(1);
                mtx.vout[0].scriptPubKey = scriptPubKey;
                mtx.vout[0].nValue = tx->GetValueOut() - CENT;
                const CTransaction txConst(mtx);
                SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[0]);
                ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, mtx.vout[0].nValue, SIGHASH_ALL), tx->vout[0].scriptPubKey, sigdata);
                UpdateInput(mtx.vin[0], sigdata);
                ScriptError serror = SCRIPT_ERR_OK;
                BOOST_CHECK(VerifyScript(mtx.vin[0].scriptSig, tx->vout[0].scriptPubKey, &mtx.vin[0].scriptWitness,
                        STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, 0, mtx.vout[0].nValue), &serror));
                txs.push_back(mtx);
            }
            CBlock b = CreateAndProcessBlock(txs, scriptPubKey);
            m_coinbase_txns.push_back(b.vtx[0]);
        }

        bool firstRun;
        wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        {
            WalletRescanReserver reserver(wallet.get());
            reserver.reserve();
            wallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {} /* stop_block */, reserver, false /* update */);
        }

        // Turn on index for staking
        g_txindex = MakeUnique<TxIndex>(1 << 20, true);
        g_txindex->Start();
        while (!g_txindex->BlockUntilSyncedToCurrentChain());

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
                    auto locked_chain = wallet->chain().lock();
                    WalletRescanReserver reserver(wallet.get());
                    reserver.reserve();
                    wallet->ScanForWalletTransactions(locked_chain->getBlockHash(chainActive.Height()), {} /* stop_block */, reserver, true /* update */);
                }
                while (!g_txindex->BlockUntilSyncedToCurrentChain());
            } catch (std::exception & e) {
                LogPrintf("Staker ran into an exception: %s\n", e.what());
            } catch (...) { }
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing);
        }
    }

    ~TestChainPoS() {
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

/**
 * Ensure that the mempool won't accept coinstake transactions.
 */
BOOST_FIXTURE_TEST_CASE(staking_tests_nocoinstake, TestChainPoS)
{
    CMutableTransaction coinstake;
    coinstake.nVersion = 1;
    coinstake.vin.resize(1);
    coinstake.vout.resize(2);

    // Sign valid input
    CBlock block; ReadBlockFromDisk(block, chainActive.Tip(), Params().GetConsensus());
    CTransactionRef tx = block.vtx[1];
    coinstake.vin[0].prevout = COutPoint(block.vtx[1]->GetHash(), 1);
    const CTransaction txConst(coinstake);
    SignatureData sigdata = DataFromTransaction(coinstake, 0, tx->vout[1]);
    ProduceSignature(*wallet, MutableTransactionSignatureCreator(&coinstake, 0, coinstake.vout[0].nValue, SIGHASH_ALL), tx->vout[1].scriptPubKey, sigdata);
    UpdateInput(coinstake.vin[0], sigdata);

    coinstake.vout[0].SetNull();
    coinstake.vout[0].nValue = 0;
    coinstake.vout[1].nValue = COIN;
    coinstake.vout[1].scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

    BOOST_CHECK(CTransaction(coinstake).IsCoinStake());

    CValidationState state;
    LOCK(cs_main);
    const auto initialPoolSize = mempool.size();
    BOOST_CHECK_EQUAL(false, AcceptToMemoryPool(mempool, state, MakeTransactionRef(coinstake),
                                                nullptr /* pfMissingInputs */,
                                                nullptr /* plTxnReplaced */,
                                                true /* bypass_limits */,
                                                0 /* nAbsurdFee */));

    // Check that the transaction hasn't been added to mempool.
    BOOST_CHECK_EQUAL(mempool.size(), initialPoolSize);

    // Check that the validation state reflects the unsuccessful attempt.
    BOOST_CHECK(state.IsInvalid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "coinstake");

    int nDoS;
    BOOST_CHECK_EQUAL(state.IsInvalid(nDoS), true);
    BOOST_CHECK_EQUAL(nDoS, 100);
}

/**
 * Ensure that bad stakes are not accepted by the protocol.
 */
BOOST_FIXTURE_TEST_CASE(staking_tests_badstakes, TestChainPoS)
{
    // Find a stake
    auto findStake = [](StakeMgr::StakeCoin & nextStake, StakeMgr & staker, const CBlockIndex *tip, std::shared_ptr<CWallet> wallet) -> bool {
        while (true) {
            try {
                std::vector<std::shared_ptr<CWallet>> wallets{wallet};
                if (staker.Update(wallets, tip, Params().GetConsensus()))
                    break;
            } catch (std::exception & e) {
                LogPrintf("Staker ran into an exception: %s\n", e.what());
            } catch (...) { }
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing);
        }

        // Get a valid stake and then modify it to try and cheat the protocol
        return staker.NextStake(nextStake, tip, Params());
    };

    auto createStakeBlock = [](const StakeMgr::StakeCoin & nextStake, const CBlockIndex *tip, CBlock & block, CMutableTransaction & coinbaseTx, CMutableTransaction & coinstakeTx) {
        // Create coinbase transaction.
        coinbaseTx.vin.resize(1);
        coinbaseTx.vin[0].prevout.SetNull();
        coinbaseTx.vout.resize(1);
        coinbaseTx.vout[0].SetNull();
        coinbaseTx.vout[0].nValue = 0;
        coinbaseTx.vin[0].scriptSig = CScript() << tip->nHeight+1 << OP_0;
        // Create coinstake transaction
        coinstakeTx.vin.resize(1);
        coinstakeTx.vin[0] = CTxIn(nextStake.coin->outpoint);
        coinstakeTx.vout.resize(2);
        coinstakeTx.vout[0].SetNull(); // coinstake
        coinstakeTx.vout[0].nValue = 0;
        // Fill in the block txs
        block.SetNull();
        block.vtx.resize(2);
        // Fill in header
        block.nVersion = ComputeBlockVersion(tip, Params().GetConsensus());
        if (Params().MineBlocksOnDemand())
            block.nVersion = gArgs.GetArg("-blockversion", block.nVersion);
        block.hashPrevBlock  = tip->GetBlockHash();
        block.nTime          = static_cast<uint32_t>(nextStake.time);
        block.nBits          = GetNextWorkRequired(tip, nullptr, Params().GetConsensus());
        block.nNonce         = 0;
        block.hashStake      = nextStake.coin->outpoint.hash;
        block.nStakeIndex    = nextStake.coin->outpoint.n;
        block.nStakeAmount   = nextStake.coin->txout.nValue;
        block.hashStakeBlock = nextStake.hashBlock;
    };

    auto signCoinstake = [](CMutableTransaction & coinstakeTx, CWallet *wallet) -> bool {
        // Sign stake input w/ keystore
        auto locked_chain = wallet->chain().lock();
        LOCK(wallet->cs_wallet);
        return wallet->SignTransaction(coinstakeTx);
    };

    // Find the first stake
    StakeMgr::StakeCoin nextStake;
    BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet));

    const CAmount stakeSubsidy = GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
    const CAmount stakeAmount = stakeSubsidy - 2000; // account for basic fee in sats
    CTxDestination stakeInputDest;
    BOOST_CHECK(ExtractDestination(nextStake.coin->txout.scriptPubKey, stakeInputDest));
    const auto keyid = GetKeyForDestination(*wallet, stakeInputDest);
    BOOST_CHECK(!keyid.IsNull());
    CPubKey paymentPubKey;
    BOOST_CHECK(wallet->GetPubKey(keyid, paymentPubKey));
    CScript paymentScript = CScript() << ToByteVector(paymentPubKey) << OP_CHECKSIG;
    // TODO Blocknet PoS unit test for p2pkh stakes

    CBlock block;
    uint256 blockHash;
    CMutableTransaction coinbaseTx;
    CMutableTransaction coinstakeTx;

    //
    // Check valid coinstake
    //
    createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
    coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
    BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
    block.vtx[1] = MakeTransactionRef(coinstakeTx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
    const auto currentIndex1 = chainActive.Tip();
    BOOST_CHECK(ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr));
    BOOST_CHECK(currentIndex1 != chainActive.Tip()); // chain tip should not match previous
    BOOST_CHECK_EQUAL(block.GetHash(), chainActive.Tip()->GetBlockHash()); // block should be accepted
    BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to updated on stake");

    //
    // Check invalid stake amount
    //
    nextStake.SetNull();
    BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet));
    createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
    coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount*100, paymentScript); // staker payment
    BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
    block.vtx[1] = MakeTransactionRef(coinstakeTx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
    blockHash = chainActive.Tip()->GetBlockHash();
    ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr);
    BOOST_CHECK_EQUAL(blockHash, chainActive.Tip()->GetBlockHash()); // block should not be accepted
    BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to updated on stake");

    //
    // Check for invalid coinstake (more than 1 vin)
    //
    {
        // Make sure wallet is updated with all utxos
        LOCK(cs_main);
        auto locked_chain = wallet->chain().lock();
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(locked_chain->getBlockHash(0), {} /* stop_block */, reserver, true /* update */);
    }
    nextStake.SetNull();
    BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet));
    createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
    CTransactionRef anotherInput = nullptr;
    for (int i = static_cast<int>(m_coinbase_txns.size() - 1); i >= 0; --i) {
        CTransactionRef tx = m_coinbase_txns[i];
        if (tx->GetHash() == nextStake.coin->outpoint.hash) // skip staked output
            continue;
        anotherInput = tx;
        break;
    }
    BOOST_CHECK_MESSAGE(anotherInput != nullptr, "Failed to find second vin for use with multiple stakes test");
    coinstakeTx.vin.resize(2);
    coinstakeTx.vin[1] = CTxIn(COutPoint(anotherInput->GetHash(), 0)); // sample valid input
    coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
    BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
    block.vtx[1] = MakeTransactionRef(coinstakeTx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
    blockHash = chainActive.Tip()->GetBlockHash();
    ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr);
    BOOST_CHECK_EQUAL(blockHash, chainActive.Tip()->GetBlockHash()); // block should not be accepted
    BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to updated on stake");
}

BOOST_AUTO_TEST_SUITE_END()
