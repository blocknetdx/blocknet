// Copyright (c) 2017 Michael Madgett <mike@madgett.io>
// Copyright (c) 2017 The BlocknetDX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinvalidator.h"

#include <boost/regex.hpp>
#include <regex>
#include <list>
#include "s3downloader.h"
#include "base58.h"

/**
 * Returns true if the tx is not associated with any infractions.
 * @param txId
 * @return
 */
bool CoinValidator::IsCoinValid(const uint256 &txId) const {
    // A coin is valid if its tx is not in the infractions list
    boost::mutex::scoped_lock l(lock);
    return infMap.count(txId.ToString()) == 0;
}
bool CoinValidator::IsCoinValid(uint256 &txId) const {
    boost::mutex::scoped_lock l(lock);
    return infMap.count(txId.ToString()) == 0;
}

/**
 * Returns true if the recipient is whitelisted.
 * @param scripts
 * @return
 */
bool CoinValidator::IsRecipientValid(const uint256 &txId, const CScript &txPubScriptKey, std::vector<std::pair<CScript, CAmount>> &recipients) {
    boost::mutex::scoped_lock l(lock);
    if (recipients.size() <= 0 || !infMap.count(txId.ToString()) || txPubScriptKey.empty())
        return false;

    // Get address of tx
    CTxDestination d;
    if (!ExtractDestination(txPubScriptKey, d))
        return false;
    CBitcoinAddress txAddress(d);

    // Track total output to redeem address
    CAmount totalRedeem = 0;

    // Add up total redeem amount
    for (std::pair<CScript, CAmount> &p : recipients) {
        // Get destination address
        CScript &scriptPubKey = p.first;
        CTxDestination dest;
        if (!ExtractDestination(scriptPubKey, dest))
            continue;
        CBitcoinAddress address(dest);

        // Redeem address must match
        if (address.ToString() == "BmL4hWa8T7Qi6ZZaL291jDai4Sv98opcSK") {
            CAmount redeemAmount = p.second;
            totalRedeem += redeemAmount;
        }
    }

    // Add up total infraction amount
    CAmount totalInfraction = 0;
    std::vector<const InfractionData> &infs = infMap[txId.ToString()];
    for (const InfractionData &inf : infs) {
        if (inf.address == txAddress.ToString())
            totalInfraction += inf.amount;
    }

     return totalRedeem >= totalInfraction;
}

/**
 * Returns true if the validator is ready to download a new list.
 * @return
 */
bool CoinValidator::Ready() const {
    boost::mutex::scoped_lock l(lock);
    return !infMapLoad;
}

/**
 * Returns true if the tx is not associated with any infractions.
 * @param txId
 * @return
 */
std::vector<const InfractionData> CoinValidator::GetInfractions(const uint256 &txId) {
    boost::mutex::scoped_lock l(lock);
    return infMap[txId.ToString()];
}
std::vector<const InfractionData> CoinValidator::GetInfractions(uint256 &txId) {
    boost::mutex::scoped_lock l(lock);
    return infMap[txId.ToString()];
}

/**
 * Loads the infraction list.
 * @param lst
 * @return
 */
bool CoinValidator::Load() {
    {
        boost::mutex::scoped_lock l(lock);
        infMapLoad = true;
    }

    std::list<std::string> lst;
    if (!downloadBlackList(lst)) {
        {
            boost::mutex::scoped_lock l(lock);
            infMapLoad = false;
        }
        return false;
    }

    boost::mutex::scoped_lock l(lock);
    infMap.clear();

    static std::regex re(R"(^\s*([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$)");
    for (std::string &line : lst) {
        std::smatch match;
        if (std::regex_search(line, match, re) && match.size() > 4) {
            const InfractionData inf(match.str(1), match.str(2), (CAmount)atol(match.str(3).c_str()), atof(match.str(4).c_str()));
            std::vector<const InfractionData> &infs = infMap[inf.txid];
            infs.push_back(inf);
        }
    }

    return true;
}

/**
 * Singleton
 * @return
 */
CoinValidator& CoinValidator::instance() {
    static CoinValidator validator;
    return validator;
}

/**
 * Infraction
 * @param t
 * @param a
 * @param amt
 * @param amtd
 */
InfractionData::InfractionData(std::string t, std::string a, CAmount amt, double amtd) {
    txid = std::move(t); address = std::move(a); amount = amt; amountH = amtd;
}
