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

BOOST_AUTO_TEST_CASE(zerocoin_spend_test)
{

    CWallet cWallet;
    CWalletTx cWalletTx;
    CBigNum coinSerial;
    uint256 txHash;
    CBigNum selectedValue;
    bool selectedUsed;
    
    int64_t nValue=0;
    CoinDenomination denom = ZQ_LOVELACE;

    std::string vString = cWallet.SpendZerocoin(nValue, denom, cWalletTx, coinSerial, txHash, selectedValue,
                                   selectedUsed);
    
    BOOST_CHECK_MESSAGE(vString == "","Problem with Create SpendZerocoin");
    
}

BOOST_AUTO_TEST_CASE(create_zerocoin_spend_transaction_test)
{

    CWallet cWallet;
    CWalletTx cWalletTx;
    CReserveKey reservekey(&cWallet);
    CBigNum coinSerial;
    uint256 txHash;
    CBigNum selectedValue;
    std::string strFailReason;
    bool selectedUsed;
    
    int64_t nValue=0;
    CoinDenomination denom = ZQ_LOVELACE;

    bool v = cWallet.CreateZerocoinSpendTransaction(nValue, denom, cWalletTx, reservekey, coinSerial, txHash, selectedValue,
                                                    selectedUsed,
                                                    strFailReason);
    
    BOOST_CHECK_MESSAGE(v,"Problem with Create ZerocoinSpendTransaction");
    
}

BOOST_AUTO_TEST_SUITE_END()
