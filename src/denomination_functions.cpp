#include "denomination_functions.h"
#include "reverse_iterate.h"
#include "util.h"

using namespace libzerocoin;
// -------------------------------------------------------------------------------------------------------
// Attempt to use coins held to exactly reach nValueTarget, return mapUsedDenom with the coin set used
// Return false if exact match is not possible
// -------------------------------------------------------------------------------------------------------
bool getIdealSpends(
    const CAmount nValueTarget,
    const std::list<CZerocoinMint>& listMints,
    const std::map<CoinDenomination, CAmount> mapOfDenomsHeld,
    std::map<CoinDenomination, CAmount>& mapUsedDenom)
{
    CAmount RemainingValue = nValueTarget;
    // Initialize
    for (const auto& denom : zerocoinDenomList)
        mapUsedDenom.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        for (const CZerocoinMint mint : listMints) {
            if (mint.IsUsed()) continue;
            if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                mapUsedDenom.at(coin)++;
                RemainingValue -= mint.GetDenominationAsAmount();
            }
            if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
        }
    }
    return (RemainingValue == 0);
}

// -------------------------------------------------------------------------------------------------------
// Return a list of Mint coins based on mapUsedDenom and the overall value in nCoinsSpentValue
// -------------------------------------------------------------------------------------------------------
std::vector<CZerocoinMint> getSpends(
    const std::list<CZerocoinMint>& listMints,
    std::map<CoinDenomination, CAmount>& mapUsedDenom,
    CAmount& nCoinsSpentValue)
{
    std::vector<CZerocoinMint> vSelectedMints;
    nCoinsSpentValue = 0;
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        do {
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed()) continue;
                if (coin == mint.GetDenomination() && mapUsedDenom.at(coin)) {
                    vSelectedMints.push_back(mint);
                    nCoinsSpentValue += ZerocoinDenominationToAmount(coin);
                    mapUsedDenom.at(coin)--;
                }
            }
        } while (mapUsedDenom.at(coin));
    }
    return vSelectedMints;
}

// -------------------------------------------------------------------------------------------------------
// Find the CoinDenomination with the most number for a given amount
// -------------------------------------------------------------------------------------------------------
CoinDenomination getDenomWithMostCoins(
    const std::map<CoinDenomination, CAmount>& mapUsedDenom)
{
    CoinDenomination maxCoins = ZQ_ERROR;
    CAmount maxNumber = 0;
    for (const auto& denom : zerocoinDenomList) {
        CAmount amount = mapUsedDenom.at(denom);
        if (amount > maxNumber) {
            maxNumber = amount;
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
}

// -------------------------------------------------------------------------------------------------------
// When the number of spends is too large, attempt to use different coins to decrease amount of spends
// needed
// -------------------------------------------------------------------------------------------------------
bool rebalanceCoinsSelect(
    int nMaxNumberOfSpends,
    bool fUseHigherDenom,
    const std::map<CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<CoinDenomination, CAmount>& mapUsedDenom)
{
    bool fCountOK = true;
    CoinDenomination maxCoin = getDenomWithMostCoins(mapUsedDenom);
    CoinDenomination nextCoin = getNextHighestDenom(maxCoin);

    if (fUseHigherDenom) {
        for (const auto& denom : zerocoinDenomList) {
            if (ZerocoinDenominationToAmount(denom) < ZerocoinDenominationToAmount(nextCoin)) {
                mapUsedDenom.at(denom) = 0;
            } else if (ZerocoinDenominationToAmount(denom) == ZerocoinDenominationToAmount(nextCoin)) {
                mapUsedDenom.at(denom)++;
            }
        }
    } else {
        for (const auto& denom : zerocoinDenomList) {
            if (ZerocoinDenominationToAmount(denom) < ZerocoinDenominationToAmount(maxCoin)) {
                mapUsedDenom.at(denom) = 0;
            } else if (ZerocoinDenominationToAmount(denom) == ZerocoinDenominationToAmount(maxCoin)) {
                mapUsedDenom.at(denom)++;
            }
        }
    }

    for (const auto& denom : zerocoinDenomList) {
        if ((mapUsedDenom.at(denom) > nMaxNumberOfSpends) || (mapUsedDenom.at(denom) > mapOfDenomsHeld.at(denom))) {
            mapUsedDenom.at(denom) = 0;
            CoinDenomination nextCoin = getNextHighestDenom(denom);
            if (nextCoin != ZQ_ERROR) {
                if (mapUsedDenom.at(nextCoin) < mapOfDenomsHeld.at(nextCoin))
                    mapUsedDenom.at(nextCoin)++;
                else {
                    do {
                        nextCoin = getNextHighestDenom(nextCoin);
                    } while ((nextCoin != ZQ_ERROR) && (mapUsedDenom.at(nextCoin) >= mapOfDenomsHeld.at(nextCoin)));
                    if (nextCoin != ZQ_ERROR) {
                        mapUsedDenom.at(nextCoin)++;
                    } else {
                        fCountOK = false;
                    }
                }
            } else {
                fCountOK = false;
            }
        }
    }
    int coin_count = 0;
    for (const auto& denom : zerocoinDenomList) {
        coin_count += mapUsedDenom.at(denom);
    }
    if (coin_count > nMaxNumberOfSpends) fCountOK = false;
    return fCountOK;
}


// -------------------------------------------------------------------------------------------------------
// Given a Target Spend Amount, attempt to meet it with a set of coins where less than nMaxNumberOfSpends
// 'spends' are required
// -------------------------------------------------------------------------------------------------------

std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue, int nMaxNumberOfSpends, const std::list<CZerocoinMint>& listMints, const std::map<CoinDenomination, CAmount> mapOfDenomsHeld)
{
    nSelectedValue = 0;
    std::vector<CZerocoinMint> vSelectedMints;
    CAmount RemainingValue = nValueTarget;
    std::map<CoinDenomination, CAmount> mapUsedDenom;
    for (const auto& denom : zerocoinDenomList)
        mapUsedDenom.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        for (const CZerocoinMint mint : listMints) {
            if (mint.IsUsed()) continue;
            if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                vSelectedMints.push_back(mint);
                mapUsedDenom.at(coin)++;
                RemainingValue -= mint.GetDenominationAsAmount();
                LogPrint("zero","%s : Using %d : Remaining zerocoins %d\n", __func__, ZerocoinDenominationToInt(coin), RemainingValue / COIN);
            }
            if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
        }
    }
    nSelectedValue = nValueTarget - RemainingValue;

    if (RemainingValue == 0) {
        // If true, we are good and done!
        if (vSelectedMints.size() <= (size_t)nMaxNumberOfSpends) {
            return vSelectedMints;
        } else {
            vSelectedMints.clear();
            // Need to figure out alternative thing to do with too much potential spends here
            std::map<CoinDenomination, CAmount> mapSavedDenoms = mapUsedDenom;
            bool fCountOK = rebalanceCoinsSelect(nMaxNumberOfSpends, false, mapOfDenomsHeld, mapUsedDenom);
            // Now need to selectMints based on mapUsedDenom (SPOCK) TBD
            //.....
            if (fCountOK) {
                LogPrint("zero","%s : Redistributing use of coins (TBD)\n", __func__);
                vSelectedMints = getSpends(listMints, mapUsedDenom, nSelectedValue);
            } else {
                // retry
                fCountOK = rebalanceCoinsSelect(nMaxNumberOfSpends, true, mapOfDenomsHeld, mapSavedDenoms);
                mapUsedDenom = mapSavedDenoms;
                if (!fCountOK) {
                    LogPrint("zero","%s : Failed to find coin set\n", __func__);
                } else {
                    vSelectedMints = getSpends(listMints, mapUsedDenom, nSelectedValue);
                }
            }
            return vSelectedMints;
        }
    } else {
        std::map<CoinDenomination, CAmount> mapSavedDenoms = mapUsedDenom;
        bool fCountOK = rebalanceCoinsSelect(nMaxNumberOfSpends, false, mapOfDenomsHeld, mapUsedDenom);
        if (!fCountOK) {
            fCountOK = rebalanceCoinsSelect(nMaxNumberOfSpends, true, mapOfDenomsHeld, mapSavedDenoms);
            mapUsedDenom = mapSavedDenoms;
        }
        if (fCountOK) {
            vSelectedMints = getSpends(listMints, mapUsedDenom, nSelectedValue);
        } else {
            vSelectedMints.clear();
        }
    }
    LogPrintf("zero","%s: Remaining %d, Fulfilled %d, Desired Amount %d\n", __func__, RemainingValue, nSelectedValue, nValueTarget);
    return vSelectedMints;
}
