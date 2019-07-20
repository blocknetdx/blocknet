// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODE_H
#define BLOCKNET_SERVICENODE_H

#include <amount.h>
#include <base58.h>
#include <chainparams.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/standard.h>

#include <set>
#include <vector>

class ServiceNode {
public:
    static const CAmount COLLATERAL_SPV = 5000 * COIN;
    enum Tier : uint32_t {
        OPEN = 0,
        SPV = 50,
    };

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(snodePubKey);
        READWRITE(tier);
        READWRITE(collateral);
        READWRITE(signature);
    }

    static uint256 CreateSigHash(const std::vector<unsigned char> & snodePubKey, const Tier & tier,
            const std::vector<COutPoint> & collateral)
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << static_cast<uint32_t>(tier) << collateral;
        return ss.GetHash();
    }

    uint256 SigHash() const {
        return CreateSigHash(snodePubKey, static_cast<Tier>(tier), collateral);
    }

    bool IsValid(const std::function<CTransactionRef(const COutPoint & out)> & getTxFunc) const {
        // Validate the snode pubkey
        {
            CPubKey pubkey(snodePubKey);
            if (!pubkey.IsFullyValid())
                return false;
        }

        // If open tier, the signature should be generated from the snode pubkey
        if (tier == Tier::OPEN) {
            const auto & sighash = SigHash();
            CPubKey pubkey2;
            if (!pubkey2.RecoverCompact(sighash, signature))
                return false; // not valid if bad sig
            return CPubKey(snodePubKey).GetID() == pubkey2.GetID();
        }

        // If not on the open tier, check collateral
        if (collateral.empty())
            return false; // not valid if no collateral

        const auto & sighash = SigHash();
        CAmount total{0}; // Track the total collateral amount
        std::set<COutPoint> unique_collateral(collateral.begin(), collateral.end()); // prevent duplicates

        // Determine if all collateral utxos validate the sig
        for (const auto & op : unique_collateral) {
            CTransactionRef tx = getTxFunc(op);
            if (!tx)
                return false; // not valid if no transaction found or utxo is already spent

            if (tx->vout.size() <= op.n)
                return false; // not valid if bad vout index

            const auto & out = tx->vout[op.n];
            total += out.nValue;

            CTxDestination address;
            if (!ExtractDestination(out.scriptPubKey, address))
                return false; // not valid if bad address

            CKeyID *keyid = boost::get<CKeyID>(&address);
            CPubKey pubkey;
            if (!pubkey.RecoverCompact(sighash, signature))
                return false; // not valid if bad sig
            if (pubkey.GetID() != *keyid)
                return false; // fail if pubkeys don't match
        }

        if (tier == Tier::SPV && total >= COLLATERAL_SPV)
            return true;

        return false;
    }

protected:
    std::vector<unsigned char> snodePubKey;
    uint32_t tier;
    std::vector<COutPoint> collateral;
    std::vector<unsigned char> signature;
};


#endif //BLOCKNET_SERVICENODE_H
