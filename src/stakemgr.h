// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2020 The Blocknet developers
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
        int64_t blockTime;
        uint256 hashBlock;
        int64_t hashBlockTime;
        uint256 hashProofOfStake;
        explicit StakeCoin() {
            SetNull();
        }
        explicit StakeCoin(std::shared_ptr<CInputCoin> coin, std::shared_ptr<CWallet> wallet, int64_t time, int64_t blockTime,
                           uint256 hashBlock, int64_t hashBlockTime, uint256 hashProofOfStake)
                                          : coin(coin), wallet(wallet), time(time), blockTime(blockTime),
                                            hashBlock(hashBlock), hashBlockTime(hashBlockTime),
                                            hashProofOfStake(hashProofOfStake) { }
        StakeCoin(const StakeCoin & stakeCoin) {
            coin = stakeCoin.coin;
            wallet = stakeCoin.wallet;
            time = stakeCoin.time;
            blockTime = stakeCoin.blockTime;
            hashBlock = stakeCoin.hashBlock;
            hashBlockTime = stakeCoin.hashBlockTime;
            hashProofOfStake = stakeCoin.hashProofOfStake;
        }
        bool IsNull() {
            return coin == nullptr;
        }
        void SetNull() {
            coin = nullptr;
            wallet = nullptr;
            time = 0;
            blockTime = 0;
            hashBlock.SetNull();
            hashBlockTime = 0;
            hashProofOfStake.SetNull();
        }
        ~StakeCoin() {
            SetNull();
        }
    };
    struct StakeOutput {
        std::shared_ptr<COutput> out;
        std::shared_ptr<CWallet> wallet;
        explicit StakeOutput() : out(nullptr), wallet(nullptr) {}
        explicit StakeOutput(std::shared_ptr<COutput> out, std::shared_ptr<CWallet> wallet) : out(out), wallet(wallet) {}
        StakeOutput(const StakeOutput & stakeOutput) {
            out = stakeOutput.out;
            wallet = stakeOutput.wallet;
        }
        bool IsNull() {
            return out == nullptr;
        }
        ~StakeOutput() {
            out = nullptr;
            wallet = nullptr;
        }
    };

public:
    bool Update(std::vector<std::shared_ptr<CWallet>> & wallets, const CBlockIndex *tip, const Consensus::Params & params, const bool & skipPeerRequirement=false);
    bool TryStake(const CBlockIndex *tip, const CChainParams & chainparams);
    bool NextStake(std::vector<StakeCoin> & nextStakes, const CBlockIndex *tip, const CChainParams & chainparams);
    bool StakeBlock(const StakeCoin & stakeCoin, const CChainParams & chainparams);
    int64_t LastUpdateTime() const;
    int LastBlockHeight() const;
    const StakeCoin & GetStake();
    bool SuitableCoin(const COutput & coin, const int & tipHeight, const Consensus::Params & params) const;
    std::vector<COutput> StakeOutputs(CWallet *wallet, const CAmount & minStakeAmount) const;
    bool GetStakesMeetingTarget(const std::shared_ptr<COutput> & coin, std::shared_ptr<CWallet> & wallet,
        const CBlockIndex *tip, const int64_t & adjustedTime, const int64_t & blockTime, const int64_t & fromTime,
        const int64_t & toTime, std::map<int64_t, std::vector<StakeCoin>> & stakes, const Consensus::Params & params);
    void Reset();

private:
    bool HasStakeModifier(const uint256 & blockHash) {
        LOCK(mu);
        return stakeModifiers.count(blockHash);
    }
    uint64_t GetStakeModifier(const uint256 & blockHash) {
        LOCK(mu);
        return stakeModifiers.count(blockHash) ? stakeModifiers[blockHash] : 0;
    }
    void UpdateStakeModifier(const uint256 & blockHash, const uint64_t & stakeModifier) {
        LOCK(mu);
        stakeModifiers[blockHash] = stakeModifier;
    }

private:
    Mutex mu;
    std::map<int64_t, std::vector<StakeCoin>> stakeTimes;
    std::map<uint256, uint64_t> stakeModifiers;
    std::atomic<int64_t> lastUpdateTime{0};
    std::atomic<int> lastBlockHeight{0};
};

extern void ThreadStakeMinter();
extern std::unique_ptr<StakeMgr> g_staker;

#endif // BITCOIN_STAKEMGR_H
