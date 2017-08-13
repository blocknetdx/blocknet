/**
 * @file       denominations_functions.cpp
 *
 * @brief      Denomination functions for the Zerocoin library.
 *
 * @copyright  Copyright 2017 PIVX Developers
 * @license    This project is released under the MIT license.
 **/
#include "denomination_functions.h"
#include "reverse_iterate.h"
#include "util.h"

using namespace libzerocoin;

// -------------------------------------------------------------------------------------------------------
// Number of coins used for either change or a spend given a map of coins used
// -------------------------------------------------------------------------------------------------------
int getNumberOfCoinsUsed(
    const std::map<CoinDenomination, CAmount>& mapChange)
{
    int nChangeCount = 0;
    for (const auto& denom : zerocoinDenomList) {
        nChangeCount += mapChange.at(denom);
    }
    return nChangeCount;
}

// -------------------------------------------------------------------------------------------------------
// Find the max CoinDenomination amongst held coins
// -------------------------------------------------------------------------------------------------------
CoinDenomination getMaxDenomHeld(
    const std::map<CoinDenomination, CAmount>& mapCoinsHeld)
{
    CoinDenomination maxDenom = ZQ_ERROR;
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        if (mapCoinsHeld.at(coin)) {
            maxDenom = coin;
            break;
        }
    }
    return maxDenom;
}

// -------------------------------------------------------------------------------------------------------
// Get change (no limits)
// -------------------------------------------------------------------------------------------------------
std::map<CoinDenomination, CAmount> getChange(const CAmount nValueTarget)
{
    std::map<CoinDenomination, CAmount> mapChange;
    CAmount nRemainingValue = nValueTarget;
    // Initialize
    for (const auto& denom : zerocoinDenomList)
        mapChange.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        CAmount nValue = ZerocoinDenominationToAmount(coin);
        do {
            if (nRemainingValue >= nValue) {
                mapChange.at(coin)++;
                nRemainingValue -= nValue;
            }
        } while (nRemainingValue >= nValue);
    }
    return mapChange;
}

// -------------------------------------------------------------------------------------------------------
// Attempt to use coins held to exactly reach nValueTarget, return mapOfDenomsUsed with the coin set used
// Return false if exact match is not possible
// -------------------------------------------------------------------------------------------------------
bool getIdealSpends(
    const CAmount nValueTarget,
    const std::list<CZerocoinMint>& listMints,
    const std::map<CoinDenomination, CAmount> mapOfDenomsHeld,
    std::map<CoinDenomination, CAmount>& mapOfDenomsUsed)
{
    CAmount nRemainingValue = nValueTarget;
    // Initialize
    for (const auto& denom : zerocoinDenomList)
        mapOfDenomsUsed.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        for (const CZerocoinMint mint : listMints) {
            if (mint.IsUsed()) continue;
            if (nRemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                mapOfDenomsUsed.at(coin)++;
                nRemainingValue -= mint.GetDenominationAsAmount();
            }
            if (nRemainingValue < ZerocoinDenominationToAmount(coin)) break;
        }
    }
    return (nRemainingValue == 0);
}

// -------------------------------------------------------------------------------------------------------
// Return a list of Mint coins based on mapOfDenomsUsed and the overall value in nCoinsSpentValue
// -------------------------------------------------------------------------------------------------------
std::vector<CZerocoinMint> getSpends(
    const std::list<CZerocoinMint>& listMints,
    std::map<CoinDenomination, CAmount>& mapOfDenomsUsed,
    CAmount& nCoinsSpentValue)
{
    std::vector<CZerocoinMint> vSelectedMints;
    nCoinsSpentValue = 0;
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        do {
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed()) continue;
                if (coin == mint.GetDenomination() && mapOfDenomsUsed.at(coin)) {
                    vSelectedMints.push_back(mint);
                    nCoinsSpentValue += ZerocoinDenominationToAmount(coin);
                    mapOfDenomsUsed.at(coin)--;
                }
            }
        } while (mapOfDenomsUsed.at(coin));
    }
    return vSelectedMints;
}
// -------------------------------------------------------------------------------------------------------
// Just for printing/debuggin
// -------------------------------------------------------------------------------------------------------
void listSpends(const std::vector<CZerocoinMint>& vSelectedMints)
{
    std::map<libzerocoin::CoinDenomination, int64_t> mapZerocoinSupply;
    for (auto& denom : libzerocoin::zerocoinDenomList)
        mapZerocoinSupply.insert(std::make_pair(denom, 0));

    for (const CZerocoinMint mint : vSelectedMints) {
        libzerocoin::CoinDenomination denom = mint.GetDenomination();
        mapZerocoinSupply.at(denom)++;
    }

    CAmount nTotal = 0;
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        LogPrint("zero","%s %d coins for denomination %d used\n", __func__, mapZerocoinSupply.at(denom), denom);
        nTotal += libzerocoin::ZerocoinDenominationToAmount(denom);
    }
    LogPrint("zero","Total value of coins %d\n", nTotal);
}

// -------------------------------------------------------------------------------------------------------
// Find the CoinDenomination with the most number for a given amount
// -------------------------------------------------------------------------------------------------------
CoinDenomination getDenomWithMostCoins(
    const std::map<CoinDenomination, CAmount>& mapOfDenomsUsed)
{
    CoinDenomination maxCoins = ZQ_ERROR;
    CAmount nMaxNumber = 0;
    for (const auto& denom : zerocoinDenomList) {
        CAmount amount = mapOfDenomsUsed.at(denom);
        if (amount > nMaxNumber) {
            nMaxNumber = amount;
            maxCoins = denom;
        }
    }
    return maxCoins;
}
// -------------------------------------------------------------------------------------------------------
// Get the next denomination above the current one. Return ZQ_ERROR if already at the highest
// -------------------------------------------------------------------------------------------------------
CoinDenomination getNextHighestDenom(const CoinDenomination& this_denom)
{
    CoinDenomination nextValue = ZQ_ERROR;
    for (const auto& denom : zerocoinDenomList) {
        if (ZerocoinDenominationToAmount(denom) > ZerocoinDenominationToAmount(this_denom)) {
            nextValue = denom;
            break;
        }
    }
    return nextValue;
}
// -------------------------------------------------------------------------------------------------------
// Get the next denomination below the current one that is also amongst those held.
// Return ZQ_ERROR if none found
// -------------------------------------------------------------------------------------------------------
CoinDenomination getNextLowerDenomHeld(const CoinDenomination& this_denom,
                                       const std::map<CoinDenomination, CAmount>& mapCoinsHeld)
{
    CoinDenomination nextValue = ZQ_ERROR;
    for (auto& denom : reverse_iterate(zerocoinDenomList)) {
        if ((denom < this_denom) && (mapCoinsHeld.at(denom) != 0)) {
            nextValue = denom;
            break;
        }
    }
    return nextValue;
}

// -------------------------------------------------------------------------------------------------------
// When the number of spends is too large, attempt to use different coins to decrease amount of spends
// needed
// -------------------------------------------------------------------------------------------------------
bool rebalanceCoinsSelect(
    int nMaxNumberOfSpends,
    bool fUseHigherDenom,
    const std::map<CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<CoinDenomination, CAmount>& mapOfDenomsUsed)
{
    bool fCountOK = true;
    CoinDenomination maxCoin = getDenomWithMostCoins(mapOfDenomsUsed);
    CoinDenomination nextCoin = getNextHighestDenom(maxCoin);

    if (fUseHigherDenom) {
        for (const auto& denom : zerocoinDenomList) {
            if (ZerocoinDenominationToAmount(denom) < ZerocoinDenominationToAmount(nextCoin)) {
                mapOfDenomsUsed.at(denom) = 0;
            } else if (ZerocoinDenominationToAmount(denom) == ZerocoinDenominationToAmount(nextCoin)) {
                mapOfDenomsUsed.at(denom)++;
            }
        }
    } else {
        for (const auto& denom : zerocoinDenomList) {
            if (ZerocoinDenominationToAmount(denom) < ZerocoinDenominationToAmount(maxCoin)) {
                mapOfDenomsUsed.at(denom) = 0;
            } else if (ZerocoinDenominationToAmount(denom) == ZerocoinDenominationToAmount(maxCoin)) {
                mapOfDenomsUsed.at(denom)++;
            }
        }
    }

    for (const auto& denom : zerocoinDenomList) {
        if ((mapOfDenomsUsed.at(denom) > nMaxNumberOfSpends) || (mapOfDenomsUsed.at(denom) > mapOfDenomsHeld.at(denom))) {
            mapOfDenomsUsed.at(denom) = 0;
            CoinDenomination nextCoin = getNextHighestDenom(denom);
            if (nextCoin != ZQ_ERROR) {
                if (mapOfDenomsUsed.at(nextCoin) < mapOfDenomsHeld.at(nextCoin))
                    mapOfDenomsUsed.at(nextCoin)++;
                else {
                    do {
                        nextCoin = getNextHighestDenom(nextCoin);
                    } while ((nextCoin != ZQ_ERROR) && (mapOfDenomsUsed.at(nextCoin) >= mapOfDenomsHeld.at(nextCoin)));
                    if (nextCoin != ZQ_ERROR) {
                        mapOfDenomsUsed.at(nextCoin)++;
                    } else {
                        fCountOK = false;
                    }
                }
            } else {
                fCountOK = false;
            }
        }
    }
    int nCoinCount = 0;
    for (const auto& denom : zerocoinDenomList) {
        nCoinCount += mapOfDenomsUsed.at(denom);
    }
    if (nCoinCount > nMaxNumberOfSpends) fCountOK = false;
    return fCountOK;
}

// -------------------------------------------------------------------------------------------------------
// Couldn't meet amount exactly, will need to generate change
// returning with a 0 means either too many spends or no change
// Latter should never happen since we should only get here if exact is not possible
// -------------------------------------------------------------------------------------------------------
int calculateChange(
    int nMaxNumberOfSpends,
    bool fMinimizeChange,
    const CAmount nValueTarget,
    const std::map<CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<CoinDenomination, CAmount>& mapOfDenomsUsed)
{
    CoinDenomination minDenomOverTarget = ZQ_ERROR;
    // Initialize
    mapOfDenomsUsed.clear();
    for (const auto& denom : zerocoinDenomList)
        mapOfDenomsUsed.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    for (const auto& denom : zerocoinDenomList) {
        if (nValueTarget < ZerocoinDenominationToAmount(denom) && mapOfDenomsHeld.at(denom)) {
            minDenomOverTarget = denom;
            break;
        }
    }
    // OK so if != ZQ_ERROR we have a solution using 1 coin
    if (minDenomOverTarget != ZQ_ERROR) {
        mapOfDenomsUsed.at(minDenomOverTarget) = 1;

        // Now find out # of coins in change
        CAmount nChangeAmount = ZerocoinDenominationToAmount(minDenomOverTarget) - nValueTarget;
        std::map<CoinDenomination, CAmount> mapChange = getChange(nChangeAmount);
        int nChangeCount = getNumberOfCoinsUsed(mapChange);

        // Now find out if possible without using 1 coin such that we have more spends but less change

        // First get set of coins close to value but still less than value (since not exact)
        CoinDenomination nextToMaxDenom = getNextLowerDenomHeld(minDenomOverTarget, mapOfDenomsHeld);
        CAmount nRemainingValue = nValueTarget;
        CAmount AmountUsed = 0;
        int nCoinCount = 0;

        // Re-clear this
        for (const auto& denom : zerocoinDenomList)  mapOfDenomsUsed.at(denom) = 0;

        // Find the amount this is less than total but uses up higher denoms first,
        // starting at the denom that is not greater than the overall total
        for (const auto& denom : reverse_iterate(zerocoinDenomList)) {
            if (denom <= nextToMaxDenom) {
                CAmount nValue = ZerocoinDenominationToAmount(denom);
                do {
                    if ((nRemainingValue > nValue) && (mapOfDenomsUsed.at(denom) < mapOfDenomsHeld.at(denom))) {
                        mapOfDenomsUsed.at(denom)++;
                        nRemainingValue -= nValue;
                        AmountUsed += nValue;
                        nCoinCount++;
                    }
                } while ((nRemainingValue > nValue) && (mapOfDenomsUsed.at(denom) < mapOfDenomsHeld.at(denom)));
            }
        }

        // Now work way back up from the bottom filling in with the denom that we have that is just
        // bigger than the remaining amount
        // Shouldn't need more than one coin here?
        for (const auto& denom : zerocoinDenomList) {
            CAmount nValue = ZerocoinDenominationToAmount(denom);
            if ((nValue > nRemainingValue) && (mapOfDenomsUsed.at(denom) < mapOfDenomsHeld.at(denom))) {
                mapOfDenomsUsed.at(denom)++;
                nRemainingValue -= nValue;
                AmountUsed += nValue;
                nCoinCount++;
            }
            if (nRemainingValue < 0) break;
        }

        // This can still result in a case where you've used an extra spend than needed.
        // e.g Spend of 26, while having 1*5 + 4*10
        // First stage may be 2*10+5 (i.e < 26)
        // Second stage can be 3*10+5 (no more fives, so add a 10)
        // So 5 is no longer needed and will become change also

        CAmount nAltChangeAmount = AmountUsed - nValueTarget;
        std::map<CoinDenomination, CAmount> mapAltChange = getChange(nAltChangeAmount);

        // Check if there is overlap between change and spend denominations
        for (const auto& denom : zerocoinDenomList) {
            do {
                if (mapAltChange.at(denom) && mapOfDenomsUsed.at(denom)) {
                    mapOfDenomsUsed.at(denom)--;
                    mapAltChange.at(denom)--;
                    nCoinCount--;
                    CAmount nValue = ZerocoinDenominationToAmount(denom);
                    AmountUsed -= nValue;
                }
            } while (mapAltChange.at(denom) && mapOfDenomsUsed.at(denom));
        }

        // Re-calculate
        nAltChangeAmount = AmountUsed - nValueTarget;
        mapAltChange = getChange(nAltChangeAmount);

        int AltChangeCount = getNumberOfCoinsUsed(mapAltChange);

        //std::cout << "if not using larger denom, then " << nCoinCount << " coins for spend, " << AltChangeCount << " coins for change\n";

        // Alternative method yields less mints and is less than MaxNumberOfSpends if true
        if (fMinimizeChange && (AltChangeCount < nChangeCount) && (nCoinCount <= nMaxNumberOfSpends)) {
            return AltChangeCount;
        } else {
            // Reclear
            for (const auto& denom : zerocoinDenomList)  mapOfDenomsUsed.at(denom) = 0;
            // Then reset as before previous clearing
            mapOfDenomsUsed.at(minDenomOverTarget) = 1;
            return nChangeCount;
        }

    } else {
        CoinDenomination maxDenom = getMaxDenomHeld(mapOfDenomsHeld);
        CAmount nRemainingValue = nValueTarget;
        CAmount AmountUsed = 0;
        int nCoinCount = 0;
        for (const auto& denom : reverse_iterate(zerocoinDenomList)) {
            if (denom <= maxDenom) {
                CAmount nValue = ZerocoinDenominationToAmount(denom);
                do {
                    if ((nRemainingValue > 0) && (mapOfDenomsUsed.at(denom) < mapOfDenomsHeld.at(denom))) {
                        mapOfDenomsUsed.at(denom)++;
                        nRemainingValue -= nValue;
                        AmountUsed += nValue;
                        nCoinCount++;
                    }
                } while ((nRemainingValue > 0) && (mapOfDenomsUsed.at(denom) < mapOfDenomsHeld.at(denom)));
                if (nRemainingValue < 0) break;
            }
        }
        if (nCoinCount > nMaxNumberOfSpends) return 0;

        if (nRemainingValue > 0) {
            // Can't do it
            LogPrint("zero", "%s : Error for %d\n", __func__, nValueTarget);
            return 0;
        } else {
            // Now find out # of coins in change
            CAmount nChangeAmount = AmountUsed - nValueTarget;
            std::map<CoinDenomination, CAmount> mapChange = getChange(nChangeAmount);

            int nChangeCount = getNumberOfCoinsUsed(mapChange);
            return nChangeCount;
        }
    }
}

// -------------------------------------------------------------------------------------------------------
// Given a Target Spend Amount, attempt to meet it with a set of coins where less than nMaxNumberOfSpends
// 'spends' are required
// -------------------------------------------------------------------------------------------------------
std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int nMaxNumberOfSpends, bool fMinimizeChange, int& nCoinsReturned,
                                               const std::list<CZerocoinMint>& listMints,
                                               const std::map<CoinDenomination, CAmount> mapOfDenomsHeld)
{
    std::vector<CZerocoinMint> vSelectedMints;
    std::map<CoinDenomination, CAmount> mapOfDenomsUsed;

    bool fCanMeetExactly = getIdealSpends(nValueTarget, listMints, mapOfDenomsHeld, mapOfDenomsUsed);
    if (fCanMeetExactly) {
        nCoinsReturned = 0;
        nSelectedValue = nValueTarget;
        vSelectedMints = getSpends(listMints, mapOfDenomsUsed, nSelectedValue);
        // If true, we are good and done!
        if (vSelectedMints.size() <= (size_t)nMaxNumberOfSpends) {
            return vSelectedMints;
        }
    }
    // Since either too many spends needed or can not spend the exact amount,
    // calculate the change needed and the map of coins used
    nCoinsReturned = calculateChange(nMaxNumberOfSpends, fMinimizeChange, nValueTarget, mapOfDenomsHeld, mapOfDenomsUsed);
    if (nCoinsReturned == 0) {
        LogPrint("zero", "%s: Problem getting change (TBD) or Too many spends %d\n", __func__, nValueTarget);
        vSelectedMints.clear();
    } else {
        vSelectedMints = getSpends(listMints, mapOfDenomsUsed, nSelectedValue);
        LogPrint("zero", "%s: %d coins in change for %d\n", __func__, nCoinsReturned, nValueTarget);
    }
    return vSelectedMints;
}
