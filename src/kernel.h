// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <miner.h>
#include <shutdown.h>
#include <streams.h>
#include <timedata.h>
#include <logging.h>
#include <util/system.h>
#include <wallet/wallet.h>

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 60;
static const unsigned int MODIFIER_INTERVAL_TESTNET = 60;
extern unsigned int nModifierInterval;
extern unsigned int getIntervalVersion(bool fTestNet);

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);
// Stake modifier selection interval
int64_t GetStakeModifierSelectionInterval();

// Stake modifier selection upgrade
bool IsProtocolV05(uint64_t nTimeTx);

uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash,unsigned int nTimeBlockFrom);
uint256 stakeHashV05(CDataStream ss, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx);

// Check whether stake kernel meets hash target
bool stakeTargetHit(uint256 hashProofOfStake, int64_t nValueIn, arith_uint256 bnTargetPerCoinDay);

bool CheckStakeKernelHash(const CBlockIndex *pindexPrev, unsigned int nBits, const uint256 txInBlockHash, const int64_t txInBlockTime,
        const CAmount txInAmount, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift, bool fCheck,
        uint256& hashProofOfStake, bool fPrintProofOfStake = false);
bool GetKernelStakeModifier(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierV03(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierBlocknet(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlockHeader & block, const CBlockIndex *pindexPrev, uint256 & hashProofOfStake, const Consensus::Params & consensusParams);

// peercoin: For use with Staking Protocol V05.
unsigned int GetStakeEntropyBit(const uint256 & blockHash, const int64_t & blockTime);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

static inline bool error(const char* format) {
    auto log_msg = std::string("ERROR: ") + format + "\n";
    LogInstance().LogPrintStr(log_msg);
    return false;
}
static std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime) {
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}
bool IsProofOfStake(int blockHeight, const Consensus::Params & consensusParams);
bool IsProofOfStake(int blockHeight);
bool SignBlock(CBlock & block, const CScript & stakeScript, const CKeyStore & keystore);

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
    bool Update(std::vector<std::shared_ptr<CWallet>> & wallets, const CBlockIndex *tip, const Consensus::Params & params, const bool & skipPeerRequirement=false) {
        if (IsInitialBlockDownload())
            return false;
        {
            LOCK(cs_main);
            if (!skipPeerRequirement && SyncProgress(chainActive.Height()) < 1.0 - std::numeric_limits<double>::epsilon())
                return false; /// not ready to stake yet (need to be synced up with peers)
        }
        const int stakeSearchPeriodSeconds{MAX_FUTURE_BLOCK_TIME_POS};
        const bool notExpired = GetAdjustedTime() <= lastUpdateTime;
        const bool tipChanged = tip->nHeight != lastBlockHeight;
        const bool staleTip = tip->nTime <= lastUpdateTime || tip->nTime < GetAdjustedTime() - params.stakeMinAge*2; // TODO Blocknet testnet could stall chain?
        if (notExpired && !tipChanged && staleTip)
            return false; // do not process if not expired, tip hasn't changed, and tip time is stale

        {
            LOCK(mu);
            stakeTimes.clear();
        }

        std::vector<StakeOutput> selected; // selected coins that meet criteria for staking
        const int coinMaturity = params.coinMaturity;
        const auto minStakeAmount = static_cast<CAmount>(gArgs.GetArg("-minstakeamount", 0) * COIN);

        for (const auto & pwallet : wallets) {
            std::vector<COutput> coins; // all confirmed coins
            {
                auto locked_chain = pwallet->chain().lock();
                LOCK2(cs_main, pwallet->cs_wallet);
                if (pwallet->IsLocked()) {
                    LogPrintf("Wallet is locked not staking inputs: %s", pwallet->GetDisplayName());
                    continue; // skip locked wallets
                }
                pwallet->AvailableCoins(*locked_chain, coins, true, nullptr, minStakeAmount, MAX_MONEY, MAX_MONEY, 0);
            }

            { // Remove all immature coins (any previous stakes that do not meet the maturity requirement)
                LOCK(cs_main);
                CCoinsViewCache &view = *pcoinsTip;
                auto pred = [&view,&coinMaturity](const COutput & c) -> bool {
                    const auto & coin = view.AccessCoin(c.GetInputCoin().outpoint);
                    return coin.IsCoinBase() && coin.nHeight < coinMaturity;
                };
                coins.erase(std::remove_if(coins.begin(), coins.end(), pred), coins.end());
            }

            // Find suitable staking coins
            for (const COutput & out : coins) {
                if (GetAdjustedTime() - out.tx->GetTxTime() < params.stakeMinAge) // skip coins that don't meet stake age
                    continue;
                if (out.tx->IsCoinBase()) // can't stake coinbase
                    continue;
                if (out.nDepth < coinMaturity) // skip non-mature coins
                    continue;
                if (!out.fSpendable) // skip coin we don't have keys for
                    continue;
                selected.emplace_back(std::make_shared<COutput>(out), pwallet);
            }
        }

        if (lastUpdateTime == 0) // Use chain tip last time on first call
            lastUpdateTime = tip->nTime;

        int64_t currentTime = GetAdjustedTime(); // current time + seconds into future
        int64_t endTime = currentTime + stakeSearchPeriodSeconds; // current time + seconds into future
        arith_uint256 bnTargetPerCoinDay;
        bnTargetPerCoinDay.SetCompact(tip->nBits);

        // Cache all possible stakes between last update and few seconds into the future
        for (const auto & item : selected) {
            const auto out = item.out;
            boost::this_thread::interruption_point();
            const auto & txInBlockHash = out->tx->hashBlock;
            int hashBlockTime{0};
            {
                LOCK(cs_main);
                const auto pindex = LookupBlockIndex(out->tx->hashBlock);
                if (!pindex)
                    continue; // skip txs with block that can't be found
                hashBlockTime = pindex->GetBlockTime();
            }

            if (IsProtocolV05(lastUpdateTime)) { // if v05 staking protocol modifier is dynamic (not in hash lookup)
                int64_t i = lastUpdateTime + 1;
                for (; i < endTime; ++i) {
                    uint64_t stakeModifier{0};
                    int stakeModifierHeight{0};
                    int64_t stakeModifierTime{0};
                    if (!GetKernelStakeModifier(tip, txInBlockHash, static_cast<const unsigned int>(i), stakeModifier, stakeModifierHeight, stakeModifierTime, false))
                        continue;

                    CDataStream ss(SER_GETHASH, 0);
                    ss << stakeModifier;

                    const auto hashProofOfStake = stakeHashV05(ss, hashBlockTime, tip->nHeight + 1, out->i, i);
                    if (!stakeTargetHit(hashProofOfStake, out->GetInputCoin().txout.nValue, bnTargetPerCoinDay))
                        continue;
                    {
                        LOCK(mu);
                        stakeTimes[i].emplace_back(std::make_shared<CInputCoin>(out->GetInputCoin()), item.wallet, i,
                                                   out->tx->hashBlock, hashBlockTime, hashProofOfStake);
                        break;
                    }
                }
            } else {
                uint64_t stakeModifier = HasStakeModifier(txInBlockHash) ? GetStakeModifier(txInBlockHash) : 0;
                int stakeModifierHeight{0};
                int64_t stakeModifierTime{0};
                const unsigned int stakeTime{0}; // this is not used here by v03 staking protocol (see GetKernelStakeModifierV03)
                if (stakeModifier == 0 && !GetKernelStakeModifier(tip, txInBlockHash, stakeTime, stakeModifier, stakeModifierHeight, stakeModifierTime, false))
                    continue;

                if (!HasStakeModifier(txInBlockHash)) {
                    LOCK(mu);
                    stakeModifiers[txInBlockHash] = stakeModifier;
                }
                CDataStream ss(SER_GETHASH, 0);
                ss << stakeModifier;

                int64_t i = lastUpdateTime + 1;
                for (; i < endTime; ++i) {
                    const auto hashProofOfStake = stakeHash(i, ss, out->i, out->tx->GetHash(), hashBlockTime);
                    if (!stakeTargetHit(hashProofOfStake, out->GetInputCoin().txout.nValue, bnTargetPerCoinDay))
                        continue;
                    {
                        LOCK(mu);
                        stakeTimes[i].emplace_back(std::make_shared<CInputCoin>(out->GetInputCoin()), item.wallet, i,
                                                   out->tx->hashBlock, hashBlockTime, hashProofOfStake);
                        break;
                    }
                }
            }

        }

        lastBlockHeight = tip->nHeight;
        lastUpdateTime = endTime;
        LogPrintf("Staker: %u\n", lastBlockHeight); // TODO Blocknet PoS move to debug category
        return !stakeTimes.empty();
    }

    bool TryStake(const CBlockIndex *tip, const CChainParams & chainparams) {
        if (!tip)
            return false; // make sure tip is valid

        std::vector<StakeCoin> nextStakes;
        if (!NextStake(nextStakes, tip, chainparams))
            return false;

        stakeTimes.clear(); // reset stake selections on success or error
        for (const auto & nextStake : nextStakes) {
            if (StakeBlock(nextStake, chainparams))
                return true;
        }
        return false;
    }

    bool NextStake(std::vector<StakeCoin> & nextStakes, const CBlockIndex *tip, const CChainParams & chainparams) {
        LOCK(mu);
        if (stakeTimes.empty())
            return false;

        const auto cutoffTime = tip->nTime; // must find stake input valid for a time newer than cutoff
        arith_uint256 bnTargetPerCoinDay; // current difficulty
        bnTargetPerCoinDay.SetCompact(tip->nBits);

        // sort ascending
        auto sortCoins = [](const StakeCoin & a, const StakeCoin & b) -> bool {
            return a.coin->txout.nValue < b.coin->txout.nValue;
        };

        for (const auto & item : stakeTimes) {
            if (item.first <= cutoffTime) // skip if input stake time doesn't meet the cutoff time
                continue;

            auto stakes = item.second;
            std::sort(stakes.begin(), stakes.end(), sortCoins);

            // Find the smallest stake input that meets the protocol requirements
            for (const auto & stake : stakes) {
                // Make sure stake still meets network requirements
                if (!stakeTargetHit(stake.hashProofOfStake, stake.coin->txout.nValue, bnTargetPerCoinDay))
                    continue;
                nextStakes.push_back(stake);
            }
        }

        return !nextStakes.empty();
    }

    bool StakeBlock(const StakeCoin & stakeCoin, const CChainParams & chainparams) {
        {
            auto locked_chain = stakeCoin.wallet->chain().lock();
            LOCK(stakeCoin.wallet->cs_wallet);
            if (stakeCoin.wallet->IsLocked()) {
                LogPrintf("Missed stake because wallet (%s) is locked!", stakeCoin.wallet->GetDisplayName());
                return false;
            }
        }
        bool fNewBlock = false;
        try {
            auto pblocktemplate = BlockAssembler(chainparams).CreateNewBlockPoS(*stakeCoin.coin, stakeCoin.hashBlock,
                    stakeCoin.time, stakeCoin.wallet.get());
            if (!pblocktemplate)
                return false;
            auto pblock = std::make_shared<const CBlock>(pblocktemplate->block);
            if (!ProcessNewBlock(chainparams, pblock, /*fForceProcessing=*/true, &fNewBlock))
                return false;
            LogPrintf("Stake found! %s %d %f\n", stakeCoin.coin->outpoint.hash.ToString(), stakeCoin.coin->outpoint.n,
                    (double)stakeCoin.coin->txout.nValue/(double)COIN);
        } catch (std::exception & e) {
            LogPrintf("Error: Staking %s\n", e.what());
        }
        return fNewBlock;
    }

    int64_t LastUpdateTime() const {
        return lastUpdateTime;
    }

    const StakeCoin & GetStake() {
        if (!stakeTimes.empty())
            return *stakeTimes.begin()->second.begin();
        return std::move(StakeCoin{});
    }

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


void static ThreadStakeMinter() {
    RenameThread("blocknet-staker");
    LogPrintf("Staker has started\n");
    StakeMgr staker;
    while (!ShutdownRequested()) {
        const int sleepTimeSeconds{1};
        if (IsInitialBlockDownload()) { // do not stake during initial download
            boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimeSeconds));
            continue;
        }
        try {
            auto wallets = GetWallets();
            CBlockIndex *pindex = nullptr;
            {
                LOCK(cs_main);
                pindex = chainActive.Tip();
            }
            if (pindex && staker.Update(wallets, pindex, Params().GetConsensus())) {
                boost::this_thread::interruption_point();
                staker.TryStake(pindex, Params());
            }
        } catch (std::exception & e) {
            LogPrintf("Staker ran into an exception: %s\n", e.what());
        } catch (...) { }
        boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimeSeconds));
    }
    LogPrintf("Staker shutdown\n");
}

#endif // BITCOIN_KERNEL_H
