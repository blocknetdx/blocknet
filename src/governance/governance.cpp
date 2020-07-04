// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <governance/governance.h>
#include <validation.h>

namespace gov {

GovernanceDB::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe)
        : CDBWrapper(GetDataDir() / "indexes" / "governance", n_cache_size, f_memory, f_wipe) { }

bool GovernanceDB::DB::ReadBestBlock(CBlockLocator & locator) const {
    bool success = Read(DB_BEST_BLOCK, locator);
    if (!success)
        locator.SetNull();
    return success;
}

bool GovernanceDB::DB::WriteBestBlock(const CBlockLocator & locator) {
    return Write(DB_BEST_BLOCK, locator);
}

bool GovernanceDB::DB::WriteProposal(const uint256 & hash, const CDiskProposal & proposal) {
    return Write(std::make_pair(DB_PROPOSAL, hash), proposal);
}

bool GovernanceDB::DB::WriteProposals(const std::vector<std::pair<uint256, CDiskProposal>> & proposals) {
    CDBBatch batch(*this);
    for (const auto & item : proposals)
        batch.Write(std::make_pair(DB_PROPOSAL, item.first), item.second);
    return WriteBatch(batch);
}

bool GovernanceDB::DB::WriteVote(const uint256 & hash, const CDiskVote & vote) {
    return Write(std::make_pair(DB_VOTE, hash), vote);
}

bool GovernanceDB::DB::WriteVotes(const std::vector<std::pair<uint256, CDiskVote>> & votes) {
    CDBBatch batch(*this);
    for (const auto & item : votes)
        batch.Write(std::make_pair(DB_VOTE, item.first), item.second);
    return WriteBatch(batch);
}

GovernanceDB::GovernanceDB(size_t n_cache_size, bool f_memory, bool f_wipe)
        : cache(n_cache_size)
        , memory(f_memory)
        , db(MakeUnique<GovernanceDB::DB>(cache, memory, f_wipe)) { }

void GovernanceDB::Reset(const bool wipe=false) {
    bestBlockIndex = nullptr;
    db.reset();
    db = MakeUnique<GovernanceDB::DB>(cache, memory, wipe);
}

void GovernanceDB::Start() {
    CBlockLocator locator;
    if (!db->ReadBestBlock(locator))
        locator.SetNull();

    LOCK(cs_main);
    if (locator.IsNull())
        bestBlockIndex = nullptr;
    else
        bestBlockIndex = FindForkInGlobalIndex(chainActive, locator);
}

void GovernanceDB::Stop() { }

const CBlockIndex* GovernanceDB::BestBlockIndex() const {
    return bestBlockIndex;
}
bool GovernanceDB::WriteBestBlock(const CBlockIndex *pindex, const CChain & chain, CCriticalSection & chainMutex) {
    AssertLockHeld(chainMutex);
    if (!db->WriteBestBlock(chain.GetLocator(pindex)))
        return error("%s: Failed to write locator to disk", __func__);
    bestBlockIndex = chain.FindFork(pindex);
    return true;
}

void GovernanceDB::AddVote(const CDiskVote & vote) {
    db->WriteVote(vote.getHash(), vote);
}

void GovernanceDB::AddVotes(const std::vector<std::pair<uint256, CDiskVote>> & votes) {
    db->WriteVotes(votes);
}

void GovernanceDB::RemoveVote(const uint256 & vote) {
    db->Erase(vote);
}

void GovernanceDB::AddProposal(const CDiskProposal & proposal) {
    db->WriteProposal(proposal.getHash(), proposal);
}

void GovernanceDB::AddProposals(const std::vector<std::pair<uint256, CDiskProposal>> & proposals) {
    db->WriteProposals(proposals);
}

void GovernanceDB::RemoveProposal(const uint256 & proposal) {
    db->Erase(proposal);
}

void GovernanceDB::BlockConnected(const std::shared_ptr<const CBlock> & block, const CBlockIndex *pindex,
                                  const std::vector<CTransactionRef> & txn_conflicted)
{
    auto blockIndex = bestBlockIndex.load();
    if (!blockIndex) {
        if (pindex->nHeight == Params().GetConsensus().governanceBlock)
            bestBlockIndex = pindex;
        return;
    }

    // Ensure block connects to an ancestor of the current best block
    if (blockIndex->GetAncestor(pindex->nHeight - 1) != pindex->pprev) {
        LogPrintf("%s: Governance WARNING: Block %s does not connect to an ancestor of known best chain (tip=%s); not updating index\n",
                  __func__, pindex->GetBlockHash().ToString(), blockIndex->GetBlockHash().ToString());
        return;
    }
    bestBlockIndex = pindex;
}

void GovernanceDB::ChainStateFlushed(const CBlockLocator & locator) {
    const uint256 & locatorTipHash = locator.vHave.front();
    const CBlockIndex *locatorTipIndex;
    {
        LOCK(cs_main);
        locatorTipIndex = LookupBlockIndex(locatorTipHash);
    }

    if (!locatorTipIndex) {
        FatalError("%s: First block (hash=%s) in locator was not found", __func__, locatorTipHash.ToString());
        return;
    }

    auto blockIndex = bestBlockIndex.load();
    if (!blockIndex)
        return;
    if (blockIndex->GetAncestor(locatorTipIndex->nHeight) != locatorTipIndex) {
        LogPrintf("%s: Governance WARNING: Locator contains block (hash=%s) not on known best chain (tip=%s); not writing index locator\n",
                  __func__, locatorTipHash.ToString(), blockIndex->GetBlockHash().ToString());
        return;
    }

    if (!db->WriteBestBlock(locator))
        error("%s: Failed to write locator to disk", __func__);
}

GovernanceDB::~GovernanceDB() {
    Stop();
    bestBlockIndex = nullptr;
    db.reset();
}

}
