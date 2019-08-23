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

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "governance",         "createproposal",         &createproposal,         {"name", "superblock", "amount", "address", "url", "description"} },
};
// clang-format on

void RegisterGovernanceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}