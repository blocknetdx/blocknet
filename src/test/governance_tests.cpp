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

void rescanWallet(CWallet *w) {
    WalletRescanReserver reserver(w);
    reserver.reserve();
    w->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, true);
};

bool cleanup(int blockCount, CWallet *wallet=nullptr) {
    while (chainActive.Height() > blockCount) {
        CValidationState state;
        InvalidateBlock(state, Params(), chainActive.Tip());
    }
    mempool.clear();
    gArgs.ForceSetArg("-proposaladdress", "");
    gov::Governance::instance().reset();
    CValidationState state;
    ActivateBestChain(state, Params());
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(psubmit, {wallet}, consensus, tx, &failReason));
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(pp1, {wallet}, consensus, pp1_tx, &failReason));
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
    params->consensus.voteMinUtxoAmount = 20*COIN;
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {wallet}, consensus, tx, &failReason));
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {wallet}, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_CHECK_MESSAGE(txns.size() == 2, strprintf("Expected 2 vote transaction to be created, %d were created", txns.size()));
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

                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                gov::NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                BOOST_CHECK_MESSAGE(obj.getType() == gov::VOTE, "Invalid vote OP_RETURN type");
                CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                gov::Vote vote({txn->GetHash(), static_cast<uint32_t>(n)}, block.GetBlockTime());
                ss2 >> vote;
                bool valid = vote.isValid(consensus);
                if (vote.getUtxo() == block.vtx[1]->vin[0].prevout) // staked inputs associated with votes should be invalid
                    BOOST_CHECK_MESSAGE(gov::IsVoteSpent(vote, true), "Vote should be marked as spent when its utxo stakes");
                else
                    BOOST_CHECK_MESSAGE(valid, "Vote should be valid");
                BOOST_CHECK_MESSAGE(gov::Governance::instance().hasVote(vote.getHash()), "Governance manager should know about the vote");
                BOOST_CHECK_MESSAGE(vote.getProposal() == proposal.getHash(), "Vote data should match the expected proposal hash");
                BOOST_CHECK_MESSAGE(vote.getVote() == proposalVote.vote, "Vote data should match the expected vote type");
                if (gov::IsVoteSpent(vote, true))
                    continue;

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
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {wallet}, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), strprintf("Failed to submit votes: %s", failReason));
        BOOST_CHECK_MESSAGE(txns.size() == 3, strprintf("Expected 3 vote transactions to be created, %d were created", txns.size()));
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {wallet}, consensus, tx, &failReason));
        BOOST_CHECK_MESSAGE(tx != nullptr, "Proposal tx should be valid");
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

        // Submit the vote (should fail)
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(!gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(!failReason.empty(), "Fail reason should be defined when submit votes fails");
        BOOST_CHECK_MESSAGE(txns.empty(), "Expected transactions list to be empty");

        // clean up
        cleanup(resetBlocks);
    }

    // Check vote tally
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        params->consensus.voteMinUtxoAmount = 5*COIN;
        params->consensus.voteBalance = 250*COIN;

        // Create and submit proposal
        gov::Proposal proposal("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000 * COIN,
                               EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
        CTransactionRef tx = nullptr;
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {wallet}, consensus, tx, &failReason));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        BOOST_CHECK(gov::Governance::instance().hasProposal(proposal.getHash()));

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

        // Submit the votes for the other wallet
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txnsOther;
        failReason.clear();
        BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txnsOther, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), "Fail reason should be empty for tally test");
        BOOST_CHECK_MESSAGE(txnsOther.size() == 1, strprintf("Expected 1 transaction, instead have %d on tally test", txnsOther.size()));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
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
        BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, {wallet}, consensus, txns, &failReason));
        BOOST_CHECK_MESSAGE(failReason.empty(), "Fail reason should be empty for tally test");
        BOOST_CHECK_MESSAGE(txns.size() == 3, strprintf("Expected %d transactions, instead have %d on tally test", 3, txns.size()));
        StakeBlocks(1), SyncWithValidationInterfaceQueue();
        auto tally = gov::Governance::getTally(proposal.getHash(), gov::Governance::instance().getVotes(), consensus);
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), consensus));
        std::set<gov::Proposal> ps;
        std::set<gov::Vote> vs;
        gov::Governance::instance().dataFromBlock(&block, ps, vs, consensus, chainActive.Tip());
        CAmount voteAmount{0};
        for (const auto & vote : vs)
            voteAmount += vote.getAmount();
        BOOST_CHECK_MESSAGE(voteAmount/consensus.voteBalance == tally.yes-tallyOther.yes,
                strprintf("Expected %d votes instead tallied %d", voteAmount/consensus.voteBalance, tally.yes-tallyOther.yes));

        // clean up
        cleanup(resetBlocks);
        otherwallet.reset();
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_AUTO_TEST_CASE(governance_tests_superblockresults)
{
    TestChainPoS pos(false);
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
    pos.Init();
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
        // Staker script
        const auto stakerScriptPubKey = coins[0].GetInputCoin().txout.scriptPubKey;
        // Create vins
        CAmount runningAmount{0};
        int coinpos{0};
        std::vector<CTxOut> txouts;
        std::vector<COutPoint> outs;
        while (runningAmount < totalVoteAmount) {
            outs.push_back(coins[coinpos].GetInputCoin().outpoint);
            txouts.push_back(coins[coinpos].GetInputCoin().txout);
            runningAmount += coins[coinpos].GetInputCoin().txout.nValue;
            ++coinpos;
        }
        outs.emplace_back(coins[coinpos].GetInputCoin().outpoint); // Cover fee
        txouts.emplace_back(coins[coinpos].GetInputCoin().txout); // Cover fee
        votetx.vin.resize(outs.size());
        for (int i = 0; i < (int)outs.size(); ++i)
            votetx.vin[i] = CTxIn(outs[i]);
        const CAmount fees = votetx.vin.size()*2*::minRelayTxFee.GetFee(250); // use 150 bytes per vin/vout
        const CAmount voteInputUtxoAmount = 5 * COIN;
        const CAmount change = txouts[txouts.size()-1].nValue - fees - voteInputUtxoAmount; // fee input change
        // Create vouts
        for (int i = 0; i < voteUtxoCount; ++i)
            votetx.vout.emplace_back(consensus.voteMinUtxoAmount, GetScriptForDestination(voteDest));
        votetx.vout.emplace_back(change, stakerScriptPubKey); // change back to staker
        votetx.vout.emplace_back(voteInputUtxoAmount, GetScriptForDestination(voteDest)); // use this for vote inputs
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
        BOOST_CHECK_MESSAGE(state.IsValid(), "Failed to submit tx for otherwallet vote utxos");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());
        BOOST_CHECK_MESSAGE(otherwallet->GetBalance() == totalVoteAmount+voteInputUtxoAmount, strprintf("other wallet expects a balance of %d, only has %d", totalVoteAmount+voteInputUtxoAmount, otherwallet->GetBalance()));
        BOOST_TEST_REQUIRE(otherwallet->GetBalance() == totalVoteAmount+voteInputUtxoAmount);
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
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
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
                BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                // Submit votes
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns;
                failReason.clear();
                BOOST_CHECK_MESSAGE(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, &failReason), failReason);
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                rescanWallet(otherwallet.get());
                // Count the votes
                CBlock block;
                ReadBlockFromDisk(block, chainActive.Tip(), consensus);
                std::set<gov::Proposal> ps;
                std::set<gov::Vote> vs;
                gov::Governance::instance().dataFromBlock(&block, ps, vs, consensus, chainActive.Tip());
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
                BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                // Submit votes
                gov::ProposalVote proposalVote{proposal, gov::YES};
                std::vector<CTransactionRef> txns;
                failReason.clear();
                BOOST_CHECK_MESSAGE(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, &failReason), failReason);
                pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
                rescanWallet(otherwallet.get());
                // Count the votes
                CBlock block;
                ReadBlockFromDisk(block, chainActive.Tip(), consensus);
                std::set<gov::Proposal> ps;
                std::set<gov::Vote> vs;
                gov::Governance::instance().dataFromBlock(&block, ps, vs, consensus, chainActive.Tip());
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

        // Test changing votes
        {
            std::vector<gov::Proposal> proposals(proposalsB.begin(), proposalsB.end());
            gov::ProposalVote proposalVote1{proposals[0], gov::NO};
            gov::ProposalVote proposalVote2{proposals[1], gov::NO};
            gov::ProposalVote proposalVote3{proposals[2], gov::NO};
            std::vector<gov::ProposalVote> castVotes{proposalVote1,proposalVote2,proposalVote3};
            std::vector<CTransactionRef> txns;
            failReason.clear();
            BOOST_CHECK_MESSAGE(gov::Governance::instance().submitVotes(castVotes, {otherwallet}, consensus, txns, &failReason), failReason);
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
        {
            CTransactionRef tx;
            BOOST_CHECK(gov::Governance::instance().submitProposal(voteCutoffProposal, {pos.wallet}, consensus, tx, &failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            proposalsB.insert(voteCutoffProposal);
        }

        // Test proposal cutoff
        {
            // Stake to a few blocks after the proposal cutoff
            const auto nextSb = gov::NextSuperblock(consensus);
            const auto blks = nextSb - chainActive.Height() - consensus.proposalCutoff;
            pos.StakeBlocks(blks), SyncWithValidationInterfaceQueue();
            const gov::Proposal proposal{"Test Proposal Cutoff", gov::NextSuperblock(consensus), 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
            proposalsB.insert(proposal);
            CTransactionRef tx;
            BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
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
            const auto blks = nextSb - chainActive.Height() - consensus.votingCutoff;
            pos.StakeBlocks(blks), SyncWithValidationInterfaceQueue();
            gov::ProposalVote proposalVote{voteCutoffProposal, gov::YES};
            std::vector<CTransactionRef> txns;
            failReason.clear();
            BOOST_CHECK_MESSAGE(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, {otherwallet}, consensus, txns, &failReason), failReason);
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            rescanWallet(otherwallet.get());
            std::vector<gov::Proposal> allProposalsB;
            std::vector<gov::Vote> allVotesB;
            gov::Governance::instance().getProposalsForSuperblock(nextSb, allProposalsB, allVotesB);
            BOOST_CHECK_MESSAGE(votesB.size() == allVotesB.size(), "Votes should not be accepted if they're submitted after the cutoff");
        }

        cleanup(resetBlocks, pos.wallet.get());
    }

    cleanup(chainActive.Height());
    UnregisterValidationInterface(&gov::Governance::instance());
}

BOOST_AUTO_TEST_CASE(governance_tests_loadgovernancedata)
{
    TestChainPoS pos(false);
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
    pos.Init();
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    // Check preloading governance proposal data
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
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
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

    cleanup(chainActive.Height());
}

BOOST_AUTO_TEST_CASE(governance_tests_loadgovernancedata2)
{
    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 500*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
        return 50 * COIN;
    };
    const auto & consensus = params->GetConsensus();
    pos.Init();
    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());

    const auto resetBlocks = chainActive.Height();
    std::string failReason;

    // Check preloading governance vote data
    {
        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Stop on superblock
        if (chainActive.Height() < consensus.superblock) // first superblock
            pos.StakeBlocks(consensus.superblock - chainActive.Height()), SyncWithValidationInterfaceQueue();

        // Store vote transactions
        std::vector<CTransactionRef> txns;
        {
            gov::Proposal proposal("Test proposal 1", nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            CTransactionRef tx = nullptr;
            BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txns1;
            BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns1, &failReason));
            txns.insert(txns.end(), txns1.begin(), txns1.end());
        }
        // Add adequate blocks to for superblock tests (test across multiple superblocks)
        pos.StakeBlocks(params->consensus.superblock), SyncWithValidationInterfaceQueue();
        {
            gov::Proposal proposal("Test proposal 2", nextSuperblock(chainActive.Height(), consensus.superblock), 250 * COIN,
                                   EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
            CTransactionRef tx = nullptr;
            BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

            gov::ProposalVote proposalVote{proposal, gov::YES};
            std::vector<CTransactionRef> txns1;
            BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns1, &failReason));
            txns.insert(txns.end(), txns1.begin(), txns1.end());
        }
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        failReason.clear();
        BOOST_CHECK_MESSAGE(gov::Governance::instance().loadGovernanceData(chainActive, cs_main, consensus, failReason), "Failed to load governance data from the chain");
        BOOST_CHECK_MESSAGE(failReason.empty(), "loadGovernanceData fail reason should be empty");

        auto gvotes = gov::Governance::instance().getVotes();
        int expecting{0};
        for (const auto & tx : txns) { // only count valid votes
            if (tx->IsCoinBase() || tx->IsCoinStake())
                continue;
            for (int n = 0; n < (int)tx->vout.size(); ++n) {
                const auto & out = tx->vout[n];
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
                    gov::Vote vote({tx->GetHash(), static_cast<uint32_t>(n)});
                    ssv >> vote;
                    if (vote.isValid(consensus) && !vote.spent() && !gov::IsVoteSpent(vote))
                        ++expecting;
                }
            }
        }
        BOOST_CHECK_MESSAGE(gvotes.size() == expecting, strprintf("Failed to load governance data votes, found %d expected %d", gvotes.size(), expecting));
    }

    cleanup(resetBlocks);
}

BOOST_AUTO_TEST_CASE(governance_tests_rpc)
{
    TestChainPoS pos(false);
    RegisterValidationInterface(&gov::Governance::instance());
    auto *params = (CChainParams*)&Params();
    params->consensus.voteMinUtxoAmount = 20*COIN;
    params->consensus.voteBalance = 1000*COIN;
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 100 * COIN;
        else if (blockHeight % consensusParams.superblock == 0)
            return 40001 * COIN;
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
        const auto resetBlocks = chainActive.Height();
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const auto nextSB = nextSuperblock(chainActive.Height(), consensus.superblock);

        const gov::Proposal proposal{"Test proposal 1", nextSB, 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        CTransactionRef tx;
        std::string failReason;
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
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

        cleanup(resetBlocks, pos.wallet.get());
    }

    // Check listproposals rpc
    {
        const auto resetBlocks = chainActive.Height();
        std::string failReason;

        // Prep vote utxo
        CTransactionRef sendtx;
        bool accepted = sendToAddress(pos.wallet.get(), dest, 2 * COIN, sendtx);
        BOOST_CHECK_MESSAGE(accepted, "Failed to create vote network fee payment address");
        BOOST_TEST_REQUIRE(accepted, "Proposal fee account should confirm to the network before continuing");
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Submit proposal
        CKey key; key.MakeNewKey(true);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        const int nextSB = gov::NextSuperblock(consensus);
        const gov::Proposal proposal{"Test proposal 1", nextSB, 250*COIN, saddr, "https://forum.blocknet.co", "Short description"};
        CTransactionRef tx;
        BOOST_CHECK(gov::Governance::instance().submitProposal(proposal, {pos.wallet}, consensus, tx, &failReason));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Submit votes
        gov::ProposalVote proposalVote{proposal, gov::YES};
        std::vector<CTransactionRef> txns;
        failReason.clear();
        BOOST_CHECK(gov::Governance::instance().submitVotes(std::vector<gov::ProposalVote>{proposalVote}, GetWallets(), consensus, txns, &failReason));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        UniValue rpcparams(UniValue::VARR);
        UniValue result;
        BOOST_CHECK_NO_THROW(result = CallRPC2("listproposals", rpcparams));
        BOOST_CHECK_MESSAGE(result.isArray(), "listproposals rpc call should return array");
        for (const auto & uprop : result.get_array().getValues()) {
            UniValue p = uprop.get_obj();
            const auto proposalHash = uint256S(find_value(p.get_obj(), "hash").get_str());
            BOOST_CHECK_MESSAGE(gov::Governance::instance().hasProposal(proposalHash), "Failed to find proposal in governance manager");
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
        }

        cleanup(resetBlocks, pos.wallet.get());
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