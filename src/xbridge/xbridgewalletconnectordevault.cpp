// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/xbridgewalletconnectordevault.h>
#include <xbridge/util/logger.h>
#include <xbridge/xbitcointransaction.h>

#include <base58.h>
#include <primitives/transaction.h>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

namespace rpc
{
bool decodeRawTransaction(const std::string & rpcuser, const std::string & rpcpasswd,
                          const std::string & rpcip, const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid, std::string & tx);
}

enum
{
    SCRIPT_ENABLE_SIGHASH_FORKID = (1U << 16),    // https://github.com/devaultcrypto/devault/blob/03e826eb6c9931dbbbf5294445c3300044b7e127/src/script/script_flags.h#L92
    SCRIPT_ENABLE_REPLAY_PROTECTION = (1U << 17), // https://github.com/devaultcrypto/devault/blob/03e826eb6c9931dbbbf5294445c3300044b7e127/src/script/script_flags.h#L96
};

//******************************************************************************
//******************************************************************************
namespace
{

// https://github.com/devaultcrypto/devault/blob/8fa24a352a19819a65d2456c5695b46428601b52/src/script/sighashtype.h#L13
/** Signature hash types/flags */
enum {
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_FORKID = 0x40,
    SIGHASH_ANYONECANPAY = 0x80,
};

/**
 * Base signature hash types
 * Base sig hash types not defined in this enum may be used, but they will be
 * represented as UNSUPPORTED.  See transaction
 * c99c49da4c38af669dea436d3e73780dfdb6c1ecf9958baa52960e8baee30e73 for an
 * example where an unsupported base sig hash of 0 was used.
 */
enum class BaseSigHashType : uint8_t {
    UNSUPPORTED = 0,
    ALL = SIGHASH_ALL,
    NONE = SIGHASH_NONE,
    SINGLE = SIGHASH_SINGLE
};

/** Signature hash type wrapper class */
class SigHashType {
private:
    uint32_t sigHash;

public:
    explicit SigHashType() : sigHash(SIGHASH_ALL) {}

    explicit SigHashType(uint32_t sigHashIn) : sigHash(sigHashIn) {}

    SigHashType withBaseType(BaseSigHashType baseSigHashType) const {
        return SigHashType((sigHash & ~0x1f) | uint32_t(baseSigHashType));
    }

    SigHashType withForkValue(uint32_t forkId) const {
        return SigHashType((forkId << 8) | (sigHash & 0xff));
    }

    SigHashType withForkId(bool forkId = true) const {
        return SigHashType((sigHash & ~SIGHASH_FORKID) |
                           (forkId ? SIGHASH_FORKID : 0));
    }

    SigHashType withAnyoneCanPay(bool anyoneCanPay = true) const {
        return SigHashType((sigHash & ~SIGHASH_ANYONECANPAY) |
                           (anyoneCanPay ? SIGHASH_ANYONECANPAY : 0));
    }

    BaseSigHashType getBaseType() const {
        return BaseSigHashType(sigHash & 0x1f);
    }

    uint32_t getForkValue() const { return sigHash >> 8; }

    bool isDefined() const {
        auto baseType =
            BaseSigHashType(sigHash & ~(SIGHASH_FORKID | SIGHASH_ANYONECANPAY));
        return baseType >= BaseSigHashType::ALL &&
               baseType <= BaseSigHashType::SINGLE;
    }

    bool hasForkId() const { return (sigHash & SIGHASH_FORKID) != 0; }

    bool hasAnyoneCanPay() const {
        return (sigHash & SIGHASH_ANYONECANPAY) != 0;
    }

    uint32_t getRawSigHashType() const { return sigHash; }

    template <typename Stream> void Serialize(Stream &s) const {
        ::Serialize(s, getRawSigHashType());
    }

    template <typename Stream> void Unserialize(Stream &s) {
        ::Unserialize(s, sigHash);
    }

    /**
     * Handy operators.
     */
    friend constexpr bool operator==(const SigHashType &a,
                                     const SigHashType &b) {
        return a.sigHash == b.sigHash;
    }

    friend constexpr bool operator!=(const SigHashType &a,
                                     const SigHashType &b) {
        return !(a == b);
    }
};

// https://github.com/devaultcrypto/devault/blob/03e826eb6c9931dbbbf5294445c3300044b7e127/src/script/interpreter.cpp#L1354
template <class T> uint256 GetPrevoutHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}
template <class T> uint256 GetSequenceHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}
template <class T> uint256 GetOutputsHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetHash();
}

//******************************************************************************
//******************************************************************************
typedef struct { // helper for bch cache pointer
    uint256 hashPrevouts;
    uint256 hashSequence;
    uint256 hashOutputs;
} cache_t;
// Reference: https://github.com/devaultcrypto/devault/blob/03e826eb6c9931dbbbf5294445c3300044b7e127/src/script/interpreter.cpp#L1399
uint256 SignatureHash(const CScript &scriptCode, const CTransactionPtr & tx,
                      unsigned int nIn, SigHashType sigHashType,
                      const CAmount amount)
{
    // XBRIDGE
    auto & txTo = *tx;
    uint32_t flags = SCRIPT_ENABLE_SIGHASH_FORKID; // devault doesn't support SCRIPT_ENABLE_REPLAY_PROTECTION at this time
    cache_t *cache = nullptr;
    // END XBRIDGE

    if (flags & SCRIPT_ENABLE_REPLAY_PROTECTION) {
        // Legacy chain's value for fork id must be of the form 0xffxxxx.
        // By xoring with 0xdead, we ensure that the value will be different
        // from the original one, even if it already starts with 0xff.
        uint32_t newForkValue = sigHashType.getForkValue() ^ 0xdead;
        sigHashType = sigHashType.withForkValue(0xff0000 | newForkValue);
    }

    if (sigHashType.hasForkId() && (flags & SCRIPT_ENABLE_SIGHASH_FORKID)) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;

        if (!sigHashType.hasAnyoneCanPay()) {
            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!sigHashType.hasAnyoneCanPay() &&
            (sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE)) {
            hashSequence = cache ? cache->hashSequence : GetSequenceHash(txTo);
        }

        if ((sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE)) {
            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(txTo);
        } else if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
                   (nIn < txTo.vout.size())) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo.vout[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo.nVersion;
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with scriptCode +
        // amount). The prevout may already be contained in hashPrevout, and the
        // nSequence may already be contain in hashSequence.
        ss << txTo.vin[nIn].prevout;
        ss << scriptCode;
        ss << amount;
        ss << txTo.vin[nIn].nSequence;
        // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;
        // Locktime
        ss << txTo.nLockTime;
        // Sighash type
        ss << sigHashType;

        return ss.GetHash();
    }

    // XBRIDGE should never end up here
    return {};
    // legacy code disabled below
    // XBRIDGE

    /*static const uint256 one(uint256S(
            "0000000000000000000000000000000000000000000000000000000000000001"));

    // Check for invalid use of SIGHASH_SINGLE
    if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
        (nIn >= txTo.vout.size())) {
        //  nOut out of range
        return one;
    }

    // Wrapper to serialize only the necessary parts of the transaction being
    // signed
    CTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn,
                                             sigHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << sigHashType;
    return ss.GetHash();*/
}

} // namespace


//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr createTransaction(const bool txWithTimeField);
xbridge::CTransactionPtr createTransaction(const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> > & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField);

//******************************************************************************
//******************************************************************************
DevaultWalletConnector::DevaultWalletConnector() : BchWalletConnector() {}

bool DevaultWalletConnector::init() {
    if (!BchWalletConnector::init())
        return false;
    replayProtection = false; // devault doesn't support this fork
    if (cashAddrPrefix.empty() || cashAddrPrefix == "bitcoincash") // override bitcoincash superclass
        cashAddrPrefix = "devault";
    params.prefix = cashAddrPrefix;
    return true;
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr createTransaction(const bool txWithTimeField = false);
xbridge::CTransactionPtr createTransaction(const WalletConnector & conn,
                                           const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField = false);

//******************************************************************************
//******************************************************************************
bool DevaultWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
                                                 const std::vector<std::pair<std::string, double> > & outputs,
                                                 const std::vector<unsigned char> & mpubKey,
                                                 const std::vector<unsigned char> & mprivKey,
                                                 const std::vector<unsigned char> & innerScript,
                                                 const uint32_t lockTime,
                                                 std::string & txId,
                                                 std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(*this, inputs, outputs, COIN, txVersion, lockTime, txWithTimeField);
    // Correctly set tx input sequence. If lockTime is specified sequence must be 2^32-2, otherwise 2^32-1 (Final)
    uint32_t sequence = lockTime > 0 ? xbridge::SEQUENCE_FINAL-1 : xbridge::SEQUENCE_FINAL;
    txUnsigned->vin[0].nSequence = sequence;

    CScript inner(innerScript.begin(), innerScript.end());

    CScript redeem;
    {
        CScript tmp;
        tmp << ToByteVector(mpubKey) << OP_TRUE << ToByteVector(inner);

        SigHashType sigHashType = SigHashType(SIGHASH_ALL).withForkId();
        std::vector<unsigned char> signature;
        uint256 hash = SignatureHash(inner, txUnsigned, 0, sigHashType, inputs[0].amount*COIN);
        if (!m_cp.sign(mprivKey, hash, signature))
        {
            LOG() << "bch sign transaction error " << __FUNCTION__;
            return false;
        }

        signature.push_back(uint8_t(sigHashType.getRawSigHashType()));

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "bch transaction not created " << __FUNCTION__;
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->nTime     = txUnsigned->nTime;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem, sequence));
    tx->vout      = txUnsigned->vout;
    tx->nLockTime = txUnsigned->nLockTime;

    rawTx = tx->toString();

    std::string json;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, reftxid, json))
    {
        LOG() << "bch decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
bool DevaultWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                  const std::vector<unsigned char> & mpubKey,
                                                  const std::vector<unsigned char> & mprivKey,
                                                  const std::vector<unsigned char> & xpubKey,
                                                  const std::vector<unsigned char> & innerScript,
                                                  std::string & txId,
                                                  std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(*this, inputs, outputs, COIN, txVersion, 0, txWithTimeField);

    CScript inner(innerScript.begin(), innerScript.end());

    SigHashType sigHashType = SigHashType(SIGHASH_ALL).withForkId();
    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, sigHashType, inputs[0].amount*COIN);
    if (!m_cp.sign(mprivKey, hash, signature))
    {
        LOG() << "bch sign transaction error " << __FUNCTION__;
        return false;
    }

    signature.push_back(uint8_t(sigHashType.getRawSigHashType()));

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << ToByteVector(inner);

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "bch transaction not created " << __FUNCTION__;
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->nTime     = txUnsigned->nTime;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem));
    tx->vout      = txUnsigned->vout;

    rawTx = tx->toString();

    std::string json;
    std::string paytxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, paytxid, json))
    {
        LOG() << "bch decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
