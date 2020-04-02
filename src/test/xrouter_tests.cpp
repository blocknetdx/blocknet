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
            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == "1e1a89b239727807b0b7ee0ca465945b33cfebb37286d570e502163b80c60ff5", "getBlockHash wrong hash");
        }
        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
        // TODO Blocknet libxrouter hex number
//        res = client->getBlockHash("BLOCK", "0x1");
//        replynull = find_value(res, "reply").isNull();
//        BOOST_CHECK_MESSAGE(!replynull, "getBlockHash reply is invalid");
//        if (!replynull) {
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").isStr(), "getBlockHash reply is not a string");
//            BOOST_CHECK_MESSAGE(find_value(res, "reply").get_str() == "1e1a89b239727807b0b7ee0ca465945b33cfebb37286d570e502163b80c60ff5", "getBlockHash wrong hash");
//        }
//        BOOST_CHECK_MESSAGE(!find_value(res, "uuid").isNull(), "getBlockHash uuid is invalid");
//        BOOST_CHECK_MESSAGE(find_value(res, "error").isNull(), "getBlockHash has error");
    }

    BOOST_CHECK(client->stop());
}

BOOST_FIXTURE_TEST_CASE(xrouter_tests_testnet_xrscalls, XRouterTestClientTestnet) {
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

BOOST_FIXTURE_TEST_CASE(xrouter_tests_mainnet_xrcalls, XRouterTestClientMainnet) {
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
    }

BOOST_AUTO_TEST_SUITE_END()
