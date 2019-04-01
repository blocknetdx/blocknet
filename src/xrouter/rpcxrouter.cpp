#include "xrouterapp.h"
#include "xroutererror.h"
#include "xrouterutils.h"

#include "uint256.h"
#include "bloom.h"
#include "core_io.h"

#include <exception>
#include <iostream>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
Value xrGetBlockCount(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockCount currency [node_count]\n"
                                 "Block count for the longest chain in the specified blockchain.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetBlockCount BLOCK\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetBlockCount BLOCK 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    int consensus{0};
    if (params.size() >= 2) {
        consensus = params[1].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockCount(uuid, currency, consensus);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlockHash(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockHash currency block_number [node_count]\n"
                                 "Hash of block with the specified block number.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "block_number (int) Block number\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetBlockHash BLOCK 0\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetBlockHash BLOCK 0 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block number not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    int block = params[1].get_int();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockHash(uuid, currency, consensus, block);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlock(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock currency hash [node_count]\n"
                                 "Block data in json format with the specified hash.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "hash (string) Block hash\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetBlock BLOCK b74e07f3badb51c5968096e055a68389706c17a9da625f0f5a710116a61549c5\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetBlock BLOCK b74e07f3badb51c5968096e055a68389706c17a9da625f0f5a710116a61549c5 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlock(uuid, currency, consensus, params[1].get_str());
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransaction currency txid [node_count]\n"
                                 "Transaction data in json format with the specified transaction id.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "txid (string) Transaction id\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetTransaction BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetTransaction BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Tx hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransaction(uuid, currency, consensus, params[1].get_str());
    return xrouter::form_reply(uuid, reply);
}

Value xrDecodeRawTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrDecodeRawTransaction currency hex [node_count]\n"
                                 "Decodes the specified transaction hex and returns the transaction data in json format.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "hex (string) Raw transaction hex string\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrDecodeRawTransaction BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrDecodeRawTransaction BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hex not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & hex = params[1].get_str();
    if (hex.empty()) {
        Object error;
        error.emplace_back("error", "Transaction hex cannot be empty");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().decodeRawTransaction(uuid, currency, consensus, hex);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlocks currency blockhash1,blockhash2,blockhash3 [node_count]\n"
                                 "List of blocks in json format with the specified block hashes.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "txhash1,txhash2,txhash3 (string) Transaction ids separated by commas (,)\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetTransactions BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetTransactions BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d 2\n");
    }

    if (params.size() < 1) {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2) {
        Object error;
        error.emplace_back("error", "Block hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::set<std::string> blockHashes;
    const auto & hashes = params[1].get_str();
    boost::split(blockHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : blockHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Block hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetBlocks BLOCK 302a309d6b6c4a65e4b9ff06c7ea81bb17e985d00abdb01978ace62cc5e18421,"
                                        "175d2a428b5649c2a4732113e7f348ba22a0e69cc0a87631449d1d77cd6e1b04,"
                                        "34989eca8ed66ff53631294519e147a12f4860123b4bdba36feac6da8db492ab");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlocks(uuid, currency, consensus, blockHashes);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactions currency txhash1,txhash2,txhash3 [node_count]\n"
                                 "List of transactions in json format with the specified transaction ids.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "txhash1,txhash2,txhash3 (string) Transaction ids separated by commas (,)\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetTransactions BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetTransactions BLOCK 6582c8028f409a98c96a73e3efeca277ea9ee43aeef174801c6fa6474b66f4e7,4d4db727a3b36e6689af82765cadabb235fd9bdfeb94de0210804c6dd5d2031d 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::set<std::string> txHashes;
    const auto & hashes = params[1].get_str();
    boost::split(txHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : txHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Transaction hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetTransactions BLOCK 24ff5506a30772acfb65012f1b3309d62786bc386be3b6ea853a798a71c010c8,"
                                        "24b6bcb44f045d7a4cf8cd47c94a14cc609352851ea973f8a47b20578391629f,"
                                        "66a5809c7090456965fe30280b88f69943e620894e1c4538a724ed9a89c769be");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransactions(uuid, currency, consensus, txHashes);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBalance(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance currency address [node_count]\n"
                                 "Balance of the account with the specified blockchain address.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "address (string) Blockchain address\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetBalance BLOCK xziGHUkfUnCBBZf2z7EkbwC4jKzVVBG82G\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetBalance BLOCK xziGHUkfUnCBBZf2z7EkbwC4jKzVVBG82G 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Account not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string currency = params[0].get_str();
    std::string address = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBalance(uuid, currency, consensus, address);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTxBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTxBloomFilter currency filter [block_number] [node_count]\n"
                                 "Lists transactions in json format matching bloom filter starting with block number.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "filter (string) Bloom filter\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetTxBloomFilter BLOCK 0x0000000018\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetTxBloomFilter BLOCK 0x0000000018 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Filter not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
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
            return error;
        }
    }
    
    const auto & currency = params[0].get_str();
    const auto & filter = params[1].get_str();
    std::string uuid;
    const auto reply = xrouter::App::instance().getTransactionsBloomFilter(uuid, currency, consensus, filter, number);
    return xrouter::form_reply(uuid, reply);
}

Value xrGenerateBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGenerateBloomFilter address1,address2 [node_count]\n"
                                 "Generates a bloom filter for given base58 addresses.\n"
                                 "\n"
                                 "address1,address2 (string) Addresses to apply to the bloom filter.\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGenerateBloomFilter BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGenerateBloomFilter BXziudHsEee8vDTgvXXNLCXwKouSssLMQ3,BgSDpy7F7PuBZpG4PQfryX9m94NNcmjWAX 2\n");
    }
    
    Object result;

    if (params.size() == 0) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return result;
    }
    
    CBloomFilter f(10 * static_cast<unsigned int>(params.size()), 0.1, 5, 0);

    Array invalid;

    for (const auto & param : params) {
        vector<unsigned char> data;
        const std::string & addr = param.get_str();
        xrouter::UnknownChainAddress address(addr);
        if (!address.IsValid()) { // Try parsing pubkey
            data = ParseHex(addr);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                invalid.push_back(Value(addr));
                continue;
            }
            f.insert(data);
        } else {
            // This is a bitcoin address
            CKeyID keyid; address.GetKeyID(keyid);
            data = vector<unsigned char>(keyid.begin(), keyid.end());
            f.insert(data);
        }
    }
    
    if (!invalid.empty()) {
        result.emplace_back("skipped-invalid", invalid);
    }
    
    if (invalid.size() == params.size()) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return result;
    }

    result.emplace_back("bloomfilter", f.to_hex());

    Object reply;
    reply.emplace_back("result", result);

    const std::string & uuid = xrouter::generateUUID();
    return xrouter::form_reply(uuid, write_string(Value(reply), false));
}

Value xrService(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrService service_name [param1 param2 param3 ... paramN]\n"
                                 "Send request to the service with the specified name.\n"
                                 "XRouter services are custom plugins that XRouter node operators advertise on\n"
                                 "the network. Anyone capable of running a service node can create or install\n"
                                 "custom XRouter services or plugins and provide access to them for free or for\n"
                                 "a fee. This is a great way to earn fees for your custom plugin.\n"
                                 "\n"
                                 "[param1 param2 param3 ... paramN] (arguments) Optional, refer to the plugin\n"
                                 "documentation for parameter requirements. Information about a custom XRouter\n"
                                 "service can be viewed in the plugin configuration. Use xrConnect to find a node\n"
                                 "with the plugin, then use xrConnectedNodes to review plugin information.\n"
                                 "\n"
                                 "Example:\n"
                                 "xrService GetBestBlockHashBTC\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrService GetBestBlockHashBTC 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    const std::string & service = params[0].get_str();
    std::vector<std::string> call_params;
    for (unsigned int i = 1; i < params.size(); i++)
        call_params.push_back(params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, 0, call_params);
    return xrouter::form_reply(uuid, reply);
}

Value xrServiceConsensus(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrServiceConsensus node_count service_name [param1 param2 param3 ... paramN]\n"
                                 "Send request to the service with the specified name and consensus number.\n"
                                 "XRouter services are custom plugins that XRouter node operators advertise on\n"
                                 "the network. Anyone capable of running a service node can create or install\n"
                                 "custom XRouter services or plugins and provide access to them for free or for\n"
                                 "a fee. This is a great way to earn fees for your custom plugin.\n"
                                 "\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query.\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "service_name (string) Name of the custom service or plugin\n"
                                 "[param1 param2 param3 ... paramN] (string) Optional, refer to the service's\n"
                                 "documentation for parameter requirements. Information about a custom XRouter\n"
                                 "service can be viewed in the plugin configuration. Use xrConnect to find a node\n"
                                 "with the service, then use xrConnectedNodes to review plugin information.\n"
                                 "\n"
                                 "Example:\n"
                                 "xrService GetBestBlockHashBTC\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrService GetBestBlockHashBTC 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Consensus number not specified, must specify at least 1");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & consensus = params[0].get_int();
    const auto & service = params[1].get_str();

    if (consensus < 1) {
        Object error;
        error.emplace_back("error", "Consensus must be at least 1");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::vector<std::string> call_params;
    for (unsigned int i = 2; i < params.size(); i++)
        call_params.push_back(params[i].get_str());

    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, consensus, call_params);
    return xrouter::form_reply(uuid, reply);
}

Value xrSendTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrSendTransaction currency signed_transaction_hex [node_count]\n"
                                 "Send a signed transaction to any supported blockchain network.\n"
                                 "This is useful if you want to send transactions to a blockchain without having\n"
                                 "to download the entire chain, or if you are running a lite-wallet/multi-wallet.\n"
                                 "\n"
                                 "currency (string) Blockchain to send transaction to\n"
                                 "signed_transaction_hex (string) Raw transaction hex (must be signed!)\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to relay this tx.(default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrSendTransaction BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrSendTransaction BLOCK 010000000101b4b67db0875632e4ff6cf1b9c6988c81d7ddefbf1be9a0ffd6b5109434eeff010000006a473044022007c31c3909ee93a5d8f589b1e99b4d71b6723507de31b90af3e0373812b7cdd602206d6fc5a3752530b634ba3b6a8d0997293b299c1184b0d90397242bedb6fc5f9a01210397b2f25181661d7c39d68667e0d1b99820ce8183b7a42da0dce3a623a3d30b67ffffffff08005039278c0400001976a914245ad0cca6ec4233791d89258e25cd7d9b5ec69e88ac00204aa9d10100001976a914216c4f3fdb628a97aed21569e7d16de369c1c30a88ac36e3c8239b0d00001976a914e89125937281a96e9ed1abf54b7529a08eb3ef9e88ac00204aa9d10100001976a91475fc439f3344039ef796fa28b2c563f29c960f0f88ac0010a5d4e80000001976a9148abaf7773d9aea7b7bec1417cb0bc002daf1952988ac0010a5d4e80000001976a9142e276ba01bf62a5ac76a818bf990047d4d0aaf5d88ac0010a5d4e80000001976a91421d5b48b854f74e7dcc89bf551e1f8dec87680cd88ac0010a5d4e80000001976a914c18d9ac6189d43f43240539491a53835219363fc88ac00000000 2\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction data not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    std::string currency = params[0].get_str();
    std::string transaction = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().sendTransaction(uuid, currency, consensus, transaction);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetReply(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetReply uuid\n"
                                 "Returns all the replies from XRouter nodes matching the specified query uuid.\n"
                                 "Useful to lookup previous calls without having to request from the XRouter"
                                 "network.\n"
                                 "\n"
                                 "Example:\n"
                                 "xrGetReply cc25f823-06a9-48e7-8245-f04991c09d6a\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Please specify the uuid");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    std::string uuid = params[0].get_str();
    Object result;
    std::string reply = xrouter::App::instance().getReply(uuid);
    return xrouter::form_reply(uuid, reply);
}

Value xrUpdateConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrUpdateConfigs [force_check]\n"
                                 "Requests latest configuration files for all known service nodes.\n"
                                 "\n"
                                 "[force_check] (boolean) If true node configs will be updated regardless of\n"
                                 "                        rate limiting checks. (default=false)\n"
                                 "\n"
                                 "Example:\n"
                                 "xrUpdateConfigs\n"
                                 "\n"
                                 "With force check parameter:\n"
                                 "xrUpdateConfigs true\n"
                                 "xrUpdateConfigs 1\n");
    }

    bool forceCheck = false;
    if (params.size() == 1)
        forceCheck = params[0].get_bool();

    Object result;
    std::string reply = xrouter::App::instance().updateConfigs(forceCheck);
    Object obj;
    obj.emplace_back("reply", reply);
    return obj;
}

Value xrShowConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrShowConfigs\n"
                                 "Shows the raw configurations received from XRouter nodes.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().printConfigs();
    return reply;
}

Value xrReloadConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrReloadConfigs\n"
                                 "Reloads the xrouter.conf and all associated plugin configs. If a plugin conf is\n"
                                 "changed while the client is running call this to apply those changes.");
    }
    
    Object result;
    xrouter::App::instance().reloadConfigs();
    return true;
}

Value xrStatus(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrStatus\n"
                                 "Show XRouter status and info.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().getStatus();
    return reply;
}

Value xrConnectedNodes(const Array& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error("xrConnectedNodes\n"
                                 "Lists all the data about current and previously connected nodes. This information\n"
                                 "includes supported blockchains, plugins, and fee schedules.");

    if (!params.empty()) {
        Object error;
        error.emplace_back("error", "This call does not support parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    const auto configs = app.getNodeConfigs();

    Array data;
    app.snodeConfigJSON(configs, data);

    return xrouter::form_reply(uuid, data);
}

Value xrConnect(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrConnect fully_qualified_service_name [node_count]\n"
                                 "Connects to XRouter nodes with the specified service and downloads their configs.\n"
                                 "This command is useful to determine how much nodes are charging for services. It's\n"
                                 "also useful to \"warm up\" connections. By connecting to nodes immediately before\n"
                                 "making a large request it can speed up the reponse time (since those connections\n"
                                 "will be open). However, XRouter nodes do close inactive connections after 15 seconds\n"
                                 "so keep that in mind. After connecting call xrConnectedNodes to display information\n"
                                 "about these XRouter nodes.\n"
                                 "\n"
                                 "fully_qualified_service_name (string) Service name including the namespace. Must specify\n"
                                 "                                      xr:: for SPV commands and xrs:: for plugin commands\n"
                                 "                                      Example: xr::BLOCK or xrs::GetBestBlockHashBTC\n"
                                 "[node_count] (int) Optional, The number of XRouter nodes to connect to. An error will\n"
                                 "                   be returned if not enough nodes can be found.\n"
                                 "\n"
                                 "Examples:\n"
                                 "\n"
                                 "Connect to one XRouter node supporting BLOCK\n"
                                 "xrConnect xr::BLOCK\n"
                                 "\n"
                                 "Connect to two XRouter nodes supporting BLOCK\n"
                                 "xrConnect xr::BLOCK 2\n"
                                 "\n"
                                 "Connect to two XRouter nodes supporting BTC\n"
                                 "xrConnect xr::BTC 2\n"
                                 "\n"
                                 "Connect to one XRouter node supporting custom service (plugin) GetBestBlockHashBTC\n"
                                 "xrConnect xr::GetBestBlockHashBTC\n"
                                 "");
    }

    if (params.size() < 1) {
        Object error;
        error.emplace_back("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const auto & service = params[0].get_str();
    if (service.empty()) {
        Object error;
        error.emplace_back("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int nodeCount{1};
    if (params.size() > 1) {
        nodeCount = params[1].get_int();
        if (nodeCount < 1) {
            Object error;
            error.emplace_back("error", "nodeCount must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    std::map<std::string, xrouter::XRouterSettingsPtr> configs;

    Array data;

    try {
        configs = app.xrConnect(service, nodeCount);
        if (configs.size() < nodeCount) {
            Object error;
            error.emplace_back("error", "Failed to connect to nodes, found " + std::to_string(configs.size()) +
                                        " expected " + std::to_string(nodeCount));
            error.emplace_back("code", xrouter::NOT_ENOUGH_NODES);
            return error;
        }
        app.snodeConfigJSON(configs, data);
    } catch (std::exception & e) {
        Object error;
        error.emplace_back("error", e.what());
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    } catch (xrouter::XRouterError & e) {
        Object error;
        error.emplace_back("error", e.msg);
        error.emplace_back("code", e.code);
        return error;
    }

    return xrouter::form_reply(uuid, data);
}

Value xrGetBlockAtTime(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockAtTime currency unix_time [node_count]\n"
                                 "Block data in json format of the block closest to the specified unix time.\n"
                                 "\n"
                                 "currency (string) Blockchain to query\n"
                                 "unix_time (int) Unix time of the desired block\n"
                                 "[node_count] (int) Optional, number of XRouter nodes to query (default=1)\n"
                                 "                         The most common reply will be returned (i.e. the reply\n"
                                 "                         with the most consensus. To see all reply results use\n"
                                 "                         xrGetReply uuid."
                                 "\n"
                                 "Example:\n"
                                 "xrGetBlockAtTime BLOCK 1553795164\n"
                                 "\n"
                                 "With consensus parameter:\n"
                                 "xrGetBlockAtTime BLOCK 1553795164 3\n");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Unix time not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int64_t time = params[1].get_int64();

    int consensus{0};
    if (params.size() >= 3) {
        consensus = params[2].get_int();
        if (consensus < 1) {
            Object error;
            error.emplace_back("error", "Consensus must be at least 1");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().convertTimeToBlockCount(uuid, currency, consensus, time);
    return xrouter::form_reply(uuid, reply);
}

Value xrRegisterDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrRegisterDomain domain [true/false] [addr]\nCreate the transactions described above needed to register the domain name. If the second parameter is true, your xrouter.conf is updated automatically. The third parameter is the destination address of hte transaction, leave this parameter blank if you want to use your service node collateral address");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Domain name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    bool update = false;
    if (params.size() > 3)
    {
        Object error;
        error.emplace_back("error", "Too many parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() >= 2) {
        if (params[1].get_str() == "false") {
            update = false;
        } else if (params[1].get_str() == "true") {
            update = true;
        } else {
            Object error;
            error.emplace_back("error", "Invalid parameter: must be true or false");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    std::string domain = params[0].get_str();
    std::string addr;
    if (params.size() <= 2)
    {
        addr = xrouter::App::instance().getMyPaymentAddress();
    }
    else
    {
        addr = params[2].get_str();
    }

    if (addr.empty()) {
        Object error;
        error.emplace_back("error", "Bad payment address");
        error.emplace_back("code", xrouter::BAD_ADDRESS);
        return error;
    }

    std::string uuid;
    auto reply = xrouter::App::instance().registerDomain(uuid, domain, addr, update);
    return xrouter::form_reply(uuid, reply);
}

Value xrQueryDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrQueryDomain domain\nCheck if the domain name is registered and return true if it is found");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Domain name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::string domain = params[0].get_str();
    std::string uuid;
    bool hasDomain = xrouter::App::instance().checkDomain(uuid, domain);
    std::string reply{hasDomain ? "true" : "false"};
    return xrouter::form_reply(uuid, reply);
}

Value xrTest(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }
    
    xrouter::App::instance().runTests();
    return "true";
}
