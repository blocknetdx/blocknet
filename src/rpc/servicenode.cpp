// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <net.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <servicenode/servicenodemgr.h>
#include <util/moneystr.h>
#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#include <xbridge/xbridgeapp.h>

#include <boost/algorithm/algorithm.hpp>

static UniValue servicenodesetup(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.empty() || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"servicenodesetup",
                "\nSets up Service Nodes by populating the servicenode.conf. Note* by default new data is appended to servicenode.conf\n",
                {
                    {"type", RPCArg::Type::STR, RPCArg::Optional::NO, "Options: auto|list|remove\n'auto' will automatically setup the number of service nodes you specify.\n'list' will setup service nodes according to a predetermined list.\n'remove' will erase the existing servicenode.conf"},
                    {"count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "'auto' number of servicenodes to create (not used with the 'list' type)"},
                    {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "'auto' service node address (not used with the 'list' type)"},
                    {"list", RPCArg::Type::ARR, RPCArg::Optional::OMITTED, R"(only used with the 'list' type, should contain a list of servicenode objects, example: [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"alias", RPCArg::Type::STR, RPCArg::Optional::NO, "Service node alias"},
                                    {"tier", RPCArg::Type::STR, RPCArg::Optional::NO, "Service node tier: SPV|OPEN"},
                                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "base58 address containing the service node collateral"},
                                },
                            },
                        }
                    }
                },
                RPCResult{
            "[\n"
            "  {\n"
            "    \"alias\": \"xxxx\",              (string) Service node name\n"
            "    \"tier\": \"xxxx\",               (string) Tier of this service node\n"
            "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded private key\n"
            "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
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
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        ret.push_back(obj);
    }

    return ret;
}

static UniValue servicenodecreateinputs(const JSONRPCRequest& request)
{
    const auto smallestInputSize = static_cast<int>(sn::ServiceNode::COLLATERAL_SPV/COIN/Params().GetConsensus().snMaxCollateralCount);
    const int defaultInputSize{1250};
    if (request.fHelp || request.params.empty() || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"servicenodecreateinputs",
                "\nCreates service node unspent transaction outputs prior to snode registration.\n",
                {
                    {"nodeaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Blocknet address for the service node. Funds will be sent here from the wallet."},
                    {"nodecount", RPCArg::Type::NUM, RPCArg::Optional::NO, "1", "Number of service nodes to create"},
                    {"inputsize", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, strprintf("%u", defaultInputSize), strprintf("Coin amount for each input size, must be larger than or equal to %u", smallestInputSize)},
                },
                RPCResult{
                "\n{"
                "    \"nodecount\": n,   (numeric) Number of service nodes\n"
                "    \"collateral\": n,  (numeric) Total collateral configured\n"
                "    \"inputsize\": n,   (numeric) Amount used for service node inputs\n"
                "    \"txid\": \"xxxx\", (string) Transaction id used to create the service node inputs\n"
                "\n}"
                },
                RPCExamples{
                    HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1")
                  + HelpExampleRpc("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1")
                  + HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 5")
                  + HelpExampleRpc("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 5")
                  + HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1 2500")
                  + HelpExampleRpc("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1 2500")
                },
            }.ToString());

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VNUM, UniValue::VNUM});

    const std::string & saddr = request.params[0].get_str();
    int count{1};
    if (request.params.size() < 2)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing nodecount");
    count = request.params[1].get_int();
    if (count <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bad nodecount, must be a number greater than or equal to 1");

    int inputSize{defaultInputSize};
    if (request.params.size() > 2)
        inputSize = request.params[2].get_int();
    if (inputSize < smallestInputSize)
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Input size (%u) must be larger or equal to the minimum %u",
                inputSize, smallestInputSize));

    // Make sure we find saddr key in the wallet (indicating ownership)
    const auto dest = DecodeDestination(saddr);
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Bad nodeaddress %s", saddr));

    std::shared_ptr<CWallet> wallet;
    bool haveAddr{false};
    for (auto & w : GetWallets()) {
        if (w->HaveKey(boost::get<CKeyID>(dest))) {
            haveAddr = true;
            wallet = w;
            break;
        }
    }
    if (!haveAddr)
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Failed to find %s address in your wallet(s)", saddr));

    // Check balance
    const CAmount inputAmount = inputSize * COIN;
    const CAmount leftOver = (sn::ServiceNode::COLLATERAL_SPV * static_cast<CAmount>(count)) % inputAmount;
    const CAmount requiredBalance = sn::ServiceNode::COLLATERAL_SPV * count + leftOver;
    CAmount balance = wallet->GetBalance();
    if (balance <= requiredBalance) {
        const std::string extra = leftOver > 0 ?
                strprintf("\nWe noticed that your input size %u isn't ideal, use an input size "
                          "that divides the required collateral amount %d with no remainder "
                          "(e.g. use %u)", inputSize, defaultInputSize, FormatMoney(requiredBalance)) : "";
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Not enough coin (%s) in wallet containing the snode address. "
                                                         "Unable to create %u service node(s). Require more than %s to "
                                                         "cover snode inputs + fee %s", FormatMoney(balance),
                                                         count, FormatMoney(requiredBalance), extra));
    }

    EnsureWalletIsUnlocked(wallet.get());

    CCoinControl cc;
    cc.fAllowOtherInputs = true;

    CAmount running{0};
    std::vector<CRecipient> vouts;
    while (running < requiredBalance) {
        vouts.push_back({GetScriptForDestination(dest), inputAmount, false});
        running += inputAmount;
    }

    // Create and send the transaction
    CReserveKey reservekey(wallet.get());
    CAmount nFeeRequired;
    std::string strError;
    int nChangePosRet = -1;
    CTransactionRef tx;
    auto locked_chain = wallet->chain().lock();
    if (!wallet->CreateTransaction(*locked_chain, vouts, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to create service node inputs: %s", strError));

    // Send all voting transaction to the network. If there's a failure
    // at any point in the process, bail out.
    if (wallet->GetBroadcastTransactions() && !g_connman)
        throw JSONRPCError(RPC_MISC_ERROR, "Peer-to-peer functionality missing or disabled");

    CValidationState state;
    if (!wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state)));

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("nodecount", count);
    ret.pushKV("collateral", requiredBalance/COIN);
    ret.pushKV("inputsize", inputSize);
    ret.pushKV("txid", tx->GetHash().ToString());
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
                "\nRegisters all service nodes specified in servicenode.conf or registers the snode with a specific alias.\n"
                "If the alias isn't specified all known service nodes (in servicenode.conf) will be registered.\n",
                {
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Optionally register snode with the specified 'alias'"}
                },
                RPCResult{
                "[\n"
                "  {\n"
                "    \"alias\": n,                     (string) Service node name\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this service node\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded private key\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
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
        std::shared_ptr<CWallet> wallet = nullptr;
        for (auto & w : GetWallets()) {
            if (w->HaveKey(entry.addressKeyId())) {
                wallet = w;
                break;
            }
        }
        if (!wallet)
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("wallet could not be found for registering %s", entry.alias));

        EnsureWalletIsUnlocked(wallet.get());

        if (!sn::ServiceNodeMgr::instance().registerSn(entry, g_connman.get(), {wallet}))
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("failed to register the service node %s", entry.alias));
    }

    for (const auto & entry : entries) {
        if (!alias.empty() && alias != entry.alias)
            continue; // skip if alias doesn't match filter
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", EncodeSecret(entry.key));
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
    obj.pushKV("tier", sn::ServiceNodeMgr::tierString(selentry.tier));
    obj.pushKV("snodekey", EncodeSecret(selentry.key));
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
    sn::ServiceNode::Tier tier; sn::ServiceNodeMgr::tierFromString(find_value(snode, "tier").get_str(), tier);
    const auto & key = DecodeSecret(find_value(snode, "snodekey").get_str());
    const auto & address = DecodeDestination(find_value(snode, "address").get_str());
    sn::ServiceNodeConfigEntry entry(alias, tier, key, address);
    std::vector<sn::ServiceNodeConfigEntry> v{entry};
    if (!sn::ServiceNodeMgr::writeSnConfig(v, false))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

    std::set<sn::ServiceNodeConfigEntry> s;
    if (!sn::ServiceNodeMgr::instance().loadSnConfig(s))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    UniValue ret(UniValue::VBOOL);
    ret.setBool(true);
    return ret;
}

static UniValue servicenodestatus(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"servicenodestatus",
                "\nLists all your service nodes and their reported statuses.\n"
                "\nIf one of your service nodes are reported offline check connectivity and verify\n"
                "they are registered with the network. See \"help servicenoderegister\"\n",
                {},
                RPCResult{
                "[\n"
                "  {\n"
                "    \"alias\": n,                     (string) Service node name\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this service node\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded private key\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "    \"timeregistered\": n,            (numeric) Unix time of when this service node was registered\n"
                "    \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "    \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "    \"status\":\"xxxx\",              (string) Status of service node (e.g. running, offline)\n"
                "    \"services\":[...],               (array<string>) List of supported services\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodestatus", "")
                  + HelpExampleRpc("servicenodestatus", "")
                },
            }.ToString());

    if (!sn::ServiceNodeMgr::instance().hasActiveSn())
        throw JSONRPCError(RPC_INVALID_REQUEST, R"(No Service Node is running, check servicenode.conf or run the "servicenodesetup" command)");

    UniValue ret(UniValue::VARR);

    // List all the service node entries and their statuses
    const auto & entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        const auto & snode = sn::ServiceNodeMgr::instance().getSn(entry.key.GetPubKey());
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        obj.pushKV("timeregistered", snode.getRegTime());
        obj.pushKV("timelastseen", snode.getPingTime());
        obj.pushKV("timelastseenstr", xbridge::iso8601(boost::posix_time::from_time_t(snode.getPingTime())));
        obj.pushKV("status", !snode.isNull() && snode.running() ? "running" : "offline");
        UniValue services(UniValue::VARR);
        if (!snode.isNull()) {
            for (const auto & service : snode.serviceList())
                services.push_back(service);
        }
        obj.pushKV("services", services);
        ret.push_back(obj);
    }

    return ret;
}

static UniValue servicenodelist(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"servicenodelist",
                "\nLists all service nodes registered on the Blocknet network\n",
                {},
                RPCResult{
                "[\n"
                "  {\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Service node's pubkey\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this Service Node\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "    \"timeregistered\": n,            (numeric) Unix time of when this service node was registered\n"
                "    \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "    \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "    \"status\":\"xxxx\",              (string) Status of this service node (e.g. running, offline)\n"
                "    \"services\":[...],               (array<string>) List of supported services\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodelist", "")
                  + HelpExampleRpc("servicenodelist", "")
                },
            }.ToString());

    UniValue ret(UniValue::VARR);

    // List all the service node entries and their statuses
    const auto & snodes = sn::ServiceNodeMgr::instance().list();
    for (const auto & snode : snodes) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("snodekey", HexStr(snode.getSnodePubKey()));
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(snode.getTier()));
        obj.pushKV("address", EncodeDestination(snode.getPaymentAddress()));
        obj.pushKV("timeregistered", snode.getRegTime());
        obj.pushKV("timelastseen", snode.getPingTime());
        obj.pushKV("timelastseenstr", xbridge::iso8601(boost::posix_time::from_time_t(snode.getPingTime())));
        obj.pushKV("status", !snode.isNull() && snode.running() ? "running" : "offline");
        UniValue services(UniValue::VARR);
        for (const auto & service : snode.serviceList())
            services.push_back(service);
        obj.pushKV("services", services);
        ret.push_back(obj);
    }

    return ret;
}

static UniValue servicenodesendping(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"servicenodeping",
                "\nSends the service node ping to the Blocknet network for the active node. This updates the network "
                "with the latest service node configs.\n",
                {},
                RPCResult{
                "{\n"
                "  \"snodekey\":\"xxxxxx\",          (string) Service node's pubkey\n"
                "  \"tier\": \"xxxx\",               (string) Tier of this Service Node\n"
                "  \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "  \"timeregistered\": n,            (numeric) Unix time of when this service node was registered\n"
                "  \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "  \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "  \"status\":\"xxxx\",              (string) Status of this service node (e.g. running, offline)\n"
                "  \"services\":[...],               (array<string>) List of supported services\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodeping", "")
                  + HelpExampleRpc("servicenodeping", "")
                },
            }.ToString());

    if (!sn::ServiceNodeMgr::instance().hasActiveSn())
        throw JSONRPCError(RPC_INVALID_REQUEST, R"(No active service node, check servicenode.conf)");

    const auto & activesn = sn::ServiceNodeMgr::instance().getActiveSn();
    auto snode = sn::ServiceNodeMgr::instance().getSn(activesn.key.GetPubKey());

    if (snode.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, strprintf("Failed to send service ping, %s is not running. See \"help servicenoderegister\"", activesn.alias));

    // Send the ping
    auto services = xbridge::App::instance().myServices();
    if (!sn::ServiceNodeMgr::instance().sendPing(xbridge::App::version(), services, g_connman.get()))
        throw JSONRPCError(RPC_INVALID_REQUEST, strprintf("Failed to send service ping for service node %s", activesn.alias));

    // Obtain latest snode data
    snode = sn::ServiceNodeMgr::instance().getSn(activesn.key.GetPubKey());

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("alias", activesn.alias);
    obj.pushKV("tier", sn::ServiceNodeMgr::tierString(activesn.tier));
    obj.pushKV("snodekey", HexStr(activesn.key.GetPubKey()));
    obj.pushKV("address", EncodeDestination(activesn.address));
    obj.pushKV("timeregistered", snode.getRegTime());
    obj.pushKV("timelastseen", snode.getPingTime());
    obj.pushKV("timelastseenstr", xbridge::iso8601(boost::posix_time::from_time_t(snode.getPingTime())));
    obj.pushKV("status", !snode.isNull() && snode.running() ? "running" : "offline");
    UniValue uservices(UniValue::VARR);
    for (const auto & service : snode.serviceList())
        uservices.push_back(service);
    obj.pushKV("services", uservices);
    return obj;
}

static UniValue servicenodelegacy(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"servicenode",
                "\nLegacy Service Node commands from the old wallet. The following legacy commands are available:\n"
                "    servicenode status\n"
                "    servicenode list\n"
                "    servicenode start\n"
                "\n"
                "\"status\" lists your running service nodes\n"
                "\"list\" lists all running service nodes registered on the Blocknet network\n"
                "\"start\" starts specified service node by registering it with the Blocknet network\n"
                "\"start-all\" starts all known service nodes by registering it with the Blocknet network\n",
                {
                    {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "Legacy service node command to run"},
                },
                RPCResult{
                "[\n"
                "  {\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Service node's pubkey\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this Service Node\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "    \"timeregistered\": n,            (numeric) Unix time of when this service node was registered\n"
                "    \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "    \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "    \"status\":\"xxxx\",              (string) Status of this service node (e.g. running, offline)\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenode", "status")
                  + HelpExampleRpc("servicenode", "status")
                  + HelpExampleCli("servicenode", "list")
                  + HelpExampleRpc("servicenode", "list")
                  + HelpExampleCli("servicenode", "start snode0")
                  + HelpExampleRpc("servicenode", "start snode0")
                  + HelpExampleCli("servicenode", "start-all")
                  + HelpExampleRpc("servicenode", "start-all")
                },
            }.ToString());

    const auto & command = request.params[0].get_str();
    auto reqcopy = request;
    reqcopy.params.clear();

    if (command == "status")
        return servicenodestatus(reqcopy);
    else if (command == "list")
        return servicenodelist(reqcopy);
    else if (command == "start-all")
        return servicenoderegister(reqcopy);
    else if (command == "start") { // parse the specified alias
        if (request.params.size() == 2)
            reqcopy.params.push_back(request.params[1]);
        return servicenoderegister(reqcopy);
    }
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Unsupported command: %s", command));
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                       actor (function)          argNames
  //  --------------------- -------------------------- ------------------------- ----------
    { "servicenode",        "servicenodesetup",        &servicenodesetup,        {"type", "options"} },
    { "servicenode",        "servicenodecreateinputs", &servicenodecreateinputs, {"nodeaddress", "nodecount", "inputsize"} },
    { "servicenode",        "servicenodegenkey",       &servicenodegenkey,       {} },
    { "servicenode",        "servicenoderegister",     &servicenoderegister,     {"alias"} },
    { "servicenode",        "servicenodeexport",       &servicenodeexport,       {"alias", "password"} },
    { "servicenode",        "servicenodeimport",       &servicenodeimport,       {"alias", "password"} },
    { "servicenode",        "servicenodestatus",       &servicenodestatus,       {} },
    { "servicenode",        "servicenodelist",         &servicenodelist,         {} },
    { "servicenode",        "servicenodesendping",     &servicenodesendping,     {} },
    { "servicenode",        "servicenode",             &servicenodelegacy,       {"command"} },
};
// clang-format on

void RegisterServiceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}