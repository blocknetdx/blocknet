// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ARITH_UINT256_H
#define BITCOIN_ARITH_UINT256_H

#include "uint256.h"

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

class uint512;
class uint256;

/** 256-bit unsigned big integer. */
class arith_uint256 : public base_uint<256>
{
public:
    arith_uint256() {}
    arith_uint256(const base_uint<256>& b) : base_uint<256>(b) {}
    arith_uint256(uint64_t b) : base_uint<256>(b) {}
    explicit arith_uint256(const std::string& str) : base_uint<256>(str) {}
    explicit arith_uint256(const std::vector<unsigned char>& vch) : base_uint<256>(vch) {}

    /**
     * The "compact" format is a representation of a whole
     * number N using an unsigned 32bit number similar to a
     * floating point format.
     * The most significant 8 bits are the unsigned exponent of base 256.
     * This exponent can be thought of as "number of bytes of N".
     * The lower 23 bits are the mantissa.
     * Bit number 24 (0x800000) represents the sign of N.
     * N = (-1^sign) * mantissa * 256^(exponent-3)
     *
     * Satoshi's original implementation used BN_bn2mpi() and BN_mpi2bn().
     * MPI uses the most significant bit of the first byte as sign.
     * Thus 0x1234560000 is compact (0x05123456)
     * and  0xc0de000000 is compact (0x0600c0de)
     *
     * Bitcoin only uses this "compact" format for encoding difficulty
     * targets, which are unsigned 256bit quantities.  Thus, all the
     * complexities of the sign bit and using base 256 are probably an
     * implementation accident.
     */
    arith_uint256& SetCompact(uint32_t nCompact, bool* pfNegative = NULL, bool* pfOverflow = NULL);
    uint32_t GetCompact(bool fNegative = false) const;

    friend uint256 ArithToUint256(const arith_uint256&);
    friend arith_uint256 UintToArith256(const uint256&);
    uint64_t GetHash(const arith_uint256& salt) const;
};


/** 512-bit unsigned big integer. */
class arith_uint512 : public base_uint<512>
{
public:
    arith_uint512() {}
    arith_uint512(const base_uint<512>& b) : base_uint<512>(b) {}
    arith_uint512(uint64_t b) : base_uint<512>(b) {}
    explicit arith_uint512(const std::string& str) : base_uint<512>(str) {}
    explicit arith_uint512(const std::vector<unsigned char>& vch) : base_uint<512>(vch) {}

    uint64_t GetHash(const arith_uint256& salt) const;

    friend arith_uint512 UintToArith512(const uint512& a);
    friend uint512 ArithToUint512(const arith_uint512& a);
};

uint256 ArithToUint256(const arith_uint256&);
arith_uint256 UintToArith256(const uint256&);
uint512 ArithToUint512(const arith_uint512&);
arith_uint512 UintToArith512(const uint512&);

#endif // BITCOIN_UINT256_H
