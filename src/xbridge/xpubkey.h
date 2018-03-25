// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XBRIDGE_BITCOIN_PUBKEY_H
#define XBRIDGE_BITCOIN_PUBKEY_H

#include "hash.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "key.h"
#include "pubkey.h"

#include <stdexcept>
#include <vector>

/**
 * secp256k1:
 * const unsigned int PRIVATE_KEY_SIZE = 279;
 * const unsigned int PUBLIC_KEY_SIZE  = 65;
 * const unsigned int SIGNATURE_SIZE   = 72;
 *
 * see www.keylength.com
 * script supports up to 75 for single byte push
 */

namespace xbridge
{

const unsigned int BIP32_EXTKEY_SIZE = 74;

/** A reference to a CKey: the Hash160 of its serialized public key */
//class CKeyID : public uint160
//{
//public:
//    CKeyID() : uint160() {}
//    CKeyID(const uint160& in) : uint160(in) {}
//};

typedef uint256 ChainCode;

/** An encapsulated public key. */
class CPubKey
{
private:

    /**
     * Just store the serialized data.
     * Its length can very cheaply be computed from the first byte.
     */
    unsigned char vch[65];


    /**
     * @brief GetLen Compute the length of a pubkey with a given first byte.
     * @param chHeader
     * @return
     */
    unsigned int static GetLen(unsigned char chHeader)
    {
        if (chHeader == 2 || chHeader == 3)
            return 33;
        if (chHeader == 4 || chHeader == 6 || chHeader == 7)
            return 65;
        return 0;
    }


    /**
     * @brief Invalidate Set this key data to be invalid
     */
    void Invalidate()
    {
        vch[0] = 0xFF;
    }

public:

    /**
     * @brief CPubKey Construct an invalid public key.
     */
    CPubKey()
    {
        Invalidate();
    }


    /**
     * @brief Set Initialize a public key using begin/end iterators to byte data.
     * @param pbegin
     * @param pend
     */
    template <typename T>
    void Set(const T pbegin, const T pend)
    {
        int len = pend == pbegin ? 0 : GetLen(pbegin[0]);
        if (len && len == (pend - pbegin))
            memcpy(vch, (unsigned char*)&pbegin[0], len);
        else
            Invalidate();
    }


    /**
     * @brief CPubKey Construct a public key using begin/end iterators to byte data.
     * @param pbegin
     * @param pend
     */
    template <typename T>
    CPubKey(const T pbegin, const T pend)
    {
        Set(pbegin, pend);
    }


    /**
     * @brief CPubKey Construct a public key from a byte vector.
     * @param _vch
     */
    explicit CPubKey(const std::vector<unsigned char>& _vch)
    {
        Set(_vch.begin(), _vch.end());
    }

    //! Simple read-only vector-like interface to the pubkey data.
    unsigned int size() const { return GetLen(vch[0]); }
    const unsigned char* begin() const { return vch; }
    const unsigned char* end() const { return vch + size(); }
    const unsigned char& operator[](unsigned int pos) const { return vch[pos]; }


    /**
     * @brief operator == Comparator implementation.
     * @param a
     * @param b
     * @return
     */
    friend bool operator==(const CPubKey& a, const CPubKey& b)
    {
        return a.vch[0] == b.vch[0] &&
               memcmp(a.vch, b.vch, a.size()) == 0;
    }
    friend bool operator!=(const CPubKey& a, const CPubKey& b)
    {
        return !(a == b);
    }
    friend bool operator<(const CPubKey& a, const CPubKey& b)
    {
        return a.vch[0] < b.vch[0] ||
               (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }


    /**
     * @brief GetSerializeSize Implement serialization, as if this was a byte vector.
     * @return
     */
    unsigned int GetSerializeSize(int /*nType*/, int /*nVersion*/) const
    {
        return size() + 1;
    }
    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        unsigned int len = size();
        ::WriteCompactSize(s, len);
        s.write((char*)vch, len);
    }
    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int len = ::ReadCompactSize(s);
        if (len <= 65) {
            s.read((char*)vch, len);
        } else {
            // invalid pubkey, skip available data
            char dummy;
            while (len--)
                s.read(&dummy, 1);
            Invalidate();
        }
    }


    /**
     * @brief GetID Get the KeyID of this public key (hash of its serialization)
     * @return
     */
    CKeyID GetID() const
    {
        return CKeyID(Hash160(vch, vch + size()));
    }

    /**
     * @brief GetHash Get the 256-bit hash of this public key.
     * @return
     */
    uint256 GetHash() const
    {
        return Hash(vch, vch + size());
    }

    /**
     * @brief IsValid Check syntactic correctness.
     * Note that this is consensus critical as CheckSig() calls it!
     * @return
     */
    bool IsValid() const
    {
        return size() > 0;
    }


    /**
     * @brief IsFullyValid  fully validate whether this is a valid public key
     * (more expensive than IsValid())
     * @return
     */
    bool IsFullyValid() const;

    /**
     * @brief IsCompressed Check whether this is a compressed public key.
     * @return
     */
    bool IsCompressed() const
    {
        return size() == 33;
    }

    /**
     * @brief Verify Verify a DER signature (~72 bytes).
     * If this public key is not fully valid, the return value will be false.
     * @param hash
     * @param vchSig
     * @return
     */
    bool Verify(const uint256& hash, const std::vector<unsigned char>& vchSig) const;

    /**
     * @brief CheckLowS Check whether a signature is normalized (lower-S).
     * @param vchSig
     * @return
     */
    static bool CheckLowS(const std::vector<unsigned char>& vchSig);


    /**
     * @brief RecoverCompact  Recover a public key from a compact signature.
     * @param hash
     * @param vchSig
     * @return
     */
    bool RecoverCompact(const uint256& hash, const std::vector<unsigned char>& vchSig);


    /**
     * @brief Decompress Turn this public key into an uncompressed public key.
     * @return
     */
    bool Decompress();

    //! Derive BIP32 child pubkey.
    // bool Derive(CPubKey& pubkeyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const;
};

struct CExtPubKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CPubKey pubkey;

    friend bool operator==(const CExtPubKey &a, const CExtPubKey &b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0], sizeof(a.vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.pubkey == b.pubkey;
    }

    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
    // bool Derive(CExtPubKey& out, unsigned int nChild) const;

    unsigned int GetSerializeSize(int /*nType*/, int /*nVersion*/) const
    {
        return BIP32_EXTKEY_SIZE+1; //add one byte for the size (compact int)
    }
    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        unsigned int len = BIP32_EXTKEY_SIZE;
        ::WriteCompactSize(s, len);
        unsigned char code[BIP32_EXTKEY_SIZE];
        Encode(code);
        s.write((const char *)&code[0], len);
    }
    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int len = ::ReadCompactSize(s);
        unsigned char code[BIP32_EXTKEY_SIZE];
        if (len != BIP32_EXTKEY_SIZE)
            throw std::runtime_error("Invalid extended key size\n");
        s.read((char *)&code[0], len);
        Decode(code);
    }
};

/** Users of this module must hold an ECCVerifyHandle. The constructor and
 *  destructor of these are not allowed to run in parallel, though. */
class ECCVerifyHandle
{
    static int refcount;

public:
    ECCVerifyHandle();
    ~ECCVerifyHandle();
};

} // namespace xbridge

#endif // XBRIDGE_BITCOIN_PUBKEY_H
