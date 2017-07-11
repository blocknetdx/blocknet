#include "denomination_functions.h"
#include "reverse_iterate.h"
#include "util.h"
std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               const CAmount zerocoinBalance,
                                               const std::list<CZerocoinMint>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> DenomMap
                                               ) 
{
    nSelectedValue = 0;
    std::vector<CZerocoinMint> vSelectedMints;
    CAmount RemainingValue = nValueTarget;
    std::map<libzerocoin::CoinDenomination, CAmount> UsedDenomMap;
    for(const auto& denom : libzerocoin::zerocoinDenomList) UsedDenomMap.insert(std::pair<libzerocoin::CoinDenomination,CAmount>(denom,0));

    if (nValueTarget > zerocoinBalance) {
        LogPrintf("%s: You don't have enough Zerocoins in your wallet: Balance %d, Desired Amount %d\n", __func__, nValueTarget, zerocoinBalance);
        return vSelectedMints;
    }
    
    // Start with the Highest Denomination coin and grab coins as long as the remaining amount is greater than the
    // current denomination value
    for (auto& coin : reverse_iterate(libzerocoin::zerocoinDenomList)) {
        for (const CZerocoinMint mint : listMints) {
            if (mint.IsUsed())            continue;
            if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                vSelectedMints.push_back(mint);
                UsedDenomMap.at(coin) += mint.GetDenominationAsAmount();
                RemainingValue -= mint.GetDenominationAsAmount();
                LogPrintf("%s : Using %d : Remaining zerocoins %d\n",__func__,ZerocoinDenominationToInt(coin), RemainingValue/COIN);
            }
            if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
        }
    }
    nSelectedValue = nValueTarget - RemainingValue;
    
    // This partially fixes the amount by possibly adding 1 more zerocoin at a higher denomination,
    // should check if we need to add more than 1 of that denomination
    
    // Also should max # zerocoin spends to 4
    
    if (RemainingValue > 0) {
        LogPrintf("%s : RemainingAmount %d (in Zerocoins)\n",__func__,RemainingValue/COIN);
        // Not possible to meet exact, but we have enough zerocoins, therefore retry. Find nearest zerocoin denom to difference
        libzerocoin::CoinDenomination BiggerOrEqualToRemainingAmountDenom = libzerocoin::ZQ_ERROR;
        for (auto& coin : reverse_iterate(libzerocoin::zerocoinDenomList)) {
            if (ZerocoinDenominationToAmount(coin) > RemainingValue) {
                // Check we have enough coins at the denomination
                if (UsedDenomMap.at(coin) < DenomMap.at(coin)) {
                    BiggerOrEqualToRemainingAmountDenom = coin;
                }
            }
        }
        LogPrintf("%s : Will add %d zerocoins and retry\n",__func__,libzerocoin::ZerocoinDenominationToInt(BiggerOrEqualToRemainingAmountDenom));
        
        RemainingValue = nValueTarget;
        vSelectedMints.clear();
        
        for (auto& coin : reverse_iterate(libzerocoin::zerocoinDenomList)) {
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed())            continue;
                if (RemainingValue >= ZerocoinDenominationToAmount(coin) && coin == mint.GetDenomination()) {
                    vSelectedMints.push_back(mint);
                    UsedDenomMap.at(coin) += mint.GetDenominationAsAmount();
                    RemainingValue -= mint.GetDenominationAsAmount();
                    LogPrintf("%s : Using %d : Remaining %d zerocoins\n",__func__,libzerocoin::ZerocoinDenominationToInt(coin), RemainingValue/COIN);
                }
                if (RemainingValue < ZerocoinDenominationToAmount(coin)) break;
            }
            // Add the extra Denom here so we will have a positive RemainingValue now
            for (const CZerocoinMint mint : listMints) {
                if (mint.IsUsed())            continue;
                if ((coin == BiggerOrEqualToRemainingAmountDenom) && (coin == mint.GetDenomination()) &&
                    (BiggerOrEqualToRemainingAmountDenom != libzerocoin::ZQ_ERROR)) {
                    vSelectedMints.push_back(mint);
                    RemainingValue -= mint.GetDenominationAsAmount();
                    LogPrintf("%s : Using %d : Remaining %d zerocoins\n",__func__,libzerocoin::ZerocoinDenominationToInt(coin), RemainingValue/COIN);
                    break;
                }
            }
        }
        nSelectedValue = nValueTarget - RemainingValue;
    }
    LogPrintf("%s: Remaining %d, Fulfilled %d, Desired Amount %d\n", __func__, RemainingValue, nSelectedValue, nValueTarget);
    return vSelectedMints;
}
