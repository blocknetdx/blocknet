//******************************************************************************
//******************************************************************************

#include "xrouterlogger.h"
#include "xrouterpacket.h"

#include "secp256k1.h"
#include "random.h"
#include "allocators.h"
#include "crypto/sha256.h"

#include <iostream>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
//******************************************************************************
//******************************************************************************
namespace
{
secp256k1_context * secpContext = nullptr;

class SecpInstance
{
public:
    SecpInstance()
    {
        assert(secpContext == nullptr);

        secp256k1_context * ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        assert(ctx != nullptr);

        {
            // Pass in a random blinding seed to the secp256k1 context.
            std::vector<unsigned char, secure_allocator<unsigned char>> vseed(32);
            GetRandBytes(vseed.data(), 32);
            bool ret = secp256k1_context_randomize(ctx, vseed.data());
            assert(ret);
        }

        secpContext = ctx;
    }
    ~SecpInstance()
    {
        secp256k1_context * ctx = secpContext;
        secpContext = nullptr;

        if (ctx)
        {
            secp256k1_context_destroy(ctx);
        }
    }
};
static SecpInstance secpInstance;

} // namespace

//******************************************************************************
//******************************************************************************
bool XRouterPacket::sign(const std::vector<unsigned char> & pubkey,
                         const std::vector<unsigned char> & privkey)
{
    if (pubkey.size() != pubkeySize || privkey.size() != privkeySize)
    {
        ERR() << "incorrect key size " << __FUNCTION__;
        return false;
    }

    memcpy(pubkeyField(), &pubkey[0], pubkeySize);
    memset(signatureField(), 0, rawSignatureSize); // unset

    unsigned char hash[CSHA256::OUTPUT_SIZE];

    {
        CSHA256 sha256;
        sha256.Write(&m_body[0], m_body.size());
        sha256.Finalize(hash);
    }

    secp256k1_ecdsa_signature sig;
    if (secp256k1_ecdsa_sign(secpContext, &sig, hash, &privkey[0], 0, 0) == 0)
    {
        return false;
    }

    secp256k1_ecdsa_signature_serialize_compact(secpContext, signatureField(), &sig);

    return verify();
}

//******************************************************************************
// verify signature
//******************************************************************************
bool XRouterPacket::verify()
{
    unsigned char signature[rawSignatureSize];
    memcpy(signature, signatureField(), rawSignatureSize);
    memset(signatureField(), 0, rawSignatureSize); // unset sig before compute hash

    unsigned char hash[CSHA256::OUTPUT_SIZE];

    {
        CSHA256 sha256;
        sha256.Write(&m_body[0], m_body.size());
        sha256.Finalize(hash);
    }

    // restore signature
    memcpy(signatureField(), signature, rawSignatureSize);

    secp256k1_ecdsa_signature sig;
    if (secp256k1_ecdsa_signature_parse_compact(secpContext, &sig, signatureField()) == 0)
    {
        ERR() << "incorrect or unparseable signature " << __FUNCTION__;
        return false;
    }

    secp256k1_pubkey scpubkey;
    if (secp256k1_ec_pubkey_parse(secpContext, &scpubkey, pubkeyField(), pubkeySize) == 0)
    {
        ERR() << "the public key could not be parsed or is invalid " << __FUNCTION__;
        return false;
    }

    if (secp256k1_ecdsa_verify(secpContext, &sig, hash, &scpubkey) != 1)
    {
        ERR() << "bad signature " << __FUNCTION__;
        return false;
    }

    // correct signature, check pubkey
    unsigned char pub[pubkeySize];
    size_t len = pubkeySize;
    secp256k1_ec_pubkey_serialize(secpContext, pub, &len, &scpubkey, SECP256K1_EC_COMPRESSED);

    if (len != pubkeySize)
    {
        ERR() << "incorrect pubkey length returned " << __FUNCTION__;
        return false;
    }

    if (memcmp(pub, pubkeyField(), pubkeySize) != 0)
    {
        ERR() << "signature correct, but different pubkeys " << __FUNCTION__;
        return false;
    }

    // all correct
    return true;
}

//******************************************************************************
// verify signature and pubkey
//******************************************************************************
bool XRouterPacket::verify(const std::vector<unsigned char> & pubkey)
{
    if (pubkey.size() != pubkeySize || memcmp(pubkeyField(), &pubkey[0], pubkeySize) != 0)
    {
        return false;
    }

    return verify();
}

bool XRouterPacket::copyFrom(const std::vector<unsigned char> & data)
{
    if (data.size() < headerSize)
    {
        ERR() << "received data size less than packet header size " << __FUNCTION__;
        return false;
    }

    m_body = data;

    if (sizeField() != static_cast<uint32_t>(data.size())-headerSize)
    {
        ERR() << "incorrect data size " << __FUNCTION__;
        return false;
    }

    return true;
}
} // namespace xrouter
