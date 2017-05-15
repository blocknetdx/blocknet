// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Zerocoin.h"
#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

BOOST_AUTO_TEST_SUITE(zerocoin_implementation_tests)

BOOST_AUTO_TEST_CASE(zcparams_test)
{
    cout << "Running zcparams_test...\n";

    bool fPassed = true;
    try{
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params *ZCParams = Params().Zerocoin_Params();
        (void)ZCParams;
    } catch(std::exception& e) {
        fPassed = false;
        std::cout << e.what() << "\n";
    }
    BOOST_CHECK(fPassed);
}

//translation from pivx quantity to zerocoin denomination
BOOST_AUTO_TEST_CASE(amount_to_denomination_test)
{
    cout << "Running amount_to_denomination_test...\n";

    //valid amount (min edge)
    CAmount amount = 1 * COIN;
    libzerocoin::CoinDenomination denomination;
    BOOST_CHECK(libzerocoin::AmountToZerocoinDenomination(amount, denomination));
    BOOST_CHECK(denomination == libzerocoin::ZQ_LOVELACE);

    //valid amount (max edge)
    CAmount amount1 = 100 * COIN;
    libzerocoin::CoinDenomination denomination1;
    BOOST_CHECK(libzerocoin::AmountToZerocoinDenomination(amount1, denomination1));
    BOOST_CHECK(denomination1 == libzerocoin::ZQ_WILLIAMSON);

    //invalid amount (too much)
    CAmount amount2 = 5000 * COIN;
    libzerocoin::CoinDenomination denomination2;
    BOOST_CHECK(!libzerocoin::AmountToZerocoinDenomination(amount2, denomination2));
    BOOST_CHECK(denomination2 == libzerocoin::ZQ_ERROR);

    //invalid amount (not enough)
    CAmount amount3 = 1;
    libzerocoin::CoinDenomination denomination3;
    BOOST_CHECK(!libzerocoin::AmountToZerocoinDenomination(amount3, denomination3));
    BOOST_CHECK(denomination3 == libzerocoin::ZQ_ERROR);

}

std::string rawTx = "0100000001514715e14cf794e7ccac5d09948b8df72db663e89e9b5c11dddb784759b445d2010000006b4830450221008eba463647b173cdfb3528a867967bc49ea62ad6aba9d880fbfcb6095cf9c26a022076c166cb3fac0890b5347fe10f6c2fb2c1df20dbfa97d6393e88c41a1acfc4530121025bad342f35cf8fa89296fcfbcf608b5d1db45db86b4437490bb90e4be4af5a76ffffffff0200e1f5050000000087c10281004c8129f53f697a947e80baf14ac5146df8360925b962c77f32db4438862e381380197dbe8f78fcacb93ad7a2b32e83a3b31c37a5718e89f66b40d713bcf7d81730bbcba519030a1c896abe6d7c597751cc06b651cdaf2cc3f37c60720c019921a3febf837db02c238bca9fa9a54c69d9b65799d38efa75b1415d46698be36745c288002c0f0d8f000000001976a914014c0b8bf95d95153035c6087d2589a2469132b988ac00000000";
//create a zerocoin mint from vecsend
BOOST_AUTO_TEST_CASE(check_zerocoinmint_test)
{
    cout << "Running check_zerocoinmint_test...\n";
    CTransaction tx;
    BOOST_CHECK(DecodeHexTx(tx, rawTx));

    CValidationState state;
    bool fFoundMint = false;
    for(unsigned int i = 0; i < tx.vout.size(); i++){
        if(!tx.vout[i].scriptPubKey.empty() && tx.vout[i].scriptPubKey.IsZerocoinMint()) {
            BOOST_CHECK(CheckZerocoinMint(tx.vout[i], state, true));
            fFoundMint = true;
        }
    }

    BOOST_CHECK(fFoundMint);
}

BOOST_AUTO_TEST_SUITE_END()
