// Copyright (c) 2017 Michael Madgett <mike@madgett.io>
// Copyright (c) 2017 The BlocknetDX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKDX_COINVALIDATOR_H
#define BLOCKDX_COINVALIDATOR_H

#include <boost/thread/mutex.hpp>
#include "uint256.h"
#include "amount.h"

/**
 * Stores infraction data.
 */
struct InfractionData {
    std::string txid;
    std::string address;
    CAmount amount;
    double amountH;
    InfractionData(std::string t, std::string a, CAmount amt, double amtd);
};

/**
 * Manages coin infractions.
 */
class CoinValidator {
public:
    bool IsCoinValid(const uint256 &txId) const;
    bool IsCoinValid(uint256 &txId) const;
    bool IsRecipientValid(const uint256 &txId, const CScript &txPubScriptKey, std::vector<std::pair<CScript, CAmount>> &recipients);
    bool Load();
    bool Ready() const;
    std::vector<const InfractionData> GetInfractions(const uint256 &txId);
    std::vector<const InfractionData> GetInfractions(uint256 &txId);
    static CoinValidator& instance();
private:
    std::map<std::string, std::vector<const InfractionData>> infMap; // Store infractions in memory
    bool infMapLoad = false;
    mutable boost::mutex lock;
};

#endif //BLOCKDX_COINVALIDATOR_H