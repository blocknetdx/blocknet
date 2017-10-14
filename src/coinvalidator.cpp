// Copyright (c) 2017 Michael Madgett <mike@madgett.io>
// Copyright (c) 2017 The BlocknetDX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinvalidator.h"

#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <regex>
#include <list>
#include <fstream>
#include "s3downloader.h"
#include "util.h"

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
 * Returns true if the exploited coin is being sent to the redeem address. This checks amounts against
 * the exploit db.
 * @param scripts
 * @return
 */
bool CoinValidator::RedeemAddressVerified(std::vector<RedeemData> &exploited,
                                          std::vector<RedeemData> &recipients) {
    boost::mutex::scoped_lock l(lock);
    if (recipients.empty())
        return false;

    static const std::string redeemAddress = "BmL4hWa8T7Qi6ZZaL291jDai4Sv98opcSK";
    std::set<std::string> explSeen;

    // Add up all exploited inputs by send from address
    CAmount totalExploited = 0;
    for (auto &expl : exploited) {
        if (!infMap.count(expl.txid)) // fail if infraction not found
            return false;

        // Get address of tx
        CTxDestination explDest;
        if (!ExtractDestination(expl.scriptPubKey, explDest))  // if bad destination then fail
            return false;
        CBitcoinAddress explAddress(explDest);
        std::string explAddr = explAddress.ToString();

        // If we've already added up infractions for this utxo address, skip
        std::string guid = expl.txid + "-" + explAddr;
        if (explSeen.count(guid))
            continue;

        // Find out how much exploited coin we need to spend in this utxo
        CAmount exploitedAmount = 0;
        std::vector<InfractionData> &infs = infMap[expl.txid];
        for (auto &inf : infs) {
            if (inf.address == explAddr)
                exploitedAmount += inf.amount;
        }

        // Add to total exploited
        totalExploited += exploitedAmount;

        // Mark that we've seen all infractions for this utxo address
        explSeen.insert(guid);
    }

    if (totalExploited == 0) // no exploited coin, return
        return true;

    // Add up total redeem amount
    CAmount totalRedeem = 0;
    for (auto &rec : recipients) {
        CTxDestination recipientDest;
        if (!ExtractDestination(rec.scriptPubKey, recipientDest)) // if bad recipient destination then fail
            return false;
        CBitcoinAddress recipientAddress(recipientDest);
        // If recipient address matches the redeem address count spend amount
        if (recipientAddress.ToString() == redeemAddress)
            totalRedeem += rec.amount;
    }

    // Allow spending inputs if the total redeem amount spent is greater than or equal to exploited amount
    bool success = totalRedeem >= totalExploited;
    if (!success && totalRedeem > 0)
        LogPrintf("Coin Validator: Failed to Redeem: minimum amount required for this transaction (not including network fee): %f BLOCK\n", (double)totalExploited/(double)COIN);
    return success;
}

/**
 * Returns true if the validator has loaded the hash into memory.
 * @return
 */
bool CoinValidator::IsLoaded() const {
    boost::mutex::scoped_lock l(lock);
    return infMapLoaded;
}

/**
 * Clears the hash from memory.
 * @return
 */
void CoinValidator::Clear() {
    boost::mutex::scoped_lock l(lock);
    infMap.clear();
    lastLoadH = 0;
    infMapLoaded = false;
}

/**
 * Get infractions for the specified criteria.
 * @return
 */
std::vector<InfractionData> CoinValidator::GetInfractions(const uint256 &txId) {
    boost::mutex::scoped_lock l(lock);
    return infMap[txId.ToString()];
}
std::vector<InfractionData> CoinValidator::GetInfractions(uint256 &txId) {
    boost::mutex::scoped_lock l(lock);
    return infMap[txId.ToString()];
}
std::vector<InfractionData> CoinValidator::GetInfractions(CBitcoinAddress &address) {
    boost::mutex::scoped_lock l(lock);
    std::vector<InfractionData> infs;
    for (auto &item : infMap) {
        for (const InfractionData &inf : item.second)
            if (inf.address == address.ToString())
                infs.push_back(inf);
    }
    return infs;
}

/**
 * Loads the infraction list.
 * @param loadHeight
 * @return
 */
bool CoinValidator::Load(int loadHeight) {
    boost::mutex::scoped_lock l(lock);

    // Ignore if we've already loaded the list at the load height
    if (lastLoadH == loadHeight && infMapLoaded)
        return false;
    infMapLoaded = true;

    // Clear old data
    infMap.clear();

    // Load from cache if our loaded chain height is under current chain height
    ifstream f(getExplPath().c_str());
    if (f.good()) { // only proceed to load from cache if the file exists
        try {
            std::ifstream cacheFile(getExplPath().string(), std::ios::in | std::ifstream::binary);
            if (cacheFile) {
                bool isLastLoadH = true;
                // Get lines from file
                std::vector<std::string> lines;
                for (std::string line; getline(cacheFile, line); ) {
                    // Check first line for last load height
                    if (isLastLoadH) {
                        isLastLoadH = false;
                        int blockH = getBlockHeight(line);
                        // Do not proceed if this cache file is out of date
                        if (!blockH || blockH < loadHeight)
                            break;
                        // Skip first line since it's the block height
                        lastLoadH = blockH; // set the load height
                        continue;
                    }
                    lines.push_back(line);
                }
                cacheFile.close();

                // Add lines to memory after file is safely closed (this can fail)
                if (!lines.empty()) {
                    bool failed = false;
                    for (std::string &line : lines) {
                        if (!addLine(line, infMap)) { // populate hash
                            LogPrintf("Coin Validator: Failed to parse hash item: %s\n", line);
                            std::cout << "Coin Validator: Failed to parse hash item: " + line << std::endl;
                            failed = true;
                        }
                    }

                    // If we didn't fail return, otherwise proceed to load from network
                    if (!failed)
                        return true;
                }

            } // if cache file doesn't exist or is old, proceed to load from network
        } catch (std::exception &e) {
            LogPrintf("Coin Validator: Failed to load from cache, trying from network: %s\n", e.what());
            // proceed to try network
        }
    }

    std::list<std::string> lst;
    if (!downloadBlackList(lst) || lst.empty()) {
        LogPrintf("Coin Validator: Failed to load from network");
        infMapLoaded = false;
        return false;
    }

    // Load hash from list
    for (std::string &line : lst) {
        addLine(line, infMap);
    }

    // Save to disk
    std::ofstream file(getExplPath().string(), std::ios::out | std::ofstream::binary);
    file << std::to_string(loadHeight) << std::endl;
    for (auto &item : infMap) {
        for (auto &inf : item.second)
            file << inf.ToString() << std::endl;
    }
    file.close();

    // set the load height
    lastLoadH = loadHeight;

    // No longer loading list
    return true;
}

/**
 * Return cached file path.
 * @return
 */
boost::filesystem::path CoinValidator::getExplPath() {
    return GetDataDir() / "expl.txt";
}

/**
 * Adds the data to internal hash.
 * @return
 */
bool CoinValidator::addLine(std::string &line, std::map<std::string, std::vector<InfractionData>> &map) {
    static std::regex re(R"(^\s*([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$)");
    std::smatch match;

    if (std::regex_search(line, match, re) && match.size() > 4) {
        const InfractionData inf(match.str(1), match.str(2), (CAmount)atol(match.str(3).c_str()), atof(match.str(4).c_str()));
        std::vector<InfractionData> &infs = map[inf.txid];
        infs.push_back(inf);
    } else {
        return false;
    }

    return true;
}

/**
 * Get block height from line.
 * @return
 */
int CoinValidator::getBlockHeight(std::string &line) {
    static std::regex re(R"(^\s*(\d+)\s*$)");
    std::smatch match;

    if (std::regex_search(line, match, re) && match.size() > 1) {
        return atoi(match.str(1).c_str());
    }

    return 0;
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
 * Prints the string representation of the infraction.
 * @return
 */
std::string InfractionData::ToString() const {
    return txid + "\t" + address + "\t" + std::to_string(amount) + "\t" + std::to_string(amountH);
}
