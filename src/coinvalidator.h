// Copyright (c) 2017 Michael Madgett
// Copyright (c) 2017 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKDX_COINVALIDATOR_H
#define BLOCKDX_COINVALIDATOR_H

#include <boost/thread/mutex.hpp>
#include <boost/filesystem/path.hpp>
#include <script/script.h>
#include "uint256.h"
#include "amount.h"
#include "base58.h"

/**
 * Stores infraction data.
 */
struct InfractionData {
    std::string txid;
    std::string address;
    CAmount amount;
    double amountH;
    InfractionData(std::string t, std::string a, CAmount amt, double amtd) {
        txid = std::move(t); address = std::move(a); amount = amt; amountH = amtd;
    }
    std::string ToString() const;
};

/**
 * Stores redeem data.
 */
struct RedeemData {
    std::string txid;
    CScript scriptPubKey;
    CAmount amount;
    RedeemData(std::string t, CScript a, CAmount amt) {
        txid = std::move(t); scriptPubKey = a; amount = amt;
    }
};

/**
 * Manages coin infractions.
 */
class CoinValidator {
public:
    static const int CHAIN_HEIGHT;
    bool IsCoinValid(const uint256 &txId) const;
    bool IsCoinValid(uint256 &txId) const;
    bool RedeemAddressVerified(std::vector<RedeemData> &exploited,
                               std::vector<RedeemData> &recipients);
    bool Load(int loadHeight);
    bool LoadStatic();
    bool IsLoaded() const;
    void Clear();
    std::vector<InfractionData> GetInfractions(const uint256 &txId);
    std::vector<InfractionData> GetInfractions(uint256 &txId);
    std::vector<InfractionData> GetInfractions(CBitcoinAddress &address);
    static std::string AmountToString(double amount);
    static CoinValidator& instance();
private:
    std::map<std::string, std::vector<InfractionData>> infMap; // Store infractions in memory
    bool infMapLoaded = false;
    int lastLoadH = 0;
    bool downloadErr = false;
    mutable boost::mutex lock;
    boost::filesystem::path getExplPath();
    bool addLine(std::string &line, std::map<std::string, std::vector<InfractionData>> &map);
    int getBlockHeight(std::string &line);
    bool downloadList(std::list<std::string> &lst, std::string &err);
    std::vector<string> getExplList();
};

#endif //BLOCKDX_COINVALIDATOR_H