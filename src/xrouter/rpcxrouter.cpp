// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <xbridge/xbridgeapp.h>
#include <xrouter/xrouterapp.h>
#include <xrouter/xroutererror.h>
#include <xrouter/xrouterutils.h>

#include <bloom.h>
#include <core_io.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <rpc/util.h>

#include <exception>
#include <regex>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>

using namespace json_spirit;

static UniValue uret_xr(const json_spirit::Value & o) {
    UniValue uv;
    const auto str = json_spirit::write_string(o, json_spirit::none, 8);
    try {
        if (!uv.read(str))
            uv.setStr(str);
    } catch (...) {
        uv.setStr(str);
    }
    return uv;
}

//******************************************************************************
//******************************************************************************
static UniValue xrGetBlockCount(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlockCount",
                "\nBlock count for the longest chain in the specified blockchain.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                "{\n"
                "  \"reply\" : n,        (numeric) Block count\n"
                "  \"uuid\" : \"xxxx\"   (string) Request uuid\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlockCount", "BTC 1")
                  + HelpExampleRpc("xrGetBlockCount", "BTC 1")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }
    
    int consensus{0};
    if (params.size() >= 2) {
        consensus = params[1].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockCount(uuid, currency, consensus);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetBlockHash(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlockHash",
                "\nHash of block with the specified block number.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"block_number", RPCArg::Type::STR, RPCArg::Optional::NO, "Block number or hex string"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                "{\n"
                "  \"reply\" : \"xxxx\", (numeric) Block hash\n"
                "  \"uuid\" : \"xxxx\"   (string) Request uuid\n"
                "}\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlockHash", "BTC 1")
                  + HelpExampleRpc("xrGetBlockHash", "BTC 1")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block number not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    unsigned int block;
    if (!xrouter::hextodec(params[1].get_str(), block)) {
        Object error;
        error.emplace_back("error", "Bad block number " + params[1].get_str());
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockHash(uuid, currency, consensus, block);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetBlock(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlock",
                "\nGet block data by hash in json format.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"hash", RPCArg::Type::STR, RPCArg::Optional::NO, "Block hash"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                R"({
                    "reply" : {
                        "hash" : "d6f34e5b35c640ce443310cc975f13d31efb7b36a6d5fd6b1abb1bec9da578df",
                        "confirmations" : 10,
                        "size" : 429,
                        "height" : 1120075,
                        "version" : 3,
                        "merkleroot" : "74f66a41138d8f2c62cb5ee9191cd847b39bdc31e4d7a7d84806ddaf1ae9390b",
                        "tx" : [
                            "8d9a4f1406a1ee9394ffafaf091e6a506ca1d771b24802d570c92ec5375be497",
                            "97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e"
                        ],
                        "time" : 1570043635,
                        "nonce" : 0,
                        "bits" : "1b04122f",
                        "difficulty" : 16097.89302059,
                        "chainwork" : "0000000000000000000000000000000000000000000000033ae7bf6cdd8e381d",
                        "previousblockhash" : "9ddfa9802278b95a6fd01d5a2dd298ae3236c4d66fc00b86305562fb4c53436f",
                        "nextblockhash" : "4d9b19d031d22ba36c676fa6773d594ca1c7db57c4aeba868ac3bf0cb167dafa"
                    },
                    "uuid" : "cced23b0-a992-4012-8230-6a869a7f0ac8"
                })"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlock", "BLOCK d6f34e5b35c640ce443310cc975f13d31efb7b36a6d5fd6b1abb1bec9da578df")
                  + HelpExampleRpc("xrGetBlock", "BLOCK d6f34e5b35c640ce443310cc975f13d31efb7b36a6d5fd6b1abb1bec9da578df")
                  + HelpExampleCli("xrGetBlock", "BLOCK d6f34e5b35c640ce443310cc975f13d31efb7b36a6d5fd6b1abb1bec9da578df 2")
                  + HelpExampleRpc("xrGetBlock", "BLOCK d6f34e5b35c640ce443310cc975f13d31efb7b36a6d5fd6b1abb1bec9da578df 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1 || (params.size() == 1 && xrouter::is_hash(params[0].get_str())))
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const auto & hash = params[1].get_str();
    if (!xrouter::is_hash(hash)) {
        Object error;
        error.emplace_back("error", "Block hash is bad");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlock(uuid, currency, consensus, hash);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTransaction",
                "\nGet transaction data in json format.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"hash", RPCArg::Type::STR, RPCArg::Optional::NO, "Transaction hash"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                R"({
                    "reply" : {
                        "txid" : "97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e",
                        "version" : 1,
                        "locktime" : 0,
                        "vin" : [
                            {
                                "txid" : "4bee60384e3ada36feacb0e2c71a26846c67a5f7faeb9e40725821e446c3d288",
                                "vout" : 1,
                                "scriptSig" : {
                                    "asm" : "304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01",
                                    "hex" : "47304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01"
                                },
                                "sequence" : 4294967295
                            }
                        ],
                        "vout" : [
                            {
                                "value" : 0.00000000,
                                "n" : 0,
                                "scriptPubKey" : {
                                    "asm" : "",
                                    "hex" : "",
                                    "type" : "nonstandard"
                                }
                            },
                            {
                                "value" : 901.19451902,
                                "n" : 1,
                                "scriptPubKey" : {
                                    "asm" : "03f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855 OP_CHECKSIG",
                                    "hex" : "2103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac",
                                    "reqSigs" : 1,
                                    "type" : "pubkey",
                                    "addresses" : [
                                        "BeDQzBfouG1WxE9ArLc43uibF8Gk4HYv3j"
                                    ]
                                }
                            },
                            {
                                "value" : 0.70000000,
                                "n" : 2,
                                "scriptPubKey" : {
                                    "asm" : "OP_DUP OP_HASH160 6499ceebaa0d586c95271575780b3f9590f9f8ca OP_EQUALVERIFY OP_CHECKSIG",
                                    "hex" : "76a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac",
                                    "reqSigs" : 1,
                                    "type" : "pubkeyhash",
                                    "addresses" : [
                                        "BcxcRo1L5Pk5e2UuabzAJcbs9NKzsMTsuu"
                                    ]
                                }
                            }
                        ]
                    },
                    "uuid" : "b334b5ee-0585-425b-86a4-02d2fecbf67b"
                })"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e")
                  + HelpExampleRpc("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e")
                  + HelpExampleCli("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e 2")
                  + HelpExampleRpc("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1 || (params.size() == 1 && xrouter::is_hash(params[0].get_str())))
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const auto & hash = params[1].get_str();
    if (!xrouter::is_hash(hash)) {
        Object error;
        error.emplace_back("error", "Transaction hash is bad");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransaction(uuid, currency, consensus, hash);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrDecodeRawTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrDecodeRawTransaction",
                "\nDecodes the specified transaction hex and returns the transaction data in json format.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"hex", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Raw transaction hex string"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                R"({
                    "reply" : {
                        "txid" : "97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e",
                        "version" : 1,
                        "locktime" : 0,
                        "vin" : [
                            {
                                "txid" : "4bee60384e3ada36feacb0e2c71a26846c67a5f7faeb9e40725821e446c3d288",
                                "vout" : 1,
                                "scriptSig" : {
                                    "asm" : "304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01",
                                    "hex" : "47304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01"
                                },
                                "sequence" : 4294967295
                            }
                        ],
                        "vout" : [
                            {
                                "value" : 0.00000000,
                                "n" : 0,
                                "scriptPubKey" : {
                                    "asm" : "",
                                    "hex" : "",
                                    "type" : "nonstandard"
                                }
                            },
                            {
                                "value" : 901.19451902,
                                "n" : 1,
                                "scriptPubKey" : {
                                    "asm" : "03f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855 OP_CHECKSIG",
                                    "hex" : "2103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac",
                                    "reqSigs" : 1,
                                    "type" : "pubkey",
                                    "addresses" : [
                                        "BeDQzBfouG1WxE9ArLc43uibF8Gk4HYv3j"
                                    ]
                                }
                            },
                            {
                                "value" : 0.70000000,
                                "n" : 2,
                                "scriptPubKey" : {
                                    "asm" : "OP_DUP OP_HASH160 6499ceebaa0d586c95271575780b3f9590f9f8ca OP_EQUALVERIFY OP_CHECKSIG",
                                    "hex" : "76a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac",
                                    "reqSigs" : 1,
                                    "type" : "pubkeyhash",
                                    "addresses" : [
                                        "BcxcRo1L5Pk5e2UuabzAJcbs9NKzsMTsuu"
                                    ]
                                }
                            }
                        ]
                    },
                    "uuid" : "b334b5ee-0585-425b-86a4-02d2fecbf67b"
                })"
                },
                RPCExamples{
                    HelpExampleCli("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000")
                  + HelpExampleRpc("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000")
                  + HelpExampleCli("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000 2")
                  + HelpExampleRpc("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1 || (params.size() == 1 && xrouter::is_hex(params[0].get_str())))
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hex not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const auto & hex = params[1].get_str();
    if (hex.empty() || !xrouter::is_hex(hex)) {
        Object error;
        error.emplace_back("error", "Transaction hex is bad");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().decodeRawTransaction(uuid, currency, consensus, hex);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetBlocks(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlocks",
                "\nList of blocks in json format for the specified block hashes.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"blockhashes", RPCArg::Type::STR, RPCArg::Optional::NO, "Comma delimited list of block hashes, example: blockhash1,blockhash2,blockhash3"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                R"({
                    "reply" : [
                        {
                            "hash" : "39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf",
                            "confirmations" : 259225,
                            "size" : 429,
                            "height" : 860877,
                            "version" : 3,
                            "merkleroot" : "0ad8234849f7cda3bc88ea55cba58a93c9f903db7fd76ce9aae1787fb6b36465",
                            "tx" : [
                                "5f7aafd0a1c0835b2602305dacba03c8f34f942d9f3d624456d551d730d55207",
                                "f046ae548f88c166259b3f9a37088abc44b7aee8cacc0efe66fad34f31173af2"
                            ],
                            "time" : 1554330467,
                            "nonce" : 0,
                            "bits" : "1b068aa1",
                            "difficulty" : 10018.31506514,
                            "chainwork" : "000000000000000000000000000000000000000000000002859b1c301496c1f3",
                            "previousblockhash" : "3c547690a4960e2c4c10b4868125cbbac3b345a333ef7f225a9f8247f2856ad3",
                            "nextblockhash" : "7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c"
                        },
                        {
                            "hash" : "7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c",
                            "confirmations" : 259225,
                            "size" : 430,
                            "height" : 860878,
                            "version" : 3,
                            "merkleroot" : "6b965ec6025a45b0b9dbf9f61b985f37c52f1b81239a1018c71e5a4216040c2f",
                            "tx" : [
                                "7f5586a47e24b16e7440652009660347087cdd847cbc4447f288a5df3662fe05",
                                "f8adadaa5af9ba5df9a2e3a21fa6b7052c85e0b446bc1a1b4764417deb0d3407"
                            ],
                            "time" : 1554330514,
                            "nonce" : 0,
                            "bits" : "1b0670c2",
                            "difficulty" : 10175.51508948,
                            "chainwork" : "000000000000000000000000000000000000000000000002859b43efc033551c",
                            "previousblockhash" : "39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf",
                            "nextblockhash" : "ac55708898c88f34c76840bf28371df097359bbc9b09cc25255e3880336e7bd4"
                        }
                    ],
                    "uuid" : "b334b5ee-0585-425b-86a4-02d2fecbf67b"
                })"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\"")
                  + HelpExampleRpc("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\"")
                  + HelpExampleCli("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\" 2")
                  + HelpExampleRpc("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\" 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1 || boost::algorithm::contains(params[0].get_str(), ",")) {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2) {
        Object error;
        error.emplace_back("error", "Block hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    std::vector<std::string> blockHashes;
    const auto & hashes = params[1].get_str();
    boost::split(blockHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : blockHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Block hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetBlocks BLOCK \"302a309d6b6c4a65e4b9ff06c7ea81bb17e985d00abdb01978ace62cc5e18421,"
                                        "175d2a428b5649c2a4732113e7f348ba22a0e69cc0a87631449d1d77cd6e1b04,"
                                        "34989eca8ed66ff53631294519e147a12f4860123b4bdba36feac6da8db492ab\"");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlocks(uuid, currency, consensus, blockHashes);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetTransactions(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTransactions",
                "\nList of blocks in json format for the specified block hashes.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"txhashes", RPCArg::Type::STR, RPCArg::Optional::NO, "Comma delimited list of transaction hashes, example: txhash1,txhash2,txhash3"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                R"({
                    "reply" : [
                        {
                            "hex" : "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff06034372010101ffffffff0100000000000000000000000000",
                            "txid" : "6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7",
                            "version" : 1,
                            "locktime" : 0,
                            "vin" : [
                                {
                                    "coinbase" : "034372010101",
                                    "sequence" : 4294967295
                                }
                            ],
                            "vout" : [
                                {
                                    "value" : 0.00000000,
                                    "n" : 0,
                                    "scriptPubKey" : {
                                        "asm" : "",
                                        "hex" : "",
                                        "type" : "nonstandard"
                                    }
                                }
                            ],
                            "blockhash" : "04f6f17d2f20ab122e9277d595d32c751e544755ee662ec5351663c6593118e6",
                            "confirmations" : 1025319,
                            "time" : 1507944959,
                            "blocktime" : 1507944959
                        },
                        {
                            "hex" : "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff06034472010101ffffffff0100000000000000000000000000",
                            "txid" : "379ba9539afbd08b5ce4351be287addd090dce57ac3429e5ed600860714df05a",
                            "version" : 1,
                            "locktime" : 0,
                            "vin" : [
                                {
                                    "coinbase" : "034472010101",
                                    "sequence" : 4294967295
                                }
                            ],
                            "vout" : [
                                {
                                    "value" : 0.00000000,
                                    "n" : 0,
                                    "scriptPubKey" : {
                                        "asm" : "",
                                        "hex" : "",
                                        "type" : "nonstandard"
                                    }
                                }
                            ],
                            "blockhash" : "56675a208311e44d8eaaed8b1dedd240a07de554e79a1c7bcb43a7728e7d6c7f",
                            "confirmations" : 1025322,
                            "time" : 1507945040,
                            "blocktime" : 1507945040
                        }
                    ],
                    "uuid" : "b334b5ee-0585-425b-86a4-02d2fecbf67b"
                })"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\"")
                  + HelpExampleRpc("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\"")
                  + HelpExampleCli("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\" 2")
                  + HelpExampleRpc("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\" 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    std::vector<std::string> txHashes;
    const auto & hashes = params[1].get_str();
    boost::split(txHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : txHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Transaction hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetTransactions BLOCK \"24ff5506a30772acfb65012f1b3309d62786bc386be3b6ea853a798a71c010c8,"
                                        "24b6bcb44f045d7a4cf8cd47c94a14cc609352851ea973f8a47b20578391629f,"
                                        "66a5809c7090456965fe30280b88f69943e620894e1c4538a724ed9a89c769be\"");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }
    
    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransactions(uuid, currency, consensus, txHashes);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetTxBloomFilter(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTxBloomFilter",
                "\nLists transactions in json format matching bloom filter starting with block number.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"filter", RPCArg::Type::STR, RPCArg::Optional::NO, "Bloom filter"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTxBloomFilter", "BLOCK 0x0000000018")
                  + HelpExampleRpc("xrGetTxBloomFilter", "BLOCK 0x0000000018")
                  + HelpExampleCli("xrGetTxBloomFilter", "BLOCK 0x0000000018 2")
                  + HelpExampleRpc("xrGetTxBloomFilter", "BLOCK 0x0000000018 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Filter not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int number{0};
    if (params.size() >= 3)
        number = params[2].get_int();

    int consensus{0};
    if (params.size() >= 4) {
        consensus = params[3].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }
    
    const auto & currency = params[0].get_str();
    const auto & filter = params[1].get_str();
    std::string uuid;
    const auto reply = xrouter::App::instance().getTransactionsBloomFilter(uuid, currency, consensus, filter, number);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGenerateBloomFilter(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGenerateBloomFilter",
                "\nGenerates a bloom filter for given base58 addresses.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"addresses", RPCArg::Type::STR, RPCArg::Optional::NO, "Comma delimited list of base58 address to generate a bloom filter for, example: address1,address2"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus). To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGenerateBloomFilter", "BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX")
                  + HelpExampleRpc("xrGenerateBloomFilter", "BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX")
                  + HelpExampleCli("xrGenerateBloomFilter", "BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX 2")
                  + HelpExampleRpc("xrGenerateBloomFilter", "BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();
    
    Object result;

    if (params.empty()) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(result);
    }
    
    CBloomFilter f(10 * static_cast<unsigned int>(params.size()), 0.1, 5, 0);

    Array invalid;

    for (const auto & param : params) {
        std::vector<unsigned char> data;
        const std::string & addr = param.get_str();
        if (!DecodeBase58(addr, data)) { // Try parsing pubkey
            data.clear();
            data = ParseHex(addr);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                invalid.push_back(Value(addr));
                continue;
            }
            f.insert(data);
        } else {
            // This is a bitcoin address
            const auto keyid = CKeyID(uint160(data));
            data = std::vector<unsigned char>(keyid.begin(), keyid.end());
            f.insert(data);
        }
    }
    
    if (!invalid.empty()) {
        result.emplace_back("skipped-invalid", invalid);
    }
    
    if (invalid.size() == params.size()) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(result);
    }

    result.emplace_back("bloomfilter", HexStr(f.data()));

    Object reply;
    reply.emplace_back("result", result);

    const std::string & uuid = xrouter::generateUUID();
    return uret_xr(xrouter::form_reply(uuid, write_string(Value(reply), false)));
}

static UniValue xrService(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrService",
                "\nSend request to the service with the specified name. XRouter services are "
                "custom plugins that XRouter node operators advertise on the network. Anyone "
                "capable of running a service node can create or install custom XRouter "
                "services or plugins and provide access to them for free or for a fee. This "
                "is a great way to earn fees for your custom plugin.\n",
                {
                    {"service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"parameters", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Array of parameters. Refer to the plugin documentation for "
                                                                                 "parameter requirements. Information about a custom XRouter "
                                                                                 "service can be viewed in the plugin configuration. Use "
                                                                                 "xrConnect to find a node with the plugin, then use "
                                                                                 "xrConnectedNodes to review plugin information."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrService", "xrs::GetBestBlockHashBTC")
                  + HelpExampleRpc("xrService", "xrs::GetBestBlockHashBTC")
                  + HelpExampleCli("xrService", "xrs::SomeXCloudPlugin param1 param2")
                  + HelpExampleRpc("xrService", "xrs::SomeXCloudPlugin param1 param2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }
    
    const std::string & service = params[0].get_str();
    std::vector<std::string> call_params;
    for (unsigned int i = 1; i < params.size(); i++)
        call_params.push_back(params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, 0, call_params);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrServiceConsensus(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrServiceConsensus",
                "\nSend requests to number of nodes indicated by node_count that are hosting the "
                "service. If all nodes cannot be found an error is returned. XRouter services are "
                "custom plugins that XRouter node operators advertise on the network. Anyone "
                "capable of running a service node can create or install custom XRouter services "
                "or plugins and provide access to them for free or for a fee. This is a great way "
                "to earn fees for your custom plugin.\n",
                {
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of XRouter nodes to query (default=1) "
                                                                             "The most common reply will be returned (i.e. the reply "
                                                                             "with the most consensus). To see all reply results use "
                                                                             "xrGetReply uuid."},
                    {"service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to query"},
                    {"parameters", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Array of parameters. Refer to the plugin documentation for "
                                                                                 "parameter requirements. Information about a custom XRouter "
                                                                                 "service can be viewed in the plugin configuration. Use "
                                                                                 "xrConnect to find a node with the plugin, then use "
                                                                                 "xrConnectedNodes to review plugin information."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrServiceConsensus", "2 xrs::GetBestBlockHashBTC")
                  + HelpExampleRpc("xrServiceConsensus", "2 xrs::GetBestBlockHashBTC")
                  + HelpExampleCli("xrServiceConsensus", "3 xrs::SomeXCloudPlugin param1 param2")
                  + HelpExampleRpc("xrServiceConsensus", "3 xrs::SomeXCloudPlugin param1 param2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Consensus number not specified, must specify at least 1");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const auto & consensus = params[0].get_int();
    const auto & service = params[1].get_str();

    if (consensus < 1) {
        Object error;
        error.emplace_back("error", "Consensus must be at least 1");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    std::vector<std::string> call_params;
    for (unsigned int i = 2; i < params.size(); i++)
        call_params.push_back(params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, consensus, call_params);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrSendTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrSendTransaction",
                "\nSend a signed transaction to any supported blockchain network. "
                "This is useful if you want to send transactions to a blockchain without having "
                "to download the entire chain, or if you are running a lite-wallet/multi-wallet.\n",
                {
                    {"currency", RPCArg::Type::STR, RPCArg::Optional::NO, "Blockchain to submit transaction to"},
                    {"signed_transaction_hex", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Signed raw transaction hex string"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                             "The most common reply will be returned (i.e. the reply "
                                                                             "with the most consensus). To see all reply results use "
                                                                             "xrGetReply uuid."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrSendTransaction", "BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000")
                  + HelpExampleRpc("xrSendTransaction", "BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction data not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }
    
    std::string currency = params[0].get_str();
    std::string transaction = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().sendTransaction(uuid, currency, consensus, transaction);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrGetReply(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetReply",
                "\nReturns all the replies from XRouter nodes matching the specified query uuid. "
                "Useful to lookup previous calls without having to request from the XRouter"
                "network.\n",
                {
                    {"uuid", RPCArg::Type::STR, RPCArg::Optional::NO, "Reply id"},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetReply", "cc25f823-06a9-48e7-8245-f04991c09d6a")
                  + HelpExampleRpc("xrGetReply", "cc25f823-06a9-48e7-8245-f04991c09d6a")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Please specify the uuid");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }
    
    std::string uuid = params[0].get_str();
    Object result;
    std::string reply = xrouter::App::instance().getReply(uuid);
    return uret_xr(xrouter::form_reply(uuid, reply));
}

static UniValue xrShowConfigs(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrShowConfigs",
                "\nShows the raw configurations received from XRouter nodes.\n",
                {},
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrShowConfigs", "")
                  + HelpExampleRpc("xrShowConfigs", "")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();
    
    Object result;
    std::string reply = xrouter::App::instance().printConfigs();
    return reply;
}

static UniValue xrReloadConfigs(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrReloadConfigs",
                "\nReloads the xrouter.conf and all associated plugin configs. If a plugin conf is changed while "
                "the client is running call this to apply those changes.\n",
                {},
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrReloadConfigs", "")
                  + HelpExampleRpc("xrReloadConfigs", "")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();
    
    Object result;
    xrouter::App::instance().reloadConfigs();
    return true;
}

static UniValue xrStatus(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrStatus",
                "\nPrints your XRouter node configuration.\n",
                {},
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrStatus", "")
                  + HelpExampleRpc("xrStatus", "")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();
    
    Object result;
    std::string reply = xrouter::App::instance().getStatus();
    return reply;
}

static UniValue xrConnectedNodes(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrConnectedNodes",
                "\nLists all the data about current and previously connected nodes. This information includes "
                "supported blockchains, services, and fee schedules.\n",
                {},
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrConnectedNodes", "")
                  + HelpExampleRpc("xrConnectedNodes", "")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (!params.empty()) {
        Object error;
        error.emplace_back("error", "This call does not support parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    const auto configs = app.getNodeConfigs();

    Array data;
    app.snodeConfigJSON(configs, data);

    return uret_xr(xrouter::form_reply(uuid, data));
}

static UniValue xrConnect(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrConnect",
                "\nConnects to XRouter nodes with the specified service and downloads their configs. "
                "This command is useful to determine how much nodes are charging for services. It's "
                "also useful to \"warm up\" connections. By connecting to nodes immediately before "
                "making a large request it can speed up the reponse time (since those connections "
                "will be open). However, XRouter nodes do close inactive connections after 15 seconds "
                "so keep that in mind. After connecting call xrConnectedNodes to display information "
                "about these XRouter nodes.\n",
                {
                    {"fully_qualified_service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Service name including the namespace. Must specify "
                                                                                              "xr:: for SPV commands and xrs:: for plugin commands. "
                                                                                              "Example: xr::BLOCK or xrs::GetBestBlockHashBTC"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query (default=1) "
                                                                                  "The most common reply will be returned (i.e. the reply "
                                                                                  "with the most consensus. To see all reply results use "
                                                                                  "xrGetReply uuid."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrConnect", "xr::BTC")
                  + HelpExampleRpc("xrConnect", "xr::BTC")
                  + HelpExampleCli("xrConnect", "xr::BTC 2")
                  + HelpExampleRpc("xrConnect", "xr::BTC 2")
                  + HelpExampleCli("xrConnect", "xrs::CustomXCloudPlugin 2")
                  + HelpExampleRpc("xrConnect", "xrs::CustomXCloudPlugin 2")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (params.size() < 1) {
        Object error;
        error.emplace_back("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const auto & service = params[0].get_str();
    if (service.empty()) {
        Object error;
        error.emplace_back("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    int nodeCount{1};
    if (params.size() > 1) {
        nodeCount = params[1].get_int();
        if (nodeCount < 1) {
            Object error;
            error.emplace_back("error", "nodeCount must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return uret_xr(error);
        }
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    std::map<std::string, xrouter::XRouterSettingsPtr> configs;

    Array data;

    try {
        uint32_t found{0};
        configs = app.xrConnect(service, nodeCount, found);
        if (configs.size() < nodeCount) {
            Object error;
            error.emplace_back("error", "Failed to connect to nodes, found " +
                                        std::to_string(found > configs.size() ? found : configs.size()) +
                                        " expected " + std::to_string(nodeCount));
            error.emplace_back("code", xrouter::NOT_ENOUGH_NODES);
            return uret_xr(error);
        }
        app.snodeConfigJSON(configs, data);
    } catch (std::exception & e) {
        Object error;
        error.emplace_back("error", e.what());
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    } catch (xrouter::XRouterError & e) {
        Object error;
        error.emplace_back("error", e.msg);
        error.emplace_back("code", e.code);
        return uret_xr(error);
    }

    return uret_xr(xrouter::form_reply(uuid, data));
}

static UniValue xrGetNetworkServices(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetNetworkServices",
                "\nLists all the XRouter services on the network.\n",
                {},
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetNetworkServices", "")
                  + HelpExampleRpc("xrGetNetworkServices", "")
                },
            }.ToString());
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();

    if (!params.empty()) {
        Object error;
        error.emplace_back("error", "This call does not support parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return uret_xr(error);
    }

    const std::string & uuid = xrouter::generateUUID();

    std::regex rwallet("^"+xrouter::xr+"::.*?$"); // match spv wallets
    std::regex rservice("^"+xrouter::xrs+"::.*?$"); // match services
    std::smatch m;

    std::map<std::string, int> counts;
    std::set<std::string> spvwallets;
    std::set<std::string> services;
    for (const auto & item : xbridge::App::allServices()) {
        const auto & xwallet = item.second;
        const auto & xservices = xwallet.services();
        for (const auto & s : xservices) {
            if (std::regex_match(s, m, rwallet)) {
                spvwallets.insert(s);
                counts[s] += 1;
            } else if (std::regex_match(s, m, rservice)) {
                services.insert(s);
                counts[s] += 1;
            }
        }
    }

    Array jspv{spvwallets.begin(), spvwallets.end()};
    Array jxr{services.begin(), services.end()};
    Object jnodes;

    Object data;
    data.emplace_back("spvwallets", jspv);
    data.emplace_back("services", jxr);
    for (const auto & item : counts) // show number of nodes with each service
        jnodes.emplace_back(item.first, item.second);
    data.emplace_back("nodecounts", jnodes);

    return uret_xr(xrouter::form_reply(uuid, data));
}

static UniValue xrTest(const JSONRPCRequest& request)
{
    if (request.fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }
    Value js; json_spirit::read_string(request.params.write(), js); Array params = js.get_array();
    
    xrouter::App::instance().runTests();
    return uret_xr(true);
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category        name                               actor (function)                 argNames
  //  --------------- ---------------------------------- -------------------------------- ----------
    { "xrouter",      "xrDecodeRawTransaction",          &xrDecodeRawTransaction,         {} },
    { "xrouter",      "xrGetBlockCount",                 &xrGetBlockCount,                {} },
    { "xrouter",      "xrGetBlockHash",                  &xrGetBlockHash,                 {} },
    { "xrouter",      "xrGetBlock",                      &xrGetBlock,                     {} },
    { "xrouter",      "xrGetBlocks",                     &xrGetBlocks,                    {} },
    { "xrouter",      "xrGetTransaction",                &xrGetTransaction,               {} },
    { "xrouter",      "xrGetTransactions",               &xrGetTransactions,              {} },
    { "xrouter",      "xrSendTransaction",               &xrSendTransaction,              {} },

    { "xrouter",      "xrConnect",                       &xrConnect,                      {} },
    { "xrouter",      "xrConnectedNodes",                &xrConnectedNodes,               {} },
    { "xrouter",      "xrGenerateBloomFilter",           &xrGenerateBloomFilter,          {} },
    { "xrouter",      "xrGetNetworkServices",            &xrGetNetworkServices,           {} },
    { "xrouter",      "xrGetReply",                      &xrGetReply,                     {} },
    { "xrouter",      "xrGetTxBloomFilter",              &xrGetTxBloomFilter,             {} },
    { "xrouter",      "xrReloadConfigs",                 &xrReloadConfigs,                {} },
    { "xrouter",      "xrService",                       &xrService,                      {} },
    { "xrouter",      "xrServiceConsensus",              &xrServiceConsensus,             {} },
    { "xrouter",      "xrShowConfigs",                   &xrShowConfigs,                  {} },
    { "xrouter",      "xrStatus",                        &xrStatus,                       {} },
    { "xrouter",      "xrTest",                          &xrTest,                         {} },
};
// clang-format on

void RegisterXRouterRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}