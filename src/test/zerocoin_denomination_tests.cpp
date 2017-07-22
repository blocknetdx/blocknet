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


//translation from pivx quantity to zerocoin denomination
BOOST_AUTO_TEST_CASE(amount_to_denomination_test)
{
    cout << "Running amount_to_denomination_test...\n";

    //valid amount (min edge)
    CAmount amount = 1 * COIN;
    BOOST_CHECK_MESSAGE(AmountToZerocoinDenomination(amount) == ZQ_ONE,"For COIN denomination should be ZQ_ONE");

    //valid amount (max edge)
    CAmount amount1 = 5000 * COIN;
    BOOST_CHECK_MESSAGE(AmountToZerocoinDenomination(amount1) == ZQ_FIVE_THOUSAND,"For 5000*COIN denomination should be ZQ_ONE");
    
    //invalid amount (too much)
    CAmount amount2 = 7000 * COIN;
    BOOST_CHECK_MESSAGE(AmountToZerocoinDenomination(amount2) == ZQ_ERROR,"For 7000*COIN denomination should be Invalid -> ZQ_ERROR");
    
    //invalid amount (not enough)
    CAmount amount3 = 1;
    BOOST_CHECK_MESSAGE(AmountToZerocoinDenomination(amount3) == ZQ_ERROR,"For 1 denomination should be Invalid -> ZQ_ERROR");
    
}

BOOST_AUTO_TEST_CASE(denomination_to_value_test)
{
    cout << "Running ZerocoinDenominationToValue_test...\n";

    int64_t Value = 1 * COIN;
    CoinDenomination denomination = ZQ_ONE;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 1");

    Value = 10 * COIN;
    denomination = ZQ_TEN;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 10");

    Value = 50 * COIN;
    denomination = ZQ_FIFTY;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 50");

    Value = 500 * COIN;
    denomination = ZQ_FIVE_HUNDRED;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 500");
    
    Value = 100 * COIN;
    denomination = ZQ_ONE_HUNDRED;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 100");

    Value = 0 * COIN;
    denomination = ZQ_ERROR;
    BOOST_CHECK_MESSAGE(ZerocoinDenominationToAmount(denomination) ==  Value, "Wrong Value - should be 0");

}

BOOST_AUTO_TEST_CASE(zerocoin_spend_test241)
{
    const int maxNumberOfSpends = 4;
    const int DenomAmounts[] = {1,2,3,4,0,0,0,0};
    CAmount nSelectedValue;
    std::list<CZerocoinMint> listMints;
    std::map<CoinDenomination, CAmount> mapDenom;

    int j=0;
    CAmount nTotalAmount = 0;
    int CoinsHeld = 0;

    // Create a set of Minted coins that fits profile given by DenomAmounts
    // Also setup Map array corresponding to DenomAmount which is the current set of coins available
    
    for (const auto& denom : zerocoinDenomList) {
        bool set = false;
        for (int i=0;i<DenomAmounts[j];i++) {
            CAmount currentAmount = ZerocoinDenominationToAmount(denom);
            nTotalAmount += currentAmount;
            CBigNum value;
            CBigNum rand;
            CBigNum serial;
            bool isUsed = false;
            CZerocoinMint mint(denom, value, rand, serial, isUsed);
            listMints.push_back(mint);
            set = true;
        }
        mapDenom.insert(std::pair<CoinDenomination, CAmount>(denom, DenomAmounts[j]));
        j++;
    }
    CoinsHeld = nTotalAmount/COIN;
    std::cout << "Curremt Amount held = " << CoinsHeld << ": ";

    // Show what we have
    j=0;
    for (const auto& denom : zerocoinDenomList) std::cout << DenomAmounts[j++] << "*" << ZerocoinDenominationToAmount(denom)/COIN << " + ";
    std::cout << "\n";

    // For DenomAmounts[] = {1,2,3,4,0,0,0,0}; we can spend up to 200 without requiring more than 4 Spends
    // Amounts above this can not be met
    CAmount MaxLimit = 200;
    CAmount OneCoinAmount = ZerocoinDenominationToAmount(ZQ_ONE);
    CAmount nValueTarget = OneCoinAmount;

    bool fDebug = 0;
    
    // Go through all possible spend between 1 and 241 and see if it's possible or not
    for (int i=0;i<CoinsHeld;i++) {
        std::vector<CZerocoinMint> vSpends = SelectMintsFromList(nValueTarget, nSelectedValue,
                                                                 maxNumberOfSpends,
                                                                 listMints,
                                                                 mapDenom);

        if (fDebug) {
            if (vSpends.size() > 0) {
                std::cout << "SUCCESS : Coins = " << nValueTarget/COIN << " # spends used = " << vSpends.size()
                          << " Spend Amount = " << nSelectedValue/COIN << " Held = " << CoinsHeld << "\n";
            } else {
                std::cout << "FAILED : Coins = " << nValueTarget/COIN << " Held = " << CoinsHeld << "\n";
            }
        }
        
        if (i < MaxLimit) {
            BOOST_CHECK_MESSAGE(vSpends.size() < 5,"Too many spends");
            BOOST_CHECK_MESSAGE(vSpends.size() > 0,"No spends");
        } else {
            BOOST_CHECK_MESSAGE(vSpends.size() < 5,"Too many spends");
            BOOST_CHECK_MESSAGE(vSpends.size() == 0,"No spends");
        }
        nValueTarget += OneCoinAmount;
    }

}
BOOST_AUTO_TEST_CASE(zerocoin_spend_test115)
{
    const int maxNumberOfSpends = 4;
    const int DenomAmounts[] = {0,1,1,2,0,0,0,0};
    CAmount nSelectedValue;
    std::list<CZerocoinMint> listMints;
    std::map<CoinDenomination, CAmount> mapDenom;

    int j=0;
    CAmount nTotalAmount = 0;
    int CoinsHeld = 0;

    // Create a set of Minted coins that fits profile given by DenomAmounts
    // Also setup Map array corresponding to DenomAmount which is the current set of coins available
    for (const auto& denom : zerocoinDenomList) {
        bool set = false;
        for (int i=0;i<DenomAmounts[j];i++) {
            CAmount currentAmount = ZerocoinDenominationToAmount(denom);
            nTotalAmount += currentAmount;
            CBigNum value;
            CBigNum rand;
            CBigNum serial;
            bool isUsed = false;
            CZerocoinMint mint(denom, value, rand, serial, isUsed);
            listMints.push_back(mint);
            set = true;
        }
        mapDenom.insert(std::pair<CoinDenomination, CAmount>(denom, DenomAmounts[j]));
        j++;
    }
    CoinsHeld = nTotalAmount/COIN;
    std::cout << "Curremt Amount held = " << CoinsHeld << ": ";

    // Show what we have
    j=0;
    for (const auto& denom : zerocoinDenomList) std::cout << DenomAmounts[j++] << "*" << ZerocoinDenominationToAmount(denom)/COIN << " + ";
    std::cout << "\n";

    CAmount OneCoinAmount = ZerocoinDenominationToAmount(ZQ_ONE);
    CAmount nValueTarget = 66*OneCoinAmount;

    bool fDebug = 1;
    
    std::vector<CZerocoinMint> vSpends = SelectMintsFromList(nValueTarget, nSelectedValue,
                                                             maxNumberOfSpends,
                                                             listMints,
                                                             mapDenom);
    
    if (fDebug) {
        if (vSpends.size() > 0) {
            std::cout << "SUCCESS : Coins = " << nValueTarget/COIN << " # spends used = " << vSpends.size()
                      << " Spend Amount = " << nSelectedValue/COIN << " Held = " << CoinsHeld << "\n";
        } else {
            std::cout << "FAILED : Coins = " << nValueTarget/COIN << " Held = " << CoinsHeld << "\n";
        }
    }
        
    BOOST_CHECK_MESSAGE(vSpends.size() < 5,"Too many spends");
    BOOST_CHECK_MESSAGE(vSpends.size() > 0,"No spends");
    nValueTarget += OneCoinAmount;

}

BOOST_AUTO_TEST_SUITE_END()
