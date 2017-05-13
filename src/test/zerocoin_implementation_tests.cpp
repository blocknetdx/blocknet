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

std::string rawTx = "01000000015bee015f9e26c5668995755f0706ace701a142aa278111f05e81311e7cd52754010000006b483045022100e95d9a9ba258be30c96f734ac519022a9be7cd824ec530979fee13a9320db11e022017df8343b391ec98c92674bda1bc8429b693930f4d6fe7b35a7298ea969dd0a50121020035f34721c6ed91ebf43cc9aca5cf9967031b24b09cfe0d5faa32f0a6879f55ffffffff022c0f0d8f000000001976a91409c6ed4955884ffbbd1917a20a2f149c5d9de83a88ac00e1f5050000000087c10281004c81c1c6b9a5830fd9abea90b548313c50ee0abaa18cf58685ab391da7542f14dd3a8cd072707c4c3d9ca2e28e9d3ace6d1cebbc1f9bf0e535ced4c054d96021637efdae77d5cb42fede9c638a7efe46db36f8d0e7621d21a267fa115cf66c4577fa2a82f67273891225675cce22cd12671110851e2a16800531f95354690cf73e990000000000";
//create a zerocoin mint from vecsend
BOOST_AUTO_TEST_CASE(check_zerocoinmint_test)
{
    CTransaction tx;
    BOOST_CHECK(DecodeHexTx(tx, rawTx));

    CValidationState state;
    for(unsigned int i = 0; i < tx.vout.size(); i++){
        if(!tx.vout[i].scriptPubKey.empty() && tx.vout[i].scriptPubKey.IsZerocoinMint())
            BOOST_CHECK(CheckZerocoinMint(tx.vout[i], state, true));
    }
}

BOOST_AUTO_TEST_SUITE_END()
