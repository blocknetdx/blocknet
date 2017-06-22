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

#include <cstdint>
#include <string>
#include <vector>

namespace libzerocoin {

//PRESSTAB: should we add an invalid representation for CoinDenomination?
enum  CoinDenomination {
    ZQ_ERROR = 0,
    ZQ_ONE,
    ZQ_FIVE,
    ZQ_TEN,
    ZQ_FIFTY,
    ZQ_ONE_HUNDRED,
    ZQ_FIVE_HUNDRED,
    ZQ_ONE_THOUSAND,
    ZQ_FIVE_THOUSAND,
};

const std::vector<CoinDenomination> zerocoinDenomList = {ZQ_ONE, ZQ_FIVE, ZQ_TEN, ZQ_FIFTY, ZQ_ONE_HUNDRED, ZQ_FIVE_HUNDRED, ZQ_ONE_THOUSAND, ZQ_FIVE_THOUSAND};

CoinDenomination IntToZerocoinDenomination(int64_t amount);
int64_t ZerocoinDenominationToAmount(const CoinDenomination& denomination);
CoinDenomination AmountToZerocoinDenomination(int64_t amount);
int64_t ZerocoinDenominationToInt(const CoinDenomination& denomination);
CoinDenomination get_denomination(std::string denomAmount);
int64_t get_amount(std::string denomAmount);

} /* namespace libzerocoin */
#endif /* DENOMINATIONS_H_ */
