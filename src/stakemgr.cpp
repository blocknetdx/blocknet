// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stakemgr.h>

#include <governance/governance.h>
#include <kernel.h>
#include <miner.h>
#include <shutdown.h>
#include <timedata.h>
#include <validation.h>

std::unique_ptr<StakeMgr> g_staker;

void ThreadStakeMinter() {
    RenameThread("blocknet-staker");
    LogPrintf("Staker has started\n");
    g_staker = MakeUnique<StakeMgr>();
    const auto stakingSkipPeers = gArgs.GetBoolArg("-stakingwithoutpeers", false);
    const auto & chainparams = Params();
    while (!ShutdownRequested()) {
        if (!stakingSkipPeers && IsInitialBlockDownload()) { // do not stake during initial download
            boost::this_thread::sleep_for(boost::chrono::seconds(1));
            continue;
        }
        try {
            auto wallets = GetWallets();
            CBlockIndex *pindex = nullptr;
            {
                LOCK(cs_main);
                pindex = chainActive.Tip();
            }
            if (pindex && g_staker->Update(wallets, pindex, chainparams.GetConsensus(), stakingSkipPeers)) {
                boost::this_thread::interruption_point();
                g_staker->TryStake(pindex, chainparams);
            }
        } catch (std::exception & e) {
            LogPrintf("Staker ran into an exception: %s\n", e.what());
        } catch (...) { }
        boost::this_thread::sleep_for(boost::chrono::seconds(1));
    }
    g_staker.reset();
    LogPrintf("Staker shutdown\n");
}

bool StakeMgr::Update(std::vector<std::shared_ptr<CWallet>> & wallets, const CBlockIndex *tip, const Consensus::Params & params, const bool & skipPeerRequirement) {
    if (!skipPeerRequirement && IsInitialBlockDownload())
        return false;
    {
        LOCK(cs_main);
        if (!skipPeerRequirement && SyncProgress(chainActive.Height()) < 1.0 - std::numeric_limits<double>::epsilon())
            return false; // not ready to stake yet (need to be synced up with peers)
    }
    // Aggressive staking will check inputs at most once per second. If a large number of staking inputs
    // are not present in the wallet this could waste cpu cycles. Normal staking happens every 15 seconds
    // (see below) and results in fewer cpu cycles.
    const int stakingInterval = std::max<int>(gArgs.GetArg("-staking", 15), 1);
    const int64_t stakeSearchPeriodSeconds = params.PoSFutureBlockTimeLimit();
    const int64_t endTime = GetAdjustedTime() + stakeSearchPeriodSeconds; // current time + seconds into future
    // A closed staking window means we've exhausted the search for a new stake
    const bool stakingWindowClosed = endTime <= lastUpdateTime + stakingInterval;
    const bool tipChanged = tip->nHeight != lastBlockHeight;
    const bool staleTip = tip->nTime <= lastUpdateTime || tip->nTime < GetAdjustedTime() - params.stakeMinAge*2; // TODO Blocknet testnet could stall chain?
    if (stakingWindowClosed && !tipChanged && staleTip)
        return false; // do not process if staking window closed, tip hasn't changed, and tip time is stale

    {
        LOCK(mu);
        stakeTimes.clear();
    }

    std::vector<StakeOutput> selected; // selected coins that meet criteria for staking
    const int coinMaturity = params.coinMaturity;
    const auto argStakeAmount = static_cast<CAmount>(gArgs.GetArg("-minstakeamount", 0));
    const auto minStakeAmount = argStakeAmount == 0 ? 1 : argStakeAmount * COIN;
    const auto tipHeight = tip->nHeight;
    const auto stakeHeight = tip->nHeight + 1;

    for (const auto & pwallet : wallets) {
        std::vector<COutput> coins; // all confirmed coins
        {
            auto locked_chain = pwallet->chain().lock();
            LOCK2(cs_main, pwallet->cs_wallet);
            if (pwallet->IsLocked()) {
                LogPrintf("Wallet is locked not staking inputs: %s\n", pwallet->GetDisplayName());
                continue; // skip locked wallets
            }
            pwallet->AvailableCoins(*locked_chain, coins, true, nullptr, minStakeAmount, MAX_MONEY, MAX_MONEY, 0);
        }

        // Find suitable staking coins
        for (const COutput & out : coins) {
            if (out.tx->IsCoinBase()) // can't stake coinbase
                continue;
            if (out.tx->IsCoinStake() && out.nDepth < coinMaturity) // skip non-mature coinstakes
                continue;
            if (!out.fSpendable) // skip coin we don't have keys for
                continue;
            // Remove all coins participating in the current superblock's vote cutoff zone
            // to avoid staking a vote and causing invalidation.
            if (gov::Governance::instance().utxoInVoteCutoff(out.GetInputCoin().outpoint, tipHeight, params))
                continue;
            selected.emplace_back(std::make_shared<COutput>(out), pwallet);
        }
    }

    // Always search for stake from last block time if the tip changed
    lastUpdateTime = tipChanged ? tip->GetBlockTime() + 1 : lastUpdateTime + 1;

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(tip->nBits);

    // Cache all possible stakes between last update and few seconds into the future
    for (const auto & item : selected) {
        boost::this_thread::interruption_point();
        const auto out = item.out;
        if (lastUpdateTime - out->tx->GetTxTime() < params.stakeMinAge) // skip coins that don't meet stake age
            continue;

        CBlockIndex *pindexStake = nullptr;
        {
            LOCK(cs_main);
            pindexStake = LookupBlockIndex(out->tx->hashBlock);
            if (!pindexStake)
                continue; // skip txs with block that can't be found
        }

        const int hashBlockTime = pindexStake->GetBlockTime();
        const auto & txInBlockHash = pindexStake->GetBlockHash();

        if (IsProtocolV05(lastUpdateTime)) { // Protocol v5+
            const auto estBlockTime = std::max(tip->GetBlockTime()+1, GetAdjustedTime());
            if (estBlockTime - params.stakeMinAge <= hashBlockTime) // valid modifier time check
                continue;

            int64_t i = lastUpdateTime;
            for (; i < endTime; ++i) {
                if (i - out->tx->GetTxTime() < params.stakeMinAge)
                    continue; // skip coins that don't meet stake age
                uint64_t stakeModifier{0};
                int stakeModifierHeight{0};
                int64_t stakeModifierTime{0};
                if (!GetKernelStakeModifier(tip, pindexStake, estBlockTime, stakeModifier, stakeModifierHeight, stakeModifierTime))
                    continue;

                CDataStream ss(SER_GETHASH, 0);
                ss << stakeModifier;

                uint256 hashProofOfStake;
                if (IsProtocolV06(estBlockTime, params)) {
                    hashProofOfStake = stakeHashV06(ss, txInBlockHash, hashBlockTime, stakeHeight, out->i, i);
                    if (!stakeTargetHitV06(hashProofOfStake, out->GetInputCoin().txout.nValue, bnTargetPerCoinDay))
                        continue;
                } else {
                    hashProofOfStake = stakeHashV05(ss, hashBlockTime, stakeHeight, out->i, i);
                    if (!stakeTargetHit(hashProofOfStake, out->GetInputCoin().txout.nValue, bnTargetPerCoinDay))
                        continue;
                }

                {
                    LOCK(mu);
                    stakeTimes[i].emplace_back(std::make_shared<CInputCoin>(out->GetInputCoin()), item.wallet, i,
                            estBlockTime, txInBlockHash, hashBlockTime, hashProofOfStake);
                    break;
                }
            }
        } else {
            uint64_t stakeModifier = HasStakeModifier(txInBlockHash) ? GetStakeModifier(txInBlockHash) : 0;
            int stakeModifierHeight{0};
            int64_t stakeModifierTime{0};
            const unsigned int stakeTime{0}; // this is not used here by v03 staking protocol (see GetKernelStakeModifierV03)
            if (stakeModifier == 0 && !GetKernelStakeModifier(tip, pindexStake, stakeTime, stakeModifier, stakeModifierHeight, stakeModifierTime))
                continue;

            if (!HasStakeModifier(txInBlockHash)) {
                LOCK(mu);
                stakeModifiers[txInBlockHash] = stakeModifier;
            }
            CDataStream ss(SER_GETHASH, 0);
            ss << stakeModifier;

            int64_t i = lastUpdateTime;
            for (; i < endTime; ++i) {
                if (i - out->tx->GetTxTime() < params.stakeMinAge) // skip coins that don't meet stake age
                    continue;
                const auto hashProofOfStake = stakeHash(i, ss, out->i, out->tx->GetHash(), hashBlockTime);
                if (!stakeTargetHit(hashProofOfStake, out->GetInputCoin().txout.nValue, bnTargetPerCoinDay))
                    continue;
                {
                    LOCK(mu);
                    stakeTimes[i].emplace_back(std::make_shared<CInputCoin>(out->GetInputCoin()), item.wallet, i, 0,
                                               out->tx->hashBlock, hashBlockTime, hashProofOfStake);
                    break;
                }
            }
        }

    }

    lastBlockHeight = tipHeight;
    lastUpdateTime = endTime;
    LogPrint(BCLog::ALL, "Staker: %u\n", lastBlockHeight);
    return !stakeTimes.empty();
}

bool StakeMgr::TryStake(const CBlockIndex *tip, const CChainParams & chainparams) {
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

bool StakeMgr::NextStake(std::vector<StakeCoin> & nextStakes, const CBlockIndex *tip, const CChainParams & chainparams) {
    LOCK(mu);
    if (stakeTimes.empty())
        return false;

    const auto cutoffTime = tip->GetBlockTime(); // must find stake input valid for a time newer than cutoff
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
            if (IsProtocolV06(stake.blockTime, chainparams.GetConsensus())) {
                if (!stakeTargetHitV06(stake.hashProofOfStake, stake.coin->txout.nValue, bnTargetPerCoinDay))
                    continue;
            } else if (!stakeTargetHit(stake.hashProofOfStake, stake.coin->txout.nValue, bnTargetPerCoinDay))
                continue;
            nextStakes.push_back(stake);
        }
    }

    return !nextStakes.empty();
}

bool StakeMgr::StakeBlock(const StakeCoin & stakeCoin, const CChainParams & chainparams) {
    {
        auto locked_chain = stakeCoin.wallet->chain().lock();
        LOCK(stakeCoin.wallet->cs_wallet);
        if (stakeCoin.wallet->IsLocked()) {
            LogPrintf("Missed stake because wallet (%s) is locked!\n", stakeCoin.wallet->GetDisplayName());
            return false;
        }
    }
    bool fNewBlock = false;
    try {
        auto pblocktemplate = BlockAssembler(chainparams).CreateNewBlockPoS(*stakeCoin.coin, stakeCoin.hashBlock,
                                                                            stakeCoin.time, stakeCoin.blockTime,
                                                                            stakeCoin.wallet.get());
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

int64_t StakeMgr::LastUpdateTime() const {
    return lastUpdateTime;
}

int StakeMgr::LastBlockHeight() const {
    return lastBlockHeight;
}

const StakeMgr::StakeCoin & StakeMgr::GetStake() {
    if (!stakeTimes.empty())
        return *stakeTimes.begin()->second.begin();
    return std::move(StakeCoin{});
}

