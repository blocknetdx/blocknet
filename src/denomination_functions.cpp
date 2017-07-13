#include "denomination_functions.h"
#include "reverse_iterate.h"
#include "util.h"

using namespace libzerocoin;

// Find the CoinDenomination with the most number for a given amount
CoinDenomination getDenomWithMostCoins(
    std::map<CoinDenomination, CAmount>& UsedDenomMap)
{
    CoinDenomination maxCoins = ZQ_ERROR;
    CAmount maxNumber = 0;
    for (const auto& denom : zerocoinDenomList) {
        CAmount amount = UsedDenomMap.at(denom);
        if (amount > maxNumber) {
            maxNumber = amount;
            maxCoins = denom;
        }
    }
    return maxCoins;
}
// Get the next denomination above the current one. Return ZQ_ERROR if already at the highest
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

// Use higher order denominations to keep total set size to be < maxNumberOfSpends
bool rebalanceCoins(
    int maxNumberOfSpends,
    const std::map<CoinDenomination, CAmount>& DenomMap,
    std::map<CoinDenomination, CAmount>& UsedDenomMap)
{
    bool count_ok = true;
    CoinDenomination maxCoin = getDenomWithMostCoins(UsedDenomMap);
    CoinDenomination nextCoin = getNextHighestDenom(maxCoin);

    for (const auto& denom : zerocoinDenomList) {
        if (ZerocoinDenominationToAmount(denom) < ZerocoinDenominationToAmount(nextCoin)) {
            UsedDenomMap.at(denom) = 0;
        } else if (ZerocoinDenominationToAmount(denom) == ZerocoinDenominationToAmount(nextCoin)) {
            UsedDenomMap.at(denom)++;
        }
    }

    int i = 0;
    for (const auto& denom : zerocoinDenomList) {
        if ((UsedDenomMap.at(denom) > maxCoinsAtDenom[i++]) || (UsedDenomMap.at(denom) > maxNumberOfSpends)) {
            UsedDenomMap.at(denom) = 0;
            CoinDenomination nextCoin = getNextHighestDenom(denom);
            if (nextCoin != ZQ_ERROR) {
                if (UsedDenomMap.at(nextCoin) < DenomMap.at(nextCoin))
                    UsedDenomMap.at(nextCoin)++;
                else {
                    do {
                        nextCoin = getNextHighestDenom(nextCoin);
                    } while ((UsedDenomMap.at(nextCoin) >= DenomMap.at(nextCoin)) && (nextCoin != ZQ_ERROR));
                    if (nextCoin != ZQ_ERROR) {
                        UsedDenomMap.at(nextCoin)++;
                    } else {
                        count_ok = false;
                    }
                }
            } else {
                count_ok = false;
            }
        }
    }
    int coin_count = 0;
    for (const auto& denom : zerocoinDenomList) {
        coin_count += UsedDenomMap.at(denom);
    }
    if (coin_count > maxNumberOfSpends) count_ok = false;
    return count_ok;
}

std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue, int maxNumberOfSpends, const std::list<CZerocoinMint>& listMints, const std::map<CoinDenomination, CAmount> DenomMap)
{
    nSelectedValue = 0;
    std::vector<CZerocoinMint> vSelectedMints;
    CAmount RemainingValue = nValueTarget;
    std::map<CoinDenomination, CAmount> UsedDenomMap;
    for (const auto& denom : zerocoinDenomList)
        UsedDenomMap.insert(std::pair<CoinDenomination, CAmount>(denom, 0));

    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(zerocoinDenomList)) {
        for (const CZerocoinMint mint : listMints) {
            if (mint.IsUsed()) continue;
            if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                vSelectedMints.push_back(mint);
                UsedDenomMap.at(coin) += mint.GetDenominationAsAmount();
                RemainingValue -= mint.GetDenominationAsAmount();
                LogPrintf("%s : Using %d : Remaining zerocoins %d\n", __func__, ZerocoinDenominationToInt(coin), RemainingValue / COIN);
            }
            if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
        }
    }
    nSelectedValue = nValueTarget - RemainingValue;

    if (RemainingValue == 0) {
        // If true, we are good and done!
        if (vSelectedMints.size() <= (size_t)maxNumberOfSpends) {
            return vSelectedMints;
        } else {
            vSelectedMints.clear();
            // Need to figure out alternative thing to do with too much potential spends here
            bool count_ok = rebalanceCoins(maxNumberOfSpends, DenomMap, UsedDenomMap);
            // Now need to selectMints based on UsedDenomMap (SPOCK) TBD
            //.....
            if (count_ok) {
                LogPrintf("%s : Redistributing use of coins (TBD)\n", __func__);
            } else {
                LogPrintf("%s : Failed to find coin set\n", __func__);
            }
            return vSelectedMints;
        }
    } else {
        // This partially fixes the amount by possibly adding 1 more zerocoin at a higher denomination,
        // should check if we need to add more than 1 of that denomination
        LogPrintf("%s : RemainingAmount %d (in Zerocoins)\n", __func__, RemainingValue / COIN);
        // Not possible to meet exact, but we have enough zerocoins, therefore retry. Find nearest zerocoin denom to difference
        CoinDenomination BiggerOrEqualToRemainingAmountDenom = ZQ_ERROR;
        for (auto& coin : reverse_iterate(zerocoinDenomList)) {
            if (ZerocoinDenominationToAmount(coin) > RemainingValue) {
                // Check we have enough coins at the denomination
                //std::cout << "For " << ZerocoinDenominationToAmount(coin)/COIN << " used = " << UsedDenomMap.at(coin)  << " have = " << DenomMap.at(coin) << "\n";
                if (UsedDenomMap.at(coin) < DenomMap.at(coin)) {
                    BiggerOrEqualToRemainingAmountDenom = coin;
                }
            }
        }
        LogPrintf("%s : Will add %d zerocoins and retry\n", __func__, ZerocoinDenominationToInt(BiggerOrEqualToRemainingAmountDenom));

        RemainingValue = nValueTarget;
        vSelectedMints.clear();

        for (auto& coin : reverse_iterate(zerocoinDenomList)) {
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed()) continue;
                if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                    vSelectedMints.push_back(mint);
                    UsedDenomMap.at(coin) += mint.GetDenominationAsAmount();
                    RemainingValue -= mint.GetDenominationAsAmount();
                    LogPrintf("%s : Using %d : Remaining %d zerocoins\n", __func__, ZerocoinDenominationToInt(coin), RemainingValue / COIN);
                }
                if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
            }
            // Add the extra Denom here so we will have a positive RemainingValue now
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed()) continue;
                if ((coin == BiggerOrEqualToRemainingAmountDenom) && (coin == mint.GetDenomination()) &&
                    (BiggerOrEqualToRemainingAmountDenom != ZQ_ERROR)) {
                    vSelectedMints.push_back(mint);
                    RemainingValue -= mint.GetDenominationAsAmount();
                    LogPrintf("%s : Using %d : Remaining %d zerocoins\n", __func__, ZerocoinDenominationToInt(coin), RemainingValue / COIN);
                    break;
                }
            }
        }
        nSelectedValue = nValueTarget - RemainingValue;

        if (vSelectedMints.size() <= (size_t)maxNumberOfSpends) {
            // Need to take action here : TBD
        }
    }
    LogPrintf("%s: Remaining %d, Fulfilled %d, Desired Amount %d\n", __func__, RemainingValue, nSelectedValue, nValueTarget);
    return vSelectedMints;
}
