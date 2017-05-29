// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Zerocoin.h"
#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "wallet.h"
#include "walletdb.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace libzerocoin;


BOOST_AUTO_TEST_SUITE(zerocoin_transactions_tests)

static CWallet cWallet("unlocked.dat");

BOOST_AUTO_TEST_CASE(zerocoin_spend_test)
{
    SelectParams(CBaseChainParams::MAIN);
    ZerocoinParams *ZCParams = Params().Zerocoin_Params();
    (void)ZCParams;

    bool fFirstRun;
    cWallet.LoadWallet(fFirstRun);
    CMutableTransaction tx;
    CWalletTx* wtx = new CWalletTx(&cWallet, tx);
    CZerocoinSpend zerocoinSpend;
    CZerocoinMint zerocoinSelected;
    
    CoinDenomination denom = ZQ_ERROR;

    std::string vString = cWallet.SpendZerocoin(denom, *wtx, zerocoinSpend, zerocoinSelected);
    
    BOOST_CHECK_MESSAGE(vString == "Invalid amount","Failed Invalid Amount Check");
    
    denom = ZQ_LOVELACE;
    vString = cWallet.SpendZerocoin(denom, *wtx, zerocoinSpend, zerocoinSelected);
    
    // if using "wallet.dat", instead of "unlocked.dat" need this
    /// BOOST_CHECK_MESSAGE(vString == "Error: Wallet locked, unable to create transaction!"," Locked Wallet Check Failed");
 
    BOOST_CHECK_MESSAGE(vString == "it has to have at least two mint coins with at least 7 confirmation in order to spend a coin", "Failed not enough mint coins");
    
    
}

BOOST_AUTO_TEST_CASE(create_zerocoin_spend_transaction_test)
{

    CWalletTx cWalletTx;
    CReserveKey reservekey(&cWallet);
    std::string strFailReason;
    CZerocoinSpend zerocoinSpend;
    CZerocoinMint zerocoinSelected;
   
    CoinDenomination denom = ZQ_ERROR;

    bool v = cWallet.CreateZerocoinSpendTransaction(denom, cWalletTx, reservekey, zerocoinSpend, zerocoinSelected,
                                                    strFailReason);
    
    BOOST_CHECK_MESSAGE(v,"Problem with Create ZerocoinSpendTransaction");
    
}

BOOST_AUTO_TEST_SUITE_END()
