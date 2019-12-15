// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STAKEMGR_H
#define BITCOIN_STAKEMGR_H

#include <chainparams.h>
#include <consensus/params.h>
#include <keystore.h>
#include <wallet/coinselection.h>
#include <wallet/wallet.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

class StakeMgr {
public:
    struct StakeCoin {
        std::shared_ptr<CInputCoin> coin;
        std::shared_ptr<CWallet> wallet;
        int64_t time;
        uint256 hashBlock;
        int64_t hashBlockTime;
        uint256 hashProofOfStake;
        explicit StakeCoin() {
            SetNull();
        }
        explicit StakeCoin(std::shared_ptr<CInputCoin> coin, std::shared_ptr<CWallet> wallet, int64_t time,
                           uint256 hashBlock, int64_t hashBlockTime, uint256 hashProofOfStake)
                                          : coin(coin), wallet(wallet), time(time),
                                            hashBlock(hashBlock), hashBlockTime(hashBlockTime),
                                            hashProofOfStake(hashProofOfStake) { }
        bool IsNull() {
            return coin == nullptr;
        }
        void SetNull() {
            coin = nullptr;
            wallet = nullptr;
            time = 0;
            hashBlock.SetNull();
            hashBlockTime = 0;
            hashProofOfStake.SetNull();
        }
    };
    struct StakeOutput {
        std::shared_ptr<COutput> out;
        std::shared_ptr<CWallet> wallet;
        explicit StakeOutput() : out(nullptr), wallet(nullptr) {}
        explicit StakeOutput(std::shared_ptr<COutput> out, std::shared_ptr<CWallet> wallet) : out(out), wallet(wallet) {}
        bool IsNull() {
            return out == nullptr;
        }
    };

public:
    bool Update(std::vector<std::shared_ptr<CWallet>> & wallets, const CBlockIndex *tip, const Consensus::Params & params, const bool & skipPeerRequirement=false);
    bool TryStake(const CBlockIndex *tip, const CChainParams & chainparams);
    bool NextStake(std::vector<StakeCoin> & nextStakes, const CBlockIndex *tip, const CChainParams & chainparams);
    bool StakeBlock(const StakeCoin & stakeCoin, const CChainParams & chainparams);
    int64_t LastUpdateTime() const;
    const StakeCoin & GetStake();

private:
    bool HasStakeModifier(const uint256 & blockHash) {
        LOCK(mu);
        return stakeModifiers.count(blockHash);
    }
    uint64_t GetStakeModifier(const uint256 & blockHash) {
        LOCK(mu);
        return stakeModifiers.count(blockHash) ? stakeModifiers[blockHash] : 0;
    }

private:
    Mutex mu;
    std::map<int64_t, std::vector<StakeCoin>> stakeTimes;
    std::map<uint256, uint64_t> stakeModifiers;
    std::atomic<int64_t> lastUpdateTime{0};
    std::atomic<int> lastBlockHeight{0};
};

extern void ThreadStakeMinter();

#endif // BITCOIN_STAKEMGR_H
