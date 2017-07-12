#include "libzerocoin/Denominations.h"
#include "primitives/zerocoin.h"
#include <list>
#include <map>
std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int maxNumberOfSpends,
                                               const std::list<CZerocoinMint>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> DenomMap
                                               );
