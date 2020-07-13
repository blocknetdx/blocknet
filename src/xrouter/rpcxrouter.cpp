// Copyright (c) 2018-2020 The Blocknet developers
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
#include <netmessagemaker.h>
#include <regex>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

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

static UniValue uret_xr(const std::string & str) {
    UniValue uv;
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
                "\nReturns the block count for the longest chain in the specified blockchain.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": 611620,
      "uuid": "34d0998e-a950-4fd8-b1d6-7571c83abb50"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | int  | The latest block number of the specified blockchain. If
                 |      | using a "node_count" greater than 1, this returns the
                 |      | most common reply. Use xrGetReply to view each nodes
                 |      | individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlockCount", "BTC")
                  + HelpExampleRpc("xrGetBlockCount", "\"BTC\"")
                  + HelpExampleCli("xrGetBlockCount", "BTC 2")
                  + HelpExampleRpc("xrGetBlockCount", "\"BTC\", 2")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    int consensus{0};
    if (request.params.size() >= 2) {
        consensus = request.params[1].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockCount(uuid, currency, consensus);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetBlockHash(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlockHash",
                "\nReturns the block hash of the specified block and blockchain.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"block_number", RPCArg::Type::STR, RPCArg::Optional::NO, "The block number or hex."},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
      "uuid": "3c84d025-8a03-4b64-848f-99892fe481ff"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | int  | The latest block hash of the specified blockchain. If
                 |      | using a "node_count" greater than 1, this returns the
                 |      | most common reply. Use xrGetReply to view each nodes
                 |      | individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlockHash", "BTC 1")
                  + HelpExampleRpc("xrGetBlockHash", "\"BTC\", \"1\"")
                  + HelpExampleCli("xrGetBlockHash", "BTC 1 2")
                  + HelpExampleRpc("xrGetBlockHash", "\"BTC\", \"1\", 2")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "block_number not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    const std::string & currency = request.params[0].get_str();
    const std::string & blocknum = request.params[1].get_str();
    unsigned int block;
    if (boost::starts_with(blocknum, "0x")) {
        if (!xrouter::hextodec(blocknum, block)) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "Bad block number " + blocknum);
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    } else {
        try {
            block = boost::lexical_cast<unsigned int>(blocknum);
        } catch (boost::bad_lexical_cast) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "Bad block number " + blocknum);
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockHash(uuid, currency, consensus, block);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetBlock(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlock",
                "\nReturns the block data for the specified block hash and blockchain in JSON format.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"block_hash", RPCArg::Type::STR, RPCArg::Optional::NO, "The block hash."},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": {
        "hash": "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
        "confirmations": 611649,
        "strippedsize": 215,
        "size": 215,
        "weight": 860,
        "height": 1,
        "version": 1,
        "versionHex": "00000001",
        "merkleroot": "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098",
        "tx": [
          "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098"
        ],
        "time": 1231469665,
        "mediantime": 1231469665,
        "nonce": 2573394689,
        "bits": "1d00ffff",
        "difficulty": 1,
        "chainwork": "0000000000000000000000000000000000000000000000000000000200020002",
        "nTx": 1,
        "previousblockhash": "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f",
        "nextblockhash": "000000006a625f06636b8bb6ac7b960a8d03705d1ace08b1a19da3fdcc99ddbd"
      },
      "uuid": "24D5B4EB-8BB2-4FF6-8C82-9D52726494CF"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | obj  | An object containing the block data for the specified
                 |      | block hash and blockchain. If using a "node_count"
                 |      | greater than 1, this returns the most common reply. Use
                 |      | xrGetReply to view each nodes individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlock", "BTC 00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048")
                  + HelpExampleRpc("xrGetBlock", "\"BTC\", \"00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048\"")
                  + HelpExampleCli("xrGetBlock", "BTC 00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048 2")
                  + HelpExampleRpc("xrGetBlock", "\"BTC\", \"00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048\", 2")
                },
            }.ToString());

    if (request.params.empty() || (request.params.size() == 1 && xrouter::is_hash(request.params[0].get_str()))) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "block_hash not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & hash = request.params[1].get_str();
    if (!xrouter::is_hash(hash)) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "block_hash is bad");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlock(uuid, currency, consensus, hash);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTransaction",
                "\nReturns the transaction data for the specified transaction ID (hash) and blockchain.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"tx_id", RPCArg::Type::STR, RPCArg::Optional::NO, "The transaction ID (hash)."},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": {
        "txid": "97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e",
        "version": 1,
        "locktime": 0,
        "vin": [
          {
            "txid": "4bee60384e3ada36feacb0e2c71a26846c67a5f7faeb9e40725821e446c3d288",
            "vout": 1,
            "scriptSig": {
              "asm": "304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01",
              "hex": "47304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01"
            },
            "sequence": 4294967295
          }
        ],
        "vout": [
          {
            "value": 0.00000000,
            "n": 0,
            "scriptPubKey": {
              "asm": "",
              "hex": "",
              "type": "nonstandard"
            }
          },
          {
            "value": 901.19451902,
            "n": 1,
            "scriptPubKey": {
              "asm": "03f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855 OP_CHECKSIG",
              "hex": "2103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac",
              "reqSigs": 1,
              "type": "pubkey",
              "addresses": [
                "BeDQzBfouG1WxE9ArLc43uibF8Gk4HYv3j"
              ]
            }
          },
          {
            "value": 0.70000000,
            "n": 2,
            "scriptPubKey": {
              "asm": "OP_DUP OP_HASH160 6499ceebaa0d586c95271575780b3f9590f9f8ca OP_EQUALVERIFY OP_CHECKSIG",
              "hex": "76a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac",
              "reqSigs": 1,
              "type": "pubkeyhash",
              "addresses": [
                "BcxcRo1L5Pk5e2UuabzAJcbs9NKzsMTsuu"
              ]
            }
          }
        ]
      },
      "uuid": "b334b5ee-0585-425b-86a4-02d2fecbf67b"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | obj  | An object containing the transaction data for the
                 |      | specified transaction ID and blockchain. If using a
                 |      | "node_count" greater than 1, this returns the most
                 |      | common reply. Use xrGetReply to view each nodes
                 |      | individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e")
                  + HelpExampleRpc("xrGetTransaction", "\"BLOCK\", \"97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e\"")
                  + HelpExampleCli("xrGetTransaction", "BLOCK 97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e 2")
                  + HelpExampleRpc("xrGetTransaction", "\"BLOCK\", \"97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e\", 2")
                },
            }.ToString());

    if (request.params.empty() || (request.params.size() == 1 && xrouter::is_hash(request.params[0].get_str()))) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "tx_id not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & hash = request.params[1].get_str();
    if (!xrouter::is_hash(hash)) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "tx_id is bad");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransaction(uuid, currency, consensus, hash);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrDecodeRawTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrDecodeRawTransaction",
                "\nDecodes the specified transaction HEX and returns the transaction data in JSON format.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"tx_hex", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The raw transaction HEX to decode."},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": {
        "txid": "97578005939f0c25afd6358772ad1ff90f6e2e089d552c7acb9c10c56d983c1e",
        "version": 1,
        "locktime": 0,
        "vin": [
          {
            "txid": "4bee60384e3ada36feacb0e2c71a26846c67a5f7faeb9e40725821e446c3d288",
            "vout": 1,
            "scriptSig": {
              "asm": "304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01",
              "hex": "47304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01"
            },
            "sequence": 4294967295
          }
        ],
        "vout": [
          {
            "value": 0.00000000,
            "n": 0,
            "scriptPubKey": {
              "asm": "",
              "hex": "",
              "type": "nonstandard"
            }
          },
          {
            "value": 901.19451902,
            "n": 1,
            "scriptPubKey": {
              "asm": "03f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855 OP_CHECKSIG",
              "hex": "2103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac",
              "reqSigs": 1,
              "type": "pubkey",
              "addresses": [
                "BeDQzBfouG1WxE9ArLc43uibF8Gk4HYv3j"
              ]
            }
          },
          {
            "value": 0.70000000,
            "n": 2,
            "scriptPubKey": {
              "asm": "OP_DUP OP_HASH160 6499ceebaa0d586c95271575780b3f9590f9f8ca OP_EQUALVERIFY OP_CHECKSIG",
              "hex": "76a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac",
              "reqSigs": 1,
              "type": "pubkeyhash",
              "addresses": [
                "BcxcRo1L5Pk5e2UuabzAJcbs9NKzsMTsuu"
              ]
            }
          }
        ]
      },
      "uuid": "b334b5ee-0585-425b-86a4-02d2fecbf67b"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | obj  | An object containing the decoded transaction data. If
                 |      | using a "node_count" greater than 1, this returns the
                 |      | most common reply. Use xrGetReply to view each nodes
                 |      | individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000")
                  + HelpExampleRpc("xrDecodeRawTransaction", "\"BLOCK\", \"010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000\"")
                  + HelpExampleCli("xrDecodeRawTransaction", "BLOCK 010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000 2")
                  + HelpExampleRpc("xrDecodeRawTransaction", "\"BLOCK\", \"010000000188d2c346e4215872409eebfaf7a5676c84261ac7e2b0acfe36da3a4e3860ee4b010000004847304402201aa718585891f0e15ef8b9bc642f9d283b66beab42fd2d63e49419d2b9f5b4fb02205f37a09acfaa7b537ddf5984e512025907628279ae6b8d0a2a5c1a256c7b95dd01ffffffff03000000000000000000feb489fb14000000232103f5b9bb3158fc036463f0fbd6fc3f7de66cd89add440d1e849c2feb572bd23855ac801d2c04000000001976a9146499ceebaa0d586c95271575780b3f9590f9f8ca88ac00000000\", 2")
                },
            }.ToString());

    if (request.params.empty() || (request.params.size() == 1 && xrouter::is_hex(request.params[0].get_str()))) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "tx_hex not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & hex = request.params[1].get_str();
    if (hex.empty() || !xrouter::is_hex(hex)) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "tx_hex is bad");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().decodeRawTransaction(uuid, currency, consensus, hex);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetBlocks(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetBlocks",
                "\nReturns an array of block data in JSON format for the specified block hashes and blockchain. "
                "Currently the maximum request is 50 blocks, although a node may set this limit to less.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"block_hashes", RPCArg::Type::STR, RPCArg::Optional::NO, "A comma-delimited string of block hashes for the blocks of interest. "
                                                                              "The hashes must be separated by a comma with no spaces. "
                                                                              "Example: \"blockhash1,blockhash2,blockhash3\""},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": [
        {
          "hash": "39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf",
          "confirmations": 259225,
          "size": 429,
          "height": 860877,
          "version": 3,
          "merkleroot": "0ad8234849f7cda3bc88ea55cba58a93c9f903db7fd76ce9aae1787fb6b36465",
          "tx": [
            "5f7aafd0a1c0835b2602305dacba03c8f34f942d9f3d624456d551d730d55207",
            "f046ae548f88c166259b3f9a37088abc44b7aee8cacc0efe66fad34f31173af2"
          ],
          "time": 1554330467,
          "nonce": 0,
          "bits": "1b068aa1",
          "difficulty": 10018.31506514,
          "chainwork": "000000000000000000000000000000000000000000000002859b1c301496c1f3",
          "previousblockhash": "3c547690a4960e2c4c10b4868125cbbac3b345a333ef7f225a9f8247f2856ad3",
          "nextblockhash": "7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c"
        },
        {
          "hash": "7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c",
          "confirmations": 259225,
          "size": 430,
          "height": 860878,
          "version": 3,
          "merkleroot": "6b965ec6025a45b0b9dbf9f61b985f37c52f1b81239a1018c71e5a4216040c2f",
          "tx": [
            "7f5586a47e24b16e7440652009660347087cdd847cbc4447f288a5df3662fe05",
            "f8adadaa5af9ba5df9a2e3a21fa6b7052c85e0b446bc1a1b4764417deb0d3407"
          ],
          "time": 1554330514,
          "nonce": 0,
          "bits": "1b0670c2",
          "difficulty": 10175.51508948,
          "chainwork": "000000000000000000000000000000000000000000000002859b43efc033551c",
          "previousblockhash": "39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf",
          "nextblockhash": "ac55708898c88f34c76840bf28371df097359bbc9b09cc25255e3880336e7bd4"
        }
      ],
      "uuid": "b334b5ee-0585-425b-86a4-02d2fecbf67b"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | arr  | An array containing objects of the block data for each
                 |      | requested block on the specified blockchain. If using a
                 |      | "node_count" greater than 1, this returns the most
                 |      | common reply. Use xrGetReply to view each nodes
                 |      | individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\"")
                  + HelpExampleRpc("xrGetBlocks", "\"BLOCK\", \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\"")
                  + HelpExampleCli("xrGetBlocks", "BLOCK \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\" 2")
                  + HelpExampleRpc("xrGetBlocks", "\"BLOCK\", \"39e11e62d89cfcfd2b0800f7e9b4bd439fa44a7d7aa111e1e7a8b235d848eadf,7b41ea6a8bf0ed93fd4f3a6a67a558941634400e9eaa51676d5af5077a01760c\", 2")
                },
            }.ToString());

    if (request.params.size() < 1 || boost::algorithm::contains(request.params[0].get_str(), ",")) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "block_hashes not specified (comma-delimited list)");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::vector<std::string> blockHashes;
    const auto & hashes = request.params[1].get_str();
    boost::split(blockHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : blockHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "block_hashes must be specified in a comma-delimited string with no spaces.\n"
                                        "Example: xrGetBlocks BLOCK \"302a309d6b6c4a65e4b9ff06c7ea81bb17e985d00abdb01978ace62cc5e18421,"
                                        "175d2a428b5649c2a4732113e7f348ba22a0e69cc0a87631449d1d77cd6e1b04,"
                                        "34989eca8ed66ff53631294519e147a12f4860123b4bdba36feac6da8db492ab\"");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlocks(uuid, currency, consensus, blockHashes);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetTransactions(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTransactions",
                "\nReturns an array of transaction data for the specified transaction IDs (hashes) and "
                "blockchain. Currently the maximum request is 50 transactions, although a node may set "
                "this limit to less.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"tx_ids", RPCArg::Type::STR, RPCArg::Optional::NO, "A comma-delimited string of transaction IDs (hashes) for the transactions "
                                                                        "of interest. The hashes must be separated by a comma with no spaces. "
                                                                        "Example: \"txhash1,txhash2,txhash3\""},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": [
        {
          "hex": "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff06034372010101ffffffff0100000000000000000000000000",
          "txid": "6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7",
          "version": 1,
          "locktime": 0,
          "vin": [
            {
              "coinbase": "034372010101",
              "sequence": 4294967295
            }
          ],
          "vout": [
            {
              "value": 0.00000000,
              "n": 0,
              "scriptPubKey": {
                "asm": "",
                "hex": "",
                "type": "nonstandard"
              }
            }
          ],
          "blockhash": "04f6f17d2f20ab122e9277d595d32c751e544755ee662ec5351663c6593118e6",
          "confirmations": 1025319,
          "time": 1507944959,
          "blocktime": 1507944959
        },
        {
          "hex": "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff06034472010101ffffffff0100000000000000000000000000",
          "txid": "379ba9539afbd08b5ce4351be287addd090dce57ac3429e5ed600860714df05a",
          "version": 1,
          "locktime": 0,
          "vin": [
            {
              "coinbase": "034472010101",
              "sequence": 4294967295
            }
          ],
          "vout": [
            {
              "value": 0.00000000,
              "n": 0,
              "scriptPubKey": {
                "asm": "",
                "hex": "",
                "type": "nonstandard"
              }
            }
          ],
          "blockhash": "56675a208311e44d8eaaed8b1dedd240a07de554e79a1c7bcb43a7728e7d6c7f",
          "confirmations": 1025322,
          "time": 1507945040,
          "blocktime": 1507945040
        }
      ],
      "uuid": "b334b5ee-0585-425b-86a4-02d2fecbf67b"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | arr  | An array containing objects with the transaction data
                 |      | for each requested transaction on the specified
                 |      | blockchain. If using a "node_count" greater than 1,
                 |      | this returns the most common reply. Use xrGetReply to
                 |      | view each nodes individual response.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\"")
                  + HelpExampleRpc("xrGetTransactions", "\"BLOCK\", \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\"")
                  + HelpExampleCli("xrGetTransactions", "BLOCK \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\" 2")
                  + HelpExampleRpc("xrGetTransactions", "\"BLOCK\", \"6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\", 2")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (request.params.size() < 2)
    {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "tx_ids not specified (comma-delimited list)");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::vector<std::string> txHashes;
    const auto & hashes = request.params[1].get_str();
    boost::split(txHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : txHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "tx_ids must be specified in a comma-delimited string with no spaces.\n"
                                        "Example: xrGetTransactions BLOCK \"24ff5506a30772acfb65012f1b3309d62786bc386be3b6ea853a798a71c010c8,"
                                        "24b6bcb44f045d7a4cf8cd47c94a14cc609352851ea973f8a47b20578391629f,"
                                        "66a5809c7090456965fe30280b88f69943e620894e1c4538a724ed9a89c769be\"");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = request.params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransactions(uuid, currency, consensus, txHashes);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetTxBloomFilter(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetTxBloomFilter",
                "\nLists transactions in json format matching bloom filter starting with block number.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"filter", RPCArg::Type::STR, RPCArg::Optional::NO, "Bloom filter"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGetTxBloomFilter", "BLOCK 0x0000000018")
                  + HelpExampleRpc("xrGetTxBloomFilter", "\"BLOCK\", \"0x0000000018\"")
                  + HelpExampleCli("xrGetTxBloomFilter", "BLOCK 0x0000000018 2")
                  + HelpExampleRpc("xrGetTxBloomFilter", "\"BLOCK\", \"0x0000000018\", 2")
                },
            }.ToString());

    if (request.params.empty())
    {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2)
    {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "filter not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int number{0};
    if (request.params.size() >= 3)
        number = request.params[2].get_int();

    int consensus{0};
    if (request.params.size() >= 4) {
        consensus = request.params[3].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    const auto & currency = request.params[0].get_str();
    const auto & filter = request.params[1].get_str();
    std::string uuid;
    const auto reply = xrouter::App::instance().getTransactionsBloomFilter(uuid, currency, consensus, filter, number);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGenerateBloomFilter(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGenerateBloomFilter",
                "\nGenerates a bloom filter for given base58 addresses.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain, represented by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"addresses", RPCArg::Type::STR, RPCArg::Optional::NO, "Comma-delimited list of base58 address to generate a bloom filter for, example: address1,address2"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                "\n"
                },
                RPCExamples{
                    HelpExampleCli("xrGenerateBloomFilter", "BLOCK \"BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX\"")
                  + HelpExampleRpc("xrGenerateBloomFilter", "\"BLOCK\", \"BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX\"")
                  + HelpExampleCli("xrGenerateBloomFilter", "BLOCK \"BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX\" 2")
                  + HelpExampleRpc("xrGenerateBloomFilter", "\"BLOCK\", \"BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX\", 2")
                },
            }.ToString());

    UniValue result(UniValue::VOBJ);

    if (request.params.empty()) {
        result.pushKV("error", "No valid addresses");
        result.pushKV("code", xrouter::INVALID_PARAMETERS);
        return result;
    }
    
    CBloomFilter f(10 * static_cast<unsigned int>(request.params.size()), 0.1, 5, 0);

    UniValue invalid(UniValue::VARR);

    for (int i = 0; i < request.params.size(); ++i) {
        const auto & param = request.params[i];
        std::vector<unsigned char> data;
        const std::string & addr = param.get_str();
        if (!DecodeBase58(addr, data)) { // Try parsing pubkey
            data.clear();
            data = ParseHex(addr);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                invalid.push_back(addr);
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
        result.pushKV("skipped-invalid", invalid);
    }
    
    if (invalid.size() == request.params.size()) {
        result.pushKV("error", "No valid addresses");
        result.pushKV("code", xrouter::INVALID_PARAMETERS);
        return result;
    }

    result.pushKV("bloomfilter", HexStr(f.data()));

    UniValue reply(UniValue::VOBJ);
    reply.pushKV("result", result);

    const std::string & uuid = xrouter::generateUUID();
    return xrouter::form_reply(uuid, reply);
}

static UniValue xrService(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrService",
                "\nSend a request to the service with the specified name. XRouter services are "
                "custom plugins that XRouter node operators advertise on the network. Anyone "
                "capable of running a service node can create or install custom XRouter "
                "services or plugins and provide access to them for free or for a fee. This "
                "is a great way to earn fees for your custom plugin.\n",
                {
                    {"service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "The service name. xrGetNetworkServices can be used to browse services with the xrs:: namespace."},
                    {"parameters", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Refer to the plugin documentation for parameter requirements. "
                                                                                 "Information about a custom XRouter service can be viewed in "
                                                                                 "the plugin configuration. Use xrConnect to find a node with the "
                                                                                 "plugin, then use xrConnectedNodes to review plugin information."},
                },
                RPCResult{
                R"(
    {
      "reply": "00000000000000000005252906539bfe0145c6683b8a785aab2f36c1cdf41ff6",
      "error": null,
      "uuid": "54b6ec00-8b06-4c2c-9e56-acdff4da69fe"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | any  | The service's response data.
    error        | obj  | The native error response if an error occurred,
                 |      | otherwise a successful response will contain a null
                 |      | error.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrService", "xrs::BTCgetbestblockhash")
                  + HelpExampleRpc("xrService", "\"xrs::BTCgetbestblockhash\"")
                  + HelpExampleCli("xrService", "xrs::SomeXCloudPlugin param1 param2")
                  + HelpExampleRpc("xrService", "\"xrs::SomeXCloudPlugin\", \"param1\", \"param2\"")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Service name not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    const std::string & service = request.params[0].get_str();
    auto uv = UniValue(UniValue::VARR);
    for (unsigned int i = 1; i < request.params.size(); i++)
        uv.push_back(request.params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, 0, uv);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrServiceConsensus(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrServiceConsensus",
                "\nSend requests to a number of nodes indicated by node_count that are hosting the "
                "service. If all nodes cannot be found an error is returned. XRouter services are "
                "custom plugins that XRouter node operators advertise on the network. Anyone "
                "capable of running a service node can create or install custom XRouter services "
                "or plugins and provide access to them for free or for a fee. This is a great way "
                "to earn fees for your custom plugin.\n",
                {
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of XRouter nodes to query. The most common response will be "
                                                                            "returned as \"reply\" (i.e. the response with the most consensus). "
                                                                            "Defaults to 1 if no consensus= setting in xrouter.conf."},
                    {"service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "The service name. xrGetNetworkServices can be used to browse services with the xrs:: namespace."},
                    {"parameters", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Refer to the plugin documentation for parameter requirements. "
                                                                                 "Information about a custom XRouter service can be viewed in "
                                                                                 "the plugin configuration. Use xrConnect to find a node with the "
                                                                                 "plugin, then use xrConnectedNodes to review plugin information."},
                },
                RPCResult{
                R"(
    {
      "reply": "00000000000000000005252906539bfe0145c6683b8a785aab2f36c1cdf41ff6",
      "error": null,
      "uuid": "2ab8ea00-3b16-9a4b-e69a-b4a3dac9abfe"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | any  | The service's response data. If using a "node_count"
                 |      | greater than 1, this returns the most common reply. Use
                 |      | xrGetReply to view each nodes individual response.
    error        | obj  | The native error response if an error occurred,
                 |      | otherwise a successful response will contain a null
                 |      | error.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrServiceConsensus", "2 xrs::BTCgetbestblockhash")
                  + HelpExampleRpc("xrServiceConsensus", "2, \"xrs::BTCgetbestblockhash\"")
                  + HelpExampleCli("xrServiceConsensus", "3 xrs::SomeXCloudPlugin param1 param2")
                  + HelpExampleRpc("xrServiceConsensus", "3, \"xrs::SomeXCloudPlugin\", \"param1\", \"param2\"")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "node_count not specified, must be an integer >= 1");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2)
    {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Service name not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & consensus = request.params[0].get_int();
    const auto & service = request.params[1].get_str();

    if (consensus < 1) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "node_count must be an integer >= 1");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    auto uv = UniValue(UniValue::VARR);
    for (unsigned int i = 2; i < request.params.size(); i++)
        uv.push_back(request.params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, consensus, uv);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrSendTransaction(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrSendTransaction",
                "\nSend a signed transaction to any supported blockchain network. This is useful if you "
                "want to send transactions to a blockchain without having to download the entire chain, "
                "or if you are running a lite-wallet/multi-wallet.\n",
                {
                    {"blockchain", RPCArg::Type::STR, RPCArg::Optional::NO, "The blockchain to submit the transaction to, represented "
                                                                            "by the asset's ticker (BTC, LTC, SYS, etc.)."},
                    {"signed_tx_hex", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Signed raw transaction hex string"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be returned "
                                                                                 "as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    {
      "reply": "9f978c31840adbc4e044395f8f893cb7369c48e2ce831a90c32090bf71ae29ae",
      "uuid": "ACA0874C-C45F-4F40-94AD-794A7E18085A"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | obj  | The transaction hash of the sent transaction.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrSendTransaction", "BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000")
                  + HelpExampleRpc("xrSendTransaction", "\"BLOCK\", \"010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000\"")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "blockchain not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (request.params.size() < 2) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "signed_tx_hex not specified");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (request.params.size() >= 3) {
        consensus = request.params[2].get_int();
        if (consensus < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    std::string currency = request.params[0].get_str();
    std::string transaction = request.params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().sendTransaction(uuid, currency, consensus, transaction);
    return xrouter::form_reply(uuid, uret_xr(reply));
}

static UniValue xrGetReply(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetReply",
                "\nUsed to look up responses from previous XRouter calls without having to request them "
                "from the network. These are cached responses from your current session.\n"
                "There are no fees for this call.\n",
                {
                    {"uuid", RPCArg::Type::STR, RPCArg::Optional::NO, "Reply ID of previous call in current session."},
                },
                RPCResult{
                R"(
    {
      "allreplies": [
        {
          "reply": "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
          "nodepubkey": "02c6c79a75846fd9bb064788b03145e347fa5464558fa9030ebb009df2833369f0",
          "score": 35,
          "address": "BqCtHRHmUVqkvqD7GhXVuHchzm77cLuXs1",
          "exr": true
        },
        {
          "reply": "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
          "nodepubkey": "0370874cad6252bb94afa9a253c90122760ce2862e623b515e57bfe0697f3fc515",
          "score": 80,
          "address": "Bqshd156VexPDKELxido2S2pDvqrRzSCi4",
          "exr": true
        }
      ],
      "mostcommon": "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
      "mostcommoncount": 2,
      "uuid": "3c84d025-8a03-4b64-848f-99892fe481ff"
    }

    Key            | Type | Description
    ---------------|------|------------------------------------------------------
    allreplies     | arr  | An array of objects with responses from each node.
                   |      | This can be useful if you wanted to do your own
                   |      | analysis or filtering of the responses.
    reply          | int  | The node's response for the respective UUID call.
    nodepubkey     | str  | The node ID.
    score          | int  | The respective nodes score based on quality of
                   |      | service. A score of -200 will ban the node for a 24hr
                   |      | period. You can change the ban threshold with the
                   |      | xrouterbanscore setting in blocknet.conf.
    address        | str  | The Service Node's payment address.
    exr            | bool | Whether the Service Node is an Enterprise XRouter
                   |      | node. EXR nodes have greater throughput and service
                   |      | capabilities.
    mostcommon     | str  | The most common response (i.e. the response with the
                   |      | most consensus). This is the value returned for
                   |      | `reply` when making the originating call.
    mostcommoncount| int  | The amount of nodes that responded with the
                   |      | `mostcommon` reply.
    uuid           | str  | The response ID.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetReply", "3c84d025-8a03-4b64-848f-99892fe481ff")
                  + HelpExampleRpc("xrGetReply", "\"3c84d025-8a03-4b64-848f-99892fe481ff\"")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Please specify the uuid");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    std::string uuid = request.params[0].get_str();
    std::string reply = xrouter::App::instance().getReply(uuid);
    return reply;
}

static UniValue xrShowConfigs(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrShowConfigs",
                "\nShows the raw configurations received from XRouter nodes.\n",
                {},
                RPCResult{
                R"(
    [
      {
        "nodepubkey": "03ca15d619cf36fdc043892b12a3881dd08f2d3905e2ff399ac39cf34b28a995c7",
        "paymentaddress": "BiBbLf8wDyYcSAzsX1SNzZKrc2zZQjS2pa",
        "config": "[Main]\nwallets=BTC,ETH,LTC,BLOCK,CRW,MERGE,TRC\nmaxfee=0\n[BTC::xrGetBlocks]\n#fee=0.1\n#clientrequestlimit=-1\ndisabled=0\nfetchlimit=50\n\n\n",
        "plugins": {
        }
      },
      {
        "nodepubkey": "0252d7959e25a8f1a15b4e3e487d310211534dd71ca3316abe463d40a5cf0d67ca",
        "paymentaddress": "BXhndtvEEM5Yh9UEPzrzpBLksjZReGV6Kv",
        "config": "[Main]\nwallets=BLOCK,LTC,BTC,PIVX,MON\nmaxfee=0\nconsensus=1\ntimeout=30\npaymentaddress=BXhndtvEEM5Yh9UEPzrzpBLksjZReGV6Kv\n\n\n",
        "plugins": {
        }
      }
    ]

    Key            | Type | Description
    ---------------|------|------------------------------------------------------
    nodepubkey     | str  | The node ID.
    paymentaddress | str  | The node's payment address, may also be specific per
                   |      | command.
    config         | str  | The raw text contents of your xrouter.conf.
    plugins        | obj  | An object containing the raw configuration text
                   |      | contents for each of this node's plugins.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrShowConfigs", "")
                  + HelpExampleRpc("xrShowConfigs", "")
                },
            }.ToString());

    std::string reply = xrouter::App::instance().printConfigs();
    return reply;
}

static UniValue xrReloadConfigs(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrReloadConfigs",
                "\nReloads xrouter.conf and all associated configurations. If a configuration is changed "
                "while the client is running, use this call to apply those changes.\n",
                {},
                RPCResult{
                R"(
    true

    Type | Description
    -----|-----------------------------------------------------------------------
    bool | A confirmation that `xrouter.conf` has been reloaded.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrReloadConfigs", "")
                  + HelpExampleRpc("xrReloadConfigs", "")
                },
            }.ToString());

    return xrouter::App::instance().reloadConfigs();
}

static UniValue xrStatus(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrStatus",
                "\nReturns your XRouter configurations.\n",
                {},
                RPCResult{
                R"(
    {
      "xrouter": true,
      "servicenode": false,
      "config": "[Main]\ntimeout=30\nconsensus=1\nmaxfee=0.5"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    xrouter      | bool | Signifies XRouter activation.
                 |      | true: XRouter is enabled.
                 |      | false: XRouter is disabled.
    servicenode  | bool | Signifies if your client is a Service Node.
                 |      | true: Client is a Service Node.
                 |      | false: Client is not a Service Node.
    config       | str  | The raw text contents of your xrouter.conf.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrStatus", "")
                  + HelpExampleRpc("xrStatus", "")
                },
            }.ToString());

    std::string reply = xrouter::App::instance().getStatus();
    return reply;
}

static UniValue xrConnectedNodes(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrConnectedNodes",
                "\nLists all the data about current and previously connected nodes. This information "
                "includes supported blockchains, services, and fee schedules.\n",
                {},
                RPCResult{
                R"(
    Key            | Type  | Description
    ---------------|-------|-----------------------------------------------------
    reply          | arr   | An array of nodes, supported services, and their
                   |       | respective configs.
    nodepubkey     | str   | The node ID.
    score          | int   | The node's score based on quality of service. A
                   |       | score of -200 will ban the node for a 24hr period.
                   |       | You can change the ban threshold with the
                   |       | xrouterbanscore setting in blocknet.conf.
    banned         | bool  | Signifies if the node is currently banned.
                   |       | true: Node is banned
                   |       | false: Node is not banned
    paymentaddress | str   | The node's payment address.
    spvwallets     | arr   | An array of supported SPV wallets, represented by
                   |       | the asset's ticker.
    spvconfigs     | arr   | An array of each SPV wallet and command
                   |       | configurations.
    spvwallet      | str   | The SPV wallet that the configurations under
                   |       | "commands" pertains to.
    commands       | arr   | An array of each SPV wallet command and respective
                   |       | configs.
    command        | str   | The SPV command.
    fee            | float | The command fee, overrides the "feedefault" and
                   |       | "fees" values. This priority has already been
                   |       | accounted for in this value.
    requestlimit   | int   | The minimum time allowed between calls in
                   |       | milliseconds. A value of -1 means there is no limit.
                   |       | If you exceed this value you will be penalized and
                   |       | eventually banned by this specific node.
    paymentaddress | str   | The nodes payment address for this specific command.
    disabled       | bool  | Signifies if the node has disabled this command.
                   |       | true: Call is disabled and not supported.
                   |       | false: Call is enabled and supported.
    feedefault     | float | The node's default service fee. This fee is
                   |       | overridden by the values specified in "fees", SPV
                   |       | command configuration "fee", and XCloud service
                   |       | command configuration "fee".
    fees           | obj   | Object of SPV commands and respective fees. These
                   |       | values are overridden by the SPV wallet-specific
                   |       | configuration "fee".
    services       | obj   | Object of the node's XCloud service calls with
                   |       | respective properties.
    parameters     | str   | Information on the parameters the command takes.
    fee            | float | The service command fee. This overrides the
                   |       | "feedefault" value.
    paymentaddress | str   | The nodes payment address for this specific command.
    requestlimit   | int   | The minimum time allowed between calls in
                   |       | milliseconds. A value of -1 means there is no limit.
                   |       | If you exceed this value you will be penalized and
                   |       | eventually banned by this specific node.
    fetchlimit     | int   | The maximum number of records returned. This
                   |       | pertains to calls such as xrGetBlocks and
                   |       | xrGetTransactions where multiple records are
                   |       | returned. A value of -1 means there is no limit. A
                   |       | value of 0 means no blocks will be processed.
    timeout        | int   | The value for "timeout" you set in xrouter.conf for
                   |       | this call. Defines how long (in seconds) your client
                   |       | waits for a response from a Service Node. The
                   |       | default value is 30.
    uuid           | str   | The response ID, which can be used to view this
                   |       | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrConnectedNodes", "")
                  + HelpExampleRpc("xrConnectedNodes", "")
                },
            }.ToString());

    if (!request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "This call does not support parameters");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    const auto configs = app.getNodeConfigs();

    Array data;
    app.snodeConfigJSON(configs, data);

    return xrouter::form_reply(uuid, uret_xr(Value(data)));
}

static UniValue xrConnect(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrConnect",
                "This optional call is used to connect to XRouter nodes with the specified service. "
                "It is no longer needed to use this command prior to making a call. All node configs "
                "are now automatically downloaded. See xrConnectedNodes to review a detailed list of "
                "nodes, their service offerings, and how much they are charging for their services.\n",
                {
                    {"fully_qualified_service_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Service name including the namespace. Must specify "
                                                                                              "xr:: for SPV commands and xrs:: for plugin commands. "
                                                                                              "Example: xr::BTC or xrs::BTCgetbestblockhashBTC"},
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of XRouter nodes to query. The most common response will be "
                                                                                 "returned as \"reply\" (i.e. the response with the most consensus). "
                                                                                 "Defaults to 1 if no consensus= setting in xrouter.conf."},
                },
                RPCResult{
                R"(
    Key            | Type  | Description
    ---------------|-------|-----------------------------------------------------
    reply          | arr   | An array of nodes providing the specified service,
                   |       | along with their configs.
    nodepubkey     | str   | The node ID.
    score          | int   | The node's score based on quality of service. A
                   |       | score of -200 will ban the node for a 24hr period.
                   |       | You can change the ban threshold with the
                   |       | xrouterbanscore setting in blocknet.conf.
    banned         | bool  | Signifies if the node is currently banned.
                   |       | true: Node is banned
                   |       | false: Node is not banned
    paymentaddress | str   | The node's payment address.
    spvwallets     | arr   | An array of supported SPV wallets, represented by
                   |       | the asset's ticker.
    spvconfigs     | arr   | An array of each SPV wallet and command configs.
    spvwallet      | str   | The SPV wallet that the configurations under
                   |       | "commands" pertains to.
    commands       | arr   | An array of each SPV wallet command and respective
                   |       | configs.
    command        | str   | The SPV command.
    fee            | float | The command fee, overrides the "feedefault" and
                   |       | "fees" values.
                   |       | This priority has already been accounted for in
                   |       | this value.
    requestlimit   | int   | The minimum time allowed between calls in
                   |       | milliseconds. A value of -1 means there is no limit.
                   |       | If you exceed this value you will be penalized and
                   |       | eventually banned by this specific node.
    paymentaddress | str   | The nodes payment address for this specific command.
    disabled       | bool  | Signifies if the node has disabled this command.
                   |       | true: Call is disabled and not supported.
                   |       | false: Call is enabled and supported.
    feedefault     | float | The node's default service fee. This fee is
                   |       | overridden by the values specified in "fees", SPV
                   |       | command configuration "fee", and XCloud service
                   |       | command configuration "fee".
    fees           | obj   | Object of SPV commands and respective fees. These
                   |       | values are overridden by the SPV wallet-specific
                   |       | configuration "fee".
    services       | obj   | Object of the node's XCloud service calls with
                   |       | respective properties.
    parameters     | str   | Information on the parameters the command takes.
    fee            | float | The service command fee. This overrides the
                   |       | "feedefault" value.
    paymentaddress | str   | The nodes payment address for this specific command.
    requestlimit   | int   | The minimum time allowed between calls in
                   |       | milliseconds. A value of -1 means there is no limit.
                   |       | If you exceed this value you will be penalized and
                   |       | eventually banned by this specific node.
    fetchlimit     | int   | The maximum number of records returned. This
                   |       | pertains to calls such as xrGetBlocks and
                   |       | xrGetTransactions where multiple records are
                   |       | returned. A value of -1 means there is no limit. A
                   |       | value of 0 means no blocks will be processed.
    timeout        | int   | The value for "timeout" you set in xrouter.conf for
                   |       | this call. Defines how long (in seconds) your client
                   |       | waits for a response from a Service Node. The
                   |       | default value is 30.
    uuid           | str   | The response ID, which can be used to view this
                   |       | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrConnect", "xr::BTC")
                  + HelpExampleRpc("xrConnect", "\"xr::BTC\"")
                  + HelpExampleCli("xrConnect", "xr::BTC 2")
                  + HelpExampleRpc("xrConnect", "\"xr::BTC\", 2")
                  + HelpExampleCli("xrConnect", "xrs::CustomXCloudPlugin 2")
                  + HelpExampleRpc("xrConnect", "\"xrs::CustomXCloudPlugin\", 2")
                },
            }.ToString());

    if (request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Service not specified. Example: xrConnect xr::BTC");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & service = request.params[0].get_str();
    if (service.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int nodeCount{1};
    if (request.params.size() > 1) {
        nodeCount = request.params[1].get_int();
        if (nodeCount < 1) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "node_count must be an integer >= 1");
            error.pushKV("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    std::map<std::string, std::pair<xrouter::XRouterSettingsPtr, sn::ServiceNode::Tier>> configs;

    Array data;

    try {
        uint32_t found{0};
        configs = app.xrConnect(service, nodeCount, found);
        if (configs.size() < nodeCount) {
            UniValue error(UniValue::VOBJ);
            error.pushKV("error", "Failed to connect to nodes, found " +
                                  std::to_string(found > configs.size() ? found : configs.size()) +
                                  " expected " + std::to_string(nodeCount));
            error.pushKV("code", xrouter::NOT_ENOUGH_NODES);
            return error;
        }
        app.snodeConfigJSON(configs, data);
    } catch (xrouter::XRouterError & e) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", e.msg);
        error.pushKV("code", e.code);
        return error;
    } catch (std::exception & e) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", e.what());
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    return xrouter::form_reply(uuid, uret_xr(Value(data)));
}

static UniValue xrGetNetworkServices(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrGetNetworkServices",
                "\nList all the XRouter SPV and plugin services currently supported on the network, along "
                "with the number of nodes supporting each service. XRouter SPV calls use the xr:: namespace. "
                "XRouter services (XCloud) use the xrs:: namespace and can be called using xrService and "
                "xrServiceConsensus.\n",
                {},
                RPCResult{
                R"(
    {
      "reply": {
        "spvwallets": [ "xr::BLOCK", "xr::BTC", "xr::LTC", "xr::MNP", "xr::SYS" ],
        "services": [ "xrs::BTCgetbestblockhash", "xrs::BTCgetblockhash", "xrs::BTCgettransaction", "xrs::SYSgetbestblockhash", "xrs::SYSgetblock", "xrs::SYSgetgovernanceinfo", "xrs::SYSgetmempool", "xrs::SYSlistoffers", "xrs::SYSofferinfo", "xrs::twilio" ],
        "nodecounts": {
          "xr::BLOCK": 27,
          "xr::BTC": 13,
          "xr::LTC": 21,
          "xr::MNP": 1,
          "xr::SYS": 9,
          "xrs::BTCgetbestblockhash": 12,
          "xrs::BTCgetblockhash": 12,
          "xrs::BTCgettransaction": 5,
          "xrs::SYSgetbestblockhash": 7,
          "xrs::SYSgetblock": 6,
          "xrs::SYSgetgovernanceinfo": 4,
          "xrs::SYSgetmempool": 4,
          "xrs::SYSlistoffers": 4,
          "xrs::SYSofferinfo": 4,
          "xrs::twilio": 1
        }
      },
      "uuid": "cd408df7-0ff8-4e29-b5cf-0148af83f93a"
    }

    Key          | Type | Description
    -------------|------|--------------------------------------------------------
    reply        | obj  | An object containing information on supported services.
    spvwallets   | arr  | An array of supported SPV wallets, represented by the
                 |      | asset's ticker.
    services     | arr  | An array of supported XCloud services.
    nodecounts   | obj  | An object of supported SPV wallets and XCloud services
                 |      | with how many nodes support each.
    -- key       | str  | The SPV wallet or XCloud service with it's namespace.
    -- value     | int  | The amount of nodes supporting each respective service.
    uuid         | str  | The response ID, which can be used to view this
                 |      | response again with xrGetReply.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrGetNetworkServices", "")
                  + HelpExampleRpc("xrGetNetworkServices", "")
                },
            }.ToString());

    if (!request.params.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "This call does not support parameters");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
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

    UniValue data(UniValue::VOBJ);
    UniValue jspv(UniValue::VARR); for (const auto & w : spvwallets) jspv.push_back(w);
    UniValue jxr(UniValue::VARR); for (const auto & s : services) jxr.push_back(s);
    data.pushKV("spvwallets", jspv);
    data.pushKV("services", jxr);
    UniValue jnodes(UniValue::VOBJ);
    for (const auto & item : counts) // show number of nodes with each service
        jnodes.pushKV(item.first, item.second);
    data.pushKV("nodecounts", jnodes);

    return xrouter::form_reply(uuid, data);
}

static UniValue xrUpdateNetworkServices(const JSONRPCRequest& request) {
    if (request.fHelp)
        throw std::runtime_error(
            RPCHelpMan{"xrUpdateNetworkServices",
                "\nQueries a random set of connected peers for the latest service node list. If node_count is specified "
                "the call will query up to the specified number. By default 1 service node is queried for services. If "
                "fewer service nodes with the ability to share the list are found then only those found will be queried.\n",
                {
                    {"node_count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "1", "Query at most this number of service nodes. It's possible that fewer or none are available."},
                },
                RPCResult{
                R"(
    true

    Type | Description
    -----|-----------------------------------------------------------------------
    bool | A confirmation that the latest Service Node list was received.
                )"
                },
                RPCExamples{
                    HelpExampleCli("xrUpdateNetworkServices", "")
                  + HelpExampleRpc("xrUpdateNetworkServices", "")
                  + HelpExampleCli("xrUpdateNetworkServices", "5")
                  + HelpExampleRpc("xrUpdateNetworkServices", "5")
                },
            }.ToString());

    if (request.params.size() > 1) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Too many parameters were specified, accepts 1 or none");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (!request.params.empty() && request.params[0].get_int() <= 0) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Must specify a node_count >= 1");
        error.pushKV("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const int askcount = request.params.empty() ? 1 : request.params[0].get_int();

    if (!xrouter::App::isEnabled() || !xrouter::App::instance().isReady()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "XRouter is not enabled");
        error.pushKV("code", xrouter::UNSUPPORTED_SERVICE);
        return error;
    }

    std::set<std::string> nodes;
    g_connman->ForEachNode([&nodes](CNode *pnode) {
        if (!(pnode->nServices & NODE_SNODE_LIST))
            return; // only look for nodes with the snode list service
        if (nodes.count(pnode->GetAddrName()))
            return; // already known
        nodes.insert(pnode->GetAddrName());
    });

    if (nodes.empty()) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "No peers found with the service node list capability");
        error.pushKV("code", xrouter::UNSUPPORTED_SERVICE);
        return error;
    }

    auto randnode = [](std::set<std::string> & vnodes) -> std::string {
        auto idx = rand() & (vnodes.size()-1);
        auto it = vnodes.begin();
        std::advance(it, idx);
        const auto addr = *it;
        vnodes.erase(it);
        return addr;
    };

    // Ask up to "askcount" number of nodes
    auto copynodes = nodes;
    const auto csize = copynodes.size();
    while (!copynodes.empty() && csize - copynodes.size() < askcount) {
        try {
            const auto addr = randnode(copynodes);
            g_connman->ForEachNode([addr](CNode *pnode) {
                if (pnode->GetAddrName() != addr)
                    return;
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::SNLIST));
            });
        } catch (...) {
            break;
        }
    }

    // If fewer nodes were queried than requested, inform client
    if (csize - copynodes.size() < askcount) {
        UniValue error(UniValue::VOBJ);
        error.pushKV("error", strprintf("Queried %u nodes, expected %u", csize - copynodes.size(), askcount));
        error.pushKV("code", xrouter::BAD_REQUEST);
        return error;
    }

    return true;
}

static UniValue xrTest(const JSONRPCRequest& request)
{
    if (request.fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }

    xrouter::App::instance().runTests();
    return true;
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
    // { "xrouter",      "xrGenerateBloomFilter",           &xrGenerateBloomFilter,          {} },
    { "xrouter",      "xrGetNetworkServices",            &xrGetNetworkServices,           {} },
    { "xrouter",      "xrGetReply",                      &xrGetReply,                     {} },
    // { "xrouter",      "xrGetTxBloomFilter",              &xrGetTxBloomFilter,             {} },
    { "xrouter",      "xrReloadConfigs",                 &xrReloadConfigs,                {} },
    { "xrouter",      "xrService",                       &xrService,                      {} },
    { "xrouter",      "xrServiceConsensus",              &xrServiceConsensus,             {} },
    { "xrouter",      "xrShowConfigs",                   &xrShowConfigs,                  {} },
    { "xrouter",      "xrStatus",                        &xrStatus,                       {} },
    { "xrouter",      "xrUpdateNetworkServices",         &xrUpdateNetworkServices,        {} },
    // { "xrouter",      "xrTest",                          &xrTest,                         {} },
};
// clang-format on

void RegisterXRouterRPCCommands(CRPCTable &t)
{
    for (const auto & command : commands)
        t.appendCommand(command.name, &command);
}