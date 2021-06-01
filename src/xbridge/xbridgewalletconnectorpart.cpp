// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/xbridgewalletconnectorpart.h>

#include <xbridge/util/logger.h>
#include <xbridge/xbitcointransaction.h>

namespace xbridge
{
//******************************************************************************
//******************************************************************************

namespace rpc
{

//*****************************************************************************
//*****************************************************************************
bool decodeRawTransaction(const std::string & rpcuser, const std::string & rpcpasswd,
                          const std::string & rpcip, const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid, std::string & tx);

bool signRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        std::string & rawtx,
                        bool & complete);
} // namespace rpc

namespace {
//******************************************************************************
//******************************************************************************
// Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/primitives/transaction.h#L35
enum OutputTypes
{
    //OUTPUT_NULL             = 0, // Marker for CCoinsView (0.14)
    OUTPUT_STANDARD         = 1,
    //OUTPUT_CT               = 2,
    //OUTPUT_RINGCT           = 3,
    //OUTPUT_DATA             = 4,
};

//******************************************************************************
//******************************************************************************
// Three methods below are returnig double SHA256 instead of single SHA256 in Particl.
// Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/script/interpreter.cpp#L1385
template <class T>
uint256 GetPrevoutHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}

template <class T>
uint256 GetSequenceHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

template <class T>
uint256 GetOutputsHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetHash();
}

// Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/serialize.h#L1177
inline void SetAmount(std::vector<uint8_t> &v, int64_t amount)
{
    v.resize(8);
    amount = (int64_t) htole64((uint64_t)amount);
    memcpy(v.data(), &amount, 8);
}

//******************************************************************************
//******************************************************************************
typedef struct { // helper for bch cache pointer
    uint256 hashPrevouts;
    uint256 hashSequence;
    uint256 hashOutputs;
    bool m_bip143_segwit_ready = false;
} cache_t;
// Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/script/interpreter.cpp#L1617
template <class T>
uint256 SignatureHash(const CScript& scriptCode, const T& txTo, unsigned int nIn, int nHashType, CAmount amountValue
                      /*, const std::vector<uint8_t>& amount
                       *, SigVersion sigversion
                       *, const PrecomputedTransactionData* cache*/)
{
    // XBRIDGE
    std::vector<uint8_t> amount(8);
    SetAmount(amount, amountValue);
    auto sigversion = SigVersion::BASE;
    cache_t *cache = nullptr;
    // END XBRIDGE

    assert(nIn < txTo.vin.size());

    if (sigversion == SigVersion::WITNESS_V0
        || txTo.IsParticlVersion()) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;
        const bool cacheready = cache && cache->m_bip143_segwit_ready;

        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
            hashPrevouts = cacheready ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashSequence = cacheready ? cache->hashSequence : GetSequenceHash(txTo);
        }

        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashOutputs = cacheready ? cache->hashOutputs : GetOutputsHash(txTo);
        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.GetNumVOuts()) {
            CHashWriter ss(SER_GETHASH, 0);

            //if (txTo.IsParticlVersion()) {
            //    ss << *(txTo.vpout[nIn].get());
            //} else {
                ss << txTo.vout[nIn];
            //}
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo.nVersion;
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with scriptCode + amount)
        // The prevout may already be contained in hashPrevout, and the nSequence
        // may already be contain in hashSequence.
        ss << txTo.vin[nIn].prevout;
        ss << scriptCode;

        //ss << amount;
        // Match << CAmount when amount.size() == 8
        ss.write((const char*)amount.data(), amount.size());

        ss << txTo.vin[nIn].nSequence;
        // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;
        // Locktime
        ss << txTo.nLockTime;
        // Sighash type
        ss << nHashType;

        return ss.GetHash();
    }

    // XBRIDGE should never end up here
    return {};
    // legacy code disabled below
    // XBRIDGE

    /*
    // Check for invalid use of SIGHASH_SINGLE
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo.GetNumVOuts()) {
            //  nOut out of range
            return uint256::ONE;
        }
    }

    // Wrapper to serialize only the necessary parts of the transaction being signed
    CTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn, nHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
    */
}
} // namespace

class XParticlTransaction : public CTransaction
{
    static const uint8_t PARTICL_TXN_VERSION = 0xA0;
public:
    XParticlTransaction(bool serializeWithTimeField = false) : CTransaction(serializeWithTimeField)
    {}

    XParticlTransaction(const uint8_t version, bool serializeWithTimeField = false) : CTransaction(serializeWithTimeField)
    {
        nVersion = version;
        assert((nVersion & 0xFF) >= PARTICL_TXN_VERSION);
    }

    inline bool IsParticlVersion() const
    {
        assert((nVersion & 0xFF) >= PARTICL_TXN_VERSION);
        return true;
    }

    size_t GetNumVOuts() const
    {
        return vout.size();
    }

    // Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/primitives/transaction.h#L756
    template <typename Stream>
    inline void Serialize(Stream& s) const
    {
        uint8_t bv = nVersion & 0xFF;
        s << bv;

        bv = (nVersion>>8) & 0xFF;
        s << bv; // TransactionType

        s << nLockTime;
        s << vin;

        WriteCompactSize(s, vout.size());
        for (std::size_t k = 0; k < vout.size(); ++k) {
            s << static_cast<uint8_t>(OUTPUT_STANDARD);
            s << vout[k];
        }
        for (auto &txin : vin) {
            s << txin.scriptWitness.stack;
        }
        return;
    }

    // Reference: https://github.com/particl/particl-core/blob/d45e8ecf75646142c2d8525ccc18eaa4f73673e1/src/primitives/transaction.h#L662
    template <typename Stream>
    inline void Unserialize(Stream& s)
    {
        const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

        uint8_t bv;
        nVersion = 0;
        s >> bv;

        if (bv >= PARTICL_TXN_VERSION) {
            nVersion = bv;

            s >> bv;
            nVersion |= bv<<8; // TransactionTypes

            s >> nLockTime;

            vin.clear();
            s >> vin;

            size_t nOutputs = ReadCompactSize(s);
            vout.clear();
            vout.reserve(nOutputs);
            for (size_t k = 0; k < nOutputs; ++k) {
                s >> bv;
                switch (bv) {
                    case OUTPUT_STANDARD:
                        vout.push_back(CTxOut());
                        break;
                    /*case OUTPUT_CT:
                        tx.vpout.push_back(MAKE_OUTPUT<CTxOutCT>());
                        break;
                    case OUTPUT_RINGCT:
                        tx.vpout.push_back(MAKE_OUTPUT<CTxOutRingCT>());
                        break;
                    case OUTPUT_DATA:
                        tx.vpout.push_back(MAKE_OUTPUT<CTxOutData>());
                        break;*/
                    default:
                        throw std::ios_base::failure("Unknown transaction output type");
                }
                s >> vout[k];
            }

            if (fAllowWitness) {
                for (auto &txin : vin)
                    s >> txin.scriptWitness.stack;
            }
            return;
        } else {
            throw std::ios_base::failure("Unsupported transaction version");
        }
    }

    virtual std::string toString() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return HexStr(ss.begin(), ss.end());
    }
};

namespace  {
//******************************************************************************
//******************************************************************************
XParticlTransaction createTransaction(const bool txWithTimeField)
{
    return xbridge::XParticlTransaction(txWithTimeField);
}

XParticlTransaction createTransaction(const WalletConnector & conn,
                                           const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField)
{
    xbridge::XParticlTransaction tx(txversion, txWithTimeField);
    tx.nLockTime = lockTime;

    for (const XTxIn & in : inputs)
    {
        tx.vin.push_back(CTxIn(COutPoint(uint256S(in.txid), in.n)));
    }

    for (const std::pair<std::string, double> & out : outputs)
    {
        std::vector<unsigned char> id = conn.toXAddr(out.first);

        CScript scr;
        scr << OP_DUP << OP_HASH160 << ToByteVector(id) << OP_EQUALVERIFY << OP_CHECKSIG;

        tx.vout.push_back(CTxOut(out.second * COIN, scr));
    }

    return tx;
}
} // namespace

//******************************************************************************
//******************************************************************************
PartWalletConnector::PartWalletConnector(){}

//******************************************************************************
//******************************************************************************
bool PartWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                  const std::vector<unsigned char> & mpubKey,
                                                  const std::vector<unsigned char> & mprivKey,
                                                  const std::vector<unsigned char> & innerScript,
                                                  const uint32_t lockTime,
                                                  std::string & txId,
                                                  std::string & rawTx)
{
    XParticlTransaction txUnsigned = createTransaction(*this,
                                                            inputs, outputs,
                                                            COIN, txVersion,
                                                            lockTime, txWithTimeField);
    // Correctly set tx input sequence. If lockTime is specified sequence must be 2^32-2, otherwise 2^32-1 (Final)
    uint32_t sequence = lockTime > 0 ? xbridge::SEQUENCE_FINAL-1 : xbridge::SEQUENCE_FINAL;
    txUnsigned.vin[0].nSequence = sequence;

    CScript inner(innerScript.begin(), innerScript.end());

    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, SIGHASH_ALL, inputs[0].amount.Get64() * COIN);
    if (!m_cp.sign(mprivKey, hash, signature)) {
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    signature.push_back((unsigned char)SIGHASH_ALL);

    XParticlTransaction tx(createTransaction(txWithTimeField));
    tx.nVersion  = txUnsigned.nVersion;
    tx.nTime     = txUnsigned.nTime;

    CTxIn in(txUnsigned.vin[0].prevout, CScript(), sequence);
    in.scriptWitness.stack.push_back(signature);
    in.scriptWitness.stack.push_back(ToByteVector(mpubKey));
    in.scriptWitness.stack.push_back(std::vector<unsigned char>(1, OP_TRUE));
    in.scriptWitness.stack.push_back(ToByteVector(inner));
    tx.vin.push_back(in);

    tx.vout      = txUnsigned.vout;
    tx.nLockTime = txUnsigned.nLockTime;

    rawTx = tx.toString();

    std::string json;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, reftxid, json)) {
        LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
bool PartWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                                   const std::vector<std::pair<std::string, double> > & outputs,
                                                   const std::vector<unsigned char> & mpubKey,
                                                   const std::vector<unsigned char> & mprivKey,
                                                   const std::vector<unsigned char> & xpubKey,
                                                   const std::vector<unsigned char> & innerScript,
                                                   std::string & txId,
                                                   std::string & rawTx)
{
    XParticlTransaction txUnsigned = createTransaction(*this,
                                                            inputs, outputs,
                                                            COIN, txVersion,
                                                            0, txWithTimeField);

    CScript inner(innerScript.begin(), innerScript.end());

    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, SIGHASH_ALL, inputs[0].amount.Get64() * COIN);
    if (!m_cp.sign(mprivKey, hash, signature)) {
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    signature.push_back((unsigned char)SIGHASH_ALL);

    XParticlTransaction tx(createTransaction(txWithTimeField));
    tx.nVersion  = txUnsigned.nVersion;
    tx.nTime     = txUnsigned.nTime;

    CTxIn in(txUnsigned.vin[0].prevout);
    in.scriptWitness.stack.push_back(xpubKey);
    in.scriptWitness.stack.push_back(signature);
    in.scriptWitness.stack.push_back(mpubKey);
    in.scriptWitness.stack.push_back(std::vector<unsigned char>(1, OP_FALSE));
    in.scriptWitness.stack.push_back(ToByteVector(inner));

    tx.vin.push_back(in);
    tx.vout      = txUnsigned.vout;

    rawTx = tx.toString();

    std::string json;
    std::string paytxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, paytxid, json)) {
        LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
