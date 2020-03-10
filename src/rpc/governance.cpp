// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <governance/governance.h>
#include <net.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <governance/governancewallet.h>
#endif // ENABLE_WALLET

static UniValue createproposal(const JSONRPCRequest& request)
{
#ifndef ENABLE_WALLET
    return "Wallet required";
#endif // ENABLE_WALLET
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
        throw std::runtime_error(
            RPCHelpMan{"createproposal",
                "\nCreates a new proposal and submits it to the network. There is a fee associated with proposal submissions. Obtain with \"proposalfee\" rpc call.\n",
                {
                    {"name", RPCArg::Type::STR, RPCArg::Optional::NO, R"(Proposal name, only alpha number characters are accepted (example: "My Proposal 1" or "My_Proposal-1"))"},
                    {"superblock", RPCArg::Type::NUM, RPCArg::Optional::NO, strprintf("Block number of Superblock. Specify 0 to automatically submit for the next Superblock %d", gov::NextSuperblock(Params().GetConsensus()))},
                    {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "Amount of BLOCK being requested in the proposal"},
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "Blocknet payment address"},
                    {"url", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Url where voters can read more details"},
                    {"description", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Brief description. Note, if description is too long the proposal submission will fail"},
                },
                RPCResult{
                "{\n"
                "  \"hash\":\"xxxx\",                      (string) Hex string of the proposal hash\n"
                "  \"txid\":\"xxxx\",                      (string) Hex string of the proposal transaction hash\n"
                "  \"name\": \"proposal name\",            (string) Service node name\n"
                "  \"superblock\": n,                      (numeric) Upcoming Superblock to receive payment. Obtain the next Superblock with \"nextsuperblock\" rpc call.\n"
                "  \"amount\": n,                          (numeric) Amount of BLOCK being requested in the proposal\n"
                "  \"address\":\"blocknet address\",       (string) Blocknet payment address\n"
                "  \"url\":\"https://forum.blocknet.co\",  (string) Url where voters can read more details\n"
                "  \"description\":\"xxxx\"                (string) Brief description. Note, if description is too long the proposal submission will fail\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("createproposal", R"("Dev Proposal" 43200 750 "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa" "https://forum.blocknet.co" "Dev proposal for building xyz")")
                  + HelpExampleRpc("createproposal", R"("Dev Proposal", 43200, 750, "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa", "https://forum.blocknet.co", "Dev proposal for building xyz")")
                },
            }.ToString());

    const std::string & name = request.params[0].get_str();
    const bool shouldPickNextSuperblock = request.params[1].get_int() == 0;
    const int superblock = shouldPickNextSuperblock ? gov::Governance::nextSuperblock(Params().GetConsensus()) : request.params[1].get_int();
    const CAmount amount = request.params[2].get_int() * COIN;
    const std::string & address = request.params[3].get_str();
    const std::string & url = !request.params[4].isNull() ? request.params[4].get_str() : "";
    const std::string & description = !request.params[5].isNull() ? request.params[5].get_str() : "";

    std::string failReason;

    gov::Proposal proposal(name, superblock, amount, address, url, description);
    if (!proposal.isValid(Params().GetConsensus(), &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit proposal: %s", failReason));

    int currentBlockHeight{0};
    {
        LOCK(cs_main);
        currentBlockHeight = chainActive.Height();
    }
    if (currentBlockHeight == 0)
        throw JSONRPCError(RPC_MISC_ERROR, "Failed to submit proposal because current block height is invalid");

    if (!gov::Governance::outsideProposalCutoff(proposal, currentBlockHeight, Params().GetConsensus())) {
        const int nextsb = gov::Governance::nextSuperblock(Params().GetConsensus(), superblock + 1);
        if (shouldPickNextSuperblock)
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit proposal for Superblock %u because the proposal cutoff time has passed. "
                                                         "Please submit the proposal for Superblock %u", superblock, nextsb));
        else
            throw JSONRPCError(RPC_MISC_ERROR, "Failed to submit proposal because the proposal cutoff time has passed");
    }

    CTransactionRef tx;
#ifdef ENABLE_WALLET
    if (!gov::SubmitProposal(proposal, GetWallets(), Params().GetConsensus(), tx, g_connman.get(), &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit proposal: %s", failReason));
#endif // ENABLE_WALLET

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", proposal.getHash().ToString());
    ret.pushKV("txid", tx->GetHash().ToString());
    ret.pushKV("name", proposal.getName());
    ret.pushKV("superblock", proposal.getSuperblock());
    ret.pushKV("amount", proposal.getAmount()/COIN);
    ret.pushKV("address", proposal.getAddress());
    ret.pushKV("url", proposal.getUrl());
    ret.pushKV("description", proposal.getDescription());
    return ret;
}

static UniValue listproposals(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"listproposals",
                "\nLists proposals since the specified block. By default lists the current and upcoming proposals.\n",
                {
                    {"sinceblock", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "default=0 which pull most recent proposals. Otherwise specify the block number."},
                },
                RPCResult{
                "{\n"
                "  \"hash\":\"xxxx\",                (string) Hex string of the proposal hash\n"
                "  \"name\": \"proposal name\",      (string) Service node name\n"
                "  \"superblock\": n,                (numeric) Upcoming Superblock to receive payment. Obtain the next Superblock with \"nextsuperblock\" rpc call.\n"
                "  \"amount\": n,                    (numeric) Amount of BLOCK being requested in the proposal\n"
                "  \"address\":\"blocknet address\", (string) Blocknet payment address\n"
                "  \"url\":\"xxxx\",                 (string) Url where voters can read more details\n"
                "  \"description\":\"xxxx\",         (string) Brief description. Note, if description is too long the proposal submission will fail\n"
                "  \"votes_yes\": n,                 (numeric) All yes votes\n"
                "  \"votes_no\": n,                  (numeric) All no votes\n"
                "  \"votes_abstain\": n              (numeric) All abstain votes\n"
                "  \"status\": \"xxxx\"              (string) Statuses include: passing, failing, passed, failed, pending (future proposal)\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("listproposals", "")
                  + HelpExampleCli("listproposals", "1036800")
                  + HelpExampleRpc("listproposals", "")
                  + HelpExampleRpc("listproposals", "1036800")
                },
            }.ToString());

    const auto & consensus = Params().GetConsensus();
    const auto & prevSuperblock = gov::PreviousSuperblock(consensus);
    const auto & sinceBlock = request.params[0].isNull() || request.params[0].get_int() <= 0
                                                         ? prevSuperblock
                                                         : request.params[0].get_int();
    {
        LOCK(cs_main);
        if (sinceBlock > chainActive.Height())
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("'sinceblock' is bad, cannot be greater than %d", chainActive.Height()));
    }

    std::vector<gov::Proposal> proposals;
    std::vector<gov::Vote> votes;
    auto ps = gov::Governance::instance().getProposalsSince(sinceBlock);
    for (const auto & proposal : ps) {
        proposals.push_back(proposal);
        const auto & v = gov::Governance::instance().getVotes(proposal.getHash());
        votes.insert(votes.end(), v.begin(), v.end());
    }

    const auto superblock = gov::NextSuperblock(consensus);
    std::map<gov::Proposal, gov::Tally> results;

    UniValue ret(UniValue::VARR);
    for (const auto & proposal : proposals) {
        if (!results.count(proposal) && proposal.getSuperblock() <= superblock) {
            auto presults = gov::Governance::instance().getSuperblockResults(proposal.getSuperblock(), consensus);
            results.insert(presults.begin(), presults.end());
        }
        std::string status = "pending"; // superblocks in the future beyond the next one
        if (proposal.getSuperblock() == superblock) { // next superblock (present tense)
            status = "failing";
            if (results.count(proposal))
                status = "passing";
        } else if (proposal.getSuperblock() < superblock) { // previous superblock (past tense)
            status = "failed";
            if (results.count(proposal))
                status = "passed";
        }
        const auto tally = gov::Governance::getTally(proposal.getHash(), votes, consensus);
        UniValue prop(UniValue::VOBJ);
        prop.pushKV("hash", proposal.getHash().ToString());
        prop.pushKV("name", proposal.getName());
        prop.pushKV("superblock", proposal.getSuperblock());
        prop.pushKV("amount", proposal.getAmount() / COIN);
        prop.pushKV("address", proposal.getAddress());
        prop.pushKV("url", proposal.getUrl());
        prop.pushKV("description", proposal.getDescription());
        prop.pushKV("votes_yes", tally.yes);
        prop.pushKV("votes_no", tally.no);
        prop.pushKV("votes_abstain", tally.abstain);
        prop.pushKV("status", status);
        ret.push_back(prop);
    }
    return ret;
}

static UniValue vote(const JSONRPCRequest& request)
{
#ifndef ENABLE_WALLET
    return "Wallet required";
#endif
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"vote",
                "\nVote on a proposal. Specify the proposal's hash and the vote type to cast the vote. This will\n"
                "attempt to cast as many votes as the wallet allows, which is based on the amount of BLOCK in the\n"
                "wallet.",
                {
                    {"proposal", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Proposal hash to cast votes for"},
                    {"vote", RPCArg::Type::STR, RPCArg::Optional::NO, "Vote options: yes/no/abstain"},
                    {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Only vote with funds from this address"},
                },
                RPCResult{
                "{\n"
                "  \"hash\":\"xxxx\",                      (string) Hex string of the proposal hash\n"
                "  \"name\": \"proposal name\",            (string) Service node name\n"
                "  \"superblock\": n,                      (numeric) Upcoming Superblock to receive payment. Obtain the next Superblock with \"nextsuperblock\" rpc call.\n"
                "  \"amount\": n,                          (numeric) Amount of BLOCK being requested in the proposal\n"
                "  \"address\":\"blocknet address\",       (string) Blocknet payment address\n"
                "  \"url\":\"https://forum.blocknet.co\",  (string) Url where voters can read more details\n"
                "  \"description\":\"xxxx\"                (string) Brief description. Note, if description is too long the proposal submission will fail\n"
                "  \"vote\": \"vote cast\",                (string) Vote that was cast\n"
                "  \"txids\":[\"...\"],                    (array<txid>) Array of hex strings (tx hashes submitting the votes to the network)\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "yes")")
                  + HelpExampleCli("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "no")")
                  + HelpExampleCli("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "abstain")")
                  + HelpExampleCli("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "yes" "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3", "yes")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3", "no")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3", "abstain")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3", "yes", "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa")")
                },
            }.ToString());

    const auto & hash = request.params[0].get_str();
    const auto & voteType = request.params[1].get_str();

    if (!IsHex(hash))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Proposal hash must be a valid hex string");
    const auto & proposalHash = uint256S(hash);

    if (!gov::Governance::instance().hasProposal(proposalHash))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Proposal not found %s", proposalHash.ToString()));

    gov::VoteType castVote;
    if (!gov::Vote::voteTypeForString(voteType, castVote))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("'vote' parameter %s is invalid, expected yes/no/abstain", voteType));

    const gov::Proposal & proposal = gov::Governance::instance().getProposal(proposalHash);
    if (!gov::Governance::outsideVotingCutoff(proposal, chainActive.Height(), Params().GetConsensus()))
        throw JSONRPCError(RPC_MISC_ERROR, "Failed to submit the vote because the voting period for this proposal has ended");

    CTxDestination dest{CNoDestination()};
    if (!request.params[2].isNull()) {
        const auto & address = request.params[2].get_str();
        CTxDestination destination = DecodeDestination(address);
        if (!IsValidDestination(destination))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("'address' parameter %s is invalid", address));
        dest = destination;
    }
    gov::ProposalVote vote{proposal, castVote, dest};
    std::vector<CTransactionRef> txns;
    std::string failReason;
#ifdef ENABLE_WALLET
    if (!gov::SubmitVotes({vote}, GetWallets(), Params().GetConsensus(), txns, g_connman.get(), &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit vote: %s", failReason));
#endif // ENABLE_WALLET

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", proposal.getHash().ToString());
    ret.pushKV("name", proposal.getName());
    ret.pushKV("superblock", proposal.getSuperblock());
    ret.pushKV("amount", proposal.getAmount()/COIN);
    ret.pushKV("address", proposal.getAddress());
    ret.pushKV("url", proposal.getUrl());
    ret.pushKV("description", proposal.getDescription());
    ret.pushKV("vote", gov::Vote::voteTypeToString(castVote));
    UniValue txids(UniValue::VARR);
    for (const auto & tx : txns)
        txids.push_back(tx->GetHash().ToString());
    ret.pushKV("txids", txids);
    return ret;
}

static UniValue proposalfee(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"proposalfee",
                "\nFee in BLOCK for proposal submissions.\n",
                {},
                RPCResult{
                "10\n"
                },
                RPCExamples{
                    HelpExampleCli("proposalfee", "")
                  + HelpExampleRpc("proposalfee", "")
                },
            }.ToString());

    UniValue ret(UniValue::VSTR);
    ret.setStr(FormatMoney(Params().GetConsensus().proposalFee));
    return ret;
}

static UniValue nextsuperblock(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"nextsuperblock",
                "\nReturns the next superblock\n",
                {},
                RPCResult{
                "43200\n"
                },
                RPCExamples{
                    HelpExampleCli("nextsuperblock", "")
                  + HelpExampleRpc("nextsuperblock", "")
                },
            }.ToString());

    UniValue ret(UniValue::VNUM);
    ret.setInt(gov::Governance::nextSuperblock(Params().GetConsensus()));
    return ret;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "governance",         "createproposal",         &createproposal,         {"name", "superblock", "amount", "address", "url", "description"} },
    { "governance",         "listproposals",          &listproposals,          {"sinceblock"} },
    { "governance",         "vote",                   &vote,                   {"proposal", "vote", "address"} },
    { "governance",         "proposalfee",            &proposalfee,            {} },
    { "governance",         "nextsuperblock",         &nextsuperblock,         {} },
};
// clang-format on

void RegisterGovernanceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}