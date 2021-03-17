// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <consensus/merkle.h>
#include <core_io.h>
#include <net.h>
#include <node/transaction.h>
#include <wallet/coincontrol.h>

std::map<std::string, std::shared_ptr<TestChainPoSData>> g_CachedTestChainPoS;

// Proof-of-Stake tests
bool StakingSetupFixtureSetup{false};
struct StakingSetupFixture {
    explicit StakingSetupFixture() {
        if (StakingSetupFixtureSetup) return; StakingSetupFixtureSetup = true;
        chain_default();
    }
    void chain_default() {
        auto pos_ptr = std::make_shared<TestChainPoS>(true);
        pos_ptr.reset();
    }
};

bool sendToAddress(CWallet *wallet, const CTxDestination & dest, const CAmount & amount, CTransactionRef & tx) {
    // Create and send the transaction
    CReserveKey reservekey(wallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {GetScriptForDestination(dest), amount, false};
    vecSend.push_back(recipient);
    CCoinControl cc;
    auto locked_chain = wallet->chain().lock();
    if (!wallet->CreateTransaction(*locked_chain, vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc))
        return false;

    CValidationState state;
    auto sent = wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state);
    BOOST_CHECK_MESSAGE(state.IsValid(), state.GetRejectReason());
    return sent && state.IsValid();
}

void createStakeBlock(const StakeMgr::StakeCoin & nextStake, const CBlockIndex *tip, CBlock & block, CMutableTransaction & coinbaseTx, CMutableTransaction & coinstakeTx)
{
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
    block.nBits          = GetNextWorkRequired(tip, nullptr, Params().GetConsensus());
    block.hashStake      = nextStake.coin->outpoint.hash;
    block.nStakeIndex    = nextStake.coin->outpoint.n;
    block.nStakeAmount   = nextStake.coin->txout.nValue;
    block.hashStakeBlock = nextStake.hashBlock;
    // Staking protocol v6 upgrade, set block time to current time and nonce to stake time
    if (IsProtocolV06(nextStake.blockTime, Params().GetConsensus())) {
        block.nTime = nextStake.blockTime;
        block.nNonce = static_cast<uint32_t>(nextStake.time);
    } else { // v05 and lower
        block.nTime = static_cast<uint32_t>(nextStake.time);
        block.nNonce = 0;
    }
}

// Find a stake
bool findStake(StakeMgr::StakeCoin & nextStake, StakeMgr & staker,
               const CBlockIndex *tip, const std::shared_ptr<CWallet> & wallet, const CAmount & stakeAmount,
               const CScript & paymentScript)
{
    while (true) {
        try {
            std::vector<std::shared_ptr<CWallet>> wallets{wallet};
            if (staker.Update(wallets, tip, Params().GetConsensus(), true)) {
                std::vector<StakeMgr::StakeCoin> nextStakes;
                if (!staker.NextStake(nextStakes, tip, Params()))
                    continue;
                for (const auto & ns : nextStakes) {
                    CBlock block;
                    CMutableTransaction coinbaseTx;
                    CMutableTransaction coinstakeTx;
                    createStakeBlock(ns, tip, block, coinbaseTx, coinstakeTx);
                    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
                    coinstakeTx.vout[1] = CTxOut(ns.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
                    block.vtx[1] = MakeTransactionRef(coinstakeTx);
                    block.hashMerkleRoot = BlockMerkleRoot(block);
                    uint256 haspos;
                    if (CheckProofOfStake(block, tip, haspos, Params().GetConsensus())) {
                        nextStake = ns;
                        return true;
                    }
                }
            }
        } catch (std::exception & e) {
            LogPrintf("Staker ran into an exception: %s\n", e.what());
        } catch (...) { }
        SetMockTime(GetAdjustedTime() + Params().GetConsensus().PoSFutureBlockTimeLimit(tip->GetBlockTime()));
    }

    return false;
}

// Find a stake in certain time
bool findStakeUntilTime(StakeMgr::StakeCoin & nextStake, StakeMgr & staker,
                        const CBlockIndex *tip, const std::shared_ptr<CWallet> & wallet, const CAmount & stakeAmount,
                        const CScript & paymentScript, const int64_t endTime)
{
    while (GetAdjustedTime() <= endTime) {
        try {
            std::vector<std::shared_ptr<CWallet>> wallets{wallet};
            if (staker.Update(wallets, tip, Params().GetConsensus(), true)) {
                std::vector<StakeMgr::StakeCoin> nextStakes;
                if (!staker.NextStake(nextStakes, tip, Params()))
                    continue;
                for (const auto & ns : nextStakes) {
                    CBlock block;
                    CMutableTransaction coinbaseTx;
                    CMutableTransaction coinstakeTx;
                    createStakeBlock(ns, tip, block, coinbaseTx, coinstakeTx);
                    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
                    coinstakeTx.vout[1] = CTxOut(ns.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
                    block.vtx[1] = MakeTransactionRef(coinstakeTx);
                    block.hashMerkleRoot = BlockMerkleRoot(block);
                    uint256 haspos;
                    if (CheckProofOfStake(block, tip, haspos, Params().GetConsensus())) {
                        nextStake = ns;
                        return true;
                    }
                }
            }
        } catch (std::exception & e) {
            LogPrintf("Staker ran into an exception: %s\n", e.what());
        } catch (...) { }
        SetMockTime(GetAdjustedTime() + 1);
    }

    return false;
}

bool findStakeInPast(StakeMgr::StakeCoin & nextStake, StakeMgr & staker, const CBlockIndex *tip, std::shared_ptr<CWallet> wallet)
{
    const auto & params = Params().GetConsensus();
    const auto blockTime = tip->GetBlockTime()-1; // always select a block time that's prior to the latest chain tip
    const auto tipHeight = tip->nHeight;
    const auto adjustedTime = GetAdjustedTime();
    const auto fromTime = tip->GetBlockTime() + 1;
    const auto toTime = adjustedTime + params.PoSFutureBlockTimeLimit(blockTime);
    std::vector<StakeMgr::StakeOutput> selected;
    const std::vector<COutput> & coins = staker.StakeOutputs(wallet.get(), 1);
    for (const COutput & out : coins) {
        if (staker.SuitableCoin(out, tipHeight, params))
            selected.emplace_back(std::make_shared<COutput>(out), wallet);
    }
    StakeMgr::StakeCoin stake;
    for (const auto & item : selected) {
        const auto out = item.out;
        std::map<int64_t, std::vector<StakeMgr::StakeCoin>> stakes;
        if (!staker.GetStakesMeetingTarget(out, wallet, tip, adjustedTime, blockTime, fromTime, toTime, stakes, params))
            continue;
        if (!stakes.empty()) {
            nextStake = stakes.begin()->second.front();
            return true;
        }
    }
    return false;
}

bool signCoinstake(CMutableTransaction & coinstakeTx, CWallet *wallet) {
    // Sign stake input w/ keystore
    auto locked_chain = wallet->chain().lock();
    LOCK(wallet->cs_wallet);
    return wallet->SignTransaction(coinstakeTx);
}

void scanWalletTxs(CWallet *wallet) {
    // Make sure wallet is updated with all utxos
    LOCK(cs_main);
    auto locked_chain = wallet->chain().lock();
    WalletRescanReserver reserver(wallet);
    reserver.reserve();
    wallet->ScanForWalletTransactions(locked_chain->getBlockHash(0), {}, reserver, true);
}

BOOST_FIXTURE_TEST_SUITE(staking_tests, StakingSetupFixture)

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
                                                nullptr, nullptr, true, 0));

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
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.lastPOWBlock = 150;
    params->consensus.governanceBlock = 10000; // disable governance
    params->consensus.stakingV05UpgradeTime = std::numeric_limits<int>::max(); // set far into future
    params->consensus.stakingV06UpgradeTime = std::numeric_limits<int>::max(); // disable v06
    params->consensus.stakingV07UpgradeTime = std::numeric_limits<int>::max(); // disable v07
    pos.Init("150,v03");
    const int blocks{100};
    // 4 selection intervals worth of blocks
    while (chainActive.Height() < blocks) {
        pos.StakeBlocks(blocks/4);
        SetMockTime(GetAdjustedTime() + GetStakeModifierSelectionInterval());
    }

    const auto endTime = GetAdjustedTime() + params->consensus.nPowTargetSpacing*blocks; // 100 blocks worth of selection intervals
    uint64_t firstStakeModifier{0};
    const auto stakeIndex = chainActive[chainActive.Height() - blocks + 10];
    CBlock stakeBlock; BOOST_CHECK(ReadBlockFromDisk(stakeBlock, stakeIndex, params->consensus));
    while (GetAdjustedTime() < endTime) {
        int64_t runningTime = GetAdjustedTime();
        uint64_t nStakeModifier{0};
        int nStakeModifierHeight{0};
        int64_t nStakeModifierTime{0};
        BOOST_CHECK(GetKernelStakeModifier(chainActive.Tip(), stakeIndex, runningTime, nStakeModifier, nStakeModifierHeight, nStakeModifierTime));
        if (firstStakeModifier == 0)
            firstStakeModifier = nStakeModifier;
        else BOOST_CHECK_MESSAGE(nStakeModifier == firstStakeModifier, "Stake modifier v03 should be the same indefinitely");
        pos.StakeBlocks(1);
    }

    pos_ptr.reset();
}

/// Check that v05 staking modifier changes for each new selection interval
BOOST_AUTO_TEST_CASE(staking_tests_v05modifier)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.lastPOWBlock = 150;
    params->consensus.governanceBlock = 10000; // disable governance
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime(); // set to current
    params->consensus.stakingV06UpgradeTime = std::numeric_limits<int>::max(); // disable v06
    params->consensus.stakingV07UpgradeTime = std::numeric_limits<int>::max(); // disable v07
    pos.Init("150,v05");
    const int blocks{100};
    // 4 selection intervals worth of blocks
    while (chainActive.Height() < blocks) {
        pos.StakeBlocks(blocks/4);
        SetMockTime(GetAdjustedTime() + GetStakeModifierSelectionInterval());
    }

    const auto endTime = GetAdjustedTime() + params->consensus.nPowTargetSpacing*blocks; // 100 blocks worth of selection intervals
    uint64_t lastStakeModifier{0};
    const auto stakeIndex = chainActive[chainActive.Height() - blocks + 10];
    CBlock stakeBlock; BOOST_CHECK(ReadBlockFromDisk(stakeBlock, stakeIndex, params->consensus));
    while (GetAdjustedTime() < endTime) {
        int64_t runningTime = GetAdjustedTime();
        uint64_t nStakeModifier{0};
        int nStakeModifierHeight{0};
        int64_t nStakeModifierTime{0};
        BOOST_CHECK(GetKernelStakeModifier(chainActive.Tip(), stakeIndex, runningTime, nStakeModifier, nStakeModifierHeight, nStakeModifierTime));
        if (lastStakeModifier > 0 && chainActive.Height() % 3 == 0) {
            BOOST_CHECK_MESSAGE(nStakeModifier != lastStakeModifier, strprintf("Stake modifier should be different for every new selection interval: new modifier %d vs previous %d", nStakeModifier, lastStakeModifier));
            lastStakeModifier = nStakeModifier;
        }
        pos.StakeBlocks(1);
    }

    pos_ptr.reset();
}

/// Check that the v03 to v05 staking protocol upgrade works properly
BOOST_AUTO_TEST_CASE(staking_tests_protocolupgrade_v05)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.lastPOWBlock = 150;
    params->consensus.governanceBlock = 10000; // disable governance
    params->consensus.stakingV05UpgradeTime = std::numeric_limits<int>::max(); // disable v05
    params->consensus.stakingV06UpgradeTime = std::numeric_limits<int>::max(); // disable v06
    params->consensus.stakingV07UpgradeTime = std::numeric_limits<int>::max(); // disable v07
    pos.Init("150,v03");
    pos.StakeBlocks(25);

    // Switch on the upgrade
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime() + params->GetConsensus().nPowTargetSpacing * 10; // set 10 min in future (~10 blocks)
    int blocks = chainActive.Height();
    pos.StakeBlocks(25); // make sure seemless upgrade to v05 staking protocol occurs
    BOOST_CHECK_EQUAL(chainActive.Height(), blocks + 25);

    pos_ptr.reset();
}

/// Check that the v05 to v06 staking protocol upgrade works properly
BOOST_AUTO_TEST_CASE(staking_tests_protocolupgrade_v06)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.lastPOWBlock = 150;
    params->consensus.governanceBlock = 10000; // disable governance
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime();
    params->consensus.stakingV06UpgradeTime = std::numeric_limits<int>::max(); // disable v06
    params->consensus.stakingV07UpgradeTime = std::numeric_limits<int>::max(); // disable v07
    pos.Init("150,v05-v06");
    pos.StakeBlocks(25);

    // Switch on the upgrade
    params->consensus.stakingV06UpgradeTime = GetAdjustedTime() + params->GetConsensus().nPowTargetSpacing * 10; // set 10 min in future (~10 blocks)
    int blocks = chainActive.Height();
    pos.StakeBlocks(25); // make sure seemless upgrade to v06 staking protocol occurs
    BOOST_CHECK_EQUAL(chainActive.Height(), blocks + 25);

    pos_ptr.reset();
}

/// Check that the v06 to v07 staking protocol upgrade works properly
BOOST_AUTO_TEST_CASE(staking_tests_protocolupgrade_v07)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.lastPOWBlock = 150;
    params->consensus.governanceBlock = 10000; // disable governance
    params->consensus.stakingV05UpgradeTime = GetAdjustedTime();
    params->consensus.stakingV06UpgradeTime = GetAdjustedTime();
    params->consensus.stakingV07UpgradeTime = std::numeric_limits<int>::max(); // disable v07
    pos.Init("150,v06-v07");
    pos.StakeBlocks(25);

    // Switch on the upgrade
    params->consensus.stakingV07UpgradeTime = GetAdjustedTime() + params->GetConsensus().nPowTargetSpacing * 10; // set 10 min in future (~10 blocks)
    int blocks = chainActive.Height();
    pos.StakeBlocks(25); // make sure seemless upgrade to v06 staking protocol occurs
    BOOST_CHECK_EQUAL(chainActive.Height(), blocks + 25);

    pos_ptr.reset();
}

/// Ensure that bad stakes are not accepted by the protocol.
BOOST_FIXTURE_TEST_CASE(staking_tests_stakes, TestChainPoS)
{
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
        staker.Reset();
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
    }
    // Find a stake before target spacing
    {
        staker.Reset();
        SetMockTime(chainActive.Tip()->nNonce);
        auto endTime = GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing*0.85;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStakeUntilTime(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript, endTime));
    }
    // Find a stake after target spacing
    {
        // Just after 1 block worth of spacing
        staker.Reset();
        SetMockTime(chainActive.Tip()->nNonce + Params().GetConsensus().nPowTargetSpacing);
        auto endTime = GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing/2;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStakeUntilTime(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript, endTime));
        // Just after 2 blocks worth of spacing
        staker.Reset();
        SetMockTime(chainActive.Tip()->nNonce + Params().GetConsensus().nPowTargetSpacing*2);
        endTime = GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing;
        BOOST_CHECK(findStakeUntilTime(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript, endTime));
    }

    // Check valid coinstake
    {
        staker.Reset();
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
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

    // Check amount other than 0 specified in coinstake should not be accepted
    {
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
        createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
        block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[0] = CTxOut(stakeAmount, paymentScript);
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue, paymentScript); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
        block.vtx[1] = MakeTransactionRef(coinstakeTx);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
        blockHash = chainActive.Tip()->GetBlockHash();
        ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr);
        BOOST_CHECK_EQUAL(blockHash, chainActive.Tip()->GetBlockHash()); // block should not be accepted
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex should not update on bad stake");
    }

    // Check multiple coinstakes should be rejected
    {
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx1;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
        createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx1);
        CMutableTransaction coinstakeTx2 = coinstakeTx1;
        block.vtx.resize(3);
        block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx1.vout[1] = CTxOut(nextStake.coin->txout.nValue, paymentScript); // staker payment
        coinstakeTx2.vout[1] = CTxOut(stakeAmount, paymentScript); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx1, wallet.get()));
        block.vtx[1] = MakeTransactionRef(coinstakeTx1);
        block.vtx[2] = MakeTransactionRef(coinstakeTx2);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
        blockHash = chainActive.Tip()->GetBlockHash();
        ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr);
        BOOST_CHECK_EQUAL(blockHash, chainActive.Tip()->GetBlockHash()); // block should not be accepted
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex should not update on bad stake");
    }

    // Check invalid stake amount
    {
        scanWalletTxs(wallet.get());
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
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

    // Check negative stake amount
    {
        scanWalletTxs(wallet.get());
        CBlock block;
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
        createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
        block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(-1 * (nextStake.coin->txout.nValue + stakeAmount), paymentScript); // staker payment
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
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
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

    // Check that orphaned coinstakes are abandoned
    {
        const auto tip1 = chainActive.Tip();
        StakeBlocks(1); SyncWithValidationInterfaceQueue();
        const auto tipInvalidate = chainActive.Tip();
        CBlock block;
        {
            LOCK(cs_main);
            BOOST_CHECK(ReadBlockFromDisk(block, tipInvalidate, Params().GetConsensus()));
        }

        // reset chain tip
        CValidationState state;
        InvalidateBlock(state, Params(), tipInvalidate, false);
        ActivateBestChain(state, Params());
        SyncWithValidationInterfaceQueue();
        StakeBlocks(6); SyncWithValidationInterfaceQueue();

        BOOST_CHECK_MESSAGE(tipInvalidate != chainActive[tipInvalidate->nHeight], "expecting coinstake to be properly abandoned when invalidated/disconnected");

        {
            LOCK2(cs_main, wallet->cs_wallet);
            const auto orphanedStake = block.vtx[1];
            auto it = wallet->mapWallet.find(orphanedStake->GetHash());
            BOOST_CHECK_MESSAGE(it == wallet->mapWallet.end(), "orphaned stake should not be in the wallet");
            auto prevout = block.vtx[1]->vin[0].prevout;
            auto previt = wallet->mapWallet.find(prevout.hash);
            BOOST_CHECK_MESSAGE(previt != wallet->mapWallet.end(), "orphaned prevout *should* be in the wallet");
            BOOST_CHECK_MESSAGE(!wallet->IsSpent(*locked_chain.get(), prevout.hash, prevout.n) , "orphaned prevout should not be spent");
        }
    }

    // Check forking via staking
    {
        scanWalletTxs(wallet.get());
        while (chainActive.Height() < 165) {
            StakeMgr staker;
            CBlockIndex *pindex = chainActive.Tip();
            const int forkchainLen{3};
            std::vector<std::shared_ptr<CBlock>> forkA; forkA.resize(forkchainLen);
            std::vector<std::shared_ptr<CBlock>> forkB; forkB.resize(forkchainLen);
            {
                CBlockIndex *prevIndex = pindex;
                for (int i = 0; i < forkchainLen; ++i) {
                    if (!prevIndex)
                        break;
                    CBlock block;
                    CMutableTransaction coinbaseTx;
                    CMutableTransaction coinstakeTx;
                    StakeMgr::StakeCoin nextStake;
                    BOOST_CHECK(findStake(nextStake, staker, prevIndex, wallet, stakeAmount, paymentScript));
                    createStakeBlock(nextStake, prevIndex, block, coinbaseTx, coinstakeTx);
                    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
                    coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
                    BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
                    block.vtx[1] = MakeTransactionRef(coinstakeTx);
                    block.hashMerkleRoot = BlockMerkleRoot(block);
                    BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
                    auto blockptr = std::make_shared<CBlock>(block);
                    if (ProcessNewBlock(Params(), blockptr, false, nullptr)) {
                        LOCK(cs_main);
                        if (mapBlockIndex.count(block.GetHash()))
                            prevIndex = mapBlockIndex[block.GetHash()];
                    }
                    forkA[i] = blockptr;
                }
            }
            {
                CBlockIndex *prevIndex = pindex;
                for (int i = 0; i < forkchainLen; ++i) {
                    if (!prevIndex)
                        break;
                    CBlock block;
                    CMutableTransaction coinbaseTx;
                    CMutableTransaction coinstakeTx;
                    StakeMgr::StakeCoin nextStake;
                    BOOST_CHECK(findStake(nextStake, staker, prevIndex, wallet, stakeAmount, paymentScript));
                    createStakeBlock(nextStake, prevIndex, block, coinbaseTx, coinstakeTx);
                    block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
                    coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
                    BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
                    block.vtx[1] = MakeTransactionRef(coinstakeTx);
                    block.hashMerkleRoot = BlockMerkleRoot(block);
                    BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
                    auto blockptr = std::make_shared<CBlock>(block);
                    if (ProcessNewBlock(Params(), blockptr, false, nullptr)) {
                        LOCK(cs_main);
                        if (mapBlockIndex.count(block.GetHash()))
                            prevIndex = mapBlockIndex[block.GetHash()];
                    }
                    forkB[i] = blockptr;
                }
            }

            StakeBlocks(1), SyncWithValidationInterfaceQueue();
        }
    }

    // Check stakes meet v6 staking protocol
    {
        CBlock block;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
        createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
        block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
        block.vtx[1] = MakeTransactionRef(coinstakeTx);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
        const auto currentIndex1 = chainActive.Tip();
        const auto validNonce = block.nNonce;
        block.nNonce = 0;
        BOOST_CHECK_MESSAGE(!ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr), "stake should not be accepted with nonce of 0");
        block.nNonce = -1;
        BOOST_CHECK_MESSAGE(!ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr), "stake should not be accepted with nonce less than 0");
        block.nNonce = GetAdjustedTime() + Params().GetConsensus().PoSFutureBlockTimeLimit(block.nTime) + 10;
        BOOST_CHECK_MESSAGE(!ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr), "stake should not be accepted with nonce greater than adjusted time + max future stake time");
        block.nNonce = validNonce;
        BOOST_CHECK_MESSAGE(ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr), "stake with valid nonce should be accepted");
        BOOST_CHECK(currentIndex1 != chainActive.Tip()); // chain tip should not match previous
        BOOST_CHECK_EQUAL(block.GetHash(), chainActive.Tip()->GetBlockHash()); // block should be accepted
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to update on stake");
    }

    // Check stakes meet v6 staking protocol, block submitted with timestamp prior to the
    // current chain tip should not be accepted
    {
        CBlock block;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStakeInPast(nextStake, staker, chainActive.Tip(), wallet));
        createStakeBlock(nextStake, chainActive.Tip(), block, coinbaseTx, coinstakeTx);
        block.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
        block.vtx[1] = MakeTransactionRef(coinstakeTx);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        BOOST_CHECK(SignBlock(block, nextStake.coin->txout.scriptPubKey, *wallet));
        const auto currentIndex1 = chainActive.Tip();
        BOOST_CHECK_MESSAGE(!ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr), "stake should not be accepted with block time prior to current tip");
        BOOST_CHECK(currentIndex1 == chainActive.Tip()); // chain tip should match previous
        BOOST_CHECK_MESSAGE(g_txindex->BestBlockIndex()->GetBlockHash() == chainActive.Tip()->GetBlockHash(), "global txindex failed to update on stake");
    }

    // Check -staketoaddress
    {
        CKey stakeToKey; stakeToKey.MakeNewKey(true);
        BOOST_CHECK(wallet->LoadKey(stakeToKey, stakeToKey.GetPubKey()));
        gArgs.ForceSetArg("-staketoaddress", EncodeDestination(GetDestinationForKey(stakeToKey.GetPubKey(), OutputType::LEGACY)));
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, chainActive.Tip(), wallet, stakeAmount, paymentScript));
        auto blocktemplate = BlockAssembler(Params()).CreateNewBlockPoS(*nextStake.coin, nextStake.hashBlock, nextStake.time, nextStake.blockTime, nextStake.wallet.get(), false);
        CBlock block = blocktemplate->block;
        BOOST_CHECK_MESSAGE(block.vtx[1]->vout[1].nValue == nextStake.coin->txout.nValue, "Stake-to-address expecting no stake reward on the coinstake");
        auto stakeReward = Params().GetConsensus().GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
        BOOST_CHECK_MESSAGE(stakeReward == block.vtx[1]->vout[2].nValue, "Stake-to-address payment should match expected stake reward");
        auto success = ProcessNewBlock(Params(), std::make_shared<CBlock>(block), true, nullptr);
        BOOST_CHECK_MESSAGE(success, "Stake-to-address block should be accepted in ProcessNewBlock");
        gArgs.ForceSetArg("-staketoaddress", "");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
    }

    // TODO Blocknet PoS unit test for p2pkh stakes
}

/// Check CLTV
BOOST_FIXTURE_TEST_CASE(staking_tests_cltv, TestChainPoS)
{
    const auto height = chainActive.Height();
    const auto lockTime = height + 5;
    CKey counterPartyKey; counterPartyKey.MakeNewKey(true);

    CScript cltvScript;
    cltvScript << OP_IF
                   << lockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                   << OP_DUP << OP_HASH160 << ToByteVector(coinbaseKey.GetPubKey().GetID()) << OP_EQUALVERIFY << OP_CHECKSIG
               << OP_ELSE
                   << OP_DUP << OP_HASH160 << ToByteVector(counterPartyKey.GetPubKey().GetID()) << OP_EQUALVERIFY << OP_CHECKSIGVERIFY
               << OP_ENDIF;

    CMutableTransaction deposit;

    // CLTV p2sh deposit should succeed
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, wallet->cs_wallet);
            wallet->AvailableCoins(*locked_chain, coins, true, nullptr, 25*COIN);
        }
        auto coin = coins[0];
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(coin.GetInputCoin().outpoint, CScript(), CTxIn::SEQUENCE_FINAL);
        mtx.vout[0] = CTxOut(10*COIN, GetScriptForDestination(CScriptID(cltvScript)));
        SignatureData sigdata = DataFromTransaction(mtx, 0, coin.GetInputCoin().txout);
        ProduceSignature(*wallet, MutableTransactionSignatureCreator(&mtx, 0, coin.GetInputCoin().txout.nValue, SIGHASH_ALL), coin.GetInputCoin().txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        deposit = mtx;
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send cltv tx: %s", errstr));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
//        UniValue utx(UniValue::VOBJ); uint256 block{chainActive.Tip()->GetBlockHash()}; TxToUniv(CTransaction(deposit), block, utx);
//        std::cout << utx.write(2) << std::endl;
    }

    // CLTV should fail if locktime has not expired
    {
        CMutableTransaction mtx;
        mtx.nLockTime = lockTime;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(COutPoint{deposit.GetHash(), 0}, CScript(), CTxIn::SEQUENCE_FINAL-1);
        mtx.vout[0] = CTxOut(8*COIN, GetScriptForDestination(CTxDestination{coinbaseKey.GetPubKey().GetID()}));
        uint256 hash = SignatureHash(cltvScript, mtx, 0, SIGHASH_ALL, deposit.vout[0].nValue, SigVersion::BASE);
        std::vector<unsigned char> signature;
        BOOST_CHECK(coinbaseKey.Sign(hash, signature));
        signature.push_back(static_cast<unsigned char>(SIGHASH_ALL));
        CScript redeem;
        redeem << signature << ToByteVector(coinbaseKey.GetPubKey()) << OP_TRUE << ToByteVector(cltvScript);
        mtx.vin[0].scriptSig = redeem;
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err != TransactionError::OK, strprintf("Failed to send redeem tx: %s", errstr));
    }

    StakeBlocks(4), SyncWithValidationInterfaceQueue();

    // CLTV redeem should fail on bad vins nSequence (CTxIn::SEQUENCE_FINAL)
    {
        CMutableTransaction mtx;
        mtx.nLockTime = lockTime;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(COutPoint{deposit.GetHash(), 0}, CScript(), CTxIn::SEQUENCE_FINAL);
        mtx.vout[0] = CTxOut(8*COIN, GetScriptForDestination(CTxDestination{coinbaseKey.GetPubKey().GetID()}));
        uint256 hash = SignatureHash(cltvScript, mtx, 0, SIGHASH_ALL, deposit.vout[0].nValue, SigVersion::BASE);
        std::vector<unsigned char> signature;
        BOOST_CHECK(coinbaseKey.Sign(hash, signature));
        signature.push_back(static_cast<unsigned char>(SIGHASH_ALL));
        CScript redeem;
        redeem << signature << ToByteVector(coinbaseKey.GetPubKey()) << OP_TRUE << ToByteVector(cltvScript);
        mtx.vin[0].scriptSig = redeem;
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err != TransactionError::OK, strprintf("Redeem tx should fail if inputs marked as final: %s", errstr));
    }

    // CLTV redeem should fail on missing locktime
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(COutPoint{deposit.GetHash(), 0}, CScript(), CTxIn::SEQUENCE_FINAL-1);
        mtx.vout[0] = CTxOut(8*COIN, GetScriptForDestination(CTxDestination{coinbaseKey.GetPubKey().GetID()}));
        uint256 hash = SignatureHash(cltvScript, mtx, 0, SIGHASH_ALL, deposit.vout[0].nValue, SigVersion::BASE);
        std::vector<unsigned char> signature;
        BOOST_CHECK(coinbaseKey.Sign(hash, signature));
        signature.push_back(static_cast<unsigned char>(SIGHASH_ALL));
        CScript redeem;
        redeem << signature << ToByteVector(coinbaseKey.GetPubKey()) << OP_TRUE << ToByteVector(cltvScript);
        mtx.vin[0].scriptSig = redeem;
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err != TransactionError::OK, strprintf("Redeem tx should fail if locktime is missing: %s", errstr));
    }

    // CLTV should succeed if locktime has properly expired
    {
        mempool.clear();
        CMutableTransaction mtx;
        mtx.nLockTime = lockTime;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(COutPoint{deposit.GetHash(), 0}, CScript(), CTxIn::SEQUENCE_FINAL-1);
        mtx.vout[0] = CTxOut(8*COIN, GetScriptForDestination(CTxDestination{coinbaseKey.GetPubKey().GetID()}));
        uint256 hash = SignatureHash(cltvScript, mtx, 0, SIGHASH_ALL, deposit.vout[0].nValue, SigVersion::BASE);
        std::vector<unsigned char> signature;
        BOOST_CHECK(coinbaseKey.Sign(hash, signature));
        signature.push_back(static_cast<unsigned char>(SIGHASH_ALL));
        CScript redeem;
        redeem << signature << ToByteVector(coinbaseKey.GetPubKey()) << OP_TRUE << ToByteVector(cltvScript);
        mtx.vin[0].scriptSig = redeem;
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send redeem tx: %s", errstr));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
    }
}

/// Orphaned stakes should disconnect properly
BOOST_FIXTURE_TEST_CASE(staking_tests_walletorphans, TestChainPoS)
{
    CPubKey paymentPubKey;
    CTxDestination stakeInputDest;
    ExtractDestination(m_coinbase_txns[0]->vout[0].scriptPubKey, stakeInputDest);
    const auto keyid = GetKeyForDestination(*wallet, stakeInputDest);
    CScript walletPaymentScript = GetScriptForDestination(keyid);
    wallet->GetPubKey(keyid, paymentPubKey);
    const CAmount stakeSubsidy = GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
    const CAmount stakeAmount = stakeSubsidy - 2000; // account for basic fee in sats

    auto wallet1 = std::make_shared<CWallet>(*chain, WalletLocation("wallet1"), WalletDatabase::CreateMock());
    bool wallet1FirstRun; wallet1->LoadWallet(wallet1FirstRun);
    CKey key1; key1.MakeNewKey(true); AddKey(*wallet1, key1);
    wallet1->SetBroadcastTransactions(true);
    RegisterValidationInterface(wallet1.get());

    auto wallet2 = std::make_shared<CWallet>(*chain, WalletLocation("wallet2"), WalletDatabase::CreateMock());
    bool wallet2FirstRun; wallet2->LoadWallet(wallet2FirstRun);
    CKey key2; key2.MakeNewKey(true); AddKey(*wallet2, key2);
    wallet2->SetBroadcastTransactions(true);
    RegisterValidationInterface(wallet2.get());

    const CAmount walletUtxo1 = 501*COIN;
    const CAmount walletUtxo2 = 502*COIN;

    // Prep transactions
    CTransactionRef tx;
    sendToAddress(wallet.get(), GetDestinationForKey(key1.GetPubKey(), OutputType::LEGACY), walletUtxo1, tx);
    sendToAddress(wallet.get(), GetDestinationForKey(key2.GetPubKey(), OutputType::LEGACY), walletUtxo2, tx);
    sendToAddress(wallet.get(), GetDestinationForKey(key2.GetPubKey(), OutputType::LEGACY), walletUtxo2, tx);
    StakeBlocks(Params().GetConsensus().coinMaturity + 1); SyncWithValidationInterfaceQueue();

    const auto tip = chainActive.Tip();
    CBlock block1;
    CBlock block2;
    CBlock block3;
    CScript paymentScript1 = GetScriptForDestination(key1.GetPubKey().GetID());
    CScript paymentScript2 = GetScriptForDestination(key2.GetPubKey().GetID());

    auto balance = [](CWallet *w, interfaces::Chain::Lock *lc) -> CAmount {
        LOCK2(cs_main, w->cs_wallet);
        std::vector<COutput> coins;
        w->AvailableCoins(*lc, coins, false);
        CAmount b{0};
        for (auto & c : coins)
            b += c.GetInputCoin().txout.nValue;
        return b;
    };
    auto balance2 = [](std::vector<COutput> & rutxos) -> CAmount {
        CAmount r{0};
        for (const auto & utxo : rutxos)
            r += utxo.GetInputCoin().txout.nValue;
        return r;
    };
    auto utxos = [](CWallet *w, interfaces::Chain::Lock *lc) -> std::vector<COutput> {
        LOCK2(cs_main, w->cs_wallet);
        std::vector<COutput> coins;
        for (const auto& entry : w->mapWallet) {
            const uint256 & wtxid = entry.first;
            const CWalletTx *pcoin = &entry.second;int nDepth = pcoin->GetDepthInMainChain(*lc);
            bool safeTx = pcoin->IsTrusted(*lc);
            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
                isminetype mine = w->IsMine(pcoin->tx->vout[i]);
                if (mine == ISMINE_NO)
                    continue;
                if (w->IsSpent(*lc, wtxid, i))
                    continue;
                bool solvable = IsSolvable(*w, pcoin->tx->vout[i].scriptPubKey);
                bool spendable = (mine & ISMINE_SPENDABLE) != ISMINE_NO;
                coins.push_back(COutput(pcoin, i, nDepth, spendable, solvable, safeTx, false));
            }
        }
        return coins;
    };
    auto lookupIndex = [](const uint256 & blockhash) -> CBlockIndex* {
        LOCK(cs_main);
        return LookupBlockIndex(blockhash);
    };

    scanWalletTxs(wallet1.get());
    scanWalletTxs(wallet2.get());

    CAmount walletBalance1 = balance(wallet1.get(), locked_chain.get()); // prior to any stakes
    CAmount walletBalance2{0};
    std::vector<COutput> wallet1Utxos = utxos(wallet1.get(), locked_chain.get());
    std::vector<COutput> wallet2Utxos = utxos(wallet2.get(), locked_chain.get());

    { // wallet1: find stake
        staker.Reset();
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, tip, wallet1, stakeAmount, paymentScript1));
        createStakeBlock(nextStake, tip, block1, coinbaseTx, coinstakeTx);
        block1.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript1); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet1.get()));
        block1.vtx[1] = MakeTransactionRef(coinstakeTx);
        block1.hashMerkleRoot = BlockMerkleRoot(block1);
        BOOST_CHECK(SignBlock(block1, nextStake.coin->txout.scriptPubKey, *wallet1));
    }

    { // wallet2: find stake
        staker.Reset();
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, tip, wallet2, stakeAmount, paymentScript2));
        createStakeBlock(nextStake, tip, block2, coinbaseTx, coinstakeTx);
        block2.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript2); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet2.get()));
        block2.vtx[1] = MakeTransactionRef(coinstakeTx);
        block2.hashMerkleRoot = BlockMerkleRoot(block2);
        BOOST_CHECK(SignBlock(block2, nextStake.coin->txout.scriptPubKey, *wallet2));
    }

    const auto tipBefore = chainActive.Tip();

    { // wallet2: block2 should be accepted
        auto block2ptr = std::make_shared<const CBlock>(block2);
        bool newBlock{false};
        BOOST_CHECK_MESSAGE(ProcessNewBlock(Params(), block2ptr, true, &newBlock), "block2 failed to process");
        SyncWithValidationInterfaceQueue();
        const auto tipAfter2 = lookupIndex(block2.GetHash());
        BOOST_CHECK_MESSAGE(tipAfter2 == chainActive.Tip(), "expecting block2 to be chain tip");
        BOOST_CHECK_MESSAGE(tipAfter2->nHeight == tipBefore->nHeight + 1, "expecting block2 to have correct height");
    }

    { // wallet1: block1 should be accepted and cause competing fork (same height as block1)
        bool newBlock{false};
        auto block1ptr = std::make_shared<const CBlock>(block1);
        BOOST_CHECK_MESSAGE(ProcessNewBlock(Params(), block1ptr, true, &newBlock), "block1 failed to process");
        SyncWithValidationInterfaceQueue();
        const auto tipAfter1 = lookupIndex(block1.GetHash());
        BOOST_CHECK_MESSAGE(tipAfter1->nHeight == tipBefore->nHeight + 1, "expecting block1 to have correct height");
    }

    CBlockIndex *block1Index = nullptr;
    CBlockIndex *block2Index = nullptr;
    {
        LOCK(cs_main);
        block1Index = LookupBlockIndex(block1.GetHash());
        block2Index = LookupBlockIndex(block2.GetHash());
    }
    BOOST_CHECK_MESSAGE(block1Index->nHeight == block2Index->nHeight, "expecting block1 and block2 to have same height");

    // At this point in the test we want to build the rest of the chain on block2
    // which will result in orphaning block1 and forcing wallet1 to reorg.
    CBlockIndex *block2Tip = lookupIndex(block2.GetHash());

    { // wallet2: find stake and submit block3 that builds on block2 chain
        SetMockTime(GetAdjustedTime() + Params().GetConsensus().PoSFutureBlockTimeLimit(chainActive.Tip()->GetBlockTime()));
        staker.Reset();
        uint256 blockHash;
        CMutableTransaction coinbaseTx;
        CMutableTransaction coinstakeTx;
        StakeMgr::StakeCoin nextStake;
        BOOST_CHECK(findStake(nextStake, staker, block2Tip, wallet2, stakeAmount, paymentScript2));
        createStakeBlock(nextStake, block2Tip, block3, coinbaseTx, coinstakeTx);
        block3.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
        coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, paymentScript2); // staker payment
        BOOST_CHECK(signCoinstake(coinstakeTx, wallet2.get()));
        block3.vtx[1] = MakeTransactionRef(coinstakeTx);
        block3.hashMerkleRoot = BlockMerkleRoot(block3);
        BOOST_CHECK(SignBlock(block3, nextStake.coin->txout.scriptPubKey, *wallet2));
    }

    CBlockIndex *block3Tip;
    { // wallet2: accept block3 resulting in block1 orphan
        auto block3ptr = std::make_shared<const CBlock>(block3);
        bool newBlock{false};
        BOOST_CHECK_MESSAGE(ProcessNewBlock(Params(), block3ptr, true, &newBlock), "block3 failed to process");
        SyncWithValidationInterfaceQueue();
        block3Tip = lookupIndex(block3.GetHash());
        BOOST_CHECK_MESSAGE(block3Tip->nHeight == tipBefore->nHeight + 2, "expecting block3 to be accepted with 2 + height_of_beginning_tip");
    }

    { // Build on block3 chain to move chain tip to longest chain
        bool processed{false};
        int max{0};
        while (!processed && max < 20) {
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().PoSFutureBlockTimeLimit(chainActive.Tip()->GetBlockTime()));
            staker.Reset();
            CBlock block4;
            uint256 blockHash;
            CMutableTransaction coinbaseTx;
            CMutableTransaction coinstakeTx;
            StakeMgr::StakeCoin nextStake;
            BOOST_CHECK(findStake(nextStake, staker, block3Tip, wallet, stakeAmount, walletPaymentScript));
            createStakeBlock(nextStake, block3Tip, block4, coinbaseTx, coinstakeTx);
            block4.vtx[0] = MakeTransactionRef(coinbaseTx); // set coinbase
            coinstakeTx.vout[1] = CTxOut(nextStake.coin->txout.nValue + stakeAmount, walletPaymentScript); // staker payment
            BOOST_CHECK(signCoinstake(coinstakeTx, wallet.get()));
            block4.vtx[1] = MakeTransactionRef(coinstakeTx);
            block4.hashMerkleRoot = BlockMerkleRoot(block4);
            BOOST_CHECK(SignBlock(block4, nextStake.coin->txout.scriptPubKey, *wallet));
            auto block4ptr = std::make_shared<const CBlock>(block4);
            bool newBlock{false};
            processed = ProcessNewBlock(Params(), block4ptr, true, &newBlock);
            SyncWithValidationInterfaceQueue();
            max++;
        }
        BOOST_CHECK_MESSAGE(processed, "block4 failed to process");
    }

    // block3 should be chain tip
    BOOST_CHECK_MESSAGE(lookupIndex(block3.GetHash()) == chainActive[chainActive.Height()-1], "expecting block3 to be the new chain tip");

    // at least 6 blocks to trigger orphan stake check
    StakeBlocks(7); SyncWithValidationInterfaceQueue();

    {
        LOCK2(wallet1->cs_wallet, wallet2->cs_wallet);
        auto it1 = wallet1->mapWallet.find(block1.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it1 == wallet1->mapWallet.end(), "a) wallet1 orphaned tx should not be present in wallet db");
        auto it2 = wallet2->mapWallet.find(block2.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it2 != wallet2->mapWallet.end(), "a) wallet2 tx 1 should be present");
        auto it3 = wallet2->mapWallet.find(block3.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it3 != wallet2->mapWallet.end(), "a) wallet2 tx 2 should be present");
        BOOST_CHECK_MESSAGE(!wallet1->HasWalletSpend(block1.vtx[1]->GetHash()), "a) wallet1 orphaned tx prevout should not be spent");
    }

    {
        StakeBlocks(50); SyncWithValidationInterfaceQueue();
        LOCK2(wallet1->cs_wallet, wallet2->cs_wallet);
        auto it1 = wallet1->mapWallet.find(block1.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it1 == wallet1->mapWallet.end(), "b) wallet1 orphaned tx should not be present in wallet db");
        auto it2 = wallet2->mapWallet.find(block2.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it2 != wallet2->mapWallet.end(), "b) wallet2 tx 1 should be present");
        auto it3 = wallet2->mapWallet.find(block3.vtx[1]->GetHash());
        BOOST_CHECK_MESSAGE(it3 != wallet2->mapWallet.end(), "b) wallet2 tx 2 should be present");
    }

    wallet1Utxos = utxos(wallet1.get(), locked_chain.get());
    wallet2Utxos = utxos(wallet2.get(), locked_chain.get());
    BOOST_CHECK_MESSAGE(wallet1->GetBalance(ISMINE_ALL, 0) == walletBalance1, "balance on wallet1 with orphaned stake should be restored to pre-orphan amount");
    walletBalance2 = wallet2->GetBalance(ISMINE_ALL, 0);
    CAmount walletBalance2_expected = (walletUtxo2 + stakeAmount) * 2;
    BOOST_CHECK_MESSAGE(walletBalance2 == walletBalance2_expected, "balance on wallet2 with orphaned stake should be restored to pre-orphan amount");

    UnregisterValidationInterface(wallet1.get());
    UnregisterValidationInterface(wallet2.get());
    wallet1.reset();
    wallet2.reset();
}

BOOST_AUTO_TEST_SUITE_END()
