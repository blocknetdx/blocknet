// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <rpc/protocol.h>
#include <rpc/util.h>
#include <servicenode/servicenodemgr.h>

static UniValue servicenodesetup(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.empty() || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"servicenodesetup",
                "\nSets up Service Nodes by populating the servicenode.conf. Note* that the existing data in servicenode.conf will be deleted.\n",
                {
                    {"type", RPCArg::Type::STR, RPCArg::Optional::NO, "Options: auto|list 'auto' will automatically setup the number of service nodes you specify. 'list' will setup service nodes according to a predetermined list",
                     {
                        {"count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "'auto' number of servicenodes to create (not used with the 'list' type)"},
                        {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "'auto' service node address (not used with the 'list' type)"},
                        {"list", RPCArg::Type::ARR, RPCArg::Optional::OMITTED, R"(only used with the 'list' type, should contain a list of servicenode objects, example: [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])"}
                     },
                    }
                },
                RPCResult{
            "[\n"
            "  {\n"
            "    \"alias\": n,                       (string) Service node name\n"
            "    \"tier\": n,                        (numeric) Tier of this service node\n"
            "    \"servicenodeprivkey\":\"xxxxxx\",  (string) Base58 encoded private key\n"
            "    \"address\":\"blocknet address\",   (string) Blocknet address associated with the service node\n"
            "  }\n"
            "  ,...\n"
            "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodesetup", R"("auto" 5 "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa")")
                  + HelpExampleCli("servicenodesetup", R"("list" [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])")
                  + HelpExampleRpc("servicenodesetup", R"("auto" 5 "Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa")")
                  + HelpExampleRpc("servicenodesetup", R"("list" [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])")
                },
            }.ToString());

    UniValue ret(UniValue::VARR);
    std::set<sn::ServiceNodeConfigEntry> entries;

    const UniValue & type = request.params[0];
    if (type.get_str() == "auto") {
        const UniValue & scount = request.params[1];
        const UniValue & saddress = request.params[2];
        int snodecount = scount.get_int();
        if (snodecount <= 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("bad count specified: %d", snodecount));

        // Validate address
        auto address = DecodeDestination(saddress.get_str());
        if (!IsValidDestination(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("bad address specified: %s", saddress.get_str()));

        std::vector<sn::ServiceNodeConfigEntry> tmpEntries;
        for (int i = 0; i < snodecount; ++i) {
            auto alias = "snode" + std::to_string(i); // i.e. snode0, snode1, snode2 ...
            auto tier = sn::ServiceNode::SPV;
            CKey key; key.MakeNewKey(true);
            tmpEntries.emplace_back(alias, tier, key, address);
        }
        if (!sn::ServiceNodeMgr::instance().writeSnConfig(tmpEntries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

        if (!sn::ServiceNodeMgr::instance().loadSnConfig(entries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    } else if (type.get_str() == "list") {
        const UniValue & list = request.params[1];
        if (!list.isArray())
            throw JSONRPCError(RPC_INVALID_PARAMETER, R"(the list type must be a javascript array of objects, for example [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])");

        const auto & arr = list.get_array();
        if (arr.empty())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "the list cannot be empty");
        if (!arr[0].isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, R"(the list type must be a javascript array of objects, for example [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])");

        std::vector<sn::ServiceNodeConfigEntry> tmpEntries;
        for (int i = 0; i < static_cast<int>(arr.size()); ++i) {
            std::map<std::string, UniValue> snode;
            arr[i].getObjMap(snode);

            // Check alias
            if (!snode.count("alias") || !snode["alias"].isStr())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "bad alias");
            const auto & alias = snode["alias"].get_str();
            // no spaces allowed
            int spaces = std::count_if(alias.begin(), alias.end(), [](const unsigned char & c) { return std::isspace(c); });
            if (spaces > 0)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "bad alias, whitespace is not allowed: " + alias);

            // Check tier
            sn::ServiceNode::Tier tier;
            if (!snode.count("tier") || !sn::ServiceNodeMgr::tierFromString(snode["tier"].get_str(), tier))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "bad tier for " + alias);

            // Check address (required for non-free tier)
            std::string address;
            if (!snode.count("address") && !sn::ServiceNodeMgr::freeTier(tier)) // only free tier can have an optional address
                throw JSONRPCError(RPC_INVALID_PARAMETER, "address is required for " + alias);
            else // if paid tier
                address = snode["address"].get_str();
            CTxDestination addr;
            if (!sn::ServiceNodeMgr::freeTier(tier)) {
                addr = DecodeDestination(address);
                if (!IsValidDestination(addr))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "bad address for " + alias);
            }

            CKey key; key.MakeNewKey(true);
            tmpEntries.emplace_back(alias, tier, key, addr);
        }

        if (!sn::ServiceNodeMgr::instance().writeSnConfig(tmpEntries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

        if (!sn::ServiceNodeMgr::instance().loadSnConfig(entries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    } else {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Acceptable types are: auto,list");
    }

    for (const auto & entry : entries) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", entry.tier);
        obj.pushKV("servicenodeprivkey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        ret.push_back(obj);
    }

    return ret;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "servicenode",        "servicenodesetup",       &servicenodesetup,       {"type", "options"} },
};
// clang-format on

void RegisterServiceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}