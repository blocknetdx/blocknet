// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>

#include <uint256.h>
#include <util/strencodings.h>
#include <crypto/common.h>

#include <stdio.h>
#include <string.h>


// This implementation directly uses shifts instead of going
// through an intermediate MPI representation.
arith_uint256& arith_uint256::SetCompact(uint32_t nCompact, bool* pfNegative, bool* pfOverflow)
{
    int nSize = nCompact >> 24;
    uint32_t nWord = nCompact & 0x007fffff;
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        *this = nWord;
    } else {
        *this = nWord;
        *this <<= 8 * (nSize - 3);
    }
    if (pfNegative)
        *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
    if (pfOverflow)
        *pfOverflow = nWord != 0 && ((nSize > 34) ||
                                        (nWord > 0xff && nSize > 33) ||
                                        (nWord > 0xffff && nSize > 32));
    return *this;
}

uint32_t arith_uint256::GetCompact(bool fNegative) const
{
    int nSize = (bits() + 7) / 8;
    uint32_t nCompact = 0;
    if (nSize <= 3) {
        nCompact = GetLow64() << 8 * (3 - nSize);
    } else {
        arith_uint256 bn = *this >> 8 * (nSize - 3);
        nCompact = bn.GetLow64();
    }
    // The 0x00800000 bit denotes the sign.
    // Thus, if it is already set, divide the mantissa by 256 and increase the exponent.
    if (nCompact & 0x00800000) {
        nCompact >>= 8;
        nSize++;
    }
    assert((nCompact & ~0x007fffff) == 0);
    assert(nSize < 256);
    nCompact |= nSize << 24;
    nCompact |= (fNegative && (nCompact & 0x007fffff) ? 0x00800000 : 0);
    return nCompact;
}

uint256 ArithToUint256(const arith_uint256& a)
{
    uint256 b;
    for (int x = 0; x < a.WIDTH; ++x)
        WriteLE32(b.begin() + x * 4, a.pn[x]);
    return b;
}
arith_uint256 UintToArith256(const uint256& a)
{
    arith_uint256 b;
    for (int x = 0; x < b.WIDTH; ++x)
        b.pn[x] = ReadLE32(a.begin() + x * 4);
    return b;
}

// Explicit instantiations for base_uint<512>
// template base_uint<512>::base_uint(const std::string&);
// template base_uint<512>& base_uint<512>::operator<<=(unsigned int);
// template base_uint<512>& base_uint<512>::operator>>=(unsigned int);
// template base_uint<512>& base_uint<512>::operator*=(uint32_t b32);
// template base_uint<512>& base_uint<512>::operator*=(const base_uint<512>& b);
// template base_uint<512>& base_uint<512>::operator/=(const base_uint<512>& b);
// template int base_uint<512>::CompareTo(const base_uint<512>&) const;
// template bool base_uint<512>::EqualTo(uint64_t) const;
// template double base_uint<512>::getdouble() const;
// template<> std::string base_uint<512>::GetHex() const { return ArithToUint512(*this).GetHex(); }
// template std::string base_uint<512>::ToString() const;
// template<> void base_uint<512>::SetHex(const char* psz) { *this = UintToArith512(uint512S(psz)); }
// template void base_uint<512>::SetHex(const std::string&);
// template unsigned int base_uint<512>::bits() const;


uint512 ArithToUint512(const arith_uint512& a)
{
    uint512 b;
    for (int x = 0; x < a.WIDTH; ++x)
        WriteLE32(b.begin() + x * 4, a.pn[x]);
    return b;
}

arith_uint512 UintToArith512(const uint512& a)
{
    arith_uint512 b;
    for (int x = 0; x < b.WIDTH; ++x)
        b.pn[x] = ReadLE32(a.begin() + x * 4);
    return b;
}
