// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zerocoin.h"
#include "amount.h"
#include "hash.h"
#include "utilstrencodings.h"

uint256 CZerocoinMint::GetHash() const
{
    return Hash(BEGIN(value), END(value));
}

CAmount CZerocoinMint::GetDenominationAsAmount() const
{
    return denominationAsInt * COIN;
}

uint256 CZerocoinSpend::GetHash() const
{
    return Hash(BEGIN(coinSerial), END(coinSerial));
}

