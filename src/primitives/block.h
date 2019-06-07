// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/standard.h>
#include <serialize.h>
#include <uint256.h>

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint256 hashStake;
    uint32_t nStakeIndex;
    int64_t nStakeAmount;
    uint256 hashStakeBlock;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(hashStake);
        READWRITE(nStakeIndex);
        READWRITE(nStakeAmount);
        READWRITE(hashStakeBlock);
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        hashStake.SetNull();
        nStakeIndex = 0;
        nStakeAmount = 0;
        hashStakeBlock.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CBlockHeader, *this);
        READWRITE(vtx);
        if (vtx.size() > 1 && vtx[1]->IsCoinStake())
            READWRITE(vchBlockSig);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        vchBlockSig.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.hashStake      = hashStake;
        block.nStakeIndex    = nStakeIndex;
        block.nStakeAmount   = nStakeAmount;
        block.hashStakeBlock = hashStakeBlock;
        return block;
    }

    std::string ToString() const;

    // ppcoin: PoS
    bool IsProofOfStake() const {
        return (vtx.size() > 1 && vtx[1]->IsCoinStake());
    }

    /**
     * Pubkey used to sign the stake input must match the block signature.
     * @param stakeScript
     * @return
     */
    bool VerifySig(const CScript & stakeScript) const {
        if (vchBlockSig.empty())
            return false;

        const auto & txin = vtx[1]->vin[0];
        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype type = Solver(stakeScript, vSolutions);

        if (type == TX_PUBKEY) {
            CPubKey pubkey(vSolutions[0]);
            if (!pubkey.IsValid())
                return false;
            return pubkey.Verify(GetHash(), vchBlockSig);
        }
        else if (type == TX_PUBKEYHASH) {
            // Get pubkey from scriptsig
            CScript::const_iterator pc = txin.scriptSig.begin();
            std::vector<unsigned char> data;
            while (pc < txin.scriptSig.end()) {
                opcodetype opcode;
                if (!txin.scriptSig.GetOp(pc, opcode, data))
                    break;
                if (data.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE
                      || data.size() == CPubKey::PUBLIC_KEY_SIZE)
                    break;
            }
            if (data.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE
                && data.size() != CPubKey::PUBLIC_KEY_SIZE) {
                return false;
            }
            CPubKey pubkey(data);
            if (!pubkey.IsValid())
                return false;
            return pubkey.Verify(GetHash(), vchBlockSig);
        }

        return false;
    }
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
