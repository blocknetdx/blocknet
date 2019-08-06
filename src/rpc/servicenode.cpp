// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <net.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <servicenode/servicenodemgr.h>

static UniValue servicenodesetup(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.empty() || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"servicenodesetup",
                "\nSets up Service Nodes by populating the servicenode.conf. Note* by default new data is appended to servicenode.conf\n",
                {
                    {"type", RPCArg::Type::STR, RPCArg::Optional::NO, "Options: auto|list|remove\n'auto' will automatically setup the number of service nodes you specify.\n'list' will setup service nodes according to a predetermined list.\n'remove' will erase the existing servicenode.conf",
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
                  + HelpExampleRpc("servicenodesetup", "remove")
                  + HelpExampleRpc("servicenodesetup", "remove")
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
        if (!sn::ServiceNodeMgr::writeSnConfig(tmpEntries))
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

        if (!sn::ServiceNodeMgr::writeSnConfig(tmpEntries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

        if (!sn::ServiceNodeMgr::instance().loadSnConfig(entries))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    } else if (type.get_str() == "remove") {
        std::vector<sn::ServiceNodeConfigEntry> none;
        if (!sn::ServiceNodeMgr::writeSnConfig(none, false))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");
        UniValue r(UniValue::VBOOL); r.setBool(true);
        return r; // done
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Acceptable types are: auto,list,remove");
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

static UniValue servicenodegenkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            RPCHelpMan{"servicenodegenkey",
                "\nGenerates a base58 encoded private key for use with servicenode.conf\n",
                {},
                RPCResult{"(string) base58 encoded private key\n"},
                RPCExamples{
                    HelpExampleCli("servicenodegenkey", "")
                  + HelpExampleRpc("servicenodegenkey", "")
                },
            }.ToString());

    UniValue ret(UniValue::VSTR);
    CKey key; key.MakeNewKey(true);
    ret.setStr(EncodeSecret(key));
    return ret;
}

static UniValue servicenoderegister(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"servicenoderegister",
                "\nRegisters all service nodes specified in servicenode.conf or registers the snode with a specific alias.\n",
                {
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Optionally register snode with the specified 'alias'"}
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
                    HelpExampleCli("servicenoderegister", "")
                  + HelpExampleRpc("servicenoderegister", "")
                  + HelpExampleCli("servicenoderegister", "snode0")
                  + HelpExampleRpc("servicenoderegister", "snode0")
                },
            }.ToString());

    std::string alias;
    if (request.params.size() == 1)
        alias = request.params[0].get_str();

    UniValue ret(UniValue::VARR);
    const auto entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        if (!alias.empty() && alias != entry.alias)
            continue; // skip if alias doesn't match filter
        if (!sn::ServiceNodeMgr::instance().registerSn(entry, g_connman.get()))
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("failed to register the service node", entry.alias));
    }

    for (const auto & entry : entries) {
        if (!alias.empty() && alias != entry.alias)
            continue; // skip if alias doesn't match filter
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", entry.tier);
        obj.pushKV("servicenodeprivkey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        ret.push_back(obj);
    }

    if (ret.empty())
        throw JSONRPCError(RPC_MISC_ERROR, "No service nodes registered, is servicenode.conf populated?");

    return ret;
}

static UniValue servicenodeexport(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            RPCHelpMan{"servicenodeexport",
                "\nExport the service node data with the specified alias.\n",
                {
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::NO, "Service Node alias to export (example: snode0)"},
                    {"password", RPCArg::Type::STR, RPCArg::Optional::NO, "Password used to encrypt the export data"}
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodeexport", "snode0 mypassword1")
                  + HelpExampleRpc("servicenodeexport", "snode0 mypassword1")
                },
            }.ToString());

    const auto & alias = request.params[0].get_str();
    SecureString passphrase; passphrase.reserve(100);
    passphrase = request.params[1].get_str().c_str();

    UniValue ret(UniValue::VSTR);
    sn::ServiceNodeConfigEntry selentry;
    bool found{false};

    const auto entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        if (alias != entry.alias)
            continue;
        found = true;
        selentry = entry;
        break;
    }

    if (!found)
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("No service nodes found with alias %s", alias));


    UniValue obj(UniValue::VOBJ);
    obj.pushKV("alias", selentry.alias);
    obj.pushKV("tier", selentry.tier);
    obj.pushKV("servicenodeprivkey", EncodeSecret(selentry.key));
    obj.pushKV("address", EncodeDestination(selentry.address));
    const std::string & exportt = obj.write();
    std::vector<unsigned char> input(exportt.begin(), exportt.end());

    std::vector<unsigned char> vchSalt = ParseHex("0000aabbccee0000"); // not using salt
    CCrypter crypt;
    crypt.SetKeyFromPassphrase(passphrase, vchSalt, 100, 0);

    std::vector<unsigned char> cypher;
    if (!crypt.Encrypt(CKeyingMaterial(input.begin(), input.end()), cypher))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Bad passphrase %s", passphrase));

    ret.setStr(HexStr(cypher));
    return ret;
}

static UniValue servicenodeimport(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            RPCHelpMan{"servicenodeimport",
                "\nImport the service node data, requires export password (not wallet password).\n",
                {
                    {"importdata", RPCArg::Type::STR, RPCArg::Optional::NO, "Service Node data to import (from export)"},
                    {"password", RPCArg::Type::STR, RPCArg::Optional::NO, "Export password used to encrypt the service node data, this is not the wallet passphrase"}
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodeimport", "aae6d7aedaed54ade57da4eda3e5d4a7de8a67d8e7a8d768ea567da5e467d4ea7a6d7a6d7a6d75a7d5a757da5 mypassword1")
                  + HelpExampleRpc("servicenodeimport", "aae6d7aedaed54ade57da4eda3e5d4a7de8a67d8e7a8d768ea567da5e467d4ea7a6d7a6d7a6d75a7d5a757da5 mypassword1")
                },
            }.ToString());

    const std::vector<unsigned char> & input = ParseHex(request.params[0].get_str());
    SecureString passphrase; passphrase.reserve(100);
    passphrase = request.params[1].get_str().c_str();

    std::vector<unsigned char> vchSalt = ParseHex("0000aabbccee0000"); // not using salt
    CCrypter crypt;
    crypt.SetKeyFromPassphrase(passphrase, vchSalt, 100, 0);

    CKeyingMaterial plaintext;
    if (!crypt.Decrypt(input, plaintext))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Bad passphrase %s", passphrase));

    UniValue snode(UniValue::VOBJ);
    if (!snode.read(std::string(plaintext.begin(), plaintext.end())))
        throw JSONRPCError(RPC_MISC_ERROR, "Failed to add the service node, is the password correct?");

    const auto & alias = find_value(snode, "alias").get_str();
    const auto & tier = static_cast<sn::ServiceNode::Tier>(find_value(snode, "tier").get_int());
    const auto & key = DecodeSecret(find_value(snode, "servicenodeprivkey").get_str());
    const auto & address = DecodeDestination(find_value(snode, "address").get_str());
    sn::ServiceNodeConfigEntry entry(alias, tier, key, address);
    std::vector<sn::ServiceNodeConfigEntry> v{entry};
    if (!sn::ServiceNodeMgr::writeSnConfig(v, false))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

    std::set<sn::ServiceNodeConfigEntry> s(v.begin(), v.end());
    if (!sn::ServiceNodeMgr::instance().loadSnConfig(s))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    UniValue ret(UniValue::VBOOL);
    ret.setBool(true);
    return ret;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "servicenode",        "servicenodesetup",       &servicenodesetup,       {"type", "options"} },
    { "servicenode",        "servicenodegenkey",      &servicenodegenkey,      {} },
    { "servicenode",        "servicenoderegister",    &servicenoderegister,    {"alias"} },
    { "servicenode",        "servicenodeexport",      &servicenodeexport,      {"alias", "password"} },
    { "servicenode",        "servicenodeimport",      &servicenodeimport,      {"alias", "password"} },
};
// clang-format on

void RegisterServiceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}