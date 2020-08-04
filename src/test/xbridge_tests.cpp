// Copyright (c) 2011-2018 The Bitcoin Core developers
// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <test/test_bitcoin.h>
#include <xbridge/util/xutil.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(xbridge_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(xbridge_partialorderdriftcheck) {
    { // full order should pass drift check
        CAmount makerSource{11220000}, makerDest{5000000}, takerSource{5000000}, takerDest{11220000};
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "exact order should pass drift check");
        makerSource = 5000000, makerDest = 11220000, takerSource = 11220000, takerDest = 5000000;
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "exact order should pass drift check (inverse)");
    }
    { // maker source > maker dest drift should pass
        CAmount makerSource{11220000}, makerDest{100}, takerSource{62}, takerDest{7000000};
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "maker source > maker dest should pass");
    }
    { // maker source < maker dest drift should pass
        CAmount makerSource{100}, makerDest{11220000}, takerSource{7000000}, takerDest{62};
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "maker source < maker dest should pass");
    }
    { // bad taker price (source) should fail upper limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{63}, takerDest{7000000};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (source) should fail upper limit");
        makerSource = 100, makerDest = 11220000, takerSource = 7000000, takerDest = 63;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (source) should fail upper limit (inverse)");
    }
    { // bad taker price (source) should fail lower limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{61}, takerDest{7000000};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (source) should fail lower limit");
        makerSource = 100, makerDest = 11220000, takerSource = 7000000, takerDest = 61;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (source) should fail lower limit (inverse)");
    }
    { // bad taker price (dest) should fail upper limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{62}, takerDest{7068600+1};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (dest) should fail upper limit");
        makerSource = 100, makerDest = 11220000, takerSource = 7068600+1, takerDest = 62;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (dest) should fail upper limit (inverse)");
    }
    { // bad taker price (dest) should fail lower limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{62}, takerDest{6844200};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (dest) should fail lower limit");
        makerSource = 100, makerDest = 11220000, takerSource = 6844200, takerDest = 62;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "bad taker price (dest) should fail lower limit (inverse)");
    }
    { // taker price should pass upper limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{62}, takerDest{7068600};
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should pass upper limit");
        makerSource = 100, makerDest = 11220000, takerSource = 7068600, takerDest = 62;
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should pass upper limit (inverse)");
    }
    { // taker price should fail lower limit
        CAmount makerSource{11220000}, makerDest{100}, takerSource{62}, takerDest{6844200};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail lower limit");
        makerSource = 100, makerDest = 11220000, takerSource = 6844200, takerDest = 62;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail lower limit (inverse)");
    }
    { // taker price should pass divisible limits
        CAmount makerSource{100000}, makerDest{1000}, takerSource{1}, takerDest{100};
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should pass divisible limits");
        makerSource = 1000, makerDest = 100000, takerSource = 100, takerDest = 1;
        BOOST_CHECK_MESSAGE(xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should pass divisible limits (inverse)");
    }
    { // taker price should fail divisible limits
        CAmount makerSource{100000}, makerDest{1000}, takerSource{10}, takerDest{10000};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail divisible limits");
        makerSource = 1000, makerDest = 100000, takerSource = 10000, takerDest = 10;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail divisible limits (inverse)");
    }
    { // taker price should fail on bad taker size
        CAmount makerSource{100000}, makerDest{1000}, takerSource{100}, takerDest{990};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail on bad taker size");
        makerSource = 1000, makerDest = 100000, takerSource = 990, takerDest = 100;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail on bad taker size (inverse)");
    }
    { // taker price should fail on bad taker size
        CAmount makerSource{100000}, makerDest{1000}, takerSource{10}, takerDest{990};
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail on bad taker size");
        makerSource = 1000, makerDest = 100000, takerSource = 990, takerDest = 10;
        BOOST_CHECK_MESSAGE(!xbridge::xBridgePartialOrderDriftCheck(makerSource, makerDest, takerSource, takerDest), "taker price should fail on bad taker size (inverse)");
    }
}

BOOST_AUTO_TEST_CASE(xbridge_pricecheck) {
    {   // exact price conversions should pass
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (10000,        10000,      100000),    100000);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (100000,       10000,      100000),    10000);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (1,            1,          10),        10);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (10,           1,          10),        1);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (62,           62,         100),       100);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (100,          62,         100),       62);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (11220000,     11220000,   62),        62);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (62,           11220000,   62),        11220000);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (99,           99,         199),       199);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (199,          99,         199),       99);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (10999,        10999,      20999),     20999);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (20999,        10999,      20999),     10999);
    }
    {   // partial price conversions should pass
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (892,          10000,      100000),    8920);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (8920,         10000,      100000),    892);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (1,            5,          10),        2);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (2,            5,          10),        1);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (99,           1,          10),        990);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (990,          1,          10),        99);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (34567,        12345678,   9999),      27);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (27,           12345678,   9999),      33336);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (99999999999,  99,         9),         9090909090);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (9090909090,   99,         9),         99999999990);
        BOOST_CHECK_EQUAL(xbridge::xBridgeDestAmountFromPrice      (999,          999,        999),       999);
        BOOST_CHECK_EQUAL(xbridge::xBridgeSourceAmountFromPrice    (1111,         1111,       1111),      1111);
    }
    {   // bad price conversions should fail (changes only 2nd column price, all other values same as previous test set)
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (892,          100000,     100000)  != 8920);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (8920,         100000,     100000)  != 892);
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (1,            20,          10)     != 2);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (2,            20,          10)     != 1);
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (99,           2,          10)      != 990);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (990,          2,          10)      != 99);
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (34567,        123456789,  9999)    != 27);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (27,           123456789,  9999)    != 33336);
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (99999999999,  990,        9)       != 9090909090);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (9090909090,   990,        9)       != 99999999990);
        BOOST_CHECK(xbridge::xBridgeDestAmountFromPrice            (999,          9992,       999)     != 999);
        BOOST_CHECK(xbridge::xBridgeSourceAmountFromPrice          (1111,         11112,      1111)    != 1111);
    }
}

BOOST_AUTO_TEST_SUITE_END()
