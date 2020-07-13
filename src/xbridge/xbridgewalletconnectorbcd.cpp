// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/xbridgewalletconnectorbcd.h>

#include <xbridge/util/logger.h>
#include <xbridge/xbitcointransaction.h>

#include <base58.h>
#include <primitives/transaction.h>
#include <time.h>


//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

namespace rpc
{

//*****************************************************************************
//*****************************************************************************
bool decodeRawTransaction(const std::string & rpcuser, const std::string & rpcpasswd,
                          const std::string & rpcip, const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid, std::string & tx);
}

class BCDTransaction {
public:
    static const int32_t CURRENT_VERSION_FORK=12;
    int nVersion{0};
    unsigned int nTime{0};
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime{0};
    uint256 preBlockHash;
    bool serializeWithTimeField{false};

    BCDTransaction() { SetNull(); }

    void SetNull() {
        nVersion = CTransaction::CURRENT_VERSION;
        nTime = static_cast<unsigned int>(time(nullptr));
        nLockTime = 0;
        vin.clear();
        vout.clear();
        preBlockHash.SetNull();
    }

    bool IsNull() const {
        return (vin.empty() && vout.empty());
    }

    uint256 GetHash() const {
        return SerializeHash(*this);
    }

    friend bool operator==(const BCDTransaction & a, const BCDTransaction & b) {
        return (a.nVersion  == b.nVersion &&
                a.preBlockHash == b.preBlockHash &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }
    friend bool operator!=(const BCDTransaction & a, const BCDTransaction & b) {
        return !(a == b);
    }

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }

    std::string toString(int version=70015) const {
        CDataStream ss(SER_NETWORK, version);
        ss << *this;
        return HexStr(ss.begin(), ss.end());
    }

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        BCDSerializeTransaction(*this, s);
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        BCDUnserializeTransaction(*this, s);
    }
};

template<typename Stream, typename TxType>
inline void BCDSerializeTransaction(const TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s << tx.nVersion;
    if (tx.nVersion == BCDTransaction::CURRENT_VERSION_FORK){
        s << tx.preBlockHash;
    }
    unsigned char flags = 0;
    // Consistency check
    if (fAllowWitness) {
        /* Check whether witnesses need to be serialized. */
        if (tx.HasWitness()) {
            flags |= 1;
        }
    }
    if (flags) {
        /* Use extended format in case witnesses are to be serialized. */
        std::vector<CTxIn> vinDummy;
        s << vinDummy;
        s << flags;
    }
    s << tx.vin;
    s << tx.vout;
    if (flags & 1) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s << tx.vin[i].scriptWitness.stack;
        }
    }
    s << tx.nLockTime;
}

template<typename Stream, typename TxType>
inline void BCDUnserializeTransaction(TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s >> tx.nVersion;
    if (tx.nVersion == BCDTransaction::CURRENT_VERSION_FORK){
        s >> tx.preBlockHash;
    }
    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
    s >> tx.vin;
    if (tx.vin.size() == 0 && fAllowWitness) {
        /* We read a dummy or an empty vin. */
        s >> flags;
        if (flags != 0) {
            s >> tx.vin;
            s >> tx.vout;
        }
    } else {
        /* We read a non-empty vin. Assume a normal vout follows. */
        s >> tx.vout;
    }
    if ((flags & 1) && fAllowWitness) {
        /* The witness flag is present, and we support witnesses. */
        flags ^= 1;
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> tx.vin[i].scriptWitness.stack;
        }
    }
    if (flags) {
        /* Unknown flag in the serialization */
        throw std::ios_base::failure("Unknown transaction optional data");
    }
    s >> tx.nLockTime;
}

/**
 * Wrapper that serializes like CTransaction, but with the modifications
 *  required for the signature hash done in-place
 */
template <class T>
class BCDTransactionSignatureSerializer
{
private:
    const T& txTo;             //!< reference to the spending transaction (the one being serialized)
    const CScript& scriptCode; //!< output script being consumed
    const unsigned int nIn;    //!< input index of txTo being signed
    const bool fAnyoneCanPay;  //!< whether the hashtype has the SIGHASH_ANYONECANPAY flag set
    const bool fHashSingle;    //!< whether the hashtype is SIGHASH_SINGLE
    const bool fHashNone;      //!< whether the hashtype is SIGHASH_NONE

public:
    BCDTransactionSignatureSerializer(const T& txToIn, const CScript& scriptCodeIn, unsigned int nInIn, int nHashTypeIn) :
            txTo(txToIn), scriptCode(scriptCodeIn), nIn(nInIn),
            fAnyoneCanPay(!!(nHashTypeIn & SIGHASH_ANYONECANPAY)),
            fHashSingle((nHashTypeIn & 0x1f) == SIGHASH_SINGLE),
            fHashNone((nHashTypeIn & 0x1f) == SIGHASH_NONE) {}

    /** Serialize the passed scriptCode, skipping OP_CODESEPARATORs */
    template<typename S>
    void SerializeScriptCode(S &s) const {
        CScript::const_iterator it = scriptCode.begin();
        CScript::const_iterator itBegin = it;
        opcodetype opcode;
        unsigned int nCodeSeparators = 0;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR)
                nCodeSeparators++;
        }
        ::WriteCompactSize(s, scriptCode.size() - nCodeSeparators);
        it = itBegin;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                s.write((char*)&itBegin[0], it-itBegin-1);
                itBegin = it;
            }
        }
        if (itBegin != scriptCode.end())
            s.write((char*)&itBegin[0], it-itBegin);
    }

    /** Serialize an input of txTo */
    template<typename S>
    void SerializeInput(S &s, unsigned int nInput) const {
        // In case of SIGHASH_ANYONECANPAY, only the input being signed is serialized
        if (fAnyoneCanPay)
            nInput = nIn;
        // Serialize the prevout
        ::Serialize(s, txTo.vin[nInput].prevout);
        // Serialize the script
        if (nInput != nIn)
            // Blank out other inputs' signatures
            ::Serialize(s, CScript());
        else
            SerializeScriptCode(s);
        // Serialize the nSequence
        if (nInput != nIn && (fHashSingle || fHashNone))
            // let the others update at will
            ::Serialize(s, (int)0);
        else
            ::Serialize(s, txTo.vin[nInput].nSequence);
    }

    /** Serialize an output of txTo */
    template<typename S>
    void SerializeOutput(S &s, unsigned int nOutput) const {
        if (fHashSingle && nOutput != nIn)
            // Do not lock-in the txout payee at other indices as txin
            ::Serialize(s, CTxOut());
        else
            ::Serialize(s, txTo.vout[nOutput]);
    }

    /** Serialize txTo */
    template<typename S>
    void Serialize(S &s) const {
        // Serialize nVersion
        ::Serialize(s, txTo.nVersion);
        if (txTo.nVersion == BCDTransaction::CURRENT_VERSION_FORK){
            ::Serialize(s, txTo.preBlockHash);
        }
        // Serialize vin
        unsigned int nInputs = fAnyoneCanPay ? 1 : txTo.vin.size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++)
            SerializeInput(s, nInput);
        // Serialize vout
        unsigned int nOutputs = fHashNone ? 0 : (fHashSingle ? nIn+1 : txTo.vout.size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++)
            SerializeOutput(s, nOutput);
        // Serialize nLockTime
        ::Serialize(s, txTo.nLockTime);
    }
};

namespace
{

// https://github.com/eveybcd/BitcoinDiamond/blob/5688f3b27699447f32031836f1d74826b97ff0b6/src/script/interpreter.cpp#L1189
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

// https://github.com/eveybcd/BitcoinDiamond/blob/21191d445bbc92ff23a94fbe41b0c0f86bc6a4b0/src/script/interpreter.h#L131
enum class SigVersion
{
    BASE = 0,
    WITNESS_V0 = 1,
};

//******************************************************************************
//******************************************************************************
typedef struct { // helper for bcd cache pointer
    uint256 hashPrevouts;
    uint256 hashSequence;
    uint256 hashOutputs;
    bool ready{false};
} cache_t;
// Reference: https://github.com/eveybcd/BitcoinDiamond/blob/5688f3b27699447f32031836f1d74826b97ff0b6/src/script/interpreter.cpp#L1237
template <class T>
uint256 SignatureHash(const CScript &scriptCode, const T & txTo,
                      unsigned int nIn, int nHashType, const CAmount amount,
                      SigVersion sigversion)
{
    // XBRIDGE
    cache_t *cache = nullptr;
    // END XBRIDGE

    if (sigversion == SigVersion::WITNESS_V0) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;
        const bool cacheready = cache && cache->ready;

        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
            hashPrevouts = cacheready ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashSequence = cacheready ? cache->hashSequence : GetSequenceHash(txTo);
        }


        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashOutputs = cacheready ? cache->hashOutputs : GetOutputsHash(txTo);
        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.vout.size()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo.vout[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo.nVersion;
        if (txTo.nVersion == BCDTransaction::CURRENT_VERSION_FORK){
            ss << txTo.preBlockHash;
        }
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with scriptCode + amount)
        // The prevout may already be contained in hashPrevout, and the nSequence
        // may already be contain in hashSequence.
        ss << txTo.vin[nIn].prevout;
        ss << scriptCode;
        ss << amount;
        ss << txTo.vin[nIn].nSequence;
        // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;
        // Locktime
        ss << txTo.nLockTime;
        // Sighash type
        ss << nHashType;

        return ss.GetHash();
    }

    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

    // Check for invalid use of SIGHASH_SINGLE
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo.vout.size()) {
            //  nOut out of range
            return one;
        }
    }

    // Wrapper to serialize only the necessary parts of the transaction being signed
    BCDTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn, nHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

} // namespace

BCDTransaction createTransaction() {
    return BCDTransaction{};
}

BCDTransaction createTransaction(const WalletConnector & conn,
                                           const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const uint256 preBlockHash)
{
    BCDTransaction bcdTx;
    BCDTransaction *tx = &bcdTx;
    tx->nVersion  = txversion;
    tx->nLockTime = lockTime;
    tx->preBlockHash = preBlockHash;

    for (const XTxIn & in : inputs)
    {
        tx->vin.emplace_back(COutPoint(uint256S(in.txid), in.n));
    }

    for (const std::pair<std::string, double> & out : outputs)
    {
        std::vector<unsigned char> id = conn.toXAddr(out.first);

        CScript scr;
        scr << OP_DUP << OP_HASH160 << ToByteVector(id) << OP_EQUALVERIFY << OP_CHECKSIG;

        tx->vout.emplace_back(out.second * COIN, scr);
    }

    return bcdTx;
}

//******************************************************************************
//******************************************************************************
BCDWalletConnector::BCDWalletConnector() : BtcWalletConnector() {}


//******************************************************************************
//******************************************************************************
bool BCDWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
                                                 const std::vector<std::pair<std::string, double> > & outputs,
                                                 const std::vector<unsigned char> & mpubKey,
                                                 const std::vector<unsigned char> & mprivKey,
                                                 const std::vector<unsigned char> & innerScript,
                                                 const uint32_t lockTime,
                                                 std::string & txId,
                                                 std::string & rawTx)
{
    rpc::WalletInfo info;
    if (!getInfo(info)) {
        LOG() << "bcd failed to obtain preBlockHash " << __FUNCTION__;
        return false;
    }
    uint256 preBlockHash = info.bestblockhash;
    auto txUnsigned = createTransaction(*this, inputs, outputs, COIN, txVersion, lockTime, preBlockHash);
    // Correctly set tx input sequence. If lockTime is specified sequence must be 2^32-2, otherwise 2^32-1 (Final)
    uint32_t sequence = lockTime > 0 ? xbridge::SEQUENCE_FINAL-1 : xbridge::SEQUENCE_FINAL;
    txUnsigned.vin[0].nSequence = sequence;

    CScript inner(innerScript.begin(), innerScript.end());

    CScript redeem;
    {
        CScript tmp;
        tmp << ToByteVector(mpubKey) << OP_TRUE << ToByteVector(inner);

        int nHashType = SIGHASH_ALL;
        std::vector<unsigned char> signature;
        uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount*COIN, SigVersion::BASE);
        if (!m_cp.sign(mprivKey, hash, signature))
        {
            LOG() << "bcd sign transaction error " << __FUNCTION__;
            return false;
        }

        signature.push_back(uint8_t(nHashType));

        redeem << signature;
        redeem += tmp;
    }

    auto tx = createTransaction();
    tx.preBlockHash = txUnsigned.preBlockHash;
    tx.nVersion = txUnsigned.nVersion;
    tx.nTime = txUnsigned.nTime;
    tx.vin.emplace_back(txUnsigned.vin[0].prevout, redeem, sequence);
    tx.vout = txUnsigned.vout;
    tx.nLockTime = txUnsigned.nLockTime;

    rawTx = tx.toString();

    std::string json;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, reftxid, json))
    {
        LOG() << "bcd decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
bool BCDWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                  const std::vector<unsigned char> & mpubKey,
                                                  const std::vector<unsigned char> & mprivKey,
                                                  const std::vector<unsigned char> & xpubKey,
                                                  const std::vector<unsigned char> & innerScript,
                                                  std::string & txId,
                                                  std::string & rawTx)
{
    rpc::WalletInfo info;
    if (!getInfo(info)) {
        LOG() << "bcd failed to obtain preBlockHash " << __FUNCTION__;
        return false;
    }
    uint256 preBlockHash = info.bestblockhash;
    auto txUnsigned = createTransaction(*this, inputs, outputs, COIN, txVersion, 0, preBlockHash);

    CScript inner(innerScript.begin(), innerScript.end());

    int nHashType = SIGHASH_ALL;
    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount*COIN, SigVersion::BASE);
    if (!m_cp.sign(mprivKey, hash, signature))
    {
        LOG() << "bcd sign transaction error " << __FUNCTION__;
        return false;
    }

    signature.push_back(uint8_t(nHashType));

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << ToByteVector(inner);

    auto tx = createTransaction();
    tx.nVersion = txUnsigned.nVersion;
    tx.preBlockHash = txUnsigned.preBlockHash;
    tx.nTime = txUnsigned.nTime;
    tx.vin.emplace_back(txUnsigned.vin[0].prevout, redeem);
    tx.vout = txUnsigned.vout;

    rawTx = tx.toString();

    std::string json;
    std::string paytxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, paytxid, json))
    {
        LOG() << "bcd decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
