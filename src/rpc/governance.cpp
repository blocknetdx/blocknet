// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <governance/governance.h>
#include <net.h>
#include <rpc/protocol.h>
#include <rpc/util.h>

static UniValue createproposal(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
        throw std::runtime_error(
            RPCHelpMan{"createproposal",
                "\nCreates a new proposal and submits it to the network. There is a fee associated with proposal submissions. Obtain with \"proposalfee\" rpc call.\n",
                {
                    {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "Proposal name, only alpha number characters are accepted (example: \"My Proposal 1\" or \"My_Proposal-1\")"},
                    {"superblock", RPCArg::Type::NUM, "0", strprintf("default=%d, block number of Superblock. Specify 0 to automatically submit for the next Superblock", gov::NextSuperblock(Params().GetConsensus()))},
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
                  + HelpExampleRpc("createproposal", R"("Dev Proposal" 43200 750 "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa" "https://forum.blocknet.co" "Dev proposal for building xyz")")
                },
            }.ToString());

    const std::string & name = request.params[0].get_str();
    const CAmount & superblock = request.params[1].get_int() == 0 ? gov::Governance::nextSuperblock(Params().GetConsensus()) : request.params[1].get_int();
    const CAmount & amount = request.params[2].get_int() * COIN;
    const std::string & address = request.params[3].get_str();
    const std::string & url = request.params.size() > 4 ? request.params[4].get_str() : "";
    const std::string & description = request.params.size() > 5 ? request.params[5].get_str() : "";

    std::string failReason;

    gov::Proposal proposal(name, superblock, amount, address, url, description);
    if (!proposal.isValid(Params().GetConsensus(), &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit proposal: %s", failReason));

    CTransactionRef tx;
    if (!gov::Governance::submitProposal(proposal, Params().GetConsensus(), tx, &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit proposal: %s", failReason));

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

static UniValue vote(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"vote",
                "\nVote on a proposal. Specify the proposal's hash and the vote type to cast the vote. This will\n"
                "attempt to cast as many votes as the wallet allows, which is based on the amount of BLOCK in the\n"
                "wallet.",
                {
                    {"proposal", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Proposal hash to cast votes for"},
                    {"vote", RPCArg::Type::STR, RPCArg::Optional::NO, "Vote options: yes/no/abstain"},
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
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "yes")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "no")")
                  + HelpExampleRpc("vote", R"("cd28d4830f5510d64b2b3df7781d316825045b85f6d7ce8622eec0a42039b6e3" "abstain")")
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
    gov::ProposalVote vote{proposal, castVote};
    std::vector<CTransactionRef> txns;
    std::string failReason;
    if (!gov::Governance::submitVotes({vote}, Params().GetConsensus(), txns, &failReason))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to submit vote: %s", failReason));

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
        ret.push_back(tx->GetHash().ToString());
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

    UniValue ret(UniValue::VNUM);
    ret.setInt(Params().GetConsensus().proposalFee/COIN);
    return ret;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "governance",         "createproposal",         &createproposal,         {"name", "superblock", "amount", "address", "url", "description"} },
    { "governance",         "vote",                   &vote,                   {"proposal", "vote"} },
    { "governance",         "proposalfee",            &proposalfee,            {} },
};
// clang-format on

void RegisterGovernanceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}