// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "denomination_functions.h"
#include "amount.h"
#include "chainparams.h"
#include "coincontrol.h"
#include "main.h"
#include "wallet.h"
#include "walletdb.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace libzerocoin;

BOOST_AUTO_TEST_SUITE(zerocoin_denom_tests)

BOOST_AUTO_TEST_CASE(zerocoin_spend_test)
{
    const int maxNumberOfSpends = 4;
    const int DenomAmounts[] = {1,2,3,4,0,0,0,0};
    //const int DenomAmounts[] = {1,2,3,4,5,6,7,8};
    CAmount nSelectedValue;
    std::list<CZerocoinMint> listMints;
    std::map<CoinDenomination, CAmount> mapDenom;

    int j=0;
    CAmount nTotalAmount = 0;
    int CoinsHeld = 0;
    for (const auto& denom : zerocoinDenomList) {
        bool set = false;
        for (int i=0;i<DenomAmounts[j];i++) {
            CAmount currentAmount = ZerocoinDenominationToAmount(denom);
            nTotalAmount += currentAmount;
            //std::cout << "Total for " << j << " = " << nTotalAmount/COIN << "\n";
            mapDenom.insert(std::pair<CoinDenomination, CAmount>(denom, currentAmount));
            CBigNum value;
            CBigNum rand;
            CBigNum serial;
            bool isUsed = false;
            CZerocoinMint mint(denom, value, rand, serial, isUsed);
            listMints.push_back(mint);
            set = true;
        }
        if (!set) {
            mapDenom.insert(std::pair<CoinDenomination, CAmount>(denom, 0));
        }
        j++;
    }
    CoinsHeld = nTotalAmount/COIN;
    std::cout << "Curremt Amount held = " << CoinsHeld << "\n";
    
    CAmount OneCoinAmount = ZerocoinDenominationToAmount(ZQ_ONE);
    CAmount nValueTarget = OneCoinAmount;
    for (int i=0;i<CoinsHeld;i++) {
        std::vector<CZerocoinMint> vSpends = SelectMintsFromList(nValueTarget, nSelectedValue,
                                                                 maxNumberOfSpends,
                                                                 listMints,
                                                                 mapDenom);


        std::cout << "Coins = " << i+1 << " spends = " << vSpends.size() << " Selected = " << nSelectedValue/COIN << "\n";
        BOOST_CHECK_MESSAGE(vSpends.size() < 5,"Too many spends");
        //BOOST_CHECK_MESSAGE(nSelectedValue == nValueTarget,"An exact amount expected");
        nValueTarget += OneCoinAmount;
    }

    
}

BOOST_AUTO_TEST_SUITE_END()
