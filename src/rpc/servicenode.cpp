// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <net.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <servicenode/servicenodemgr.h>
#include <util/moneystr.h>

#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#endif // ENABLE_WALLET

#include <governance/governance.h>
#include <xbridge/xbridgeapp.h>
#include <xrouter/xrouterapp.h>

#include <regex>

#include <boost/date_time/posix_time/posix_time.hpp>

static UniValue servicenodesetup(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.empty() || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"servicenodesetup",
                "\nAdds a Service Node to the servicenode.conf. Note* new snodes are appended to servicenode.conf\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "Blocknet address containing the service node collateral"},
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Service node alias, alpha numeric with no spaces (a-z,A-Z,0-9,_-)"},
                },
                RPCResult{
                "{\n"
                "  \"alias\": \"xxxx\",              (string) Service node name\n"
                "  \"tier\": \"xxxx\",               (string) Tier of this service node\n"
                "  \"snodekey\":\"xxxxxx\",          (string) Base58 encoded public key\n"
                "  \"snodeprivkey\":\"xxxxxx\",      (string) Base58 encoded private key\n"
                "  \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodesetup", R"("Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa" "snode0")")
                  + HelpExampleRpc("servicenodesetup", R"("Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa", "snode0")")
                },
            }.ToString());

    auto & smgr = sn::ServiceNodeMgr::instance();

    // Validate address
    const UniValue & saddress = request.params[0];
    auto address = DecodeDestination(saddress.get_str());
    if (!IsValidDestination(address))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("bad address specified: %s", saddress.get_str()));

    auto existingEntries = smgr.getSnEntries();
    std::string snodeAlias;

    // If an alias is specified use that, otherwise generate an alias
    if (request.params[1].isNull()) {
        std::set<std::string> usedAliases;
        for (const auto & entry : existingEntries)
            usedAliases.insert(entry.alias);

        int i = existingEntries.size();
        do {
            auto alias = "snode" + std::to_string(i); // i.e. snode0, snode1, snode2 ...
            if (!usedAliases.count(alias)) {
                snodeAlias = alias;
                break;
            }
            ++i;
        } while(i < 200);
    } else
        snodeAlias = request.params[1].get_str();

    if (snodeAlias.empty())
        throw JSONRPCError(RPC_MISC_ERROR, "Bad service node alias, it's empty");

    std::regex re("^[a-zA-Z0-9_\\-]+$");
    std::smatch m;
    if (!std::regex_match(snodeAlias, m, re))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Bad service node alias, only alpha numeric aliases are allowed with no spaces: %s", snodeAlias));

    // Add new snode entry
    CKey key; key.MakeNewKey(true);
    auto tier = sn::ServiceNode::SPV;
    sn::ServiceNodeConfigEntry entry{snodeAlias, tier, key, address};
    existingEntries.emplace_back(entry);

    if (!sn::ServiceNodeMgr::writeSnConfig(existingEntries))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

    std::set<sn::ServiceNodeConfigEntry> entries;
    if (!sn::ServiceNodeMgr::instance().loadSnConfig(entries))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("alias", entry.alias);
    obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
    obj.pushKV("snodekey", HexStr(entry.key.GetPubKey()));
    obj.pushKV("snodeprivkey", EncodeSecret(entry.key));
    obj.pushKV("address", EncodeDestination(entry.address));
    return obj;
}

static UniValue servicenodesetuplist(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"servicenodesetuplist",
                "\nSets up Service Nodes by populating the servicenode.conf. Note* by default new data is appended to servicenode.conf\n",
                {
                    {"list", RPCArg::Type::ARR, RPCArg::Optional::NO, R"(Should contain a list of servicenode objects in json format, example: [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"key", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "The hex-encoded public key"},
                                    {"alias", RPCArg::Type::STR, RPCArg::Optional::NO, "Service node alias"},
                                    {"tier", RPCArg::Type::STR, RPCArg::Optional::NO, "Service node tier: SPV|OPEN"},
                                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "base58 address containing the service node collateral"},
                                }
                            }
                        }
                    },
                },
                RPCResult{
            "[\n"
            "  {\n"
            "    \"alias\": \"xxxx\",              (string) Service node name\n"
            "    \"tier\": \"xxxx\",               (string) Tier of this service node\n"
            "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded public key\n"
            "    \"snodeprivkey\":\"xxxxxx\",      (string) Base58 encoded private key\n"
            "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
            "  }\n"
            "  ,...\n"
            "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodesetuplist", R"([{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])")
                  + HelpExampleRpc("servicenodesetuplist", R"([{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])")
                },
            }.ToString());

    UniValue ret(UniValue::VARR);

    const UniValue & list = request.params[0];
    if (list.isNull() || !list.isArray())
        throw JSONRPCError(RPC_INVALID_PARAMETER, R"(the list type must be a javascript array of objects, for example [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])");

    const auto & arr = list.get_array();
    if (arr.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "the list cannot be empty");
    if (!arr[0].isObject())
        throw JSONRPCError(RPC_INVALID_PARAMETER, R"(the list type must be a javascript array of objects, for example [{"alias":"snode1","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"},{"alias":"snode2","tier":"SPV","address":"Bdu16u6WPBkDh5f23Zhqo5k8Dp6DS4ffJa"}])");

    // Map existing aliases
    auto existingEntries = sn::ServiceNodeMgr::instance().getSnEntries();
    std::set<std::string> usedAliases;
    for (const auto & entry : existingEntries)
        usedAliases.insert(entry.alias);

    // alias regex
    std::regex re("^[a-zA-Z0-9_\\-]+$");

    std::vector<sn::ServiceNodeConfigEntry> tmpEntries;
    for (int i = 0; i < static_cast<int>(arr.size()); ++i) {
        std::map<std::string, UniValue> snode;
        arr[i].getObjMap(snode);

        // Check alias
        if (!snode.count("alias") || !snode["alias"].isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "bad alias");

        const auto & alias = snode["alias"].get_str();

        // Check if alias already exists
        if (usedAliases.count(alias))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Bad service node alias, alias %s already exists in the config", alias));

        std::smatch m;
        if (!std::regex_match(alias, m, re))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Bad service node alias, only alpha numeric aliases are allowed with no spaces: %s", alias));

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

    existingEntries.insert(existingEntries.end(), tmpEntries.begin(), tmpEntries.end());
    if (!sn::ServiceNodeMgr::writeSnConfig(tmpEntries))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");

    std::set<sn::ServiceNodeConfigEntry> entries;
    if (!sn::ServiceNodeMgr::instance().loadSnConfig(entries))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

    for (const auto & entry : entries) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", HexStr(entry.key.GetPubKey()));
        obj.pushKV("snodeprivkey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        ret.push_back(obj);
    }

    return ret;
}

static UniValue servicenoderemove(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"servicenoderemove",
                "\nRemoves all service nodes from the servicenode.conf. Or if \"alias\" is specified, only removes the\n"
                "service node with the specified alias.\n",
                {
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Service node alias"},
                },
                RPCResult{
                "true\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenoderemove", "")
                  + HelpExampleRpc("servicenoderemove", "")
                  + HelpExampleCli("servicenoderemove", "snode0")
                  + HelpExampleRpc("servicenoderemove", "\"snode0\"")
                },
            }.ToString());

    UniValue r(UniValue::VBOOL); r.setBool(false);

    if (request.params[0].isNull()) {
        std::vector<sn::ServiceNodeConfigEntry> none;
        if (!sn::ServiceNodeMgr::writeSnConfig(none, false))
            throw JSONRPCError(RPC_MISC_ERROR, "failed to write to servicenode.conf, check file permissions");
        sn::ServiceNodeMgr::instance().removeSnEntries();
        r.setBool(true);
        return r;
    }

    const auto alias = request.params[0].get_str();
    auto entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        if (entry.alias == alias) {
            sn::ServiceNodeMgr::instance().removeSnEntry(entry);
            r.setBool(true);
            break;
        }
    }

    return r;
}

static UniValue servicenodecreateinputs(const JSONRPCRequest& request)
{
    const auto smallestInputSize = static_cast<int>(sn::ServiceNode::COLLATERAL_SPV/COIN/Params().GetConsensus().snMaxCollateralCount);
    const int defaultInputSize{1250};
    if (request.fHelp || request.params.empty() || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"servicenodecreateinputs",
                strprintf("\nCreates service node unspent transaction outputs prior to snode registration. This will also create "
                "a %s BLOCK voting input. The voting input is separate from the service node UTXOs and is used to cast votes.\n", FormatMoney(gov::VOTING_UTXO_INPUT_AMOUNT)),
                {
                    {"nodeaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Blocknet address for the service node. Funds will be sent here from the wallet."},
                    {"nodecount", RPCArg::Type::NUM, "1", "Number of service nodes to create"},
                    {"inputsize", RPCArg::Type::NUM, strprintf("%u", defaultInputSize), strprintf("Coin amount for each input size, must be larger than or equal to %u", smallestInputSize)},
                },
                RPCResult{
                "{\n"
                "  \"nodeaddress\": \"xxxx\", (string) Service node address\n"
                "  \"nodecount\": n,          (numeric) Number of service nodes\n"
                "  \"collateral\": n,         (numeric) Total collateral configured\n"
                "  \"inputsize\": n,          (numeric) Amount used for service node inputs\n"
                "  \"txid\": \"xxxx\",        (string) Transaction id used to create the service node inputs\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1")
                  + HelpExampleRpc("servicenodecreateinputs", R"("BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS", 1)")
                  + HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 5")
                  + HelpExampleRpc("servicenodecreateinputs", R"("BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS", 5)")
                  + HelpExampleCli("servicenodecreateinputs", "BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS 1 2500")
                  + HelpExampleRpc("servicenodecreateinputs", R"("BoH7E2KtFqJzGnPjS7qAA4gpnkvo5FBUeS", 1, 2500)")
                },
            }.ToString());
#ifndef ENABLE_WALLET
    throw JSONRPCError(RPC_INVALID_REQUEST, R"(This rpc call requires the wallet to be enabled)");
#else
    const std::string & saddr = request.params[0].get_str();
    int count{1};
    if (!request.params[1].isNull())
        count = request.params[1].get_int();
    if (count <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bad nodecount, must be a number greater than or equal to 1");

    int inputSize{defaultInputSize};
    if (!request.params[2].isNull())
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
    const CAmount votingInput = gov::VOTING_UTXO_INPUT_AMOUNT;
    const CAmount requiredBalance = sn::ServiceNode::COLLATERAL_SPV * count + leftOver + votingInput;
    CAmount balance = wallet->GetBalance();
    if (balance <= requiredBalance) {
        const std::string extra = leftOver > 0 ?
                strprintf("\nWe noticed that your input size %u isn't ideal, use an input size "
                          "that divides the required collateral amount %s with no remainder "
                          "(e.g. use %u or %u)", inputSize, FormatMoney(sn::ServiceNode::COLLATERAL_SPV * count), defaultInputSize, defaultInputSize*2) : "";
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Not enough coin (%s) in wallet containing the snode address. "
                                                         "Unable to create %u service node(s). Require more than %s to "
                                                         "cover snode utxos, voting input, and transaction fee for "
                                                         "network submission.", FormatMoney(balance),
                                                         count, FormatMoney(requiredBalance)));
    }

    EnsureWalletIsUnlocked(wallet.get());

    CCoinControl cc;
    cc.fAllowOtherInputs = true;
    cc.destChange = dest;

    CAmount running{0};
    std::vector<CRecipient> vouts;
    while (running < requiredBalance - votingInput) {
        vouts.push_back({GetScriptForDestination(dest), inputAmount, false});
        running += inputAmount;
    }
    // Add voting input
    vouts.push_back({GetScriptForDestination(dest), votingInput, false});

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
    ret.pushKV("nodeaddress", saddr);
    ret.pushKV("nodecount", count);
    ret.pushKV("collateral", (requiredBalance-votingInput)/COIN);
    ret.pushKV("inputsize", inputSize);
    ret.pushKV("txid", tx->GetHash().ToString());
    return ret;
#endif // ENABLE_WALLET
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
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::OMITTED_NAMED_ARG, "Optionally register snode with the specified 'alias'"}
                },
                RPCResult{
                "[\n"
                "  {\n"
                "    \"alias\": n,                     (string) Service node name\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this service node\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded public key\n"
                "    \"snodeprivkey\":\"xxxxxx\",      (string) Base58 encoded private key\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenoderegister", "")
                  + HelpExampleRpc("servicenoderegister", "")
                  + HelpExampleCli("servicenoderegister", "snode0")
                  + HelpExampleRpc("servicenoderegister", "\"snode0\"")
                },
            }.ToString());
#ifndef ENABLE_WALLET
    throw JSONRPCError(RPC_INVALID_REQUEST, R"(This rpc call requires the wallet to be enabled)");
#else
    std::string alias;
    if (!request.params[0].isNull())
        alias = request.params[0].get_str();

    std::set<sn::ServiceNodeConfigEntry> snentries;
    if (!sn::ServiceNodeMgr::instance().loadSnConfig(snentries))
        throw JSONRPCError(RPC_MISC_ERROR, "failed to load config, check servicenode.conf");

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

        std::string failReason;
        if (!sn::ServiceNodeMgr::instance().registerSn(entry, g_connman.get(), {wallet}, &failReason))
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("failed to register the service node %s: %s", entry.alias, failReason));
    }

    for (const auto & entry : entries) {
        if (!alias.empty() && alias != entry.alias)
            continue; // skip if alias doesn't match filter
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", HexStr(entry.key.GetPubKey()));
        obj.pushKV("snodeprivkey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
        ret.push_back(obj);
    }

    if (ret.empty())
        throw JSONRPCError(RPC_MISC_ERROR, "No service nodes registered, is servicenode.conf populated?");

    return ret;
#endif // ENABLE_WALLET
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
                  + HelpExampleRpc("servicenodeexport", R"("snode0", "mypassword1")")
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
#ifdef ENABLE_WALLET
    std::vector<unsigned char> vchSalt = ParseHex("0000aabbccee0000"); // not using salt
    CCrypter crypt;
    crypt.SetKeyFromPassphrase(passphrase, vchSalt, 100, 0);

    std::vector<unsigned char> cypher;
    if (!crypt.Encrypt(CKeyingMaterial(input.begin(), input.end()), cypher))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Bad passphrase %s", passphrase));

    ret.setStr(HexStr(cypher));
#else
    ret.setStr(HexStr(input));
#endif // ENABLE_WALLET
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
                  + HelpExampleRpc("servicenodeimport", R"("aae6d7aedaed54ade57da4eda3e5d4a7de8a67d8e7a8d768ea567da5e467d4ea7a6d7a6d7a6d75a7d5a757da5", "mypassword1")")
                },
            }.ToString());

    const std::vector<unsigned char> & input = ParseHex(request.params[0].get_str());
    SecureString passphrase; passphrase.reserve(100);
    passphrase = request.params[1].get_str().c_str();
#ifdef ENABLE_WALLET
    std::vector<unsigned char> vchSalt = ParseHex("0000aabbccee0000"); // not using salt
    CCrypter crypt;
    crypt.SetKeyFromPassphrase(passphrase, vchSalt, 100, 0);

    CKeyingMaterial plaintext;
    if (!crypt.Decrypt(input, plaintext))
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Bad passphrase %s", passphrase));
#else
    auto plaintext = input;
#endif // ENABLE_WALLET
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
                "    \"snodekey\":\"xxxxxx\",          (string) Base58 encoded public key\n"
                "    \"snodeprivkey\":\"xxxxxx\",      (string) Base58 encoded private key\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
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

    if (!sn::ServiceNodeMgr::instance().hasActiveSn() && sn::ServiceNodeMgr::instance().getSnEntries().empty())
        throw JSONRPCError(RPC_INVALID_REQUEST, R"(No Service Node is running, check servicenode.conf or run the "servicenodesetup" command)");

    UniValue ret(UniValue::VARR);

    // List all the service node entries and their statuses
    const auto & entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        const auto & snode = sn::ServiceNodeMgr::instance().getSn(entry.key.GetPubKey());
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("alias", entry.alias);
        obj.pushKV("tier", sn::ServiceNodeMgr::tierString(entry.tier));
        obj.pushKV("snodekey", HexStr(entry.key.GetPubKey()));
        obj.pushKV("snodeprivkey", EncodeSecret(entry.key));
        obj.pushKV("address", EncodeDestination(entry.address));
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
                "    \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "    \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "    \"exr\": n,                       (boolean) Enterprise XRouter compatibility\n"
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
        obj.pushKV("timelastseen", snode.getPingTime());
        obj.pushKV("timelastseenstr", xbridge::iso8601(boost::posix_time::from_time_t(snode.getPingTime())));
        obj.pushKV("exr", snode.isEXRCompatible());
        obj.pushKV("status", !snode.isNull() && snode.running() ? "running" : "offline");
        obj.pushKV("score", xrouter::App::instance().isReady() ? xrouter::App::instance().getScore(snode.getHostPort()) : 0);
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
            RPCHelpMan{"servicenodesendping",
                "\nSends the service node ping to the Blocknet network for the active node. This updates the network "
                "with the latest service node configs.\n",
                {},
                RPCResult{
                "{\n"
                "  \"snodekey\":\"xxxxxx\",          (string) Service node's pubkey\n"
                "  \"tier\": \"xxxx\",               (string) Tier of this Service Node\n"
                "  \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "  \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "  \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "  \"status\":\"xxxx\",              (string) Status of this service node (e.g. running, offline)\n"
                "  \"services\":[...],               (array<string>) List of supported services\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodesendping", "")
                  + HelpExampleRpc("servicenodesendping", "")
                },
            }.ToString());
#ifndef ENABLE_WALLET
    throw JSONRPCError(RPC_INVALID_REQUEST, R"(This rpc call requires the wallet to be enabled)");
#else
    if (!sn::ServiceNodeMgr::instance().hasActiveSn())
        throw JSONRPCError(RPC_INVALID_REQUEST, R"(No active service node, check servicenode.conf)");

    const auto & activesn = sn::ServiceNodeMgr::instance().getActiveSn();
    auto snode = sn::ServiceNodeMgr::instance().getSn(activesn.key.GetPubKey());

    if (snode.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, strprintf("Failed to send service ping, %s is not running. See \"help servicenoderegister\"", activesn.alias));

    // Send the ping
    const auto & jservices = xbridge::App::instance().myServicesJSON();
    if (!sn::ServiceNodeMgr::instance().sendPing(XROUTER_PROTOCOL_VERSION, jservices, g_connman.get()))
        throw JSONRPCError(RPC_INVALID_REQUEST, strprintf("Failed to send service ping for service node %s", activesn.alias));

    // Obtain latest snode data
    snode = sn::ServiceNodeMgr::instance().getSn(activesn.key.GetPubKey());

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("alias", activesn.alias);
    obj.pushKV("tier", sn::ServiceNodeMgr::tierString(activesn.tier));
    obj.pushKV("snodekey", HexStr(activesn.key.GetPubKey()));
    obj.pushKV("address", EncodeDestination(activesn.address));
    obj.pushKV("timelastseen", snode.getPingTime());
    obj.pushKV("timelastseenstr", xbridge::iso8601(boost::posix_time::from_time_t(snode.getPingTime())));
    obj.pushKV("status", !snode.isNull() && snode.running() ? "running" : "offline");
    UniValue uservices(UniValue::VARR);
    for (const auto & service : snode.serviceList())
        uservices.push_back(service);
    obj.pushKV("services", uservices);
    return obj;
#endif // ENABLE_WALLET
}

static UniValue servicenodecount(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            RPCHelpMan{"servicenodecount",
                "\nLists service node counts on the network.\n",
                {},
                RPCResult{
                "{\n"
                "  \"total\": n,    (numeric) Total service nodes on the network\n"
                "  \"online\": n,   (numeric) Total online service nodes\n"
                "  \"offline\": n,  (numeric) Total offline service nodes\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenodecount", "")
                  + HelpExampleRpc("servicenodecount", "")
                },
            }.ToString());

    const auto & list = sn::ServiceNodeMgr::instance().list();
    const auto total = static_cast<int>(list.size());
    int online{0};
    int offline{0};
    for (const auto & s : list) {
        if (s.running())
            ++online;
        else
            ++offline;
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("total", total);
    obj.pushKV("online", online);
    obj.pushKV("offline", offline);
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
                "    servicenode count\n"
                "\n"
                "\"status\" lists your running service nodes\n"
                "\"list\" lists all running service nodes registered on the Blocknet network\n"
                "\"start\" starts specified service node by registering it with the Blocknet network\n"
                "\"start-all\" starts all known service nodes by registering it with the Blocknet network\n"
                "\"count\" lists the number of service nodes\n",
                {
                    {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "Legacy service node command to run"},
                },
                RPCResult{
                "[\n"
                "  {\n"
                "    \"snodekey\":\"xxxxxx\",          (string) Service node's pubkey\n"
                "    \"tier\": \"xxxx\",               (string) Tier of this Service Node\n"
                "    \"address\":\"blocknet address\", (string) Blocknet address associated with the service node\n"
                "    \"timelastseen\": n,              (numeric) Unix time of when this service node was last seen\n"
                "    \"timelastseenstr\":\"xxxx\",     (string) ISO 8601 of last seen date\n"
                "    \"status\":\"xxxx\",              (string) Status of this service node (e.g. running, offline)\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                },
                RPCExamples{
                    HelpExampleCli("servicenode", "status")
                  + HelpExampleRpc("servicenode", "\"status\"")
                  + HelpExampleCli("servicenode", "list")
                  + HelpExampleRpc("servicenode", "\"list\"")
                  + HelpExampleCli("servicenode", "start snode0")
                  + HelpExampleRpc("servicenode", "\"start\", \"snode0\"")
                  + HelpExampleCli("servicenode", "start-all")
                  + HelpExampleRpc("servicenode", "\"start-all\"")
                  + HelpExampleCli("servicenode", "count")
                  + HelpExampleRpc("servicenode", "\"count\"")
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
    else if (command == "count")
        return servicenodecount(reqcopy);
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Unsupported command: %s", command));
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                       actor (function)          argNames
  //  --------------------- -------------------------- ------------------------- ----------
    { "servicenode",        "servicenodesetup",        &servicenodesetup,        {"address", "alias"} },
    { "servicenode",        "servicenodesetuplist",    &servicenodesetuplist,    {"list"} },
    { "servicenode",        "servicenodecreateinputs", &servicenodecreateinputs, {"nodeaddress", "nodecount", "inputsize"} },
    { "servicenode",        "servicenodegenkey",       &servicenodegenkey,       {} },
    { "servicenode",        "servicenoderegister",     &servicenoderegister,     {"alias"} },
    { "servicenode",        "servicenodeexport",       &servicenodeexport,       {"alias", "password"} },
    { "servicenode",        "servicenodeimport",       &servicenodeimport,       {"alias", "password"} },
    { "servicenode",        "servicenodestatus",       &servicenodestatus,       {} },
    { "servicenode",        "servicenodelist",         &servicenodelist,         {} },
    { "servicenode",        "servicenodesendping",     &servicenodesendping,     {} },
    { "servicenode",        "servicenoderemove",       &servicenoderemove,       {"alias"} },
    { "servicenode",        "servicenodecount",        &servicenodecount,        {} },
    { "servicenode",        "servicenode",             &servicenodelegacy,       {"command"} },
};
// clang-format on

void RegisterServiceNodeRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}