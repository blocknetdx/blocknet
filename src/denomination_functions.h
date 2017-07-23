/**
 * @file       denominations_functions.h
 *
 * @brief      Denomination functions for the Zerocoin library.
 *
 * @copyright  Copyright 2017 PIVX Developers
 * @license    This project is released under the MIT license.
 **/
#include "libzerocoin/Denominations.h"
#include "primitives/zerocoin.h"
#include <list>
#include <map>
std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int nMaxNumberOfSpends,
                                               const std::list<CZerocoinMint>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> mapDenomsHeld
                                               );
void listSpends(const std::vector<CZerocoinMint>& vSelectedMints);
