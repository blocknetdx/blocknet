// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/util/logger.h>
#include <xbridge/xbridgecryptoproviderbtc.h>

#include <random.h>
#include <support/allocators/secure.h>

//******************************************************************************
//******************************************************************************
/** This function is taken from the libsecp256k1 distribution and implements
 *  DER parsing for ECDSA signatures, while supporting an arbitrary subset of
 *  format violations.
 *
 *  Supported violations include negative integers, excessive padding, garbage
 *  at the end, and overly long length descriptors. This is safe to use in
 *  Bitcoin because since the activation of BIP66, signatures are verified to be
 *  strict DER before being passed to this module, and we know it supports all
 *  violations present in the blockchain before that point.
 */
static int ecdsa_signature_parse_der_lax(const secp256k1_context* ctx, secp256k1_ecdsa_signature* sig,
                                         const unsigned char *input, size_t inputlen)
{
    size_t rpos, rlen, spos, slen;
    size_t pos = 0;
    size_t lenbyte;
    unsigned char tmpsig[64] = {0};
    int overflow = 0;

    /* Hack to initialize sig with a correctly-parsed but invalid signature. */
    secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);

    /* Sequence tag byte */
    if (pos == inputlen || input[pos] != 0x30) {
        return 0;
    }
    pos++;

    /* Sequence length bytes */
    if (pos == inputlen) {
        return 0;
    }
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (pos + lenbyte > inputlen) {
            return 0;
        }
        pos += lenbyte;
    }

    /* Integer tag byte for R */
    if (pos == inputlen || input[pos] != 0x02) {
        return 0;
    }
    pos++;

    /* Integer length for R */
    if (pos == inputlen) {
        return 0;
    }
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (pos + lenbyte > inputlen) {
            return 0;
        }
        while (lenbyte > 0 && input[pos] == 0) {
            pos++;
            lenbyte--;
        }
        if (lenbyte >= sizeof(size_t)) {
            return 0;
        }
        rlen = 0;
        while (lenbyte > 0) {
            rlen = (rlen << 8) + input[pos];
            pos++;
            lenbyte--;
        }
    } else {
        rlen = lenbyte;
    }
    if (rlen > inputlen - pos) {
        return 0;
    }
    rpos = pos;
    pos += rlen;

    /* Integer tag byte for S */
    if (pos == inputlen || input[pos] != 0x02) {
        return 0;
    }
    pos++;

    /* Integer length for S */
    if (pos == inputlen) {
        return 0;
    }
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (pos + lenbyte > inputlen) {
            return 0;
        }
        while (lenbyte > 0 && input[pos] == 0) {
            pos++;
            lenbyte--;
        }
        if (lenbyte >= sizeof(size_t)) {
            return 0;
        }
        slen = 0;
        while (lenbyte > 0) {
            slen = (slen << 8) + input[pos];
            pos++;
            lenbyte--;
        }
    } else {
        slen = lenbyte;
    }
    if (slen > inputlen - pos) {
        return 0;
    }
    spos = pos;
    pos += slen;

    /* Ignore leading zeroes in R */
    while (rlen > 0 && input[rpos] == 0) {
        rlen--;
        rpos++;
    }
    /* Copy R value */
    if (rlen > 32) {
        overflow = 1;
    } else {
        memcpy(tmpsig + 32 - rlen, input + rpos, rlen);
    }

    /* Ignore leading zeroes in S */
    while (slen > 0 && input[spos] == 0) {
        slen--;
        spos++;
    }
    /* Copy S value */
    if (slen > 32) {
        overflow = 1;
    } else {
        memcpy(tmpsig + 64 - slen, input + spos, slen);
    }

    if (!overflow) {
        overflow = !secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);
    }
    if (overflow) {
        /* Overwrite the result again with a correctly-parsed but invalid
           signature if parsing failed. */
        memset(tmpsig, 0, 64);
        secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);
    }
    return 1;
}

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
BtcCryptoProvider::BtcCryptoProvider()
{
    context = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

    // Pass in a random blinding seed to the secp256k1 context.
    std::vector<unsigned char, secure_allocator<unsigned char>> seed(32);
    GetRandBytes(seed.data(), 32);
    bool ret = secp256k1_context_randomize(context, seed.data());
    if (!ret)
        ERR() << "can't randomize secp256k1 context " << __FUNCTION__;
}

//*****************************************************************************
//*****************************************************************************
BtcCryptoProvider::~BtcCryptoProvider()
{
    secp256k1_context_destroy(context);
}

//*****************************************************************************
//*****************************************************************************
bool BtcCryptoProvider::check(const std::vector<unsigned char> & key)
{
    return secp256k1_ec_seckey_verify(context, &key[0]);
}

//*****************************************************************************
//*****************************************************************************
void BtcCryptoProvider::makeNewKey(std::vector<unsigned char> & key)
{
    key.resize(32);
    do
    {
        GetStrongRandBytes(&key[0], key.size());
    } while (!check(key));
}

//*****************************************************************************
//*****************************************************************************
bool BtcCryptoProvider::getPubKey(const std::vector<unsigned char> & key, std::vector<unsigned char> & pub)
{
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(context, &pubkey, &key[0]))
    {
        return false;
    }

    pub.resize(65);
    size_t clen = 65;
    secp256k1_ec_pubkey_serialize(context, &pub[0],
                                  &clen, &pubkey,
                                  SECP256K1_EC_COMPRESSED);
    pub.resize(clen);
    return true;
}

//******************************************************************************
//******************************************************************************
bool BtcCryptoProvider::sign(const std::vector<unsigned char> & key,
                             const uint256 & data,
                             std::vector<unsigned char> & signature)
{
    size_t signatureLength = 72;
    signature.resize(72);

    secp256k1_ecdsa_signature sig;
    int ret = secp256k1_ecdsa_sign(context, &sig, data.begin(), &key[0],
                                   secp256k1_nonce_function_rfc6979, NULL);
    if (!ret)
    {
        return false;
    }

    secp256k1_ecdsa_signature_serialize_der(context, &signature[0], &signatureLength, &sig);
    signature.resize(signatureLength);
    return true;
}

//******************************************************************************
//******************************************************************************
bool BtcCryptoProvider::verify(const std::vector<unsigned char> & pubkey,
                               const uint256 & data,
                               const std::vector<unsigned char> & signature)
{
    secp256k1_pubkey _pubkey;
    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ec_pubkey_parse(context, &_pubkey, &pubkey[0], pubkey.size()))
    {
        return false;
    }
    if (signature.size() == 0)
    {
        return false;
    }
    if (!ecdsa_signature_parse_der_lax(context, &sig, &signature[0], signature.size()))
    {
        return false;
    }
    // libsecp256k1's ECDSA verification requires lower-S signatures, which have
    // not historically been enforced in Bitcoin, so normalize them first.
    secp256k1_ecdsa_signature_normalize(context, &sig, &sig);
    return secp256k1_ecdsa_verify(context, &sig, data.begin(), &_pubkey);
}

} // namespace xbridge
