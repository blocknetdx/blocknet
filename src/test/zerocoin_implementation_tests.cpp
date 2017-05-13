// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Zerocoin.h"
#include "amount.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(zerocoin_implementation_tests)

BOOST_AUTO_TEST_CASE(zcparams_test)
{
    bool fPassed = true;
    try{
        libzerocoin::Params *ZCParams = Params().Zerocoin_Params();
    } catch(std::exception& e) {
        fPassed = false;
    }
    BOOST_CHECK(fPassed);
}

//translation from pivx quantity to zerocoin denomination
BOOST_AUTO_TEST_CASE(amount_to_denomination_test)
{
    CAmount amount = 1 * COIN;
    libzerocoin::CoinDenomination denomination;
    BOOST_CHECK(libzerocoin::AmountToZerocoinDenomination(amount, denomination));
    BOOST_CHECK(denomination == libzerocoin::ZQ_LOVELACE);
}

BOOST_AUTO_TEST_SUITE_END()
