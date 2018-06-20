// Copyright (c) 2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <tinyformat.h>
#include <utilmoneystr.h>
#include "masternode-budget.h"

BOOST_AUTO_TEST_SUITE(budget_tests)

void CheckBudgetValue(int nHeight, std::string strNetwork, CAmount nExpectedValue)
{
    CBudgetManager budget;
    CAmount nBudget = budget.GetTotalBudget(nHeight);
    std::string strError = strprintf("Budget is not as expected for %s. Result: %s, Expected: %s", strNetwork, FormatMoney(nBudget), FormatMoney(nExpectedValue));
    BOOST_CHECK_MESSAGE(nBudget == nExpectedValue, strError);
}

BOOST_AUTO_TEST_CASE(budget_value)
{
    SelectParams(CBaseChainParams::MAIN);
    int nHeightTest = Params().Zerocoin_Block_V2_Start() + 1;
    CheckBudgetValue(nHeightTest, "mainnet", 43200*COIN);

    SelectParams(CBaseChainParams::TESTNET);
    nHeightTest = Params().Zerocoin_Block_V2_Start() + 1;
    CheckBudgetValue(nHeightTest, "testnet", 7300*COIN);
}

BOOST_AUTO_TEST_SUITE_END()