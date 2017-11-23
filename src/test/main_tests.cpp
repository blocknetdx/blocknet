// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"
#include "main.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(main_tests)

CAmount nMoneySupplyPoWEnd = 18000000 * COIN;

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight < 1; nHeight += 1) {
        /* premine in block 1 (17,500,000 PHR) */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 17500000 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 1; nHeight < 201; nHeight += 1) {
        /* PoW premine (500,000 PHR) */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 2500 * COIN);
        nSum += nSubsidy;
    }

    BOOST_CHECK(nSum > 0 && nSum <= nMoneySupplyPoWEnd);

    for (int nHeight = 201; nHeight < 250001; nHeight += 1) {
        /* PoS Phase One */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 7 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 250001; nHeight < 518400; nHeight += 1) {
        /* PoS Phase Two */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 4.5 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 518400; nHeight < 1036799; nHeight += 1) {
        /* PoS Phase Two */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 3.6 * COIN);
        nSum += nSubsidy;
    }
}

BOOST_AUTO_TEST_SUITE_END()
