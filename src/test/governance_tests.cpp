// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <consensus/tx_verify.h>
#include <consensus/merkle.h>
#include <governance/governancewallet.h>
#include <net.h>
#include <node/transaction.h>
#include <wallet/coincontrol.h>

#include <boost/test/test_tools.hpp>

bool GovernanceSetupFixtureSetup{false};
struct GovernanceSetupFixture {
    explicit GovernanceSetupFixture() {
        if (GovernanceSetupFixtureSetup) return; GovernanceSetupFixtureSetup = true;
        chain_100_40001_50();
        chain_200_40001_50();
    }
    void chain_100_40001_50() {
        auto pos = std::make_shared<TestChainPoS>(false);
        auto *params = (CChainParams*)&Params();
        params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 100 * COIN;
            else if (blockHeight % consensusParams.superblock == 0)
                return 40001 * COIN;
            return 50 * COIN;
        };
        pos->Init("100,40001,50");
        pos.reset();
    }
    void chain_200_40001_50() {
        auto pos = std::make_shared<TestChainPoS>(false);
        auto *params = (CChainParams*)&Params();
        params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 200 * COIN;
            else if (blockHeight % consensusParams.superblock == 0)
                return 40001 * COIN;
            return 50 * COIN;
        };
        pos->Init("200,40001,50");
        pos.reset();
    }
};

BOOST_FIXTURE_TEST_SUITE(governance_tests, GovernanceSetupFixture)

int nextSuperblock(const int & block, const int & superblock) {
    return block + (superblock - block % superblock);
}

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

bool sendToRecipients(CWallet *wallet, const std::vector<CRecipient> & recipients, CTransactionRef & tx, std::vector<std::pair<CTxOut,COutPoint>> *recvouts=nullptr) {
    // Create and send the transaction
    CReserveKey reservekey(wallet);
    CAmount nFeeRequired;
    std::string strError;
    int nChangePosRet = -1;
    CCoinControl cc;
    auto locked_chain = wallet->chain().lock();
    if (!wallet->CreateTransaction(*locked_chain, recipients, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc))
        return false;
    if (recvouts) { // ensure vouts in order of recipients
        std::set<COutPoint> used;
        for (int i = 0;  i < recipients.size(); ++i) {
            auto & rec = recipients[i];
            for (int j = 0; j < tx->vout.size(); ++j) {
                auto vout = tx->vout[j];
                if (used.count({tx->GetHash(), (uint32_t)j}))
                    continue;
                if (vout.scriptPubKey == rec.scriptPubKey && vout.nValue == rec.nAmount) {
                    recvouts->emplace_back(vout, COutPoint(tx->GetHash(), j));
                    used.insert({tx->GetHash(), (uint32_t)j});
                    break;
                }
            }
        }
    }
    CValidationState state;
    auto sent = wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state);
    BOOST_CHECK_MESSAGE(state.IsValid(), state.GetRejectReason());
    return sent && state.IsValid();
}

bool newWalletAddress(CWallet *wallet, CTxDestination & dest) {
    wallet->TopUpKeyPool();
    CPubKey newKey;
    if (!wallet->GetKeyFromPool(newKey))
        return false;
    wallet->LearnRelatedScripts(newKey, OutputType::LEGACY);
    dest = GetDestinationForKey(newKey, OutputType::LEGACY);
    return true;
}

bool sendProposal(gov::Proposal & proposal, CTransactionRef & tx, TestChainPoS *testChainPoS, const CChainParams & params) {
    // Check that proposal block was accepted with proposal tx in it
    const int blockHeight = chainActive.Height();
    testChainPoS->StakeBlocks(1), SyncWithValidationInterfaceQueue();
    BOOST_CHECK_EQUAL(blockHeight+1, chainActive.Height());
    CBlock proposalBlock;
    BOOST_CHECK(ReadBlockFromDisk(proposalBlock, chainActive.Tip(), params.GetConsensus()));
    bool found{false};
    for (const auto & txn : proposalBlock.vtx) {
        if (txn->GetHash() == tx->GetHash()) {
            found = true;
            break;
        }
    }
    BOOST_CHECK_MESSAGE(found, "Proposal tx output was not found in the chain tip");
    bool govHasProp = gov::Governance::instance().hasProposal(proposal.getHash());
    BOOST_CHECK_MESSAGE(govHasProp, "Failed to add proposal to the governance proposal list");
    return found && govHasProp;
}

bool createUtxos(const CAmount & targetAmount, const CAmount & utxoAmount, TestChainPoS *testChainPoS) {
    CWallet *wallet = testChainPoS->wallet.get();
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        wallet->AvailableCoins(*testChainPoS->locked_chain, coins);
    }
    std::sort(coins.begin(), coins.end(), [](const COutput & a, const COutput & b) {
        return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
    });
    const CAmount feeAmount = 1 * COIN;
    CAmount runningAmount{0};
    CMutableTransaction mtx;
    int pos{0};
    while (runningAmount < targetAmount - feeAmount) {
        mtx.vin.emplace_back(coins[pos].GetInputCoin().outpoint);
        // slice vouts
        for (CAmount i = 0; i < coins[pos].GetInputCoin().txout.nValue; i += utxoAmount)
            mtx.vout.emplace_back(utxoAmount, coins[pos].GetInputCoin().txout.scriptPubKey);
        runningAmount += coins[pos].GetInputCoin().txout.nValue;
        ++pos;
    }
    // Cover fee
    mtx.vin.emplace_back(coins[pos].GetInputCoin().outpoint);
    mtx.vout.emplace_back(coins[pos].GetInputCoin().txout.nValue - feeAmount, coins[pos].GetInputCoin().txout.scriptPubKey);

    // Sign the tx inputs
    const auto scriptPubKey = coins[0].GetInputCoin().txout.scriptPubKey;
    for (int i = 0; i < (int)mtx.vin.size(); ++i) {
        auto & vin = mtx.vin[i];
        SignatureData sigdata = DataFromTransaction(mtx, i, coins[i].GetInputCoin().txout);
        BOOST_CHECK(ProduceSignature(*wallet, MutableTransactionSignatureCreator(&mtx, i, coins[i].GetInputCoin().txout.nValue, SIGHASH_ALL), scriptPubKey, sigdata));
        UpdateInput(vin, sigdata);
    }
    // Send transaction
    CReserveKey reservekey(wallet);
    CValidationState state;
    return wallet->CommitTransaction(MakeTransactionRef(mtx), {}, {}, reservekey, g_connman.get(), state);
}

bool applySuperblockPayees(TestChainPoS & pos, CBlockTemplate *blocktemplate, const StakeMgr::StakeCoin & stake,
        const std::vector<CTxOut> & payees, const Consensus::Params & consensus, const CAmount addStakeSubsidy=0)
{
    CBlock *pblock = &blocktemplate->block;
    const int nHeight = chainActive.Height() + 1;
    // Create coinstake transaction
    CMutableTransaction coinstakeTx;
    coinstakeTx.vin.resize(1);
    coinstakeTx.vin[0] = CTxIn(stake.coin->outpoint);
    coinstakeTx.vout.resize(2); // coinstake + stake payment
    coinstakeTx.vout[0].SetNull(); // coinstake
    coinstakeTx.vout[0].nValue = 0;
    coinstakeTx.vout.resize(2 + payees.size()); // coinstake + stake payment + payees
    for (int i = 0; i < static_cast<int>(payees.size()); ++i)
        coinstakeTx.vout[2 + i] = payees[i];
    const bool feesEnabled = IsNetworkFeesEnabled(chainActive.Tip(), consensus);
    // Can't claim any part of the superblock amount as stake reward
    const auto stakeSubsidy = GetBlockSubsidy(nHeight, consensus) -
                              (gov::Governance::isSuperblock(nHeight, consensus) ? consensus.proposalMaxAmount : 0);
    const auto stakeAmount = (feesEnabled ? blocktemplate->vTxFees[0] : 0) + stakeSubsidy + addStakeSubsidy;
    // Find pubkey of stake input
    CTxDestination stakeInputDest;
    if (!ExtractDestination(stake.coin->txout.scriptPubKey, stakeInputDest))
        return false;
    const auto keyid = GetKeyForDestination(*pos.wallet.get(), stakeInputDest);
    if (keyid.IsNull())
        return false;
    CScript paymentScript;
    if (VersionBitsState(chainActive.Tip(), consensus, Consensus::DEPLOYMENT_STAKEP2PKH, versionbitscache) == ThresholdState::ACTIVE) { // Stake to p2pkh
        paymentScript = GetScriptForDestination(keyid);
    } else { // stake to p2pk
        CPubKey paymentPubKey;
        if (!pos.wallet->GetPubKey(keyid, paymentPubKey))
            throw std::runtime_error(strprintf("%s: Failed to find staked input pubkey", __func__));
        paymentScript = CScript() << ToByteVector(paymentPubKey) << OP_CHECKSIG;
    }
    // stake amount and payment script
    coinstakeTx.vout[1] = CTxOut(stake.coin->txout.nValue + stakeAmount, paymentScript); // staker payment
    // Sign stake input w/ keystore
    auto signInput = [](CMutableTransaction & tx, CWallet *keystore) -> bool {
        SignatureData empty; // clean script sig on all inputs
        for (auto & txin : tx.vin)
            UpdateInput(txin, empty);
        auto locked_chain = keystore->chain().lock();
        LOCK(keystore->cs_wallet);
        if (!keystore->SignTransaction(tx))
            return false;
        return true;
    };
    // Calculate network fee for coinbase/coinstake txs
    if (!signInput(coinstakeTx, pos.wallet.get()))
        return false;
    const auto coinbaseBytes = ::GetSerializeSize(pblock->vtx[0], PROTOCOL_VERSION);
    const auto coinstakeBytes = ::GetSerializeSize(coinstakeTx, PROTOCOL_VERSION);
    CAmount estimatedNetworkFee = static_cast<CAmount>(::minRelayTxFee.GetFee(coinbaseBytes) + ::minRelayTxFee.GetFee(coinstakeBytes));
    coinstakeTx.vout[1] = CTxOut(stake.coin->txout.nValue + stakeAmount - estimatedNetworkFee, paymentScript); // staker payment w/ network fee taken out
    if (!signInput(coinstakeTx, pos.wallet.get())) // resign with correct fee estimation
        return false;
    // Assign coinstake tx
    pblock->vtx[1] = MakeTransactionRef(std::move(coinstakeTx));
    blocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, chainActive.Tip(), consensus);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    blocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);
    return SignBlock(*pblock, stake.coin->txout.scriptPubKey, *pos.wallet);
}

bool stakeWallet(std::shared_ptr<CWallet> & wallet, StakeMgr & staker, const COutPoint & stakeInput, const int & tryiter) {
    const CChainParams & params = Params();
    int tries{0};
    int64_t tipBlockTime{0};
    const int currentBlockHeight = chainActive.Height();
    while (chainActive.Height() < currentBlockHeight + 1) {
        try {
            CBlockIndex *tip = nullptr;
            CBlockIndex *stakeIndex = nullptr;
            std::shared_ptr<COutput> output = nullptr;
            CTransactionRef tx;
            uint256 block;
            {
                LOCK(cs_main);
                tip = chainActive.Tip();
                tipBlockTime = tip->GetBlockTime();
                if (!GetTransaction(stakeInput.hash, tx, params.GetConsensus(), block))
                    return false;
                stakeIndex = LookupBlockIndex(block);
                if (!stakeIndex)
                    return false;
                {
                    LOCK(wallet->cs_wallet);
                    const CWalletTx *wtx = wallet->GetWalletTx(tx->GetHash());
                    output = std::make_shared<COutput>(wtx, stakeInput.n, tip->nHeight - stakeIndex->nHeight, true, true, true);
                }
            }
            const auto adjustedTime = GetAdjustedTime();
            const auto fromTime = std::max(tip->GetBlockTime()+1, adjustedTime);
            const auto blockTime = fromTime;
            const auto toTime = fromTime + params.GetConsensus().PoSFutureBlockTimeLimit(blockTime);
            std::map<int64_t, std::vector<StakeMgr::StakeCoin>> stakes;
            if (staker.GetStakesMeetingTarget(output, wallet, tip, adjustedTime, blockTime, fromTime, toTime,
                                              stakes, params.GetConsensus())) {
                for (auto & item : stakes) {
                    for (auto & sc : item.second) {
                        if (staker.StakeBlock(sc, params))
                            return true;
                    }
                }
            }
        } catch (std::exception & e) {
            LogPrintf("Staker ran into an exception: %s\n", e.what());
            throw e;
        } catch (...) {
            throw std::runtime_error("Staker unknown error");
        }
        if (++tries > tryiter)
            throw std::runtime_error("Staker failed to find stake");
        SetMockTime(GetAdjustedTime() + params.GetConsensus().PoSFutureBlockTimeLimit(tipBlockTime));
    }
    return false;
}

bool isTxInBlock(const CBlockIndex *blockhash, const uint256 & txhash, const Consensus::Params & consensus) {
    CBlock block;
    ReadBlockFromDisk(block, blockhash, consensus);
    bool txInBlock{false};
    for (auto & vtx : block.vtx) {
        if (vtx->GetHash() == txhash) {
            txInBlock = true;
            break;
        }
    }
    return txInBlock;
}

bool cleanup(int blockCount, CWallet *wallet=nullptr) {
    const auto & params = Params();
    {
        LOCK2(cs_main, mempool.cs);
        mempool.clear();
    }
    CValidationState state;
    while (chainActive.Height() > blockCount)
        InvalidateBlock(state, params, chainActive.Tip(), false);
    ActivateBestChain(state, params); SyncWithValidationInterfaceQueue();
    gArgs.ForceSetArg("-proposaladdress", "");
    removeGovernanceDBFiles();
    gov::Governance::instance().reset();
    if (wallet) {
        std::vector<CWalletTx> wtx;
        wallet->ZapWalletTx(wtx);
        WalletRescanReserver reserver(wallet);
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
    }
    return true;
}

BOOST_FIXTURE_TEST_CASE(governance_tests_proposals, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    const auto & params = Params();
    const auto & consensus = params.GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());

    // Check vote copy constructor
    {
        gov::Vote vote1(COutPoint{m_coinbase_txns[5]->vin[0].prevout});
        gov::Vote vote2;
        vote2 = vote1;
        BOOST_CHECK_MESSAGE(vote1 == vote2, "Vote copy constructor should work");
    }

    // Check proposal copy constructor
    {
        gov::Proposal proposal1("Test proposal-1", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                         EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        gov::Proposal proposal2;
        proposal2 = proposal1;
        BOOST_CHECK_MESSAGE(proposal1 == proposal2, "Proposal copy constructor should work");
    }

    // Check normal proposal
    gov::Proposal p1("Test proposal-1", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(p1.isValid(consensus), "Basic proposal should be valid");

    // Proposal with underscores should pass
    gov::Proposal p2("Test proposal_2", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(p2.isValid(consensus), "Basic proposal should be valid");

    // Proposal with empty description should pass
    gov::Proposal p2a("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "https://forum.blocknet.co", "");
    BOOST_CHECK_MESSAGE(p2a.isValid(consensus), "Proposal should be valid with empty description");

    // Proposal with empty url should pass
    gov::Proposal p2b("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "", "Short description");
    BOOST_CHECK_MESSAGE(p2b.isValid(consensus), "Proposal should be valid with empty url");

    // Proposal with empty name should fail
    gov::Proposal p2c("", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p2c.isValid(consensus), "Proposal should fail with empty name");

    // Proposal with minimum size name should pass
    gov::Proposal p2d("ab", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
            EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(p2d.isValid(consensus), "Proposal should be valid with minimal name");

    // Proposal with maxed out size should pass (157 bytes is the max size of a proposal)
    gov::Proposal p2m("Test proposal max", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "This description is the maximum allowed for this particular prp");
    BOOST_CHECK_MESSAGE(p2m.isValid(consensus), "Proposal at max description should pass");

    // Proposal with maxed out size + 1 should fail
    gov::Proposal p2n("Test proposal max", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", p2m.getDescription() + "1");
    BOOST_CHECK_MESSAGE(!p2n.isValid(consensus), "Proposal with max description + 1 should fail");

    // Proposal should fail if description is too long
    gov::Proposal p3("Test proposal-3", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "This is a long description that causes the proposal to fail. Proposals are limited by the OP_RETURN size");
    BOOST_CHECK_MESSAGE(!p3.isValid(consensus), "Proposal should fail if its serialized size is too large");

    // Should fail if amount is too high
    gov::Proposal p4("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 100000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p4.isValid(consensus), "Proposal should fail if amount is too large");

    // Should fail if amount is too low
    gov::Proposal p4a("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), consensus.proposalMinAmount-1,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p4a.isValid(consensus), "Proposal should fail if amount is too small");

    // Should fail if proposal address is bad
    gov::Proposal p5("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     "fjdskjfksdafjksdajfkdsajfkasjdfk", "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p5.isValid(consensus), "Proposal should fail on bad proposal address");

    // Should fail on bad superblock height
    gov::Proposal p6("Test proposal", 17, 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p6.isValid(consensus), "Proposal should fail on bad superblock height");

    // Should fail on bad proposal name (special chars)
    gov::Proposal p7("Test $proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p7.isValid(consensus), "Proposal should fail on bad proposal name (special chars)");

    // Should fail on bad proposal name (starts with spaces)
    gov::Proposal p8(" Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p8.isValid(consensus), "Proposal should be invalid if name starts with whitespace");

    // Should fail on bad proposal name (ends with spaces)
    gov::Proposal p9("Test proposal ", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(!p9.isValid(consensus), "Proposal should be invalid if name ends with whitespace");

    // Check that proposal submission tx is added to mempool
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal psubmit("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                              EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(psubmit, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        BOOST_CHECK_MESSAGE(mempool.exists(tx->GetHash()), "Proposal submission tx should be in the mempool");
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        ss << psubmit;
        bool found{false};
        for (const auto & out : tx->vout) {
            if (out.scriptPubKey[0] == OP_RETURN) {
                BOOST_CHECK_EQUAL(consensus.proposalFee,  out.nValue);
                BOOST_CHECK_MESSAGE((CScript() << OP_RETURN << ToByteVector(ss)) == out.scriptPubKey, "Proposal submission OP_RETURN script in tx should match expected");
                found = true;
            }
        }
        BOOST_CHECK_MESSAGE(found, "Proposal submission tx must contain an OP_RETURN");
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    // Check proposal that is under the pushdata1 requirements should be properly processed
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal psubmit("tt", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                              EncodeDestination(dest), "", "");
        CTransactionRef tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(psubmit, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        BOOST_CHECK_MESSAGE(mempool.exists(tx->GetHash()), "Proposal submission tx should be in the mempool");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(!gov::Governance::instance().getProposal(psubmit.getHash()).isNull(), "Very small proposal should exist");
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    // Check -proposaladdress config option
    {
        const auto resetBlocks = chainActive.Height();
        CTxDestination newDest;
        BOOST_CHECK(newWalletAddress(wallet.get(), newDest));
        gArgs.ForceSetArg("-proposaladdress", EncodeDestination(newDest));

        // Send coin to the proposal address
        CTransactionRef tx;
        bool accepted = sendToAddress(wallet.get(), newDest, 50 * COIN, tx);
        if (!accepted) cleanup(resetBlocks);
        BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create and submit proposal
        gov::Proposal pp1("Test -proposaladdress", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                              EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef pp1_tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(pp1, {wallet}, consensus, pp1_tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit proposal: %s", failReason));

        // Check that proposal tx was accepted
        accepted = pp1_tx != nullptr && sendProposal(pp1, pp1_tx, this, params);
        if (!accepted) cleanup(resetBlocks);
        BOOST_REQUIRE_MESSAGE(accepted, "Proposal tx should confirm to the network before continuing");

        // Check that proposal tx pays change to -proposaladdress
        uint256 block;
        CTransactionRef txPrev;
        BOOST_CHECK_MESSAGE(GetTransaction(pp1_tx->vin[0].prevout.hash, txPrev, params.GetConsensus(), block), "Failed to find vin transaction");
        const auto & prevOut = txPrev->vout[pp1_tx->vin[0].prevout.n];
        const auto & inAmount = prevOut.nValue;
        const auto & inAddress = prevOut.scriptPubKey;
        CTxDestination extractAddr;
        BOOST_CHECK_MESSAGE(ExtractDestination(inAddress, extractAddr), "Failed to extract payment address from proposaladdress vin");
        BOOST_CHECK_MESSAGE(newDest == extractAddr, "Address vin should match -proposaladdress config flag");
        bool foundOpReturn{false};
        bool foundChangeAddress{false};
        for (const auto & out : pp1_tx->vout) {
            if (out.scriptPubKey[0] == OP_RETURN && out.nValue == consensus.proposalFee)
                foundOpReturn = true;
            if (out.scriptPubKey == GetScriptForDestination(extractAddr)) {
                const auto fee = inAmount - pp1_tx->GetValueOut();
                BOOST_CHECK_MESSAGE(fee > 0, "Proposal tx must account for network fee");
                foundChangeAddress = out.nValue == inAmount - consensus.proposalFee - fee;
            }
        }
        BOOST_CHECK_MESSAGE(foundOpReturn, "Failed to find proposal fee payment");
        BOOST_CHECK_MESSAGE(foundChangeAddress, "Failed to find proposal change address payment");
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_votes, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        wallet->AvailableCoins(*locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");
    const gov::VinHash & vinHash = gov::makeVinHash(coins.front().GetInputCoin().outpoint);
    std::set<gov::VinHash> vinHashes{vinHash};

    // Check normal proposal
    gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(proposal.isValid(consensus), "Basic proposal should be valid");

    // Check YES vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::YES, coins.begin()->GetInputCoin().outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote YES signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(vinHashes, consensus), "Vote YES should be valid upon signing");
    }

    // Check NO vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::NO, coins.begin()->GetInputCoin().outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote NO signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(vinHashes, consensus), "Vote NO should be valid upon signing");
    }

    // Check ABSTAIN vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::ABSTAIN, coins.begin()->GetInputCoin().outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote ABSTAIN signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(vinHashes, consensus), "Vote ABSTAIN should be valid upon signing");
    }

    // Bad vote type should fail
    {
        gov::Vote vote(proposal.getHash(), (gov::VoteType)99, coins.begin()->GetInputCoin().outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(vinHashes, consensus), "Vote with invalid type should fail");
    }

    // Signing with key not matching utxo should fail
    {
        CKey key; key.MakeNewKey(true);
        gov::Vote vote(proposal.getHash(), gov::YES, coins.begin()->GetInputCoin().outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(key), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(vinHashes, consensus), "Vote with bad signing key should fail");
    }

    // Vote with utxo from a non-owned address in mempool should fail
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        CTransactionRef tx;
        BOOST_CHECK(sendToAddress(wallet.get(), newDest, 50 * COIN, tx));
        // find n pos
        COutPoint outpoint;
        for (int i = 0; i < static_cast<int>(tx->vout.size()); ++i) {
            const auto & out = tx->vout[i];
            CTxDestination destination;
            ExtractDestination(out.scriptPubKey, destination);
            if (newDest == destination) {
                outpoint = {tx->GetHash(), static_cast<uint32_t>(i)};
                break;
            }
        }
        BOOST_CHECK_MESSAGE(!outpoint.IsNull(), "Vote utxo should not be null in non-owned address check");
        gov::Vote vote(proposal.getHash(), gov::YES, outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(vinHashes, consensus), "Vote with bad utxo should fail");
        // clean up
        cleanup(resetBlocks);
    }

    // Vote with utxo from a non-owned address should fail
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        CTransactionRef tx;
        bool sent = sendToAddress(wallet.get(), newDest, 50 * COIN, tx);
        BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        if (sent) StakeBlocks(1), SyncWithValidationInterfaceQueue();
        // find n pos
        COutPoint outpoint;
        for (int i = 0; i < static_cast<int>(tx->vout.size()); ++i) {
            const auto & out = tx->vout[i];
            CTxDestination destination;
            ExtractDestination(out.scriptPubKey, destination);
            if (newDest == destination) {
                outpoint = {tx->GetHash(), static_cast<uint32_t>(i)};
                break;
            }
        }
        BOOST_CHECK_MESSAGE(!outpoint.IsNull(), "Vote utxo should not be null in non-owned address check");
        gov::Vote vote(proposal.getHash(), gov::YES, outpoint, vinHash);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(vinHashes, consensus), "Vote with bad utxo should fail");
        // Clean up
        cleanup(resetBlocks);
    }

    // Voting with spent utxo should fail
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        CTransactionRef tx;
        CTransactionRef txVoteInput;
        bool sent = sendToAddress(wallet.get(), newDest, 200 * COIN, tx)
                 && sendToAddress(wallet.get(), newDest, 1 * COIN, txVoteInput);
        BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        CTransactionRef ptx; // proposal tx
        std::string failReason;
        gov::SubmitProposal(proposal, {wallet}, consensus, ptx, g_connman.get(), &failReason);
        if (sent) StakeBlocks(1), SyncWithValidationInterfaceQueue();
        COutPoint outpoint;
        CTxOut txout;
        for (int i = 0; i < static_cast<int>(tx->vout.size()); ++i) {
            const auto & out = tx->vout[i];
            CTxDestination destination;
            ExtractDestination(out.scriptPubKey, destination);
            if (newDest == destination) {
                outpoint = {tx->GetHash(), static_cast<uint32_t>(i)};
                txout = out;
                break;
            }
        }
        BOOST_CHECK_MESSAGE(!outpoint.IsNull(), "Vote utxo should not be null");
        // Submit the vote with spent utxo
        CBasicKeyStore keystore;
        keystore.AddKey(key);
        // Vote should not be accepted since the input is being spent in the voting tx's itself
        {
            gov::VinHash voteVinHash = gov::makeVinHash(outpoint);
            gov::Vote vote(proposal.getHash(), gov::YES, outpoint, voteVinHash);
            BOOST_CHECK_MESSAGE(vote.sign(key), "Vote signing should succeed");
            BOOST_CHECK_MESSAGE(vote.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote should be valid");
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(outpoint);
            CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
            ss << vote;
            auto voteScript = CScript() << OP_RETURN << ToByteVector(ss);
            mtx.vout.resize(2);
            mtx.vout[0] = CTxOut(0, voteScript); // vote here w/ spent utxo
            mtx.vout[1] = CTxOut(200 * COIN - COIN, txout.scriptPubKey);
            SignatureData sigdata = DataFromTransaction(mtx, 0, txout);
            ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, txout.nValue, SIGHASH_ALL),
                             txout.scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            // Send transaction
            uint256 txid;
            std::string errstr;
            const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 100 * COIN);
            BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            BOOST_CHECK_MESSAGE(!gov::Governance::instance().hasVote(vote.getHash()), "Vote should not be accepted since its input was spent in mempool");
            // update outpoint with latest utxo
            outpoint = {mtx.GetHash(), 1};
            txout = mtx.vout[1];
        }
        // Vote should not be accepted since it was spent in the last block
        // Spend a utxo and then attempt to vote with that spent utxo
        {
            CMutableTransaction mtx1;
            mtx1.vin.resize(1);
            mtx1.vin[0] = CTxIn(outpoint);
            mtx1.vout.resize(1);
            mtx1.vout[0] = CTxOut(txout.nValue - COIN, txout.scriptPubKey);
            {
                SignatureData sigdata = DataFromTransaction(mtx1, 0, txout);
                ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx1, 0, txout.nValue, SIGHASH_ALL),
                                 txout.scriptPubKey, sigdata);
                UpdateInput(mtx1.vin[0], sigdata);
                // Send transaction
                uint256 txid;
                std::string errstr;
                const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx1), txid, errstr, 100 * COIN);
                BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
                StakeBlocks(1), SyncWithValidationInterfaceQueue();
            }

            // Obtain valid utxo to use in submitting the vote
            COutPoint prevout;
            CTxOut prevtxout;
            for (int i = 0; i < static_cast<int>(txVoteInput->vout.size()); ++i) {
                const auto & out = txVoteInput->vout[i];
                CTxDestination destination;
                ExtractDestination(out.scriptPubKey, destination);
                if (newDest == destination) {
                    prevout = {txVoteInput->GetHash(), static_cast<uint32_t>(i)};
                    prevtxout = out;
                    break;
                }
            }
            COutPoint voteOutpoint = outpoint; // reference the spent utxo
            gov::VinHash voteVinHash = gov::makeVinHash(prevout); // valid prevout to submit the vote with
            gov::Vote vote(proposal.getHash(), gov::YES, voteOutpoint, voteVinHash);
            BOOST_CHECK_MESSAGE(vote.sign(key), "Vote signing should succeed");
            BOOST_CHECK_MESSAGE(vote.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote should be valid");

            // Create voting transaction, make sure the vote is referencing the spent utxo
            // but is being submitted by a valid unspent vin
            CMutableTransaction mtx2;
            mtx2.vin.resize(1);
            mtx2.vin[0] = CTxIn(prevout);
            CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
            ss << vote;
            auto voteScript = CScript() << OP_RETURN << ToByteVector(ss);
            mtx2.vout.resize(2);
            mtx2.vout[0] = CTxOut(0, voteScript); // vote here w/ spent utxo
            mtx2.vout[1] = CTxOut(prevtxout.nValue - 10000, prevtxout.scriptPubKey);
            {
                SignatureData sigdata = DataFromTransaction(mtx2, 0, prevtxout);
                ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx2, 0, prevtxout.nValue, SIGHASH_ALL),
                                 prevtxout.scriptPubKey, sigdata);
                UpdateInput(mtx2.vin[0], sigdata);
                // Send transaction
                uint256 txid;
                std::string errstr;
                const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx2), txid, errstr, 100 * COIN);
                BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
                StakeBlocks(1), SyncWithValidationInterfaceQueue();
                BOOST_CHECK_MESSAGE(!gov::Governance::instance().hasVote(vote.getHash()), "Vote should not be accepted since its input was spent in previous block");
            }
        }
        // Clean up
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_votes_undo, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 200*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        wallet->AvailableCoins(*locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");
    const gov::VinHash & vinHash = gov::makeVinHash(coins.front().GetInputCoin().outpoint);
    std::set<gov::VinHash> vinHashes{vinHash};

    // Check normal proposal
    gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(proposal.isValid(consensus), "Basic proposal should be valid");

    // Voting with spent utxo should fail
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        // Submit the vote with spent utxo
        bool firstRun;
        auto otherwallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
        otherwallet->LoadWallet(firstRun);
        AddKey(*otherwallet, key);
        otherwallet->SetBroadcastTransactions(true);
        rescanWallet(otherwallet.get());
        RegisterValidationInterface(otherwallet.get());
        // Vote inputs
        {
            CTransactionRef tx;
            CTransactionRef txVoteInput;
            bool sent = sendToAddress(wallet.get(), newDest, 200 * COIN, tx)
                     && sendToAddress(wallet.get(), newDest, 3 * COIN, txVoteInput);
            BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        }
        // Create proposal
        {
            CTransactionRef ptx; // proposal tx
            std::string failReason;
            gov::SubmitProposal(proposal, {wallet}, consensus, ptx, g_connman.get(), &failReason);
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
        }
        // 1) Vote on a proposal
        {
            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txs;
            std::string failReason;
            bool success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txs, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
        }
        // 2) Spend vote
        {
            CTransactionRef tx;
            bool sent = sendToAddress(otherwallet.get(), newDest, otherwallet->GetBalance()-COIN, tx);
            BOOST_CHECK_MESSAGE(sent, "Spending vote utxos failed");
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(vs.empty(), strprintf("Expecting 0 votes, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_CHECK_MESSAGE(pvs.size() == 1 && pvs[0].spent(), "Expecting 1 spent vote");
        }
        // 3) Simulate block invalidation/disconnect and make sure votes are properly unspent
        {
            CValidationState state;
            BOOST_CHECK_MESSAGE(InvalidateBlock(state, *params, chainActive.Tip(), false), "Failed to invalidate the block with spent vote");
            ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
        }
        // 4) Check vote is valid after new block
        {
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
        }
        // Clean up
        UnregisterValidationInterface(otherwallet.get());
        otherwallet.reset();
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_votes_changescutoff, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 200*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        wallet->AvailableCoins(*locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");
    const gov::VinHash & vinHash = gov::makeVinHash(coins.front().GetInputCoin().outpoint);
    std::set<gov::VinHash> vinHashes{vinHash};

    // Check normal proposal
    gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(proposal.isValid(consensus), "Basic proposal should be valid");

    // Casting change votes inside voting cutoff period should not influence the tally
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        bool firstRun;
        auto otherwallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
        otherwallet->LoadWallet(firstRun);
        AddKey(*otherwallet, key);
        otherwallet->SetBroadcastTransactions(true);
        rescanWallet(otherwallet.get());
        RegisterValidationInterface(otherwallet.get());
        // Vote inputs
        {
            CTransactionRef tx;
            CTransactionRef txVoteInput;
            bool sent = sendToAddress(wallet.get(), newDest, 200 * COIN, tx)
                     && sendToAddress(wallet.get(), newDest, 3 * COIN, txVoteInput);
            BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        }
        // Create proposal
        {
            CTransactionRef ptx; // proposal tx
            std::string failReason;
            gov::SubmitProposal(proposal, {wallet}, consensus, ptx, g_connman.get(), &failReason);
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
        }
        // 1) Vote on a proposal
        gov::ProposalVote proposalVoteYes{proposal, gov::YES};
        {
            std::vector<CTransactionRef> txs;
            std::string failReason;
            bool success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVoteYes}, {otherwallet}, consensus, txs, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_CHECK_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
        }
        // 2) Stake to cutoff period
        StakeBlocks(gov::NextSuperblock(consensus, chainActive.Height()) - consensus.votingCutoff), SyncWithValidationInterfaceQueue();
        // 3) Attempt to change vote
        {
            gov::ProposalVote proposalVoteNo{proposal, gov::NO};
            std::vector<CTransactionRef> txs;
            std::string failReason;
            bool success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVoteNo}, {otherwallet}, consensus, txs, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Change vote failed: %s", failReason));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            BOOST_CHECK_MESSAGE(vs[0].getVote() == proposalVoteYes.vote, strprintf("Expecting vote to remain unchanged in cutoff period"));
        }
        // Clean up
        UnregisterValidationInterface(otherwallet.get());
        otherwallet.reset();
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_undo_submissions, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 200*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    std::string failReason;

    // Check normal proposal
    gov::Proposal proposal("Test Proposal Undo", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "");
    BOOST_REQUIRE_MESSAGE(proposal.isValid(consensus), "Proposal should be valid");
    CTransactionRef ptx; // proposal tx
    gov::SubmitProposal(proposal, {wallet}, consensus, ptx, g_connman.get(), &failReason);
    StakeBlocks(1), SyncWithValidationInterfaceQueue();
    BOOST_REQUIRE_MESSAGE(gov::Governance::instance().getProposal(proposal.getHash()).isValid(consensus), "Proposal should be valid");

    // Setup other wallet to cast votes from
    CKey key; key.MakeNewKey(true);
    const auto & voteDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
    // Submit the vote with spent utxo
    auto otherwallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
    bool firstRun; otherwallet->LoadWallet(firstRun);
    AddKey(*otherwallet, key);
    otherwallet->SetBroadcastTransactions(true);
    rescanWallet(otherwallet.get());
    RegisterValidationInterface(otherwallet.get());
    std::vector<std::pair<CTxOut,COutPoint>> recvouts;

    // Send vote coin to otherwallet
    {
        CTransactionRef sendtx;
        auto recipients = std::vector<CRecipient>{
            {GetScriptForDestination(voteDest), 200*COIN, false},
            {GetScriptForDestination(voteDest), 2*COIN, false}
        };
        bool sent = sendToRecipients(wallet.get(), recipients, sendtx, &recvouts);
        BOOST_REQUIRE_MESSAGE(sent, "Send to another address failed");
        int checkHeight = chainActive.Height();
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_REQUIRE_MESSAGE(recvouts[0].first.nValue == 200 * COIN, strprintf("Expecting a vote utxo with %s BLOCK, found %s BLOCK", FormatMoney(200*COIN), FormatMoney(recvouts[0].first.nValue)));
        BOOST_REQUIRE_MESSAGE(recvouts[1].first.nValue == 2 * COIN, strprintf("Expecting a vote input with %s BLOCK, found %s BLOCK", FormatMoney(2*COIN), FormatMoney(recvouts[1].first.nValue)));
        BOOST_REQUIRE_MESSAGE(checkHeight+1 == chainActive.Height(), "Block should be accepted");
        BOOST_REQUIRE_MESSAGE(isTxInBlock(chainActive.Tip(), sendtx->GetHash(), consensus), "Expecting transaction to be included in the block");
    }

    // Vote on the proposal
    gov::ProposalVote proposalVote{proposal, gov::YES};
    {
        std::vector<CTransactionRef> txs;
        bool success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txs, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        auto vs = gov::Governance::instance().getVotes(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
        BOOST_REQUIRE_MESSAGE(vs[0].getAmount() == 200*COIN, strprintf("Expecting a vote against utxo with amount %s, found %s", FormatMoney(200*COIN), FormatMoney(vs[0].getAmount())));
    }

    StakeBlocks(4), SyncWithValidationInterfaceQueue();

    // Simulate orphaned votes
    for (int i = 0; i < 25; ++i) {
        {
            UnregisterValidationInterface(otherwallet.get());
            otherwallet.reset();
            otherwallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
            otherwallet->LoadWallet(firstRun);
            AddKey(*otherwallet, key);
            otherwallet->SetBroadcastTransactions(true);
            rescanWallet(otherwallet.get());
            RegisterValidationInterface(otherwallet.get());
        }

        const auto resetBlocks = chainActive.Height();

        // Submit proposal for testing proposal invalidation
        gov::Proposal prop("Test Proposal Undo 2", nextSuperblock(chainActive.Height(), consensus.superblock), 1500*COIN,
                EncodeDestination(dest), "https://forum.blocknet.co", "2");
        BOOST_CHECK_MESSAGE(prop.isValid(consensus), "Proposal should be valid");
        {
            CTransactionRef proptx; // proposal tx
            gov::SubmitProposal(prop, {wallet}, consensus, proptx, g_connman.get(), &failReason);
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            BOOST_CHECK_MESSAGE(gov::Governance::instance().getProposal(prop.getHash()).isValid(consensus), "Proposal should be valid");
        }
        // Change proposal vote from yes to no
        {
            gov::ProposalVote proposalVoteNo{proposal, gov::NO};
            std::vector<CTransactionRef> txs;
            bool success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVoteNo}, {otherwallet}, consensus, txs, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            BOOST_REQUIRE_MESSAGE(isTxInBlock(chainActive.Tip(), txs[0]->GetHash(), consensus), "Expecting transaction to be included in the block");
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            BOOST_REQUIRE_MESSAGE(vs[0].getVote() == proposalVoteNo.vote, strprintf("Expecting vote to be %s, found %s", gov::Vote::voteTypeToString(proposalVoteNo.vote), gov::Vote::voteTypeToString(vs[0].getVote())));
        }
        // Simulate orphaned vote and make sure no vote is properly unrecorded
        {
            CValidationState state;
            BOOST_REQUIRE_MESSAGE(InvalidateBlock(state, *params, chainActive.Tip(), false), "Failed to invalidate the block with spent vote");
            ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_REQUIRE_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
            BOOST_REQUIRE_MESSAGE(vs[0].getVote() == proposalVote.vote, strprintf("Expecting vote to be %s, found %s", gov::Vote::voteTypeToString(proposalVote.vote), gov::Vote::voteTypeToString(vs[0].getVote())));
        }
        // Spend vote
        {
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(recvouts[0].second);
            mtx.vout.resize(1);
            mtx.vout[0] = CTxOut(recvouts[0].first.nValue - 10000, recvouts[0].first.scriptPubKey);
            SignatureData sigdata = DataFromTransaction(mtx, 0, recvouts[0].first);
            ProduceSignature(*otherwallet, MutableTransactionSignatureCreator(&mtx, 0, recvouts[0].first.nValue, SIGHASH_ALL),
                    recvouts[0].first.scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            uint256 txid;
            std::string errstr;
            const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 1 * COIN);
            BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            BOOST_REQUIRE_MESSAGE(isTxInBlock(chainActive.Tip(), mtx.GetHash(), consensus), "Expecting transaction to be included in the block");
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.empty(), strprintf("Expecting 0 votes, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_REQUIRE_MESSAGE(pvs.size() == 1 && pvs[0].spent(), "Expecting 1 spent vote");
        }
        // Simulate orphaned block and make sure votes are properly unspent
        {
            CValidationState state;
            BOOST_REQUIRE_MESSAGE(InvalidateBlock(state, *params, chainActive.Tip(), false), "Failed to invalidate the block with spent vote");
            ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_REQUIRE_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
        }
        // Check vote is valid after new block
        {
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_REQUIRE_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
            BOOST_REQUIRE_MESSAGE(pvs[0].getVote() == proposalVote.vote, strprintf("Expecting vote to be %s, found %s", gov::Vote::voteTypeToString(proposalVote.vote), gov::Vote::voteTypeToString(pvs[0].getVote())));
            BOOST_REQUIRE_MESSAGE(pvs[0].getAmount() == 200*COIN, strprintf("Expecting a vote against utxo with amount %s, found %s", FormatMoney(200*COIN), FormatMoney(pvs[0].getAmount())));
        }

        // Invalidate all blocks added during this iteration
        CValidationState state;
        while (chainActive.Height() > resetBlocks)
            InvalidateBlock(state, *params, chainActive.Tip(), false);
        ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
        rescanWallet(wallet.get());

        // Check vote is valid after cleanup
        {
            auto vs = gov::Governance::instance().getVotes(proposal.getHash());
            BOOST_REQUIRE_MESSAGE(vs.size() == 1, strprintf("Expecting 1 vote, found %u", vs.size()));
            auto pvs = gov::Governance::instance().getVotes(proposal.getHash(), true);
            BOOST_REQUIRE_MESSAGE(pvs.size() == 1 && !pvs[0].spent(), "Expecting 1 unspent vote");
            BOOST_REQUIRE_MESSAGE(pvs[0].getVote() == proposalVote.vote, strprintf("Expecting vote to be %s, found %s", gov::Vote::voteTypeToString(proposalVote.vote), gov::Vote::voteTypeToString(pvs[0].getVote())));
        }
        // Check proposal was removed
        {
            BOOST_CHECK_MESSAGE(gov::Governance::instance().getProposal(prop.getHash()).isNull(), "Proposal should be null");
        }
    }

    UnregisterValidationInterface(otherwallet.get());
    otherwallet.reset();
    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_AUTO_TEST_CASE(governance_tests_votereplayattacks)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 500*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());
    bool success{false};

    // Create voting wallet
    CKey voteDestKey; voteDestKey.MakeNewKey(true);
    CTxDestination voteDest(voteDestKey.GetPubKey().GetID());
    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    AddKey(*otherwallet, voteDestKey);
    otherwallet->SetBroadcastTransactions(true);
    rescanWallet(otherwallet.get());
    const int voteInputs{2};
    const CAmount voteInputAmount{1*COIN};
    const CAmount totalVoteAmount = consensus.voteBalance*2;
    const auto voteUtxoCount = static_cast<int>(totalVoteAmount/consensus.voteMinUtxoAmount);
    const auto maxVotes = totalVoteAmount/consensus.voteBalance;

    // Get to the nearest future SB if necessary
    if (gov::PreviousSuperblock(consensus, chainActive.Height()) == 0) {
        const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
        pos.StakeBlocks(blocks), SyncWithValidationInterfaceQueue();
    }

    // Prepare the utxos to use for votes
    {
        CMutableTransaction votetx;
        // Create tx with enough inputs to cover votes
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins);
        }
        std::sort(coins.begin(), coins.end(), [](const COutput & a, const COutput & b) {
            return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
        });
        // Staker script
        const auto stakerScriptPubKey = coins[0].GetInputCoin().txout.scriptPubKey;
        // Create vins
        CAmount runningAmount{0};
        int coinpos{0};
        std::vector<CTxOut> txouts;
        std::vector<COutPoint> outs;
        while (runningAmount < totalVoteAmount + voteInputAmount*voteInputs) {
            outs.push_back(coins[coinpos].GetInputCoin().outpoint);
            txouts.push_back(coins[coinpos].GetInputCoin().txout);
            runningAmount += coins[coinpos].GetInputCoin().txout.nValue;
            ++coinpos;
        }
        votetx.vin.resize(outs.size());
        for (int i = 0; i < (int)outs.size(); ++i)
            votetx.vin[i] = CTxIn(outs[i]);
        const CAmount fees = 1 * COIN;
        const CAmount voteInputTotal = voteInputAmount * voteInputs;
        const CAmount change = runningAmount - consensus.voteMinUtxoAmount*voteUtxoCount - voteInputs*voteInputAmount - fees;
        // Create vouts
        for (int i = 0; i < voteUtxoCount; ++i)
            votetx.vout.emplace_back(consensus.voteMinUtxoAmount, GetScriptForDestination(voteDest));
        if (change > 10000)
            votetx.vout.emplace_back(change, stakerScriptPubKey); // change back to staker
        for (int i = 0; i < voteInputs; ++i) // create vote inputs
            votetx.vout.emplace_back(voteInputAmount, GetScriptForDestination(voteDest));
        // Sign the tx inputs
        for (int i = 0; i < (int)votetx.vin.size(); ++i) {
            auto & vin = votetx.vin[i];
            SignatureData sigdata = DataFromTransaction(votetx, i, txouts[i]);
            ProduceSignature(*pos.wallet, MutableTransactionSignatureCreator(&votetx, i, txouts[i].nValue, SIGHASH_ALL), stakerScriptPubKey, sigdata);
            UpdateInput(vin, sigdata);
        }
        // Send transaction
        CReserveKey reservekey(pos.wallet.get());
        CValidationState state;
        BOOST_CHECK(pos.wallet->CommitTransaction(MakeTransactionRef(votetx), {}, {}, reservekey, g_connman.get(), state));
        BOOST_CHECK_MESSAGE(state.IsValid(), strprintf("Failed to submit tx for otherwallet vote utxos: %s", state.GetRejectReason()));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
        BOOST_REQUIRE_MESSAGE(otherwallet->GetBalance() == totalVoteAmount+voteInputTotal, strprintf("other wallet expects a balance of %d, only has %d", totalVoteAmount+voteInputTotal, otherwallet->GetBalance()));
    }

    gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                           EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    gov::Vote firstVote;
    COutPoint firstVoteVinPrevout;
    std::string failReason;

    // Create and submit proposal (use regular wallet)
    {
        CTransactionRef tx = nullptr;
        success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));
    }

    RegisterValidationInterface(otherwallet.get());

    // Submit the first vote and then submit vote change
    {
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit YES vote in replay attack test: %s", failReason));
        failReason.clear();
        BOOST_REQUIRE_MESSAGE(txns.size() == 1 && txns[0]->vin.size() == 1, "Expecting only 1 vote transaction with 1 vin");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block; // use to check staked inputs used in votes
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        std::set<gov::Proposal> ps;
        std::set<gov::Vote> vs;
        std::map<uint256, std::set<gov::VinHash>> vh;
        gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
        gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
        BOOST_REQUIRE_MESSAGE(!vs.empty(), strprintf("Expecting at least 1 vote, found %u", vs.size()));
        firstVote = *vs.begin(); // store first vote to use with replay attack
        firstVoteVinPrevout = txns[0]->vin[0].prevout;
        txns.clear();
        // Change vote to NO
        gov::ProposalVote proposalVoteNo{proposal, gov::NO};
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVoteNo}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit NO vote in replay attack test: %s", failReason));
        BOOST_REQUIRE_MESSAGE(txns.size() == 1 && txns[0]->vin.size() == 1, "Expecting only 1 vote transaction with 1 vin");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
    }

    // Try and replay YES vote with non-owner wallet (should fail)
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins);
        }
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(2);
        mtx.vin[0] = CTxIn(coins.front().GetInputCoin().outpoint);
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        ss << firstVote;
        auto script = CScript() << OP_RETURN << ToByteVector(ss);
        mtx.vout[0] = CTxOut(0, script);
        mtx.vout[1] = CTxOut(coins.front().GetInputCoin().txout.nValue - COIN, coins.front().GetInputCoin().txout.scriptPubKey);
        SignatureData sigdata = DataFromTransaction(mtx, 0, coins.front().GetInputCoin().txout);
        ProduceSignature(*pos.wallet, MutableTransactionSignatureCreator(&mtx, 0, coins.front().GetInputCoin().txout.nValue, SIGHASH_ALL),
                coins.front().GetInputCoin().txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        // Send transaction
        CReserveKey reservekey(pos.wallet.get());
        CValidationState state;
        BOOST_CHECK(pos.wallet->CommitTransaction(MakeTransactionRef(mtx), {}, {}, reservekey, g_connman.get(), state));
        BOOST_CHECK_MESSAGE(state.IsValid(), "Failed to submit vote tx in replay attack test");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block; // use to check staked inputs used in votes
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        std::set<gov::Proposal> ps;
        std::set<gov::Vote> vs;
        std::map<uint256, std::set<gov::VinHash>> vh;
        gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
        gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
        // Vote should not be accepted
        BOOST_CHECK_MESSAGE(vs.empty(), "Vote replay attack should fail on non-owner wallet");
    }

    // Try and replay YES vote with same wallet but random input (should fail too)
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, otherwallet->cs_wallet);
            otherwallet->AvailableCoins(*pos.locked_chain, coins);
        }
        const COutput & selected = coins.front();
        const auto & prevoutVinHash = gov::makeVinHash(firstVoteVinPrevout);
        BOOST_CHECK_MESSAGE(memcmp(&firstVote.getVinHash(), &prevoutVinHash, firstVote.getVinHash().size()) == 0,
                "Expecting first vote's vin prevout to match first vote's vin hash");
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(2);
        mtx.vin[0] = CTxIn(selected.GetInputCoin().outpoint);
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        ss << firstVote;
        auto script = CScript() << OP_RETURN << ToByteVector(ss);
        mtx.vout[0] = CTxOut(0, script);
        mtx.vout[1] = CTxOut(selected.GetInputCoin().txout.nValue - 5000, selected.GetInputCoin().txout.scriptPubKey);
        SignatureData sigdata = DataFromTransaction(mtx, 0, selected.GetInputCoin().txout);
        ProduceSignature(*otherwallet, MutableTransactionSignatureCreator(&mtx, 0, selected.GetInputCoin().txout.nValue, SIGHASH_ALL),
                         selected.GetInputCoin().txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        // Send transaction
        CReserveKey reservekey(otherwallet.get());
        CValidationState state;
        BOOST_CHECK(otherwallet->CommitTransaction(MakeTransactionRef(mtx), {}, {}, reservekey, g_connman.get(), state));
        BOOST_CHECK_MESSAGE(state.IsValid(), strprintf("Failed to submit vote tx in replay attack test: %s", state.GetRejectReason()));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block; // use to check staked inputs used in votes
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        std::set<gov::Proposal> ps;
        std::set<gov::Vote> vs;
        std::map<uint256, std::set<gov::VinHash>> vh;
        gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
        gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
        // Vote should not be accepted
        BOOST_CHECK_MESSAGE(vs.empty(), "Vote replay attack should fail on same wallet");
    }

    UnregisterValidationInterface(otherwallet.get());
    otherwallet.reset();
    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_submissions, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());

    // Check proposal submission is accepted by the network
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal proposal("Test proposal 1", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit proposal: %s", failReason));
        auto accepted = tx != nullptr && sendProposal(proposal, tx, this, *params);
        BOOST_CHECK_MESSAGE(accepted, "Proposal submission failed");
        // clean up
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    // Check that results are empty for proposal without any votes
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal proposal("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit proposal: %s", failReason));
        auto accepted = tx != nullptr && sendProposal(proposal, tx, this, *params);
        BOOST_CHECK_MESSAGE(accepted, "Proposal submission failed");
        auto results = gov::Governance::instance().getSuperblockResults(nextSuperblock(chainActive.Height(), consensus.superblock), consensus);
        BOOST_CHECK_MESSAGE(results.empty(), "Superblock results on a proposal with 0 votes should be empty");
        // clean up
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    // Check proposal voting with specific address
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal proposal("Test proposal addrs", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        std::string failReason;
        auto success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        // Prep vote utxo
        CKey key; key.MakeNewKey(true);
        AddKey(*wallet, key);
        CTxDestination voteDest(key.GetPubKey().GetID());
        CTransactionRef sendtx;
        auto recipients = std::vector<CRecipient>{
            {GetScriptForDestination(voteDest), 1000 * COIN, false},
            {GetScriptForDestination(voteDest), 1 * COIN, false}
        };
        bool sent = sendToRecipients(wallet.get(), recipients, sendtx);
        BOOST_CHECK_MESSAGE(sent, "Failed to create vote network fee payment address");
        if (sent) StakeBlocks(1), SyncWithValidationInterfaceQueue();
        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES, voteDest};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {wallet}, consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_REQUIRE_MESSAGE(txns.size() == 1, strprintf("Expected 1 vote transaction to be created, %d were created", txns.size()));
        // check that tx is standard
        BOOST_CHECK_MESSAGE(IsStandardTx(*txns[0], failReason), strprintf("Vote transaction is not standard: %s", failReason));
        failReason.clear();
        BOOST_CHECK_MESSAGE(txns[0]->vin.size() == 1, strprintf("Expected 1 vote input, found %u", txns[0]->vin.size()));
        {
            uint256 hashBlock;
            CTransactionRef vtx;
            BOOST_CHECK_MESSAGE(GetTransaction(txns[0]->vin[0].prevout.hash, vtx, consensus, hashBlock), "Vote input tx should be valid");
            BOOST_CHECK_MESSAGE(vtx->vout[txns[0]->vin[0].prevout.n].nValue == 1 * COIN, strprintf("Vote input tx amount is wrong, expected 1 BLOCK found %s", FormatMoney(vtx->vout[txns[0]->vin[0].prevout.n].nValue)));
            StakeBlocks(1), SyncWithValidationInterfaceQueue();
        }
        auto votes = gov::Governance::instance().getVotes(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(votes.size() == 1, strprintf("Expected 1 vote, found %u", votes.size()));
        {
            uint256 hashBlock;
            CTransactionRef vtx;
            BOOST_CHECK_MESSAGE(GetTransaction(votes[0].getUtxo().hash, vtx, consensus, hashBlock), "Vote utxo tx should be valid");
            BOOST_CHECK_MESSAGE(vtx->vout[votes[0].getUtxo().n].nValue == 1000 * COIN, strprintf("Vote utxo amount is wrong, expected 1000 BLOCK found %s", FormatMoney(vtx->vout[votes[0].getUtxo().n].nValue)));
            CTxDestination vdest;
            BOOST_CHECK(ExtractDestination(vtx->vout[votes[0].getUtxo().n].scriptPubKey, vdest));
            BOOST_CHECK_MESSAGE(vdest == voteDest, "Vote utxo address does not match the expected address the 1000 BLOCK was sent to");
        }
        // clean up
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    // Check proposal submission votes are accepted by the network
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool sent = sendToAddress(wallet.get(), dest, 1 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(sent, "Failed to create vote network fee payment address");
        if (sent) StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create and submit proposal
        gov::Proposal proposal("Test proposal 3", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        auto success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_REQUIRE_MESSAGE(txns.size() == 2, strprintf("Expected 2 vote transaction to be created, %d were created", txns.size()));
        // check that tx is standard
        BOOST_CHECK_MESSAGE(IsStandardTx(*txns[0], failReason), strprintf("Vote transaction is not standard: %s", failReason));
        failReason.clear();
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block; // use to check staked inputs used in votes
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        bool foundVoteTx{false};
        for (const auto & txn : block.vtx) {
            if (txn->GetHash() == txns[0]->GetHash()) {
                foundVoteTx = true;
                break;
            }
        }
        BOOST_CHECK_MESSAGE(foundVoteTx, "Vote transaction failed to be accepted in block");

        // Check all the vote outputs and test that they're valid
        // Add up all the amounts and make sure votes across the
        // proposal are larger than the minimum amount.
        CAmount voteAmount{0};
        for (const auto & txn : txns) {
            std::set<gov::VinHash> vinHashes;
            for (const auto & vin : txn->vin)
                vinHashes.insert(gov::makeVinHash(vin.prevout));
            for (int n = 0; n < (int)txn->vout.size(); ++n) {
                const auto & out = txn->vout[n];
                if (out.scriptPubKey[0] != OP_RETURN)
                    continue;
                CScript::const_iterator pc = out.scriptPubKey.begin();
                std::vector<unsigned char> data;
                while (pc < out.scriptPubKey.end()) {
                    opcodetype opcode;
                    if (!out.scriptPubKey.GetOp(pc, opcode, data))
                        break;
                    if (!data.empty())
                        break;
                }

                CDataStream ss(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                gov::NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                BOOST_CHECK_MESSAGE(obj.getType() == gov::VOTE, "Invalid vote OP_RETURN type");
                CDataStream ss2(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                gov::Vote vote({txn->GetHash(), static_cast<uint32_t>(n)}, block.GetBlockTime());
                ss2 >> vote;
                bool valid = vote.loadVoteUTXO() && vote.isValid(vinHashes, consensus);
                BOOST_CHECK_MESSAGE(vote.getProposal() == proposal.getHash(), "Vote data should match the expected proposal hash");
                BOOST_CHECK_MESSAGE(vote.getVote() == proposalVote.vote, "Vote data should match the expected vote type");
                if (vote.getUtxo() == block.vtx[1]->vin[0].prevout) { // staked inputs associated with votes should be invalid
                    BOOST_CHECK_MESSAGE(gov::IsVoteSpent(vote, chainActive.Height(), consensus.governanceBlock, false), "Vote should be marked as spent when its utxo stakes");
                    BOOST_CHECK_MESSAGE(!gov::Governance::instance().hasVote(vote.getHash()), "Governance manager should not know about spent votes");
                    continue;
                } else {
                    BOOST_CHECK_MESSAGE(valid, "Vote should be valid");
                    BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote.getHash()), "Governance manager should know about the vote");
                }

                // Search for vote utxo transaction
                uint256 blk;
                CTransactionRef txUtxo;
                bool utxoFound = GetTransaction(vote.getUtxo().hash, txUtxo, consensus, blk);
                BOOST_CHECK_MESSAGE(utxoFound, "Failed to find utxo used to cast vote");
                if (utxoFound) {
                    const auto amount = txUtxo->vout[vote.getUtxo().n].nValue;
                    BOOST_CHECK_MESSAGE(amount >= consensus.voteMinUtxoAmount, "Vote utxo fails to meet minimum utxo vote amount");
                    voteAmount += amount;
                }
            }
        }
        BOOST_CHECK_MESSAGE(voteAmount >= consensus.voteBalance, "Vote transaction failed to meet the minimum balance requirement");

        // clean up
        cleanup(resetBlocks, wallet.get());
        ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_FIXTURE_TEST_CASE(governance_tests_vote_limits, TestChainPoS)
{
    auto *params = (CChainParams*)&Params();
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    bool success{false};

    // Check that maxing out the votes per tx creates multiple transactions
    {
        RegisterValidationInterface(&gov::Governance::instance());
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 5*COIN;
        params->consensus.voteBalance = 600*COIN;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(wallet.get(), dest, 5 * COIN, sendtx)
                        && sendToAddress(wallet.get(), dest, 5 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Prep utxos for use with votes
        success = createUtxos(params->consensus.voteBalance, params->consensus.voteMinUtxoAmount, this);
        BOOST_REQUIRE_MESSAGE(success, "Failed to create the required utxo set for use with vote limit tests");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create and submit proposal
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        success = gov::Governance::instance().hasProposal(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(success, "Proposal not found");

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {wallet}, consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_CHECK_MESSAGE(txns.size() == 3, strprintf("Expected 3 vote transactions to be created, %d were created", txns.size()));
        BOOST_REQUIRE_MESSAGE(!txns.empty(), "Proposal tx should confirm to the network before continuing");
        for (const auto & txn : txns)
            BOOST_CHECK_MESSAGE(IsStandardTx(*txn, failReason), strprintf("Vote transaction is not standard: %s", failReason));
        failReason.clear();
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block; // use to check staked inputs used in votes
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        bool foundVoteTx{false};
        for (const auto & txn : block.vtx) {
            if (txn->GetHash() == txns[0]->GetHash()) {
                foundVoteTx = true;
                break;
            }
        }
        BOOST_CHECK_MESSAGE(foundVoteTx, "Vote transaction failed to be accepted in block");

        // clean up
        ReloadWallet();
        UnregisterValidationInterface(&gov::Governance::instance());
        gov::Governance::instance().reset();
    }

    // Check situation where there's not enough vote balance
    {
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        RegisterValidationInterface(&gov::Governance::instance());
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 100*COIN;
        params->consensus.voteBalance = 50000*COIN;

        // Create and submit proposal
        gov::Proposal proposal("Test proposal vote balance", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        success = gov::Governance::instance().hasProposal(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(success, "Proposal not found");

        // Submit the vote (should fail)
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, g_connman.get(), &failReason);
        BOOST_CHECK_MESSAGE(!success, "Vote submission should fail");
        BOOST_CHECK_MESSAGE(!failReason.empty(), "Fail reason should be defined when submit votes fails");
        BOOST_CHECK_MESSAGE(txns.empty(), "Expected transactions list to be empty");

        // clean up
        ReloadWallet();
        UnregisterValidationInterface(&gov::Governance::instance());
        gov::Governance::instance().reset();
    }

    // Check vote tally
    {
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        RegisterValidationInterface(&gov::Governance::instance());
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 5*COIN;
        params->consensus.voteBalance = 250*COIN;

        // Create and submit proposal
        gov::Proposal proposal("Test proposal vote tally", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        success = gov::SubmitProposal(proposal, {wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        success = gov::Governance::instance().hasProposal(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(success, "Proposal not found");

        // Prep vote utxos for non-wallet address to simulate another user voting
        CKey notInWalletKey; notInWalletKey.MakeNewKey(true);
        CTxDestination notInWalletDest(notInWalletKey.GetPubKey().GetID());

        std::vector<CRecipient> notInWalletOuts; notInWalletOuts.resize(51);
        for (int i = 0; i < (int)notInWalletOuts.size(); ++i)
            notInWalletOuts[i] = {GetScriptForDestination(notInWalletDest), 5*COIN, false};
        CTransactionRef sendtx;
        CCoinControl cc;
        cc.destChange = dest;
        CReserveKey reservekey(wallet.get());
        CAmount nFeeRequired;
        std::string strError;
        int nChangePosRet = -1;
        CValidationState state;
        BOOST_CHECK(wallet->CreateTransaction(*locked_chain, notInWalletOuts, sendtx, reservekey, nFeeRequired, nChangePosRet, strError, cc));
        BOOST_CHECK(wallet->CommitTransaction(sendtx, {}, {}, reservekey, g_connman.get(), state));

        bool firstRun;
        auto otherwallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
        otherwallet->LoadWallet(firstRun);
        AddKey(*otherwallet, notInWalletKey);
        otherwallet->SetBroadcastTransactions(true);
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
        RegisterValidationInterface(otherwallet.get());

        // Submit the votes for the other wallet
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txnsOther;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txnsOther, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), "Fail reason should be empty for tally test");
        BOOST_CHECK_MESSAGE(txnsOther.size() == 1, strprintf("Expected 1 transaction, instead have %d on tally test", txnsOther.size()));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        auto tallyOther = gov::Governance::getTally(proposal.getHash(), gov::Governance::instance().getVotes(), consensus);
        BOOST_CHECK_EQUAL(tallyOther.yes, 1);
        BOOST_CHECK_EQUAL(tallyOther.no, 0);
        BOOST_CHECK_EQUAL(tallyOther.abstain, 0);
        BOOST_CHECK_EQUAL(tallyOther.cyes, 250*COIN);
        BOOST_CHECK_EQUAL(tallyOther.cno, 0);
        BOOST_CHECK_EQUAL(tallyOther.cabstain, 0);

        // Submit the votes for the current wallet
        std::vector<CTransactionRef> txns;
        failReason.clear();
        success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {wallet}, consensus, txns, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), "Fail reason should be empty for tally test");
        BOOST_CHECK_MESSAGE(txns.size() == 3, strprintf("Expected %d transactions, instead have %d on tally test", 3, txns.size()));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        auto tally = gov::Governance::getTally(proposal.getHash(), gov::Governance::instance().getVotes(), consensus);
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        std::set<gov::Proposal> ps;
        std::set<gov::Vote> vs;
        std::map<uint256, std::set<gov::VinHash>> vh;
        gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
        gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
        CAmount voteAmount{0};
        for (const auto & vote : vs)
            voteAmount += vote.getAmount();
        BOOST_CHECK_MESSAGE(voteAmount/consensus.voteBalance == tally.yes-tallyOther.yes,
                strprintf("Expected %d votes instead tallied %d", voteAmount/consensus.voteBalance, tally.yes-tallyOther.yes));

        // clean up
        UnregisterValidationInterface(otherwallet.get());
        otherwallet.reset();
        ReloadWallet();
        UnregisterValidationInterface(&gov::Governance::instance());
        gov::Governance::instance().reset();
    }

    cleanup(chainActive.Height(), wallet.get());
    ReloadWallet();
}

BOOST_AUTO_TEST_CASE(governance_tests_superblockresults)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 50*COIN;
    params->consensus.voteBalance = 1250*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Create voting wallet
    CKey voteDestKey; voteDestKey.MakeNewKey(true);
    CTxDestination voteDest(voteDestKey.GetPubKey().GetID());
    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    AddKey(*otherwallet, voteDestKey);
    otherwallet->SetBroadcastTransactions(true);
    rescanWallet(otherwallet.get());
    const CAmount totalVoteAmount = consensus.voteBalance*2;
    const auto voteUtxoCount = static_cast<int>(totalVoteAmount/consensus.voteMinUtxoAmount);
    const auto maxVotes = totalVoteAmount/consensus.voteBalance;
    const CAmount voteInputUtxoAmount{consensus.voteMinUtxoAmount - 10 * COIN};
    const int voteInputUtxoCount{2};
    const auto totalTxAmount = totalVoteAmount + voteInputUtxoAmount*voteInputUtxoCount;
    CMutableTransaction votetx;

    // Prepare the utxos to use for votes
    {
        // Get to the nearest future SB if necessary
        if (gov::PreviousSuperblock(consensus, chainActive.Height()) == 0) {
            const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
            pos.StakeBlocks(blocks), SyncWithValidationInterfaceQueue();
        }
        // Create tx with enough inputs to cover votes
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins);
        }
        std::sort(coins.begin(), coins.end(), [](const COutput & a, const COutput & b) {
            return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
        });
        // Create vins
        CAmount runningAmount{0};
        int coinpos{0};
        std::vector<CTxOut> txouts;
        std::vector<COutPoint> outs;
        while (runningAmount < totalVoteAmount + voteInputUtxoAmount*voteInputUtxoCount) {
            outs.push_back(coins[coinpos].GetInputCoin().outpoint);
            txouts.push_back(coins[coinpos].GetInputCoin().txout);
            runningAmount += coins[coinpos].GetInputCoin().txout.nValue;
            ++coinpos;
        }
        votetx.vin.resize(outs.size());
        for (int i = 0; i < (int)outs.size(); ++i)
            votetx.vin[i] = CTxIn(outs[i]);
        // Create vouts
        for (int i = 0; i < voteUtxoCount; ++i)
            votetx.vout.emplace_back(consensus.voteMinUtxoAmount, GetScriptForDestination(voteDest));
        for (int i = 0; i < voteInputUtxoCount; ++i)
            votetx.vout.emplace_back(voteInputUtxoAmount, GetScriptForDestination(voteDest)); // use this for vote inputs
        const auto changeScript = coins[0].GetInputCoin().txout.scriptPubKey;
        const CAmount fees = 3000 * (votetx.vin.size()*180 + votetx.vout.size()*230); // estimate bytes per vin/vout
        votetx.vout.emplace_back(runningAmount - totalVoteAmount - voteInputUtxoAmount*voteInputUtxoCount - fees, changeScript); // change back to staker
        // Sign the tx inputs
        for (int i = 0; i < (int)votetx.vin.size(); ++i) {
            auto & vin = votetx.vin[i];
            SignatureData sigdata = DataFromTransaction(votetx, i, txouts[i]);
            ProduceSignature(*pos.wallet, MutableTransactionSignatureCreator(&votetx, i, txouts[i].nValue, SIGHASH_ALL), changeScript, sigdata);
            UpdateInput(vin, sigdata);
        }
        // Send transaction
        CReserveKey reservekey(pos.wallet.get());
        CValidationState state;
        BOOST_CHECK(pos.wallet->CommitTransaction(MakeTransactionRef(votetx), {}, {}, reservekey, g_connman.get(), state));
        BOOST_CHECK_MESSAGE(state.IsValid(), strprintf("Failed to submit tx for otherwallet vote utxos: %s", state.GetRejectReason()));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
        BOOST_REQUIRE_MESSAGE(otherwallet->GetBalance() == totalVoteAmount+voteInputUtxoAmount*voteInputUtxoCount, strprintf("other wallet expects a balance of %d, only has %d", totalVoteAmount+voteInputUtxoAmount*voteInputUtxoCount, otherwallet->GetBalance()));
    }

    // Check superblock proposal and votes
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        std::set<gov::Proposal> proposalsA;
        std::set<gov::Vote> votesA;

        // Test single superblock
        {
            std::set<gov::Proposal> proposals;
            std::set<gov::Vote> votes;
            for (int i = 0; i < 16; ++i) {
                const gov::Proposal proposal{strprintf("Test Proposal A%d", i), gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
                proposals.insert(proposal);
                CTransactionRef tx;
                auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                // Submit votes with otherwallet
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns;
                failReason.clear();
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                rescanWallet(otherwallet.get());
                // Count the votes
                CBlock block;
                ReadBlockFromDisk(block, chainActive.Tip(), consensus);
                std::set<gov::Proposal> ps;
                std::set<gov::Vote> vs;
                std::map<uint256, std::set<gov::VinHash>> vh;
                gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
                gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
                BOOST_CHECK_MESSAGE(vs.size() == voteUtxoCount, strprintf("Expecting total votes cast to be %u found %u", voteUtxoCount, vs.size()));
                votes.insert(vs.begin(), vs.end());
            }

            // Stake to one block before the next superblock
            const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
            pos.StakeBlocks(blocks-1), SyncWithValidationInterfaceQueue();

            proposalsA = proposals;
            votesA = votes;

            std::vector<gov::Proposal> allProposals;
            std::vector<gov::Vote> allVotes;
            gov::Governance::instance().getProposalsForSuperblock(gov::NextSuperblock(consensus), allProposals, allVotes);
            BOOST_CHECK_MESSAGE(proposals.size() == allProposals.size(), strprintf("Expected to have %d proposals, instead have %d", proposals.size(), allProposals.size()));
            BOOST_CHECK_MESSAGE(votes.size() == allVotes.size(), strprintf("Expected to have %d votes, instead have %d", votes.size(), allVotes.size()));
            BOOST_CHECK_MESSAGE(votes.size() == voteUtxoCount*proposals.size(), strprintf("Expected to have %d votes, instead have %d", votes.size(), voteUtxoCount*proposals.size()));

            // Make sure all the expected proposals and votes are found in the data
            std::set<gov::Proposal> allProposalsSet(allProposals.begin(), allProposals.end());
            std::set<gov::Vote> allVotesSet(allVotes.begin(), allVotes.end());
            for (const auto & proposal : proposals)
                BOOST_CHECK(allProposalsSet.count(proposal));
            for (const auto & vote : votes)
                BOOST_CHECK(allVotesSet.count(vote));
        }

        // Stake to the next superblock
        const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
        pos.StakeBlocks(blocks), SyncWithValidationInterfaceQueue();

        // Check that the superblock payout was valid
        {
            CBlock block;
            ReadBlockFromDisk(block, chainActive.Tip(), consensus);
            CAmount superblockPayment{0};
            const auto & results = gov::Governance::instance().getSuperblockResults(chainActive.Height(), consensus);
            const auto & payees = gov::Governance::getSuperblockPayees(chainActive.Height(), results, consensus);
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(chainActive.Height(), allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(!results.empty(), "Superblock results should not be empty");
            BOOST_CHECK_MESSAGE(payees.size() == allProposalsB.size(), "Superblock payees should match expected proposals");
            BOOST_CHECK_MESSAGE(gov::Governance::instance().isValidSuperblock(&block, chainActive.Height(), consensus, superblockPayment), "Expected superblock payout to be valid");
            BOOST_CHECK_MESSAGE(gov::Governance::isSuperblock(chainActive.Height(), consensus), "Expected superblock to be accepted");
        }

        std::set<gov::Proposal> proposalsB;
        std::set<gov::Vote> votesB;

        // Next superblock batch
        {
            std::set<gov::Proposal> proposals;
            std::set<gov::Vote> votes;
            for (int i = 0; i < 5; ++i) {
                const gov::Proposal proposal{strprintf("Test Proposal B%d", i), gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
                proposals.insert(proposal);
                CTransactionRef tx;
                auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                // Submit votes with otherwallet
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns;
                failReason.clear();
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                rescanWallet(otherwallet.get());
                // Count the votes
                CBlock block;
                ReadBlockFromDisk(block, chainActive.Tip(), consensus);
                std::set<gov::Proposal> ps;
                std::set<gov::Vote> vs;
                std::map<uint256, std::set<gov::VinHash>> vh;
                gov::Governance::instance().dataFromBlock(&block, ps, vs, vh, consensus, chainActive.Tip()->nHeight);
                gov::Governance::instance().filterDataFromBlock(ps, vs, vh, consensus, chainActive.Tip()->nHeight, true);
                BOOST_CHECK_MESSAGE(vs.size() == voteUtxoCount, strprintf("Expecting total votes cast to be %u found %u", voteUtxoCount, vs.size()));
                votes.insert(vs.begin(), vs.end());
            }

            proposalsB = proposals;
            votesB = votes;

            // Check that all proposals and votes from batch A are valid
            std::vector<gov::Proposal> allProposalsA;
            std::vector<gov::Vote> allVotesA;
            gov::Governance::instance().getProposalsForSuperblock(gov::PreviousSuperblock(consensus), allProposalsA, allVotesA);
            BOOST_CHECK_MESSAGE(proposalsA.size() == allProposalsA.size(), strprintf("Expected to have %d proposals, instead have %d", proposalsA.size(), allProposalsA.size()));
            BOOST_CHECK_MESSAGE(votesA.size() == allVotesA.size(), strprintf("Expected to have %d votes, instead have %d", votesA.size(), allVotesA.size()));

            // Check that all proposals and votes from batch B are valid
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(gov::NextSuperblock(consensus), allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(proposalsB.size() == allProposalsB.size(), strprintf("Expected to have %d proposals, instead have %d", proposalsB.size(), allProposalsB.size()));
            BOOST_CHECK_MESSAGE(votesB.size() == allVotesB.size(), strprintf("Expected to have %d votes, instead have %d", votesB.size(), allVotesB.size()));

            // Make sure all expected proposals and votes are found in the getProposalsForSuperblock data
            std::vector<gov::Proposal> allProposals;
            allProposals.insert(allProposals.end(), allProposalsA.begin(), allProposalsA.end());
            allProposals.insert(allProposals.end(), allProposalsB.begin(), allProposalsB.end());
            std::vector<gov::Vote> allVotes;
            allVotes.insert(allVotes.end(), allVotesA.begin(), allVotesA.end());
            allVotes.insert(allVotes.end(), allVotesB.begin(), allVotesB.end());
            BOOST_CHECK_MESSAGE(allVotes.size() == voteUtxoCount*allProposals.size(), strprintf("Expected to have %d votes, instead have %d", allVotes.size(), voteUtxoCount*allProposals.size()));

            std::set<gov::Proposal> allProposalsSet(allProposals.begin(), allProposals.end());
            std::set<gov::Vote> allVotesSet(allVotes.begin(), allVotes.end());
            proposals.insert(proposalsA.begin(), proposalsA.end()); // Add proposals from batch A
            votes.insert(votesA.begin(), votesA.end()); // Add valid votes from batch A
            for (const auto & proposal : proposals)
                BOOST_CHECK(allProposalsSet.count(proposal));
            for (const auto & vote : votes)
                BOOST_CHECK(allVotesSet.count(vote));

            // Test getSuperblockResults
            std::map<uint256, std::vector<gov::Vote>> expected;
            for (const auto & vote : votes)
                expected[vote.getProposal()].push_back(vote);
            std::map<gov::Proposal, gov::Tally> tallies = gov::Governance::instance().getSuperblockResults(gov::NextSuperblock(consensus), consensus);
            for (const auto & tallyItem : tallies) {
                const auto & vs = expected[tallyItem.first.getHash()];
                CAmount voteAmount{0};
                for (const auto & vote : vs)
                    voteAmount += vote.getAmount();
                BOOST_CHECK_MESSAGE(voteAmount/consensus.voteBalance == tallyItem.second.yes,
                        strprintf("Tallied votes %d should match expected votes %d", expected[tallyItem.first.getHash()].size(), tallyItem.second.yes));
            }
        }

        const int proposalsNoVotesB{3};

        // Test changing votes
        {
            std::vector<gov::Proposal> proposals(proposalsB.begin(), proposalsB.end());
            std::vector<gov::ProposalVote> castVotes;
            for (int i = 0; i < proposalsNoVotesB; ++i)
                castVotes.emplace_back(proposals[i], gov::NO);
            std::vector<CTransactionRef> txns;
            failReason.clear();
            auto success = gov::SubmitVotes(castVotes, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            rescanWallet(otherwallet.get());
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(gov::NextSuperblock(consensus), allProposalsB, allVotesB);
            // Vote counts should not be modified for a changed vote
            BOOST_CHECK_MESSAGE(proposalsB.size() == allProposalsB.size(), strprintf("Expected to have %d proposals, instead have %d", proposalsB.size(), allProposalsB.size()));
            BOOST_CHECK_MESSAGE(votesB.size() == allVotesB.size(), strprintf("Expected to have %d votes, instead have %d", votesB.size(), allVotesB.size()));
            for (const auto & cv : castVotes) {
                const auto & tally = gov::Governance::getTally(cv.proposal.getHash(), allVotesB, consensus);
                BOOST_CHECK_MESSAGE(tally.no == maxVotes, strprintf("Expected %d no votes on the changed votes test, instead found %d", maxVotes, tally.no));
            }
        }

        // Vote invalidation tests
        {
            // Invalidate the first vote utxo in the vote tx to test that:
            // a) votes are not invalidated on the old superblock
            // b) votes are invalidated on the current superblock proposals

            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(COutPoint{votetx.GetHash(), 0});
            mtx.vout.resize(1);
            mtx.vout[0] = CTxOut(votetx.vout[0].nValue - ::minRelayTxFee.GetFee(1000), GetScriptForDestination(voteDest));
            SignatureData sigdata = DataFromTransaction(mtx, 0, votetx.vout[0]);
            ProduceSignature(*otherwallet, MutableTransactionSignatureCreator(&mtx, 0, votetx.vout[0].nValue, SIGHASH_ALL), GetScriptForDestination(voteDest), sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            CReserveKey reservekey(otherwallet.get());
            CValidationState state;
            BOOST_CHECK(otherwallet->CommitTransaction(MakeTransactionRef(mtx), {}, {}, reservekey, g_connman.get(), state));
            BOOST_CHECK_MESSAGE(state.IsValid(), strprintf("Failed to submit spent vote tx for vote invalidation checks: %s", state.GetRejectReason()));
            rescanWallet(otherwallet.get());
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            // Test a) invalidation of votes on already completed superblocks should be ignored
            // note that votes are invalidated when their associated utxos are spent.
            std::vector<gov::Proposal> allProposalsA;
            std::vector<gov::Vote> allVotesA;
            gov::Governance::instance().getProposalsForSuperblock(gov::PreviousSuperblock(consensus), allProposalsA, allVotesA);
            BOOST_CHECK_MESSAGE(proposalsA.size() == allProposalsA.size(), strprintf("Expected to have %d proposals, instead have %d", proposalsA.size(), allProposalsA.size()));
            BOOST_CHECK_MESSAGE(votesA.size() == allVotesA.size(), strprintf("Expected to have %d votes, instead have %d", votesA.size(), allVotesA.size()));

            // Test b), note that invalidating a utxo will invalidate 1 vote across every proposal
            // that it was associated with. In this case, 1 vote per proposal in the B set should
            // be invalidated.
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(gov::NextSuperblock(consensus), allProposalsB, allVotesB);
            const auto spentVotes = static_cast<int>(allProposalsB.size());
            BOOST_CHECK_MESSAGE(proposalsB.size() == allProposalsB.size(), strprintf("Expected to have %d proposals, instead have %d", proposalsB.size(), allProposalsB.size()));
            BOOST_CHECK_MESSAGE(votesB.size()-spentVotes == allVotesB.size(), strprintf("Expected to have %d votes, instead have %d", votesB.size()-spentVotes, allVotesB.size()));

            // Update state for next tests
            votesA = std::set<gov::Vote>(allVotesA.begin(), allVotesA.end());
            votesB = std::set<gov::Vote>(allVotesB.begin(), allVotesB.end());
        }

        // Create this proposal before proposal cutoff test
        const gov::Proposal voteCutoffProposal{"Test Vote Cutoff", gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        proposalsB.insert(voteCutoffProposal);
        {
            CTransactionRef tx;
            auto success = gov::SubmitProposal(voteCutoffProposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        }

        // Test proposal cutoff
        {
            // Stake to a few blocks after the proposal cutoff
            const auto nextSb = gov::NextSuperblock(consensus);
            const auto blks = nextSb - chainActive.Height() - consensus.proposalCutoff;
            pos.StakeBlocks(blks), SyncWithValidationInterfaceQueue();
            const gov::Proposal proposal{"Test Proposal Cutoff", gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
            CTransactionRef tx;
            auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(nextSb, allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(proposalsB.size() == allProposalsB.size(), "Proposal should not be accepted if it's submitted after the cutoff");
        }

        // Test vote cutoff
        {
            // Stake to a few blocks after the proposal cutoff
            const auto nextSb = gov::NextSuperblock(consensus);
            BOOST_CHECK_MESSAGE(!gov::Governance::insideVoteCutoff(nextSb, chainActive.Height(), params->GetConsensus()), strprintf("Chain tip should not be inside the vote cutoff (chain height %u, next superblock %u)", chainActive.Height(), nextSb));
            const auto blks = nextSb - chainActive.Height() - consensus.votingCutoff;
            pos.StakeBlocks(blks), SyncWithValidationInterfaceQueue();
            gov::ProposalVote proposalVote{voteCutoffProposal, gov::YES};
            std::vector<CTransactionRef> txns;
            failReason.clear();
            auto success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            rescanWallet(otherwallet.get());
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(nextSb, allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(gov::Governance::insideVoteCutoff(nextSb, chainActive.Height(), params->GetConsensus()), strprintf("Chain tip should be inside the vote cutoff (chain height %u, next superblock %u)", chainActive.Height(), nextSb));
            BOOST_CHECK_MESSAGE(votesB.size() == allVotesB.size(), "Votes should not be accepted if they're submitted after the cutoff");
            BOOST_CHECK_MESSAGE(gov::Governance::instance().utxoInVoteCutoff(allVotesB.front().getUtxo(), chainActive.Height(), params->GetConsensus()), "Utxo should be inside the vote cutoff");
        }

        // Check that the superblock payout is valid
        {
            const auto & height = chainActive.Height();
            const auto nextSb = gov::NextSuperblock(consensus);
            const auto blks = nextSb - chainActive.Height();
            pos.StakeBlocks(blks), SyncWithValidationInterfaceQueue();
            CBlock block;
            ReadBlockFromDisk(block, chainActive.Tip(), consensus);
            CAmount superblockPayment{0};
            const auto & results = gov::Governance::instance().getSuperblockResults(chainActive.Height(), consensus);
            const auto & payees = gov::Governance::getSuperblockPayees(chainActive.Height(), results, consensus);
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(chainActive.Height(), allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(!results.empty(), "Superblock results should not be empty");
            const int expectedPayees = (int)allProposalsB.size() - proposalsNoVotesB - 1; // -1 because of 0 votes on one of the proposals in the B list ("Test Vote Cutoff" proposal)
            BOOST_CHECK_MESSAGE(payees.size() == expectedPayees, "Superblock payees should match expected proposals");
            BOOST_CHECK_MESSAGE(gov::Governance::instance().isValidSuperblock(&block, chainActive.Height(), consensus, superblockPayment), "Expected superblock payout to be valid");
            BOOST_CHECK_MESSAGE(gov::Governance::isSuperblock(chainActive.Height(), consensus), "Expected superblock to be accepted");
        }

        // Check vote cutoff period ends properly after superblock
        {
            pos.StakeBlocks(5), SyncWithValidationInterfaceQueue();
            const int prevSB = gov::PreviousSuperblock(params->GetConsensus(), chainActive.Height());
            BOOST_CHECK_MESSAGE(!gov::Governance::insideVoteCutoff(prevSB, chainActive.Height(), params->GetConsensus()), strprintf("Chain tip should not be inside the vote cutoff after the superblock ends (chain height %u, previous superblock %u)", chainActive.Height(), prevSB));
            std::vector<gov::Proposal> allProposals;
            std::vector<gov::Vote> allVotes;
            gov::Governance::instance().getProposalsForSuperblock(prevSB, allProposals, allVotes);
            BOOST_CHECK_MESSAGE(!gov::Governance::instance().utxoInVoteCutoff(allVotes.front().getUtxo(), chainActive.Height(), params->GetConsensus()), "Utxo should not be inside the vote cutoff");
        }
    }

    otherwallet.reset();
    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_CASE(governance_tests_superblockstakes)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 500*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Create voting wallet
    CKey voteDestKey; voteDestKey.MakeNewKey(true);
    CTxDestination voteDest(voteDestKey.GetPubKey().GetID());
    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    AddKey(*otherwallet, voteDestKey);
    otherwallet->SetBroadcastTransactions(true);
    rescanWallet(otherwallet.get());
    const CAmount totalVoteAmount = consensus.voteBalance*2;
    const auto voteUtxoCount = static_cast<int>(totalVoteAmount/consensus.voteMinUtxoAmount);
    const auto maxVotes = totalVoteAmount/consensus.voteBalance;
    const CAmount voteInputUtxoAmount{consensus.voteMinUtxoAmount - 10 * COIN};
    const int voteInputUtxoCount{2};
    const auto totalTxAmount = totalVoteAmount + voteInputUtxoAmount*voteInputUtxoCount;
    CMutableTransaction votetx;

    // Prepare the utxos to use for votes
    {
        // Get to the nearest future SB if necessary
        if (gov::PreviousSuperblock(consensus, chainActive.Height()) == 0) {
            const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
            pos.StakeBlocks(blocks), SyncWithValidationInterfaceQueue();
        }
        // Create tx with enough inputs to cover votes
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins);
        }
        std::sort(coins.begin(), coins.end(), [](const COutput & a, const COutput & b) {
            return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
        });
        // Create vins
        CAmount runningAmount{0};
        int coinpos{0};
        std::vector<CTxOut> txouts;
        std::vector<COutPoint> outs;
        while (runningAmount < totalVoteAmount + voteInputUtxoAmount*voteInputUtxoCount) {
            outs.push_back(coins[coinpos].GetInputCoin().outpoint);
            txouts.push_back(coins[coinpos].GetInputCoin().txout);
            runningAmount += coins[coinpos].GetInputCoin().txout.nValue;
            ++coinpos;
        }
        votetx.vin.resize(outs.size());
        for (int i = 0; i < (int)outs.size(); ++i)
            votetx.vin[i] = CTxIn(outs[i]);
        // Create vouts
        for (int i = 0; i < voteUtxoCount; ++i)
            votetx.vout.emplace_back(consensus.voteMinUtxoAmount, GetScriptForDestination(voteDest));
        for (int i = 0; i < voteInputUtxoCount; ++i)
            votetx.vout.emplace_back(voteInputUtxoAmount, GetScriptForDestination(voteDest)); // use this for vote inputs
        const auto changeScript = coins[0].GetInputCoin().txout.scriptPubKey;
        const CAmount fees = 3000 * (votetx.vin.size()*180 + votetx.vout.size()*230); // estimate bytes per vin/vout
        votetx.vout.emplace_back(runningAmount - totalVoteAmount - voteInputUtxoAmount*voteInputUtxoCount - fees, changeScript); // change back to staker
        // Sign the tx inputs
        for (int i = 0; i < (int)votetx.vin.size(); ++i) {
            auto & vin = votetx.vin[i];
            SignatureData sigdata = DataFromTransaction(votetx, i, txouts[i]);
            ProduceSignature(*pos.wallet, MutableTransactionSignatureCreator(&votetx, i, txouts[i].nValue, SIGHASH_ALL), changeScript, sigdata);
            UpdateInput(vin, sigdata);
        }
        // Send transaction
        CReserveKey reservekey(pos.wallet.get());
        CValidationState state;
        BOOST_CHECK(pos.wallet->CommitTransaction(MakeTransactionRef(votetx), {}, {}, reservekey, g_connman.get(), state));
        BOOST_CHECK_MESSAGE(state.IsValid(), strprintf("Failed to submit tx for otherwallet vote utxos: %s", state.GetRejectReason()));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
        BOOST_REQUIRE_MESSAGE(otherwallet->GetBalance() == totalVoteAmount+voteInputUtxoAmount*voteInputUtxoCount, strprintf("other wallet expects a balance of %d, only has %d", totalVoteAmount+voteInputUtxoAmount*voteInputUtxoCount, otherwallet->GetBalance()));
    }

    // Check superblock proposal and votes
    {
        std::string failReason;
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        std::set<gov::Proposal> proposals;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Test single superblock
        {
            for (int i = 0; i < 5; ++i) {
                const gov::Proposal proposal{strprintf("Test Proposal A%d", i), gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
                proposals.insert(proposal);
                CTransactionRef tx;
                auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                // Submit votes
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns;
                failReason.clear();
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                rescanWallet(otherwallet.get());
            }

            // Stake to one block before the next superblock
            const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
            pos.StakeBlocks(blocks-1), SyncWithValidationInterfaceQueue();

            // Test bad superblock stake
            const auto & stake = pos.FindStake();
            const int superblock = chainActive.Height() + 1;

            // Valid superblock payees list should succeed
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                const auto & payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                bool success = ProcessNewBlock(*params, block, true, &fNewBlock);
                BOOST_REQUIRE_MESSAGE(success, "Valid superblock payee list should be accepted");
                if (success) {
                    CValidationState state;
                    InvalidateBlock(state, *params, chainActive.Tip(), false);
                    ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
                }
            }

            // Staker paying himself the superblock remainder should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                const auto & payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                CAmount leftOverAmount = [&consensus](const std::vector<CTxOut> & outs) -> CAmount {
                    CAmount total = consensus.proposalMaxAmount;
                    for (const auto & o : outs)
                        total -= o.nValue;
                    return total;
                }(payees);
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus, leftOverAmount - COIN), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                ProcessNewBlock(*params, block, true, &fNewBlock);
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Staker paying self remainder of superblock should be rejected");
            }

            // Bad superblock payees list should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                for (auto & payee : payees)
                    payee.scriptPubKey = pos.m_coinbase_txns[0]->vout[0].scriptPubKey;
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "Bad superblock payee list, scriptpubkey should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }

            // Bad superblock payee amount should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                BOOST_REQUIRE_MESSAGE(!payees.empty(), "Payees should be valid");
                payees[0].nValue = payees[0].nValue + 1;
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "Bad superblock payee nValue should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }

            // Extra superblock payee should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                BOOST_REQUIRE_MESSAGE(!payees.empty(), "Payees should be valid");
                payees.emplace_back(100 * COIN, payees[0].scriptPubKey);
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "Bad superblock payee nValue should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }

            // Duplicate superblock payee should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                BOOST_REQUIRE_MESSAGE(!payees.empty(), "Payees should be valid");
                payees.emplace_back(payees[0].nValue, payees[0].scriptPubKey);
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "Duplicate superblock payee should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }

            // Missing superblock payee should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                payees.erase(payees.begin());
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "Missing superblock payee should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }

            // All superblock payees missing should fail
            {
                auto blocktemplate = BlockAssembler(*params).CreateNewBlockPoS(*stake.coin, stake.hashBlock, stake.time, stake.blockTime, stake.wallet.get(), true);
                BOOST_CHECK_MESSAGE(blocktemplate != nullptr, "CreateNewBlockPoS failed, superblock stake test");
                const auto & results = gov::Governance::instance().getSuperblockResults(superblock, consensus);
                auto payees = gov::Governance::getSuperblockPayees(superblock, results, consensus);
                payees.clear();
                BOOST_CHECK_MESSAGE(applySuperblockPayees(pos, blocktemplate.get(), stake, payees, consensus), "Failed to create a valid PoS block for the superblock payee test");
                auto tip = chainActive.Tip()->nHeight;
                auto block = std::make_shared<const CBlock>(blocktemplate->block);
                bool fNewBlock{false};
                BOOST_CHECK_MESSAGE(!ProcessNewBlock(*params, block, true, &fNewBlock), "All superblock payees missing should fail");
                BOOST_CHECK_MESSAGE(tip == chainActive.Tip()->nHeight, "Chain tip should not advance");
            }
        }

    }

    otherwallet.reset();
    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_CASE(governance_tests_voteonstake)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.coinMaturity = 25;
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,50");

    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, pos.wallet->cs_wallet);
        pos.wallet->AvailableCoins(*pos.locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");

    // Vote on stake should work
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        auto nchain = interfaces::MakeChain();
        auto nwallet = std::make_shared<CWallet>(*nchain, WalletLocation(), WalletDatabase::CreateMock());
        bool fr; nwallet->LoadWallet(fr);
        {
            LOCK(nwallet->cs_wallet);
            nwallet->AddKeyPubKey(key, key.GetPubKey());
        }
        const CAmount voteUtxoAmt{10000*COIN};
        CTransactionRef sendtx;
        auto recipients = std::vector<CRecipient>{
            {GetScriptForDestination(newDest), voteUtxoAmt, false},
            {GetScriptForDestination(newDest), 2 * COIN, false}
        };
        std::vector<std::pair<CTxOut,COutPoint>> recvouts;
        bool sent = sendToRecipients(pos.wallet.get(), recipients, sendtx, &recvouts);
        BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
        pos.StakeBlocks(blocks+1), SyncWithValidationInterfaceQueue();

        // Create proposal
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        BOOST_CHECK_MESSAGE(proposal.isValid(consensus), "Basic proposal should be valid");
        CTransactionRef ptx; // proposal tx
        std::string failReason;
        gov::SubmitProposal(proposal, {pos.wallet}, consensus, ptx, g_connman.get(), &failReason);
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposal.getHash()), "Proposal should be accepted");

        // Voting wallet
        BOOST_REQUIRE_MESSAGE(recvouts.size() == recipients.size(), "vout out of bounds error");
        COutPoint voteUtxo = recvouts[0].second;
        COutPoint voteInput = recvouts[1].second;
        CTxOut txout = recvouts[1].first;
        CBasicKeyStore keystore;
        keystore.AddKey(key);

        // Create vote tx
        gov::VinHash voteVinHash = gov::makeVinHash(voteInput);
        gov::Vote vote(proposal.getHash(), gov::YES, voteUtxo, voteVinHash);
        BOOST_CHECK_MESSAGE(vote.sign(key), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote should be valid");
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(voteInput);
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        ss << vote;
        auto voteScript = CScript() << OP_RETURN << ToByteVector(ss);
        mtx.vout.resize(2);
        mtx.vout[0] = CTxOut(0, voteScript); // cast vote here
        mtx.vout[1] = CTxOut(txout.nValue - 0.1 * COIN, txout.scriptPubKey); // change
        SignatureData sigdata = DataFromTransaction(mtx, 0, txout);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, txout.nValue, SIGHASH_ALL),
                         txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);

        // Send vote transaction
        uint256 txid;
        std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 100 * COIN);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote.getHash()), "Vote should be accepted");

        // Stake the vote utxo to test revotes
        {
            WalletRescanReserver reserver(nwallet.get());
            reserver.reserve();
            nwallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
        }
        auto currentHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(stakeWallet(nwallet, pos.staker, voteUtxo, 1000), "Failed to stake vote utxo");
        SyncWithValidationInterfaceQueue();
        // Check that stake occurred
        BOOST_CHECK_MESSAGE(chainActive.Height() == currentHeight+1, "Stake should succeed on revote");
        auto votes = gov::Governance::instance().getVotes(proposal.getHash());
        BOOST_REQUIRE_MESSAGE(votes.size() == 1, strprintf("Should only be 1 valid vote on this proposal, found %u", votes.size()));
        BOOST_CHECK_MESSAGE(votes.front().getHash() != vote.getHash(), "Recast vote should have different hash");
        BOOST_CHECK_MESSAGE(votes.front().getVote() == vote.getVote(), "Recast vote type should match old vote type");
        CBlock block;
        ReadBlockFromDisk(block, chainActive.Tip(), consensus);
        BOOST_CHECK_MESSAGE(votes.front().getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote utxo should match latest block's coinstake outpoint");

        // Invalidate last block and ensure vote tx is abandoned
        {
            {
                LOCK(nwallet->cs_wallet);
                nwallet->AddKey(key);
            }
            AddWallet(nwallet);
            RegisterValidationInterface(nwallet.get());
            {
                WalletRescanReserver reserver(nwallet.get());
                reserver.reserve();
                nwallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
            }

            CTransactionRef votecstx;
            CTransactionRef votetx;
            uint256 hashvotecstx;
            uint256 hashvotetx;
            GetTransaction(block.vtx[1]->GetHash(), votecstx, consensus, hashvotecstx);
            GetTransaction(block.vtx[2]->GetHash(), votetx, consensus, hashvotetx);
            CBlockIndex *votecsidx = nullptr;
            CBlockIndex *voteidx = nullptr;
            {
                LOCK(cs_main);
                votecsidx = LookupBlockIndex(hashvotecstx);
                voteidx = LookupBlockIndex(hashvotetx);
            }

            CValidationState state;
            InvalidateBlock(state, *params, chainActive.Tip(), false);
            ActivateBestChain(state, *params); SyncWithValidationInterfaceQueue();
            BOOST_CHECK_MESSAGE(!chainActive.Contains(votecsidx), "Revote coinstake should be abandoned");
            BOOST_CHECK_MESSAGE(!chainActive.Contains(voteidx), "Revote tx should be abandoned");
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            UnregisterValidationInterface(nwallet.get());
            RemoveWallet(nwallet);
        }

        // Clean up
        nwallet.reset();
        cleanup(resetBlocks, pos.wallet.get());
        pos.ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

/// Check vote on stake across multiple proposals
BOOST_AUTO_TEST_CASE(governance_tests_voteonstakeproposals)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.coinMaturity = 25;
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 500 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,500");

    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, pos.wallet->cs_wallet);
        pos.wallet->AvailableCoins(*pos.locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");

    // Vote on stake should work
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & newDest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
        auto nchain = interfaces::MakeChain();
        auto nwallet = std::make_shared<CWallet>(*nchain, WalletLocation(), WalletDatabase::CreateMock());
        bool fr; nwallet->LoadWallet(fr);
        {
            LOCK(nwallet->cs_wallet);
            nwallet->AddKeyPubKey(key, key.GetPubKey());
        }
        const CAmount voteUtxoAmt{5000*COIN};
        CTransactionRef sendtx;
        auto recipients = std::vector<CRecipient>{
                {GetScriptForDestination(newDest), voteUtxoAmt, false},
                {GetScriptForDestination(newDest), voteUtxoAmt, false},
                {GetScriptForDestination(newDest), 5 * COIN, false}
        };
        std::vector<std::pair<CTxOut,COutPoint>> recvouts;
        bool sent = sendToRecipients(pos.wallet.get(), recipients, sendtx, &recvouts);
        BOOST_CHECK_MESSAGE(sent, "Send to another address failed");
        const auto blocks = gov::NextSuperblock(consensus) - chainActive.Height();
        pos.StakeBlocks(blocks+1), SyncWithValidationInterfaceQueue();

        // Create proposals
        gov::Proposal proposal1("Test proposal 1", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        BOOST_CHECK_MESSAGE(proposal1.isValid(consensus), "Proposal 1 should be valid");
        gov::Proposal proposal2("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        BOOST_CHECK_MESSAGE(proposal2.isValid(consensus), "Proposal 2 should be valid");
        CTransactionRef ptx; // proposal tx
        std::string failReason;
        gov::SubmitProposal(proposal1, {pos.wallet}, consensus, ptx, g_connman.get(), &failReason);
        gov::SubmitProposal(proposal2, {pos.wallet}, consensus, ptx, g_connman.get(), &failReason);
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposal1.getHash()), "Proposal1 should be accepted");
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposal2.getHash()), "Proposal2 should be accepted");

        // Voting wallet
        COutPoint voteUtxo1 = recvouts[0].second;
        COutPoint voteUtxo2 = recvouts[1].second;
        COutPoint voteInput = recvouts[2].second;
        CTxOut txout = recvouts[2].first;
        CBasicKeyStore keystore;
        keystore.AddKey(key);

        // Create vote tx
        gov::VinHash voteVinHash = gov::makeVinHash(voteInput);
        gov::Vote vote1a(proposal1.getHash(), gov::YES, voteUtxo1, voteVinHash);
        gov::Vote vote1b(proposal1.getHash(), gov::YES, voteUtxo2, voteVinHash);
        gov::Vote vote2a(proposal2.getHash(), gov::YES, voteUtxo1, voteVinHash);
        gov::Vote vote2b(proposal2.getHash(), gov::YES, voteUtxo2, voteVinHash);
        BOOST_CHECK_MESSAGE(vote1a.sign(key), "Vote1a signing should succeed");
        BOOST_CHECK_MESSAGE(vote1a.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote1a should be valid");
        BOOST_CHECK_MESSAGE(vote1b.sign(key), "Vote1b signing should succeed");
        BOOST_CHECK_MESSAGE(vote1b.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote1b should be valid");
        BOOST_CHECK_MESSAGE(vote2a.sign(key), "Vote2a signing should succeed");
        BOOST_CHECK_MESSAGE(vote2a.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote2a should be valid");
        BOOST_CHECK_MESSAGE(vote2b.sign(key), "Vote2b signing should succeed");
        BOOST_CHECK_MESSAGE(vote2b.isValid(std::set<gov::VinHash>{voteVinHash}, consensus), "Vote2b should be valid");
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(voteInput);
        CDataStream ss1a(SER_NETWORK, GOV_PROTOCOL_VERSION); ss1a << vote1a;
        CDataStream ss1b(SER_NETWORK, GOV_PROTOCOL_VERSION); ss1b << vote1b;
        CDataStream ss2a(SER_NETWORK, GOV_PROTOCOL_VERSION); ss2a << vote2a;
        CDataStream ss2b(SER_NETWORK, GOV_PROTOCOL_VERSION); ss2b << vote2b;
        auto voteScript1a = CScript() << OP_RETURN << ToByteVector(ss1a);
        auto voteScript1b = CScript() << OP_RETURN << ToByteVector(ss1b);
        auto voteScript2a = CScript() << OP_RETURN << ToByteVector(ss2a);
        auto voteScript2b = CScript() << OP_RETURN << ToByteVector(ss2b);
        mtx.vout.resize(5); // 4 votes total + change
        mtx.vout[0] = CTxOut(0, voteScript1a); // cast 1a vote here
        mtx.vout[1] = CTxOut(0, voteScript1b); // cast 1b vote here
        mtx.vout[2] = CTxOut(0, voteScript2a); // cast 2a vote here
        mtx.vout[3] = CTxOut(0, voteScript2b); // cast 2b vote here
        mtx.vout[4] = CTxOut(txout.nValue - 0.1 * COIN, txout.scriptPubKey); // change
        SignatureData sigdata = DataFromTransaction(mtx, 0, txout);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, txout.nValue, SIGHASH_ALL),
                         txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);

        // Send vote transaction
        uint256 txid;
        std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 100 * COIN);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send vote transaction: %s", errstr));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote1a.getHash()), "Vote1a should be accepted");
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote1b.getHash()), "Vote1b should be accepted");
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote2a.getHash()), "Vote2a should be accepted");
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote2b.getHash()), "Vote2b should be accepted");

        // Make sure we have sufficient coin maturity
        pos.StakeBlocks(consensus.coinMaturity), SyncWithValidationInterfaceQueue();

        // Stake the vote utxo to test revotes
        {
            WalletRescanReserver reserver(nwallet.get());
            reserver.reserve();
            nwallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
        }
        auto currentHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(stakeWallet(nwallet, pos.staker, voteUtxo1, 1000), "Failed to stake vote utxo 1");
        SyncWithValidationInterfaceQueue();
        // Check that stake occurred
        BOOST_CHECK_MESSAGE(chainActive.Height() == currentHeight+1, "Stake should succeed on revote");

        CBlock block;
        ReadBlockFromDisk(block, chainActive.Tip(), consensus);

        // Check that "a" votes are changed and that "b" votes are unchanged
        auto votes1 = gov::Governance::instance().getVotes(proposal1.getHash());
        auto votes2 = gov::Governance::instance().getVotes(proposal2.getHash());
        BOOST_REQUIRE_MESSAGE(votes1.size() == 2, "Should be 2 valid votes on proposal 1");
        BOOST_REQUIRE_MESSAGE(votes2.size() == 2, "Should be 2 valid votes on proposal 2");
        if (votes1[0].getUtxo() == vote1b.getUtxo()) {
            BOOST_CHECK_MESSAGE(votes1[0].getUtxo() == vote1b.getUtxo(), "vote1b should be unchanged");
            BOOST_CHECK_MESSAGE(votes1[1].getUtxo() != vote1a.getUtxo(), "vote1a should be changed");
            BOOST_CHECK_MESSAGE(votes1[0].getHash() == vote1b.getHash(), "vote1b hash should be unchanged");
            BOOST_CHECK_MESSAGE(votes1[1].getHash() != vote1a.getHash(), "vote1a hash should be different");
            BOOST_CHECK_MESSAGE(votes1[0].getVote() == vote1b.getVote(), "vote1b type should match old vote type");
            BOOST_CHECK_MESSAGE(votes1[1].getVote() == vote1a.getVote(), "vote1a type should match old vote type");
            BOOST_CHECK_MESSAGE(votes1[1].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote1a utxo should match latest block's coinstake outpoint");
            BOOST_CHECK_MESSAGE(votes1[0].getUtxo() != COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote1b utxo should not match latest block's coinstake outpoint");
        } else {
            BOOST_CHECK_MESSAGE(votes1[1].getUtxo() == vote1b.getUtxo(), "vote1b should be unchanged");
            BOOST_CHECK_MESSAGE(votes1[0].getUtxo() != vote1a.getUtxo(), "vote1a should be changed");
            BOOST_CHECK_MESSAGE(votes1[1].getHash() == vote1b.getHash(), "vote1b hash should be unchanged");
            BOOST_CHECK_MESSAGE(votes1[0].getHash() != vote1a.getHash(), "vote1a hash should be different");
            BOOST_CHECK_MESSAGE(votes1[1].getVote() == vote1b.getVote(), "vote1b type should match old vote type");
            BOOST_CHECK_MESSAGE(votes1[0].getVote() == vote1a.getVote(), "vote1a type should match old vote type");
            BOOST_CHECK_MESSAGE(votes1[0].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote1a utxo should match latest block's coinstake outpoint");
            BOOST_CHECK_MESSAGE(votes1[1].getUtxo() != COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote1b utxo should not match latest block's coinstake outpoint");
        }
        if (votes2[0].getUtxo() == vote2b.getUtxo()) {
            BOOST_CHECK_MESSAGE(votes2[0].getUtxo() == vote2b.getUtxo(), "vote2b should be unchanged");
            BOOST_CHECK_MESSAGE(votes2[1].getUtxo() != vote2a.getUtxo(), "vote2a should be changed");
            BOOST_CHECK_MESSAGE(votes2[0].getHash() == vote2b.getHash(), "vote2b hash should be unchanged");
            BOOST_CHECK_MESSAGE(votes2[1].getHash() != vote2a.getHash(), "vote2a hash should be different");
            BOOST_CHECK_MESSAGE(votes2[0].getVote() == vote2b.getVote(), "vote2b type should match old vote type");
            BOOST_CHECK_MESSAGE(votes2[1].getVote() == vote2a.getVote(), "vote2a type should match old vote type");
            BOOST_CHECK_MESSAGE(votes2[1].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote2a utxo should match latest block's coinstake outpoint");
            BOOST_CHECK_MESSAGE(votes2[0].getUtxo() != COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote2b utxo should not match latest block's coinstake outpoint");
        } else {
            BOOST_CHECK_MESSAGE(votes2[1].getUtxo() == vote2b.getUtxo(), "vote2b should be unchanged");
            BOOST_CHECK_MESSAGE(votes2[0].getUtxo() != vote2a.getUtxo(), "vote2a should be changed");
            BOOST_CHECK_MESSAGE(votes2[1].getHash() == vote2b.getHash(), "vote2b hash should be unchanged");
            BOOST_CHECK_MESSAGE(votes2[0].getHash() != vote2a.getHash(), "vote2a hash should be different");
            BOOST_CHECK_MESSAGE(votes2[1].getVote() == vote2b.getVote(), "vote2b type should match old vote type");
            BOOST_CHECK_MESSAGE(votes2[0].getVote() == vote2a.getVote(), "vote2a type should match old vote type");
            BOOST_CHECK_MESSAGE(votes2[0].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote2a utxo should match latest block's coinstake outpoint");
            BOOST_CHECK_MESSAGE(votes2[1].getUtxo() != COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote2b utxo should not match latest block's coinstake outpoint");
        }

        // Stake the 2nd vote utxo to test revotes
        {
            WalletRescanReserver reserver(nwallet.get());
            reserver.reserve();
            nwallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
        }
        currentHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(stakeWallet(nwallet, pos.staker, voteUtxo2, 1000), "Failed to stake vote utxo 2");
        SyncWithValidationInterfaceQueue();
        // Check that stake occurred
        BOOST_CHECK_MESSAGE(chainActive.Height() == currentHeight+1, "Stake should succeed on revote");

        ReadBlockFromDisk(block, chainActive.Tip(), consensus);

        // Check that "b" votes are changed
        votes1 = gov::Governance::instance().getVotes(proposal1.getHash());
        votes2 = gov::Governance::instance().getVotes(proposal2.getHash());
        BOOST_REQUIRE_MESSAGE(votes1.size() == 2, strprintf("Should be 2 valid votes on proposal 1, found %u", votes1.size()));
        BOOST_REQUIRE_MESSAGE(votes2.size() == 2, strprintf("Should be 2 valid votes on proposal 2, found %u", votes2.size()));

        BOOST_CHECK_MESSAGE(votes1[0].getUtxo() != vote1b.getUtxo() && votes1[1].getUtxo() != vote1b.getUtxo(), "vote1b should be changed");
        BOOST_CHECK_MESSAGE(votes1[0].getHash() != vote1b.getHash() && votes1[1].getHash() != vote1b.getHash(), "vote1b hash should be changed");
        BOOST_CHECK_MESSAGE(votes1[0].getVote() == vote1b.getVote() && votes1[1].getVote() == vote1b.getVote(), "vote1b type should match old vote type");
        BOOST_CHECK_MESSAGE(votes1[0].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1) || votes1[1].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote1b utxo should match latest block's coinstake outpoint");

        BOOST_CHECK_MESSAGE(votes2[0].getUtxo() != vote2b.getUtxo() && votes2[1].getUtxo() != vote2b.getUtxo(), "vote2b should be changed");
        BOOST_CHECK_MESSAGE(votes2[0].getHash() != vote2b.getHash() && votes2[1].getHash() != vote2b.getHash(), "vote2b hash should be changed");
        BOOST_CHECK_MESSAGE(votes2[0].getVote() == vote2b.getVote() && votes2[1].getVote() == vote2b.getVote(), "vote2b type should match old vote type");
        BOOST_CHECK_MESSAGE(votes2[0].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1) || votes2[1].getUtxo() == COutPoint(block.vtx[1]->GetHash(), 1), "Recast vote2b utxo should match latest block's coinstake outpoint");

        // Clean up
        nwallet.reset();
        cleanup(resetBlocks, pos.wallet.get());
        pos.ReloadWallet();
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_CASE(governance_tests_loadgovernancedata_proposals)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("100,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Check preloading governance proposal data
    const auto resetBlocks = chainActive.Height();
    std::string failReason;

    // Prep vote utxo
    CTransactionRef sendtx;
    bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
    BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
    BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
    pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

    const int proposalCount = 25;
    std::vector<gov::Proposal> proposals; proposals.resize(proposalCount);
    // Create some proposals
    for (int i = 0; i < proposalCount; ++i) {
        gov::Proposal proposal(strprintf("Test proposal %d", i), nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        proposals[i] = proposal;
        if (i == 15) {
            // Add adequate blocks to for superblock tests (test across multiple superblocks)
            pos.StakeBlocks(params->consensus.superblock), SyncWithValidationInterfaceQueue();
        }
    }
    pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

    failReason.clear();
    BOOST_CHECK_MESSAGE(gov::Governance::instance().loadGovernanceData(chainActive, cs_main, consensus, failReason), "Failed to load governance data from the chain");
    BOOST_CHECK_MESSAGE(failReason.empty(), "loadGovernanceData fail reason should be empty");
    auto govprops = gov::Governance::instance().getProposals();
    BOOST_CHECK_MESSAGE(govprops.size() == proposalCount, strprintf("Failed to load governance data proposals, found %d expected %d", govprops.size(), proposalCount));

    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_CASE(governance_tests_loadgovernancedata_votes)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 500*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 200 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("200,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Create voting wallet
    CKey voteDestKey; voteDestKey.MakeNewKey(true);
    CTxDestination voteDest(voteDestKey.GetPubKey().GetID());
    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    AddKey(*otherwallet, voteDestKey);
    otherwallet->SetBroadcastTransactions(true);
    RegisterValidationInterface(otherwallet.get());

    const auto resetBlocks = chainActive.Height();
    std::string failReason;

    // Check preloading governance vote data
    {
        std::vector<gov::Proposal> sproposals;
        const int svotes{10};

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), voteDest, 10 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        for (int i = 0; i < svotes; ++i) {
            CTransactionRef tx;
            accepted = sendToAddress(pos.wallet.get(), voteDest, 150 * COIN, tx);
            BOOST_CHECK_MESSAGE(accepted, "Failed to send coin to vote address");
            BOOST_REQUIRE_MESSAGE(accepted, "Test failure");
        }
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(otherwallet->GetBalance() == 1510*COIN, strprintf("Expected balance to be 1502, found %d", (double)otherwallet->GetBalance()/(double)COIN));

        // Stop on superblock
        if (chainActive.Height() < consensus.superblock) // first superblock
            pos.StakeBlocks(consensus.superblock - chainActive.Height()), SyncWithValidationInterfaceQueue();

        // Store vote transactions
        std::vector<CTransactionRef> txns;
        // Watch for on-chain gov data
        RegisterValidationInterface(&gov::Governance::instance());

        {
            gov::Proposal proposal("Test proposal 1", nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            sproposals.push_back(proposal);
            CTransactionRef tx = nullptr;
            auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            // Submit initial votes
            {
                gov::ProposalVote proposalVote{proposal, gov::NO};
                std::vector<CTransactionRef> txns1;
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns1, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            }

            // Change votes
            {
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns1;
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns1, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
                txns.insert(txns.end(), txns1.begin(), txns1.end());
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            }
        }

        // Stake to next superblock (test across multiple superblocks)
        pos.StakeBlocks(params->consensus.superblock), SyncWithValidationInterfaceQueue();

        // Submit additional votes on new superblock
        {
            gov::Proposal proposal("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            sproposals.push_back(proposal);
            CTransactionRef tx = nullptr;
            auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            // Submit initial votes
            {
                gov::ProposalVote proposalVote{proposal, gov::NO};
                std::vector<CTransactionRef> txns1;
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns1, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            }

            // Change votes
            {
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns1;
                success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns1, g_connman.get(), &failReason);
                BOOST_REQUIRE_MESSAGE(success, strprintf("Submit votes failed: %s", failReason));
                txns.insert(txns.end(), txns1.begin(), txns1.end()); // track votes
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            }
        }

        auto countVotes = [consensus](const std::vector<gov::Vote> & votes, const std::vector<CTransactionRef> & txns, int & expecting, int & spent) {
            for (const auto & tx : txns) { // only count valid votes
                if (tx->IsCoinBase() || tx->IsCoinStake())
                    continue;
                for (int n = 0; n < (int)tx->vout.size(); ++n) {
                    const auto & out = tx->vout[n];
                    CScript::const_iterator pc = out.scriptPubKey.begin();
                    std::vector<unsigned char> data;
                    opcodetype opcode{OP_FALSE};
                    bool checkdata{false};
                    while (pc < out.scriptPubKey.end()) {
                        opcode = OP_FALSE;
                        if (!out.scriptPubKey.GetOp(pc, opcode, data))
                            break;
                        checkdata = (opcode == OP_PUSHDATA1 || opcode == OP_PUSHDATA2 || opcode == OP_PUSHDATA4)
                                    || (opcode < OP_PUSHDATA1 && opcode == data.size());
                        if (checkdata && !data.empty())
                            break;
                    }
                    if (!checkdata || data.empty())
                        continue; // skip if no data
                    CDataStream ss(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                    gov::NetworkObject obj; ss >> obj;
                    if (!obj.isValid())
                        continue; // must match expected version
                    if (obj.getType() == gov::VOTE) {
                        CDataStream ssv(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                        gov::Vote vote({tx->GetHash(), static_cast<uint32_t>(n)});
                        ssv >> vote;
                        if (vote.loadVoteUTXO() && vote.isValid(consensus) && !vote.spent() && !gov::IsVoteSpent(vote, chainActive.Height(), consensus.governanceBlock, false))
                            ++expecting;
                        else
                            ++spent;
                    }
                }
            }
        };

        // Expected proposal data
        std::vector<gov::Proposal> cps;
        std::vector<gov::Vote> cvs;
        auto ps = gov::Governance::instance().getProposals();
        for (const auto & proposal : ps) {
            cps.push_back(proposal);
            const auto & v = gov::Governance::instance().getVotes(proposal.getHash());
            cvs.insert(cvs.end(), v.begin(), v.end());
        }
        BOOST_CHECK_MESSAGE(cps.size() == sproposals.size(), strprintf("Expected %u proposals, found %u", sproposals.size(), cps.size()));
        BOOST_CHECK_MESSAGE(cvs.size() == svotes*sproposals.size(), strprintf("Expected %u votes, found %u", svotes*sproposals.size(), cvs.size()));

        // Stop watching for on-chain gov data in preparation for testing the load funcs below
        UnregisterValidationInterface(&gov::Governance::instance());

        // Load governance data with single thread
        {
            gov::Governance::instance().reset();
            failReason.clear();
            auto govsuccess = gov::Governance::instance().loadGovernanceData(chainActive, cs_main, consensus, failReason, 1);
            BOOST_CHECK_MESSAGE(govsuccess, strprintf("Failed to load governance data from the chain: %s", failReason));
            BOOST_CHECK_MESSAGE(failReason.empty(), "loadGovernanceData fail reason should be empty");
            auto gvotes = gov::Governance::instance().getVotes();
            int expecting{0};
            int spent{0};
            countVotes(gvotes, txns, expecting, spent);
            BOOST_CHECK_MESSAGE(gvotes.size() == expecting, strprintf("Failed to load governance data votes, found %u "
                                                                      "expected %u, spent or invalid %u", gvotes.size(), expecting, spent));
            BOOST_CHECK_MESSAGE(gvotes.size() == cvs.size(), strprintf("Failed to load governance data votes, found %u "
                                                                      "expected %u, spent or invalid %u", gvotes.size(), cvs.size(), spent));
        }

        // Load governance data with default multiple threads
        if (GetNumCores() >= 4) {
            gov::Governance::instance().reset();
            failReason.clear();
            auto govsuccess = gov::Governance::instance().loadGovernanceData(chainActive, cs_main, consensus, failReason, 0);
            BOOST_CHECK_MESSAGE(govsuccess, strprintf("Failed to load governance data from the chain via multiple threads: %s", failReason));
            BOOST_CHECK_MESSAGE(failReason.empty(), "loadGovernanceData fail reason should be empty");
            auto gvotes = gov::Governance::instance().getVotes();
            int expecting{0};
            int spent{0};
            countVotes(gvotes, txns, expecting, spent);
            BOOST_CHECK_MESSAGE(gvotes.size() == expecting, strprintf("Failed to load governance data votes via multiple threads, found %u "
                                                                      "expected %u, spent or invalid %u", gvotes.size(), expecting, spent));
            BOOST_CHECK_MESSAGE(gvotes.size() == cvs.size(), strprintf("Failed to load governance data votes, found %u "
                                                                       "expected %u, spent or invalid %u", gvotes.size(), cvs.size(), spent));
        }
    }

    // clean up
    RemoveWallet(otherwallet);
    UnregisterValidationInterface(otherwallet.get());
    otherwallet.reset();
    cleanup(chainActive.Height(), pos.wallet.get());
    pos_ptr.reset();
}

BOOST_AUTO_TEST_CASE(governance_tests_rpc)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    RegisterValidationInterface(&gov::Governance::instance());
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init("100,40001,50");
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Prep vote utxo
    CTransactionRef sendtx;
    bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
    BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
    BOOST_REQUIRE_MESSAGE(accepted, "Proposal fee account should confirm to the network before continuing");
    pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

    // Check createproposal rpc
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Submit proposal via rpc
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({
            "Test proposal 1",                                           // name
            nextSuperblock(chainActive.Height(), consensus.superblock),  // superblock
            250,                                                         // amount
            saddr,                                                       // address
            "https://forum.blocknet.co",                                 // url
            "Short description"                                          // description
        });
        UniValue result;
        BOOST_CHECK_NO_THROW(result = CallRPC2("createproposal", rpcparams));
        BOOST_CHECK_MESSAGE(result.isObject(), "createproposal rpc should return an object");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Validate rpc result data
        const auto proposalHash = uint256S(find_value(result.get_obj(), "hash").get_str());
        BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposalHash), "Failed to find proposal in governance manager");
        const auto txid = uint256S(find_value(result.get_obj(), "txid").get_str());
        CTransactionRef tx;
        uint256 hashBlock;
        BOOST_CHECK_MESSAGE(GetTransaction(txid, tx, consensus, hashBlock), "Failed to find proposal tx in mempool");
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "name")       .get_str(), rpcparams[0].get_str());
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "superblock") .get_int(), rpcparams[1].get_int());
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "amount")     .get_int(), rpcparams[2].get_int());
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "address")    .get_str(), rpcparams[3].get_str());
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "url")        .get_str(), rpcparams[4].get_str());
        BOOST_CHECK_EQUAL(find_value(result.get_obj(), "description").get_str(), rpcparams[5].get_str());

        // clean up
        cleanup(resetBlocks, pos.wallet.get());
        pos.ReloadWallet();
    }

    // Check createproposal rpc
    {
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const auto nextSB = nextSuperblock(chainActive.Height(), consensus.superblock);

        // Fail on bad name
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal $", nextSB, 250, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on bad superblock
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB + 1, 250, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on bad amount (too small)
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, consensus.proposalMinAmount - 1, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on bad amount (too large)
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, consensus.proposalMaxAmount + 1, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on bad address
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, 250, "bvkbvkbvkbvkbkvkbkvkbkbvkvkkbkvk", "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Succeed on empty url
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, 250, saddr, "", "Short description" });
            BOOST_CHECK_NO_THROW(CallRPC2("createproposal", rpcparams));
        }
        // Succeed on omitted description
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, 250, saddr, "https://forum.blocknet.co" });
            BOOST_CHECK_NO_THROW(CallRPC2("createproposal", rpcparams));
        }
        // Succeed on omitted url + description
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, 250, saddr });
            BOOST_CHECK_NO_THROW(CallRPC2("createproposal", rpcparams));
        }
        // Succeed on default superblock
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", 0, 250, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_NO_THROW(CallRPC2("createproposal", rpcparams));
        }
        // Fail on string amount
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, "250", saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on string superblock
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", strprintf("%d", nextSB), 250, saddr, "https://forum.blocknet.co", "Short description" });
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
        // Fail on long description
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({"Test proposal 1", nextSB, 250, saddr, "https://forum.blocknet.co", "Long description Long description Long description Long description Long description Long "
                                                                                                      "Long description Long description Long description Long description Long description Long "
                                                                                                      "Long description Long description Long description Long description Long description Long "
                                                                                                      "Long description Long description Long description Long description Long description Long "
                                                                                                      "Long description Long description Long description Long description Long description Long "});
            BOOST_CHECK_THROW(CallRPC2("createproposal", rpcparams), std::runtime_error);
        }
    }

    // Check vote rpc
    {
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const auto nextSB = nextSuperblock(chainActive.Height(), consensus.superblock);

        const gov::Proposal proposal{"Test proposal 2", nextSB, 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        CTransactionRef tx;
        std::string failReason;
        auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Succeed on proper yes vote
        {
            UniValue rpcparams(UniValue::VARR);
            const auto voteCast = gov::Vote::voteTypeToString(gov::YES);
            rpcparams.push_backV({ proposal.getHash().ToString(), voteCast });
            UniValue result;
            BOOST_CHECK_NO_THROW(result = CallRPC2("vote", rpcparams));

            // Validate rpc result data
            const auto proposalHash = uint256S(find_value(result.get_obj(), "hash").get_str());
            BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposalHash), "Failed to find proposal in governance manager");
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "name")       .get_str(), proposal.getName());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "superblock") .get_int(), proposal.getSuperblock());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "amount")     .get_int()*COIN, proposal.getAmount());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "address")    .get_str(), proposal.getAddress());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "url")        .get_str(), proposal.getUrl());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "description").get_str(), proposal.getDescription());
            BOOST_CHECK_EQUAL(find_value(result.get_obj(), "vote")       .get_str(), voteCast);
            BOOST_CHECK(find_value(result.get_obj(), "txids").isArray());
            auto txids = find_value(result.get_obj(), "txids").get_array().getValues();
            for (const auto & txid : txids) {
                BOOST_CHECK_MESSAGE(txid.isStr(), "vote rpc should return an array of strings for txids");
                CTransactionRef ttx;
                uint256 hashBlock;
                BOOST_CHECK_MESSAGE(GetTransaction(uint256S(txid.get_str()), ttx, consensus, hashBlock), "vote rpc failed to find vote tx in mempool");
            }
        }
        // Succeed on proper no vote
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), gov::Vote::voteTypeToString(gov::NO) });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }
        // Succeed on proper abstain vote
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), gov::Vote::voteTypeToString(gov::ABSTAIN) });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }

        // Succeed on proper yes vote
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), "YES" });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }
        // Succeed on proper no vote
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), "NO" });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }
        // Succeed on proper abstain vote
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), "ABSTAIN" });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }

        // Fail on bad proposal hash
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ "8784372873284728347182471283742813748127", gov::Vote::voteTypeToString(gov::YES) });
            BOOST_CHECK_THROW(CallRPC2("vote", rpcparams), std::runtime_error);
        }

        // Fail on non-hex hash
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ "zvczmnvczxnvzxcmnvxcznvmxznvmzx", gov::Vote::voteTypeToString(gov::YES) });
            BOOST_CHECK_THROW(CallRPC2("vote", rpcparams), std::runtime_error);
        }
        // Fail on bad vote type
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), "bad vote type" });
            BOOST_CHECK_THROW(CallRPC2("vote", rpcparams), std::runtime_error);
        }

        // Succeed on proper yes vote with address
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), gov::Vote::voteTypeToString(gov::YES), EncodeDestination(dest) });
            BOOST_CHECK_NO_THROW(CallRPC2("vote", rpcparams));
        }
        // Fail on invalid vote with address (bad address)
        {
            UniValue rpcparams(UniValue::VARR);
            rpcparams.push_backV({ proposal.getHash().ToString(), gov::Vote::voteTypeToString(gov::YES), "kjkdsfjaskdfjsdk" });
            BOOST_CHECK_THROW(CallRPC2("vote", rpcparams), std::runtime_error);
        }

        cleanup(resetBlocks, pos.wallet.get());
        pos.ReloadWallet();
    }

    // Check listproposals rpc
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Submit proposal
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const int nextSB = gov::NextSuperblock(consensus);
        const gov::Proposal proposal{"Test proposal 3", nextSB, 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        CTransactionRef tx;
        auto success = gov::SubmitProposal(proposal, {pos.wallet}, consensus, tx, g_connman.get(), &failReason);
        BOOST_REQUIRE_MESSAGE(success, strprintf("Proposal submission failed: %s", failReason));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Passing proposal
        {
            // Submit votes
            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txns;
            failReason.clear();
            success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {pos.wallet}, consensus, txns, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            UniValue rpcparams(UniValue::VARR);
            UniValue result;
            BOOST_CHECK_NO_THROW(result = CallRPC2("listproposals", rpcparams));
            BOOST_CHECK_MESSAGE(result.isArray(), "listproposals rpc call should return array");
            for (const auto & uprop : result.get_array().getValues()) {
                const UniValue & p = uprop.get_obj();
                const auto proposalHash = uint256S(find_value(p.get_obj(), "hash").get_str());
                BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposalHash), "Failed to find proposal in governance manager");
                if (proposalHash == proposal.getHash()) { // only check proposal for this unit test
                    BOOST_CHECK_EQUAL(find_value(p, "name")       .get_str(), proposal.getName());
                    BOOST_CHECK_EQUAL(find_value(p, "superblock") .get_int(), proposal.getSuperblock());
                    BOOST_CHECK_EQUAL(find_value(p, "amount")     .get_int(), proposal.getAmount() / COIN);
                    BOOST_CHECK_EQUAL(find_value(p, "address")    .get_str(), proposal.getAddress());
                    BOOST_CHECK_EQUAL(find_value(p, "url")        .get_str(), proposal.getUrl());
                    BOOST_CHECK_EQUAL(find_value(p, "description").get_str(), proposal.getDescription());
                    const auto tally = gov::Governance::getTally(proposal.getHash(), gov::Governance::instance().getVotes(), consensus);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_yes")  .get_int(), tally.yes);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_no")   .get_int(), tally.no);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_abstain").get_int(), tally.abstain);
                    BOOST_CHECK_EQUAL(find_value(p, "status").get_str(), "passing");
                }
            }
        }

        // Failing proposal
        {
            // Submit votes
            gov::ProposalVote proposalVote{proposal, gov::NO};
            std::vector<CTransactionRef> txns;
            failReason.clear();
            success = gov::SubmitVotes(std::vector<gov::ProposalVote>{proposalVote}, {pos.wallet}, consensus, txns, g_connman.get(), &failReason);
            BOOST_REQUIRE_MESSAGE(success, strprintf("Vote submission failed: %s", failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            UniValue rpcparams(UniValue::VARR);
            UniValue result;
            BOOST_CHECK_NO_THROW(result = CallRPC2("listproposals", rpcparams));
            BOOST_CHECK_MESSAGE(result.isArray(), "listproposals rpc call should return array");
            for (const auto & uprop : result.get_array().getValues()) {
                const UniValue & p = uprop.get_obj();
                const auto proposalHash = uint256S(find_value(p.get_obj(), "hash").get_str());
                BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposalHash), "Failed to find proposal in governance manager");
                if (proposalHash == proposal.getHash()) { // only check proposal for this unit test
                    BOOST_CHECK_EQUAL(find_value(p, "name")       .get_str(), proposal.getName());
                    BOOST_CHECK_EQUAL(find_value(p, "superblock") .get_int(), proposal.getSuperblock());
                    BOOST_CHECK_EQUAL(find_value(p, "amount")     .get_int(), proposal.getAmount() / COIN);
                    BOOST_CHECK_EQUAL(find_value(p, "address")    .get_str(), proposal.getAddress());
                    BOOST_CHECK_EQUAL(find_value(p, "url")        .get_str(), proposal.getUrl());
                    BOOST_CHECK_EQUAL(find_value(p, "description").get_str(), proposal.getDescription());
                    const auto tally = gov::Governance::getTally(proposal.getHash(), gov::Governance::instance().getVotes(), consensus);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_yes")  .get_int(), tally.yes);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_no")   .get_int(), tally.no);
                    BOOST_CHECK_EQUAL(find_value(p, "votes_abstain").get_int(), tally.abstain);
                    BOOST_CHECK_EQUAL(find_value(p, "status").get_str(), "failing");
                }
            }
        }

        cleanup(resetBlocks, pos.wallet.get());
        pos.ReloadWallet();
    }

    // Check proposalfee rpc
    {
        UniValue rpcparams(UniValue::VARR);
        UniValue result;
        BOOST_CHECK_NO_THROW(result = CallRPC2("proposalfee", rpcparams));
        BOOST_CHECK_MESSAGE(result.get_str() == FormatMoney(consensus.proposalFee), strprintf("proposalfee should match expected %d", FormatMoney(consensus.proposalFee)));
    }

    UnregisterValidationInterface(&gov::Governance::instance());
    cleanup(chainActive.Height(), pos.wallet.get());
    pos.ReloadWallet();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_SUITE_END()