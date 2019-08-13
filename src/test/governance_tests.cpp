// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <governance/governance.h>

BOOST_FIXTURE_TEST_SUITE(governance_tests, TestChainPoS)

int nextSuperblock(const int & block, const int & superblock) {
    return block + (superblock - block % superblock);
}

BOOST_AUTO_TEST_CASE(governance_tests_proposals)
{
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
                     EncodeDestination(dest), "https://forum.blocknet.co", "This description is the maximum allowed for this particular prop");
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

    // Submit proposal
    gov::Proposal psubmit("Test proposal", nextSuperblock(chainActive.Height(), consensus.superblock), 3000*COIN,
                          EncodeDestination(dest), "https://forum.blocknet.co", "Short description");
    CTransactionRef tx = nullptr;
    BOOST_CHECK_EQUAL(true, gov::Governance::submitProposal(psubmit, consensus, tx));
    BOOST_CHECK_EQUAL(false, tx == nullptr); // tx should be valid
    BOOST_CHECK_MESSAGE(mempool.exists(tx->GetHash()), "Proposal submission tx should be in the mempool");
    bool found{false};
    for (const auto & out : tx->vout) {
        if (out.scriptPubKey[0] == OP_RETURN) {
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << psubmit;
            BOOST_CHECK_MESSAGE((CScript() << OP_RETURN << ToByteVector(ss)) == out.scriptPubKey, "Proposal submission OP_RETURN script in tx should match expected");
            found = true;
        }
    }
    BOOST_CHECK_MESSAGE(found, "Proposal submission tx must contain an OP_RETURN");

}

BOOST_AUTO_TEST_SUITE_END()