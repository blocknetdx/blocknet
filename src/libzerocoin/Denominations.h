/**
 * @file       Denominations.h
 *
 * @brief      Denomination info for the Zerocoin library.
 *
 * @copyright  Copyright 2017 PIVX Developers
 * @license    This project is released under the MIT license.
 **/

#ifndef DENOMINATIONS_H_
#define DENOMINATIONS_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace libzerocoin {

//PRESSTAB: should we add an invalid representation for CoinDenomination?
enum  CoinDenomination {
    ZQ_ERROR = 0,
    ZQ_LOVELACE,
    ZQ_GOLDWASSER,
    ZQ_RACKOFF,
    ZQ_PEDERSEN,
    ZQ_WILLIAMSON,
    ZQ_6, //placeholders
    ZQ_7,
    ZQ_8,
};

const std::vector<CoinDenomination> zerocoinDenomList = {ZQ_LOVELACE, ZQ_GOLDWASSER, ZQ_RACKOFF, ZQ_PEDERSEN, ZQ_WILLIAMSON, ZQ_6, ZQ_7, ZQ_8};

CoinDenomination AmountToZerocoinDenomination(int64_t amount);
int64_t ZerocoinDenominationToValue(const CoinDenomination& denomination);
CoinDenomination get_denomination(std::string denomAmount);
int64_t get_amount(std::string denomAmount);

} /* namespace libzerocoin */
#endif /* DENOMINATIONS_H_ */
