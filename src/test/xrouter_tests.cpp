// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/xrouter_tests.h>
#include <boost/test/unit_test.hpp>

XRouterTestClient::XRouterTestClient() {
    connOptions = xrouter::XRouterClient::defaultOptions();
    connOptions.unit_test_mode = true;
}
XRouterTestClient::~XRouterTestClient() {
    client->stop();
    gArgs.ClearArgs();
}
XRouterTestClientMainnet::XRouterTestClientMainnet() : XRouterTestClient() {
    std::string a, b{"-testnet=0"}; char* argv[2] = {&a[0], &b[0]};
    client = MakeUnique<xrouter::XRouterClient>(2, argv, connOptions);
}
XRouterTestClientTestnet::XRouterTestClientTestnet() : XRouterTestClient() {
    std::string a, b{"-testnet=1"}; char* argv[2] = {&a[0], &b[0]};
    client = MakeUnique<xrouter::XRouterClient>(2, argv, connOptions);
}

BOOST_AUTO_TEST_SUITE(xrouter_tests)

BOOST_AUTO_TEST_CASE(xrouter_tests_default) {
}

#ifdef USE_XROUTERCLIENT

BOOST_FIXTURE_TEST_CASE(xrouter_tests_waitforservice, XRouterTestClientTestnet) {
    BOOST_REQUIRE_MESSAGE(client->start(), "Failed to start xrouter client");
    auto start = std::chrono::system_clock::now();
    unsigned int timeout{5000};
    BOOST_CHECK_MESSAGE(!client->waitForService(timeout, {{"xrs::some_invalid_service_83748237483784", 9999}}), "Expecting timeout to occur");
    auto end = std::chrono::system_clock::now();
    BOOST_CHECK_MESSAGE(std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()-start.time_since_epoch()).count() >= timeout, "Timeout failed");
    BOOST_CHECK(client->stop());
}

BOOST_FIXTURE_TEST_CASE(xrouter_tests_testnet_xrcalls, XRouterTestClientTestnet) {
    BOOST_REQUIRE_MESSAGE(client->start(), "Failed to start xrouter client");
    BOOST_REQUIRE_MESSAGE(client->waitForService(5000, "xr::BLOCK"), "Services not found on testnet");
    BOOST_REQUIRE_MESSAGE(!client->getServiceNodes().empty(), "Service node list should not be empty");

    const std::string testnetBlock1Hash = "1e1a89b239727807b0b7ee0ca465945b33cfebb37286d570e502163b80c60ff5";
    const std::string testnetBlock2Hash = "36ac6222b88a3db2a4f7e7f51de37a59e22ee68d23b3186f9e9b7ae8d3fdf2db";
    const std::string testnetBlock1Tx = "3b0d75a594c0ae23a389e0c9ca249cb250df95f718131ca876e9cd874bbaa136";
    const std::string testnetBlock2Tx = "9c6f33a21b4df7253aef34cd9f3c0936770df3b4edba271a87cf56562253ee4a";
    const std::string testnetBlock1TxRaw = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff03510101ffffffff010088526a740000001976a9143002c6d1bbb62ad87bfb32fdd6a4ecf89abc818988ac00000000";

    { // getBlockCount
        UniValue res = client->getBlockCount("BLOCK");
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getBlockCount reply is invalid");
        if (!replynull)
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isNum(), "getBlockCount reply is not a number");
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockCount uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockCount has error");
    }

    { // getBlockCount with namespace
        UniValue res = client->getBlockCount("xr::BLOCK");
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getBlockCount reply is invalid");
        if (!replynull)
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isNum(), "getBlockCount reply is not a number");
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockCount uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockCount has error");
    }

    { // getBlockHash
        UniValue res = client->getBlockHash("BLOCK", 1);
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getBlockHash reply is invalid");
        if (!replynull) {
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "getBlockHash reply is not a string");
            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == testnetBlock1Hash, "getBlockHash wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
        // TODO Blocknet libxrouter hex number
//        res = client->getBlockHash("BLOCK", "0x1");
//        replynull = find_value(res, "reply").isNull();
//        BOOST_CHECK_MESSAGE(!replynull, "getBlockHash reply is invalid");
//        if (!replynull) {
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "getBlockHash reply is not a string");
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == testnetBlock1Hash, "getBlockHash wrong hash");
//        }
//        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
//        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
    }

    { // getBlock
        // TODO Blocknet XRouter int block
//        UniValue res = client->getBlock("BLOCK", 1);
//        auto replynull = find_value(res, "reply").isNull();
//        BOOST_CHECK_MESSAGE(!replynull, "getBlock reply is invalid");
//        if (!replynull) {
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").isObject(), "getBlock reply is not a json obj");
//            BOOST_CHECK_MESSAGE(find_value(find_value(res, "reply").get_obj(), "hash").get_str() == testnetBlock1Hash, "getBlock wrong hash");
//        }
//        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlock uuid is invalid");
//        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlock has error");

        // string block
        auto res = client->getBlock("BLOCK", testnetBlock1Hash);
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getBlock reply is invalid");
        if (!replynull) {
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isObject(), "getBlock reply is not a json obj");
            BOOST_CHECK_MESSAGE(find_value(find_value(res, "reply").get_obj(), "hash").get_str() == testnetBlock1Hash, "getBlock wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlock uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlock has error");
    }

    { // getBlocks
        // TODO Blocknet XRouter int block numbers
//        UniValue res = client->getBlocks("BLOCK", {1, 2});
//        auto replynull = find_value(res, "reply").isNull();
//        BOOST_CHECK_MESSAGE(!replynull, "getBlocks reply is invalid");
//        if (!replynull) {
//            auto reply = find_value(res, "reply");
//            BOOST_CHECK_MESSAGE(reply.isArray(), "getBlocks reply is not a json array");
//            BOOST_CHECK_MESSAGE(reply.get_array().size() == 2, "getBlocks expected 2 results");
//        }
//        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlocks uuid is invalid");
//        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlocks has error");

        // string block hashes
        auto res = client->getBlocks("BLOCK", {testnetBlock1Hash, testnetBlock2Hash});
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getBlocks reply is invalid");
        if (!replynull) {
            auto reply = find_value(res, "reply");
            BOOST_CHECK_MESSAGE(reply.isArray(), "getBlocks reply is not a json array");
            BOOST_CHECK_MESSAGE(reply.get_array().size() == 2, "getBlocks expected 2 results");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlocks uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlocks has error");
    }

    { // getTransaction
        auto res = client->getTransaction("BLOCK", testnetBlock1Tx);
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getTransaction reply is invalid");
        if (!replynull) {
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isObject(), "getTransaction reply is not a json obj");
            BOOST_CHECK_MESSAGE(find_value(find_value(res, "reply").get_obj(), "hash").get_str() == testnetBlock1Tx, "getTransaction wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getTransaction uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getTransaction has error");
    }

    { // getTransactions
        UniValue res = client->getTransactions("BLOCK", {testnetBlock1Tx, testnetBlock2Tx});
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "getTransactions reply is invalid");
        if (!replynull) {
            auto reply = find_value(res, "reply");
            BOOST_CHECK_MESSAGE(reply.isArray(), "getTransactions reply is not a json array");
            BOOST_CHECK_MESSAGE(reply.get_array().size() == 2, "getTransactions expected 2 results");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getTransactions uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getTransactions has error");
    }

    { // decodeTransaction
        auto res = client->decodeTransaction("BLOCK", testnetBlock1TxRaw);
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "decodeTransaction reply is invalid");
        if (!replynull) {
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isObject(), "decodeTransaction reply is not a json obj");
            BOOST_CHECK_MESSAGE(find_value(find_value(res, "reply").get_obj(), "hash").get_str() == testnetBlock1Tx, "decodeTransaction wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "decodeTransaction uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "decodeTransaction has error");
    }

    { // sendTransaction
        auto res = client->sendTransaction("BLOCK", testnetBlock1TxRaw);
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "sendTransaction uuid is invalid");
        BOOST_CHECK_MESSAGE(!find_value(res, "error").isNull(), "sendTransaction expecting error since tx has already confirmed");
    }

    BOOST_CHECK(client->stop());
}

BOOST_FIXTURE_TEST_CASE(xrouter_tests_testnet_xrservicecalls, XRouterTestClientTestnet) {
    BOOST_REQUIRE_MESSAGE(client->start(), "Failed to start xrouter client");
    BOOST_REQUIRE_MESSAGE(client->waitForService(5000, {{"xr::BLOCK",1},{"xrs::block_getblockhash",1}}), "Services not found on testnet");
    BOOST_REQUIRE_MESSAGE(!client->getServiceNodes().empty(), "Service node list should not be empty");

    { // block_getblockhash
        UniValue params(UniValue::VARR);
        params.push_back(1);
        UniValue res = client->callService("xrs::block_getblockhash", params);
        auto replynull = find_value(res, "reply").isNull();
        BOOST_CHECK_MESSAGE(!replynull, "block_getblockhash reply is invalid");
        if (!replynull) {
            BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "block_getblockhash reply is not a string");
            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == "1e1a89b239727807b0b7ee0ca465945b33cfebb37286d570e502163b80c60ff5", "getBlockHash wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "block_getblockhash uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "block_getblockhash has error");
    }

    BOOST_CHECK(client->stop());
}

/*BOOST_FIXTURE_TEST_CASE(xrouter_tests_mainnet_xrcalls, XRouterTestClientMainnet) {
        BOOST_REQUIRE_MESSAGE(client->start(), "Failed to start xrouter client");
        BOOST_REQUIRE_MESSAGE(client->waitForService(10000, "xr::BLOCK"), "Services not found on mainnet");
        BOOST_REQUIRE_MESSAGE(!client->getServiceNodes().empty(), "Service node list should not be empty");

        { // getBlockCount
            UniValue res = client->getBlockCount("BLOCK");
            auto replynull = find_value(res, "reply").isNull();
            BOOST_CHECK_MESSAGE(!replynull, "getBlockCount reply is invalid");
            if (!replynull)
                BOOST_CHECK_MESSAGE(find_value(res, "reply").isNum(), "getBlockCount reply is not a number");
            BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockCount uuid is invalid");
            BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockCount has error");
        }

        { // getBlockHash
            UniValue res = client->getBlockHash("BLOCK", 1);
            auto replynull = find_value(res, "reply").isNull();
            BOOST_CHECK_MESSAGE(!replynull, "getBlockHash reply is invalid");
            if (!replynull) {
                BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "getBlockHash reply is not a string");
                BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == "000008fb89a186d6b78a281306b4dab0733329fd25b5b8175f5f11a4fc2771e6", "getBlockHash wrong hash");
            }
            BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
            BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
            // hex number
//        res = client->getBlockHash("BLOCK", "0x01");
//        replynull = find_value(res, "reply").isNull();
//        BOOST_CHECK_MESSAGE(!replynull, "getBlockHash reply is invalid");
//        if (!replynull) {
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "getBlockHash reply is not a string");
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == "000008fb89a186d6b78a281306b4dab0733329fd25b5b8175f5f11a4fc2771e6", "getBlockHash wrong hash");
//        }
//        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
//        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
        }

        BOOST_CHECK(client->stop());
    }*/

#endif // USE_XROUTERCLIENT

BOOST_AUTO_TEST_SUITE_END()

