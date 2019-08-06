// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <amount.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <index/txindex.h>
#include <kernel.h>
#include <pow.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <validation.h>
#include <wallet/wallet.h>

// Proof-of-Stake tests
BOOST_AUTO_TEST_SUITE(staking_tests)

/// Ensure that the mempool won't accept coinstake transactions.
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

/// Check that v03 staking modifier doesn't change for each new selection interval
BOOST_AUTO_TEST_CASE(staking_tests_v03modifier)
{
    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.stakingV05UpgradeTime = std::numeric_limits<int>::max(); // set far into future
    pos.Init();
    pos.StakeBlocks(40);

    const auto endTime = GetAdjustedTime() + GetStakeModifierSelectionInterval()*20;
    uint64_t firstStakeModifier{0};
    const auto stakeIndex = chainActive[chainActive.Height() - 40];
    CBlock stakeBlock; BOOST_CHECK(ReadBlockFromDisk(stakeBlock, stakeIndex, params->consensus));
    while (GetAdjustedTime() < endTime) {
        int64_t runningTime = GetAdjustedTime();
        uint64_t nStakeModifier{0};
        int nStakeModifierHeight{0};
        int64_t nStakeModifierTime{0};
        BOOST_CHECK(GetKernelStakeModifier(chainActive.Tip(), stakeBlock.GetHash(), runningTime, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false));
        if (firstStakeModifier == 0)
            firstStakeModifier = nStakeModifier;
        else BOOST_CHECK_MESSAGE(nStakeModifier == firstStakeModifier, "Stake modifier v03 should be the same indefinitely");
        pos.StakeBlocks(1);
    }
}

/// Check that v05 staking modifier changes for each new selection interval
BOOST_AUTO_TEST_CASE(staking_tests_v05modifier)
{
    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime(); // set to current
    pos.Init();
    pos.StakeBlocks(15);

    const auto endTime = GetAdjustedTime() + GetStakeModifierSelectionInterval()*20;
    uint64_t lastStakeModifier{0};
    const auto stakeIndex = chainActive[chainActive.Height() - 10];
    CBlock stakeBlock; BOOST_CHECK(ReadBlockFromDisk(stakeBlock, stakeIndex, params->consensus));
    while (GetAdjustedTime() < endTime) {
        int64_t runningTime = GetAdjustedTime();
        uint64_t nStakeModifier{0};
        int nStakeModifierHeight{0};
        int64_t nStakeModifierTime{0};
        BOOST_CHECK(GetKernelStakeModifier(chainActive.Tip(), stakeBlock.GetHash(), runningTime, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false));
        if (lastStakeModifier > 0 && chainActive.Height() % 2 == 0)
            BOOST_CHECK_MESSAGE(nStakeModifier != lastStakeModifier, "Stake modifier should be different for every new selection interval");
        lastStakeModifier = nStakeModifier;
        pos.StakeBlocks(1);
    }
}

/// Check that the v05 staking protocol upgrade works properly
BOOST_AUTO_TEST_CASE(staking_tests_protocolupgrade)
{
    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.stakingV05UpgradeTime = std::numeric_limits<int>::max();
    pos.Init();

    // Switch on the upgrade
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime() + params->GetConsensus().nPowTargetSpacing * 10; // set 10 min in future (~10 blocks)
    int blocks = chainActive.Height();
    pos.StakeBlocks(25); // make sure seemless upgrade to v05 staking protocol occurs
    BOOST_CHECK_EQUAL(chainActive.Height(), blocks + 25);
}

/// Ensure that bad stakes are not accepted by the protocol.
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

    auto scanWalletTxs = [](CWallet *wallet) {
        // Make sure wallet is updated with all utxos
        LOCK(cs_main);
        auto locked_chain = wallet->chain().lock();
        WalletRescanReserver reserver(wallet);
        reserver.reserve();
        wallet->ScanForWalletTransactions(locked_chain->getBlockHash(0), {}, reserver, true);
    };

    CPubKey paymentPubKey;
    CTxDestination stakeInputDest;
    ExtractDestination(m_coinbase_txns[0]->vout[0].scriptPubKey, stakeInputDest);
    const auto keyid = GetKeyForDestination(*wallet, stakeInputDest);
    wallet->GetPubKey(keyid, paymentPubKey);
    CScript paymentScript = CScript() << ToByteVector(paymentPubKey) << OP_CHECKSIG;
    const CAmount stakeSubsidy = GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
    const CAmount stakeAmount = stakeSubsidy - 2000; // account for basic fee in sats

    // Find a stake
    {
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet));
    }

    // Check valid coinstake
    {
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet));
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
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to update on stake");
    }

    // Check invalid stake amount
    {
        scanWalletTxs(wallet.get());
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
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
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex should not update on bad stake");
    }

    // Check for invalid coinstake (more than 1 vin)
    {
        scanWalletTxs(wallet.get());
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
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
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex should not update on bad stake");
    }

    // TODO Blocknet PoS unit test for p2pkh stakes
}

BOOST_AUTO_TEST_SUITE_END()
