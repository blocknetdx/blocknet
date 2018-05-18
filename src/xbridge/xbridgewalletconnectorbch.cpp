//******************************************************************************
//******************************************************************************

#include "xbridgewalletconnectorbch.h"

#include "xbitcoinaddress.h"
#include "xbitcointransaction.h"

#include "util/logger.h"

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

//******************************************************************************
//******************************************************************************
enum
{
    SIGHASH_FORKID = 0x40
};

//******************************************************************************
//******************************************************************************
namespace
{

//******************************************************************************
// Base signature hash types
// Base sig hash types not defined in this enum may be used, but they will be
// represented as UNSUPPORTED.  See transaction
// c99c49da4c38af669dea436d3e73780dfdb6c1ecf9958baa52960e8baee30e73 for an
// example where an unsupported base sig hash of 0 was used.
//******************************************************************************
enum class BaseSigHashType : uint8_t
{
    UNSUPPORTED = 0,
    ALL = SIGHASH_ALL,
    NONE = SIGHASH_NONE,
    SINGLE = SIGHASH_SINGLE
};

//******************************************************************************
// Signature hash type wrapper class
//******************************************************************************
class SigHashType {
private:
    uint32_t sigHash;

public:
    explicit SigHashType() : sigHash(SIGHASH_ALL) {}

    explicit SigHashType(uint32_t sigHashIn) : sigHash(sigHashIn) {}

    SigHashType withBaseType(BaseSigHashType baseSigHashType) const
    {
        return SigHashType((sigHash & ~0x1f) | uint32_t(baseSigHashType));
    }

    SigHashType withForkValue(uint32_t forkId) const
    {
        return SigHashType((forkId << 8) | (sigHash & 0xff));
    }

    SigHashType withForkId(bool forkId = true) const
    {
        return SigHashType((sigHash & ~SIGHASH_FORKID) |
                           (forkId ? SIGHASH_FORKID : 0));
    }

    SigHashType withAnyoneCanPay(bool anyoneCanPay = true) const
    {
        return SigHashType((sigHash & ~SIGHASH_ANYONECANPAY) |
                           (anyoneCanPay ? SIGHASH_ANYONECANPAY : 0));
    }

    BaseSigHashType getBaseType() const
    {
        return BaseSigHashType(sigHash & 0x1f);
    }

    uint32_t getForkValue() const { return sigHash >> 8; }

    bool isDefined() const
    {
        auto baseType =
            BaseSigHashType(sigHash & ~(SIGHASH_FORKID | SIGHASH_ANYONECANPAY));
        return baseType >= BaseSigHashType::ALL &&
               baseType <= BaseSigHashType::SINGLE;
    }

    bool hasForkId() const { return (sigHash & SIGHASH_FORKID) != 0; }

    bool hasAnyoneCanPay() const
    {
        return (sigHash & SIGHASH_ANYONECANPAY) != 0;
    }

    uint32_t getRawSigHashType() const { return sigHash; }

    template <typename Stream> void Serialize(Stream &s, int nType, int nVersion) const
    {
        ::Serialize(s, getRawSigHashType(), nType, nVersion);
    }
};

//******************************************************************************
//******************************************************************************
uint256 GetPrevoutHash(const CTransactionPtr & txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (size_t n = 0; n < txTo->vin.size(); n++)
    {
        ss << txTo->vin[n].prevout;
    }
    return ss.GetHash();
}

//******************************************************************************
//******************************************************************************
uint256 GetSequenceHash(const CTransactionPtr & txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (size_t n = 0; n < txTo->vin.size(); n++)
    {
        ss << txTo->vin[n].nSequence;
    }
    return ss.GetHash();
}

//******************************************************************************
//******************************************************************************
uint256 GetOutputsHash(const CTransactionPtr & txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (size_t n = 0; n < txTo->vout.size(); n++)
    {
        ss << txTo->vout[n];
    }
    return ss.GetHash();
}

//******************************************************************************
//******************************************************************************
uint256 SignatureHash(const CScript &scriptCode, const CTransactionPtr & txTo,
                      unsigned int nIn, SigHashType sigHashType,
                      const CAmount amount
                      /*, const PrecomputedTransactionData *cache, uint32_t flags*/)
{
// WARNING BCH Nov 15, 2018 hard fork
//    if (flags & SCRIPT_ENABLE_REPLAY_PROTECTION)
//    {
//        // Legacy chain's value for fork id must be of the form 0xffxxxx.
//        // By xoring with 0xdead, we ensure that the value will be different
//        // from the original one, even if it already starts with 0xff.
//        uint32_t newForkValue = sigHashType.getForkValue() ^ 0xdead;
//        sigHashType = sigHashType.withForkValue(0xff0000 | newForkValue);
//    }

    // if (sigHashType.hasForkId() && (flags & SCRIPT_ENABLE_SIGHASH_FORKID))
    {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;

        if (!sigHashType.hasAnyoneCanPay())
        {
            hashPrevouts = /*cache ? cache->hashPrevouts : */GetPrevoutHash(txTo);
        }

        if (!sigHashType.hasAnyoneCanPay() &&
            (sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE))
        {
            hashSequence = /*cache ? cache->hashSequence : */GetSequenceHash(txTo);
        }

        if ((sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE))
        {
            hashOutputs = /*cache ? cache->hashOutputs : */GetOutputsHash(txTo);
        }
        else if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
                   (nIn < txTo->vout.size()))
        {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo->vout[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo->nVersion;
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with scriptCode +
        // amount). The prevout may already be contained in hashPrevout, and the
        // nSequence may already be contain in hashSequence.
        ss << txTo->vin[nIn].prevout;
        ss << scriptCode;
        ss << amount; // .GetSatoshis();
        ss << txTo->vin[nIn].nSequence;
        // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;
        // Locktime
        ss << txTo->nLockTime;
        // Sighash type
        ss << sigHashType;

        return ss.GetHash();
    }

//    static const uint256 one(uint256S(
//        "0000000000000000000000000000000000000000000000000000000000000001"));
//    if (nIn >= txTo.vin.size()) {
//        //  nIn out of range
//        return one;
//    }

//    // Check for invalid use of SIGHASH_SINGLE
//    if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
//        (nIn >= txTo.vout.size())) {
//        //  nOut out of range
//        return one;
//    }

//    // Wrapper to serialize only the necessary parts of the transaction being
//    // signed
//    CTransactionSignatureSerializer txTmp(txTo, scriptCode, nIn, sigHashType);

//    // Serialize and hash
//    CHashWriter ss(SER_GETHASH, 0);
//    ss << txTmp << sigHashType;
//    return ss.GetHash();
}

}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr createTransaction(const bool txWithTimeField = false);
xbridge::CTransactionPtr createTransaction(const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField = false);

//******************************************************************************
//******************************************************************************
BchWalletConnector::BchWalletConnector()
    : BtcWalletConnector()
{

}

//******************************************************************************
//******************************************************************************
bool BchWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
                                                 const std::vector<std::pair<std::string, double> > & outputs,
                                                 const std::vector<unsigned char> & mpubKey,
                                                 const std::vector<unsigned char> & mprivKey,
                                                 const std::vector<unsigned char> & innerScript,
                                                 const uint32_t lockTime,
                                                 std::string & txId,
                                                 std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(inputs, outputs, COIN, txVersion, lockTime, txWithTimeField);
    txUnsigned->vin[0].nSequence = std::numeric_limits<uint32_t>::max()-1;

    CScript inner(innerScript.begin(), innerScript.end());

    CScript redeem;
    {
        CScript tmp;
        std::vector<unsigned char> raw(mpubKey.begin(), mpubKey.end());
        tmp << raw << OP_TRUE << inner;

        SigHashType sigHashType = SigHashType(SIGHASH_ALL).withForkId();
        std::vector<unsigned char> signature;
        uint256 hash = xbridge::SignatureHash(inner, txUnsigned, 0, sigHashType, inputs[0].amount*COIN);
        if (!sign(mprivKey, hash, signature))
        {
            // cancel transaction
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
            return false;
        }

        signature.push_back(uint8_t(sigHashType.getRawSigHashType()));

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "transaction not created " << __FUNCTION__;
//            sendCancelTransaction(xtx, crBadSettings);
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem, std::numeric_limits<uint32_t>::max()-1));
    tx->vout      = txUnsigned->vout;
    tx->nLockTime = txUnsigned->nLockTime;

    rawTx = tx->toString();

    std::string json;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, reftxid, json))
    {
        LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
//            sendCancelTransaction(xtx, crRpcError);
            return true;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
bool BchWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                  const std::vector<unsigned char> & mpubKey,
                                                  const std::vector<unsigned char> & mprivKey,
                                                  const std::vector<unsigned char> & xpubKey,
                                                  const std::vector<unsigned char> & innerScript,
                                                  std::string & txId,
                                                  std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(inputs, outputs, COIN, txVersion, 0, txWithTimeField);

    CScript inner(innerScript.begin(), innerScript.end());

    SigHashType sigHashType = SigHashType(SIGHASH_ALL).withForkId();
    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, sigHashType, inputs[0].amount*COIN);
    if (!sign(mprivKey, hash, signature))
    {
        // cancel transaction
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
        return false;
    }

    signature.push_back(uint8_t(sigHashType.getRawSigHashType()));

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << inner;

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "transaction not created " << __FUNCTION__;
//                sendCancelTransaction(xtx, crBadSettings);
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem));
    tx->vout      = txUnsigned->vout;

    rawTx = tx->toString();

    std::string json;
    std::string paytxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, paytxid, json))
    {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
