// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <governance/governance.h>

BOOST_AUTO_TEST_SUITE(governance_tests)

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
    return wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state);
}

bool newWalletAddress(CWallet *wallet, CTxDestination & dest) {
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

bool cleanup(int blockCount, CWallet *wallet=nullptr) {
    while (chainActive.Height() < blockCount) {
        CValidationState state;
        InvalidateBlock(state, Params(), chainActive.Tip());
    }
    mempool.clear();
    gArgs.ForceSetArg("-proposaladdress", "");
    gov::Governance::instance().reset();
    if (wallet) {
        WalletRescanReserver reserver(wallet);
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, false);
    }
    return true;
}

BOOST_FIXTURE_TEST_CASE(governance_tests_proposals, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    const auto & params = Params();
    const auto & consensus = params.GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());

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
        BOOST_CHECK(gov::Governance::submitProposal(psubmit, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        BOOST_CHECK_MESSAGE(mempool.exists(tx->GetHash()), "Proposal submission tx should be in the mempool");
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
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
        cleanup(resetBlocks);
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
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create and submit proposal
        gov::Proposal pp1("Test -proposaladdress", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                              EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef pp1_tx = nullptr;
        std::string failReason;
        BOOST_CHECK(gov::Governance::submitProposal(pp1, consensus, pp1_tx, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit proposal: %s", failReason));

        // Check that proposal tx was accepted
        accepted = pp1_tx != nullptr && sendProposal(pp1, pp1_tx, this, params);
        if (!accepted) cleanup(resetBlocks);
        BOOST_TEST_REQUIRE(accepted, "Proposal tx should confirm to the network before continuing");

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

        cleanup(resetBlocks);
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_FIXTURE_TEST_CASE(governance_tests_votes, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 25*COIN;
    params->consensus.voteBalance = 1000*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    std::vector<COutput> coins;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        wallet->AvailableCoins(*locked_chain, coins);
    }
    BOOST_CHECK_MESSAGE(!coins.empty(), "Vote tests require available coins");

    // Check normal proposal
    gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                     EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    BOOST_CHECK_MESSAGE(proposal.isValid(consensus), "Basic proposal should be valid");

    // Check YES vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::YES, coins.begin()->GetInputCoin().outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote YES signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(consensus), "Vote YES should be valid upon signing");
    }

    // Check NO vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::NO, coins.begin()->GetInputCoin().outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote NO signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(consensus), "Vote NO should be valid upon signing");
    }

    // Check ABSTAIN vote is valid
    {
        gov::Vote vote(proposal.getHash(), gov::ABSTAIN, coins.begin()->GetInputCoin().outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote ABSTAIN signing should succeed");
        BOOST_CHECK_MESSAGE(vote.isValid(consensus), "Vote ABSTAIN should be valid upon signing");
    }

    // Bad vote type should fail
    {
        gov::Vote vote(proposal.getHash(), (gov::VoteType)99, coins.begin()->GetInputCoin().outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(consensus), "Vote with invalid type should fail");
    }

    // Signing with key not matching utxo should fail
    {
        CKey key; key.MakeNewKey(true);
        gov::Vote vote(proposal.getHash(), gov::YES, coins.begin()->GetInputCoin().outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(key), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(consensus), "Vote with bad signing key should fail");
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
        gov::Vote vote(proposal.getHash(), gov::YES, outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(consensus), "Vote with bad utxo should fail");
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
        gov::Vote vote(proposal.getHash(), gov::YES, outpoint);
        BOOST_CHECK_MESSAGE(vote.sign(coinbaseKey), "Vote signing should succeed");
        BOOST_CHECK_MESSAGE(!vote.isValid(consensus), "Vote with bad utxo should fail");
        // Clean up
        cleanup(resetBlocks);
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_FIXTURE_TEST_CASE(governance_tests_submissions, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 25*COIN;
    params->consensus.voteBalance = 1000*COIN;
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());

    // Check proposal submission is accepted by the network
    {
        const auto resetBlocks = chainActive.Height();
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        std::string failReason;
        BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit proposal: %s", failReason));
        auto accepted = tx != nullptr && sendProposal(proposal, tx, this, *params);
        BOOST_CHECK_MESSAGE(accepted, "Proposal submission failed");
        // clean up
        cleanup(resetBlocks);
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
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(gov::Governance::submitVotes(std::vector<gov::ProposalVote>{proposalVote}, consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_CHECK_MESSAGE(txns.size() == 1, "Expected vote transaction to be created");
        if (!txns.empty()) { // check that tx is standard
            BOOST_CHECK_MESSAGE(IsStandardTx(*txns[0], failReason), strprintf("Vote transaction is not standard: %s", failReason));
            failReason.clear();
        }
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
            for (const auto & out : txn->vout) {
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

                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                gov::NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                BOOST_CHECK_MESSAGE(obj.getType() == gov::VOTE, "Invalid vote OP_RETURN type");
                CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                gov::Vote vote; ss2 >> vote;
                bool valid = vote.isValid(consensus);
                if (vote.getUtxo() == block.vtx[1]->vin[0].prevout) // staked inputs associated with votes should be invalid
                    BOOST_CHECK_MESSAGE(!valid, "Vote should invalidate on stake");
                else
                    BOOST_CHECK_MESSAGE(valid, "Vote should be valid");
                if (!valid)
                    continue;
                BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote.getHash()), "Governance manager should know about the vote");
                BOOST_CHECK_MESSAGE(vote.getProposal() == proposal.getHash(), "Vote data should match the expected proposal hash");
                BOOST_CHECK_MESSAGE(vote.getVote() == proposalVote.vote, "Vote data should match the expected vote type");

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
        cleanup(resetBlocks);
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_FIXTURE_TEST_CASE(governance_tests_vote_limits, TestChainPoS)
{
    RegisterValidationInterface(&gov::Governance::instance());

    auto *params = (CChainParams*)&Params();
    const auto & consensus = params->GetConsensus();
    CTxDestination dest(coinbaseKey.GetPubKey().GetID());

    // Check that maxing out the votes per tx creates multiple transactions
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 5*COIN;
        params->consensus.voteBalance = 600*COIN;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(wallet.get(), dest, 1 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Prep utxos for use with votes
        BOOST_TEST_REQUIRE(createUtxos(params->consensus.voteBalance, params->consensus.voteMinUtxoAmount, this),
                "Failed to create the required utxo set for use with vote limit tests");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create and submit proposal
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(gov::Governance::submitVotes(std::vector<gov::ProposalVote>{proposalVote}, consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_CHECK_MESSAGE(txns.size() == 2, "Expected 2 vote transactions to be created to handle all votes");
        BOOST_TEST_REQUIRE(!txns.empty(), "Proposal tx should confirm to the network before continuing");
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
        cleanup(resetBlocks);
    }

    // Check situation where there's not enough vote balance
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 100*COIN;
        params->consensus.voteBalance = 50000*COIN;

        // Create and submit proposal
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote (should fail)
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(!gov::Governance::submitVotes(std::vector<gov::ProposalVote>{proposalVote}, consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(!failReason.empty(), "Fail reason should be defined when submit votes fails");
        BOOST_CHECK_MESSAGE(txns.empty(), "Expected transactions list to be empty");

        // clean up
        cleanup(resetBlocks);
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_AUTO_TEST_CASE(governance_tests_loadgovernancedata)
{
//    RegisterValidationInterface(&gov::Governance::instance()); // not used in this test

    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 25*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 10000 * COIN;
        return 25 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init();
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Check preloading governance proposal data
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        const int proposalCount = 25;
        std::vector<gov::Proposal> proposals; proposals.resize(proposalCount);
        // Create some proposals
        for (int i = 0; i < proposalCount; ++i) {
            gov::Proposal proposal(strprintf("Test proposal %d", i), nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            CTransactionRef tx = nullptr;
            BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
            BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
            proposals[i] = proposal;
            if (i == 15) {
                // Add adequate blocks to for superblock tests (test across multiple superblocks)
                pos.StakeBlocks(params->consensus.superblock), SyncWithValidationInterfaceQueue();
            }
        }
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        BOOST_CHECK_MESSAGE(gov::Governance::instance().loadGovernanceData(chainActive, consensus), "Failed to load governance data from the chain");
        auto govprops = gov::Governance::instance().getProposals();
        BOOST_CHECK_MESSAGE(govprops.size() == proposalCount, strprintf("Failed to load governance data proposals, found %d expected %d", govprops.size(), proposalCount));

        // clean up
        cleanup(resetBlocks, pos.wallet.get());
    }

    // Check preloading governance vote data
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Store vote transactions
        std::vector<CTransactionRef> txns;
        {
            gov::Proposal proposal(strprintf("Test proposal 1"), nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txns1;
            BOOST_CHECK(gov::Governance::submitVotes(std::vector<gov::ProposalVote>{proposalVote}, consensus, txns1, &failReason));
            txns.insert(txns.end(), txns1.begin(), txns1.end());
        }
        // Add adequate blocks to for superblock tests (test across multiple superblocks)
        pos.StakeBlocks(params->consensus.superblock), SyncWithValidationInterfaceQueue();
        {
            gov::Proposal proposal(strprintf("Test proposal 2"), nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txns1;
            BOOST_CHECK(gov::Governance::submitVotes(std::vector<gov::ProposalVote>{proposalVote}, consensus, txns1, &failReason));
            txns.insert(txns.end(), txns1.begin(), txns1.end());
        }
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        BOOST_CHECK_MESSAGE(gov::Governance::instance().loadGovernanceData(chainActive, consensus), "Failed to load governance data from the chain");
        auto gvotes = gov::Governance::instance().getVotes();
        int expecting{0};
        for (const auto & tx : txns) { // only count valid votes
            if (tx->IsCoinBase() || tx->IsCoinStake())
                continue;
            for (const auto & out : tx->vout) {
                CScript::const_iterator pc = out.scriptPubKey.begin();
                std::vector<unsigned char> data;
                while (pc < out.scriptPubKey.end()) {
                    opcodetype opcode;
                    if (!out.scriptPubKey.GetOp(pc, opcode, data))
                        break;
                    if (!data.empty())
                        break;
                }
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                gov::NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version
                if (obj.getType() == gov::VOTE) {
                    CDataStream ssv(data, SER_NETWORK, PROTOCOL_VERSION);
                    gov::Vote vote; ssv >> vote;
                    if (vote.isValid(consensus))
                        ++expecting;
                }
            }
        }
        BOOST_CHECK_MESSAGE(gvotes.size() == expecting, strprintf("Failed to load governance data votes, found %d expected %d", gvotes.size(), expecting));

        // clean up
        cleanup(resetBlocks, pos.wallet.get());
    }

    cleanup(chainActive.Height());
//    UnregisterValidationInterface(&gov::Governance::instance()); // not used in this test
}

BOOST_AUTO_TEST_CASE(governance_tests_rpc)
{
    TestChainPoS pos(false);
    RegisterValidationInterface(&gov::Governance::instance());
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 25*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 10000 * COIN;
        return 25 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init();
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Check createproposal rpc
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

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
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const auto nextSB = nextSuperblock(chainActive.Height(), consensus.superblock);

        const gov::Proposal proposal{"Test proposal 1", nextSB, 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        CTransactionRef tx;
        std::string failReason;
        BOOST_CHECK(gov::Governance::submitProposal(proposal, consensus, tx, &failReason));
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
    }

    // Check proposalfee rpc
    {
        UniValue rpcparams(UniValue::VARR);
        UniValue result;
        BOOST_CHECK_NO_THROW(result = CallRPC2("proposalfee", rpcparams));
        BOOST_CHECK_MESSAGE(result.get_int() == consensus.proposalFee/COIN, strprintf("proposalfee should match expected %d", consensus.proposalFee/COIN));
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_AUTO_TEST_SUITE_END()