/**
 * @file       Denominations.cpp
 *
 * @brief      Functions for converting to/from Zerocoin Denominations to other values library.
 *
 * @copyright  Copyright 2017 PIVX Developers
 * @license    This project is released under the MIT license.
 **/

#include "Denominations.h"
#include "amount.h"

namespace libzerocoin {

// All denomination values should only exist in these routines for consistency.
// For serialization/unserialization enums are converted to int
CoinDenomination PivAmountToZerocoinDenomination(int64_t amount)
{
    CoinDenomination denomination;
    switch (amount) {
    case 1:		denomination = CoinDenomination::ZQ_LOVELACE; break;
    case 10:	denomination = CoinDenomination::ZQ_GOLDWASSER; break;
    case 25:	denomination = CoinDenomination::ZQ_RACKOFF; break;
    case 50:	denomination = CoinDenomination::ZQ_PEDERSEN; break;
    case 100: denomination = CoinDenomination::ZQ_WILLIAMSON; break;
    default:
        //not a valid denomination
        denomination = CoinDenomination::ZQ_ERROR; break;
    }

    return denomination;
}

int64_t ZerocoinDenominationToValue(const CoinDenomination& denomination)
{
    int64_t Value=0;
    switch (denomination) {
    case CoinDenomination::ZQ_LOVELACE: Value = 1; break;
    case CoinDenomination::ZQ_GOLDWASSER: Value = 10; break;
    case CoinDenomination::ZQ_RACKOFF: Value = 25; break;
    case CoinDenomination::ZQ_PEDERSEN : Value = 50; break;
    case CoinDenomination::ZQ_WILLIAMSON: Value = 100; break;
    default:
        // Error Case
        Value = 0; break;
    }
    return Value;
}

CoinDenomination TransactionAmountToZerocoinDenomination(int64_t amount)
{
    // Check to make sure amount is an exact integer number of COINS
    int64_t residual_amount = amount - COIN*(amount/COIN);
    if (residual_amount == 0) {
        return PivAmountToZerocoinDenomination(amount/COIN);
    } else {
        return CoinDenomination::ZQ_ERROR;
    }
}

    
CoinDenomination get_denomination(std::string denomAmount) {
    int64_t val = std::stoi(denomAmount);
    return PivAmountToZerocoinDenomination(val);
}


int64_t get_amount(std::string denomAmount) {
    int64_t nAmount = 0;
    CoinDenomination denom = get_denomination(denomAmount);
    if (denom == ZQ_ERROR) {
        // SHOULD WE THROW EXCEPTION or Something?
        nAmount = 0;
    } else {
        nAmount = ZerocoinDenominationToValue(denom) * COIN;
    }
    return nAmount;
}

} /* namespace libzerocoin */
