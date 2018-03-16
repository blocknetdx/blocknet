//******************************************************************************
//******************************************************************************

#ifndef BITCOINTRANSACTION_H
#define BITCOINTRANSACTION_H

#include "main.h"

#include <string>
#include <cstring>
#include <cstdio>
#include <memory>

namespace xbridge
{

//******************************************************************************
//******************************************************************************
class CTransaction;
typedef std::shared_ptr<CTransaction> CTransactionPtr;

//******************************************************************************
//******************************************************************************
class CTransaction
{
public:
    static const int CURRENT_VERSION=1;

    int nVersion;
    unsigned int nTime;
    /**
     * @brief vin - vector of inputs
     */
    std::vector<CTxIn> vin;
    /**
     * @brief vout - vector of outputs
     */
    std::vector<CTxOut> vout;
    unsigned int nLockTime;

    /**
     * @brief nDoS - count of Denial-of-service detection:
     */
    mutable int nDoS;
    /**
     * @brief DoS- change Denial-of-service detection counter
     * @param nDoSIn -
     * @param fIn
     * @return
     */
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }

    /**
     * @brief CTransaction - defult constructor,
     * set all fields in default values
     */
    CTransaction()
    {
        SetNull();
    }

    /**
     * @brief clone
     * @return copy of current transaction
     */
    virtual CTransactionPtr clone()
    {
        return CTransactionPtr(new CTransaction(*this));
    }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        // READWRITE(nTime);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    }

    /**
     * @brief SetNull - reset all fields to default values
     */
    void SetNull()
    {
        nVersion = CTransaction::CURRENT_VERSION;
        nTime = static_cast<unsigned int>(time(0));// GetAdjustedTime();
        vin.clear();
        vout.clear();
        nLockTime = 0;
        nDoS = 0;  // Denial-of-service prevention
    }

    /**
     * @brief IsNull - check count of inputs and outputs
     * @return true, if inputs and outputs empty
     */
    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    /**
     * @brief GetHash Compute the 256-bit hash of an object's serialization.
     * @return computed hash
     */
    virtual uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

//    bool IsFinal(int nBlockHeight=0, int64 nBlockTime=0) const
//    {
//        // Time based nLockTime implemented in 0.1.6
//        if (nLockTime == 0)
//            return true;
//        if (nBlockHeight == 0)
//            nBlockHeight = nBestHeight;
//        if (nBlockTime == 0)
//            nBlockTime = GetAdjustedTime();
//        if ((int64)nLockTime < ((int64)nLockTime < LOCKTIME_THRESHOLD ? (int64)nBlockHeight : nBlockTime))
//            return true;
//        for (const CTxIn & txin : vin)
//            if (!txin.IsFinal())
//                return false;
//        return true;
//    }

    /**
     * @brief IsNewerThan - equal "age" between old and this transaction
     * @param old - other transaction
     * @return true, if this newer than old
     */
    bool IsNewerThan(const CTransaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (unsigned int i = 0; i < vin.size(); i++)
            if (vin[i].prevout != old.vin[i].prevout)
                return false;

        bool fNewer = false;
        unsigned int nLowest = std::numeric_limits<unsigned int>::max();
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            if (vin[i].nSequence != old.vin[i].nSequence)
            {
                if (vin[i].nSequence <= nLowest)
                {
                    fNewer = false;
                    nLowest = vin[i].nSequence;
                }
                if (old.vin[i].nSequence < nLowest)
                {
                    fNewer = true;
                    nLowest = old.vin[i].nSequence;
                }
            }
        }
        return fNewer;
    }


    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && vout.size() >= 1);
    }

    /**
      * @brief IsCoinStake the coin stake transaction is marked with the first output empty
      * @return true
      */
    bool IsCoinStake() const
    {
        // ppcoin: the coin stake transaction is marked with the first output empty
        return (vin.size() > 0 && (!vin[0].prevout.IsNull()) && vout.size() >= 2 && vout[0].IsEmpty());
    }

    bool IsCoinBaseOrStake() const
    {
        return (IsCoinBase() || IsCoinStake());
    }



    /**
     * @brief IsStandard Check for standard transaction types
     * @return True if all outputs (scriptPubKeys) use only standard transaction forms
     */
    bool IsStandard() const;

    /** Check for standard transaction types
        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return True if all inputs (scriptSigs) use only standard transaction forms
        @see CTransaction::FetchInputs
    */
    // bool AreInputsStandard(const MapPrevTx& mapInputs) const;

    /**
     * @brief GetLegacySigOpCount  Count ECDSA signature operations the old-fashioned (pre-0.6) way
        @return number of sigops this transaction's outputs will produce when spent
        @see CTransaction::FetchInputs
     */
    unsigned int GetLegacySigOpCount() const;

    /** Count ECDSA signature operations in pay-to-script-hash inputs.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return maximum number of sigops required to validate this transaction's inputs
        @see CTransaction::FetchInputs
     */
    // unsigned int GetP2SHSigOpCount(const MapPrevTx& mapInputs) const;

    /** Amount of bitcoins spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
//    int64 GetValueOut() const
//    {
//        int64 nValueOut = 0;
//        for (const CTxOut & txout : vout)
//        {
//            nValueOut += txout.nValue;
//            if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
//                throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
//        }
//        return nValueOut;
//    }

    /** Amount of bitcoins coming in to this transaction
        Note that lightweight clients may not know anything besides the hash of previous transactions,
        so may not be able to calculate this.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return	Sum of value of all inputs (scriptSigs)
        @see CTransaction::FetchInputs
     */
//    int64 GetValueIn(const MapPrevTx& mapInputs) const;

//    static bool AllowFree(double dPriority)
//    {
//        // Large (in bytes) low-priority (new, small-coin) transactions
//        // need a fee.
//        return dPriority > COIN * 960 / 250;
//    }

//    int64 GetMinFee(unsigned int nBlockSize=1, bool fAllowFree=false, enum GetMinFee_mode mode=GMF_BLOCK, unsigned int nBytes = 0) const;

//    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
//    {
//        CAutoFile filein = CAutoFile(OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb"), SER_DISK, CLIENT_VERSION);
//        if (!filein)
//            return error("CTransaction::ReadFromDisk() : OpenBlockFile failed");

//        // Read transaction
//        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
//            return error("CTransaction::ReadFromDisk() : fseek failed");

//        try {
//            filein >> *this;
//        }
//        catch (std::exception &e) {
//            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
//        }

//        // Return file pointer
//        if (pfileRet)
//        {
//            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
//                return error("CTransaction::ReadFromDisk() : second fseek failed");
//            *pfileRet = filein.release();
//        }
//        return true;
//    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nVersion  == b.nVersion &&
                // a.nTime     == b.nTime &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

    /**
     * @brief toString
     * @return
     */
    virtual std::string toString() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return HexStr(ss.begin(), ss.end());
    }

//    std::string ToStringShort() const
//    {
//        std::string str;
//        str += strprintf("%s %s", GetHash().ToString().c_str(), IsCoinBase()? "base" : (IsCoinStake()? "stake" : "user"));
//        return str;
//    }

//    std::string ToString() const
//    {
//        std::string str;
//        str += IsCoinBase()? "Coinbase" : (IsCoinStake()? "Coinstake" : "CTransaction");
//        str += strprintf("(hash=%s, nTime=%d, ver=%d, vin.size=%" PRIszu ", vout.size=%" PRIszu ", nLockTime=%d)\n",
//            GetHash().ToString().substr(0,10).c_str(),
//            nTime,
//            nVersion,
//            vin.size(),
//            vout.size(),
//            nLockTime
//            );

//        for (unsigned int i = 0; i < vin.size(); i++)
//            str += "    " + vin[i].ToString() + "\n";
//        for (unsigned int i = 0; i < vout.size(); i++)
//            str += "    " + vout[i].ToString() + "\n";
//        return str;
//    }

    void print() const
    {
        // printf("%s", ToString().c_str());
    }


    // bool ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
    // bool ReadFromDisk(CTxDB& txdb, COutPoint prevout);
    // bool ReadFromDisk(COutPoint prevout);
    // bool DisconnectInputs(CTxDB& txdb);

    /** Fetch from memory and/or disk. inputsRet keys are transaction hashes.

     @param[in] txdb	Transaction database
     @param[in] mapTestPool	List of pending changes to the transaction index database
     @param[in] fBlock	True if being called to add a new best-block to the chain
     @param[in] fMiner	True if being called by CreateNewBlock
     @param[out] inputsRet	Pointers to this transaction's inputs
     @param[out] fInvalid	returns true if transaction is invalid
     @return	Returns true if all inputs are in txdb or mapTestPool
     */
//    bool FetchInputs(CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool,
//                     bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid);

    /** Sanity check previous transactions, then, if all checks succeed,
        mark them as spent by this transaction.

        @param[in] inputs	Previous transactions (from FetchInputs)
        @param[out] mapTestPool	Keeps track of inputs that need to be updated on disk
        @param[in] posThisTx	Position of this transaction on disk
        @param[in] pindexBlock
        @param[in] fBlock	true if called from ConnectBlock
        @param[in] fMiner	true if called from CreateNewBlock
        @param[in] fStrictPayToScriptHash	true if fully validating p2sh transactions
        @return Returns true if all checks succeed
     */
//    bool ConnectInputs(CTxDB& txdb, MapPrevTx inputs,
//                       std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
//                       const CBlockIndex* pindexBlock, bool fBlock, bool fMiner, bool fStrictPayToScriptHash=true);
//    bool ClientConnectInputs();
//    bool CheckTransaction() const;
//    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true, bool* pfMissingInputs=NULL);
//    bool GetCoinAge(CTxDB& txdb, uint64& nCoinAge) const;  // ppcoin: get transaction coin age

protected:
//    const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const;
};

//******************************************************************************
//******************************************************************************
typedef CTransaction CBTCTransaction;

//******************************************************************************
//******************************************************************************

class CXCTransaction : public CTransaction
{
public:
    CXCTransaction() : CTransaction()
    {

    }

    virtual CTransactionPtr clone()
    {
        return CTransactionPtr(new CTransaction(*this));
    }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nTime);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    }

    friend bool operator==(const CXCTransaction& a, const CXCTransaction& b)
    {
        return (a.nVersion  == b.nVersion &&
                a.nTime     == b.nTime &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    virtual uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    virtual std::string toString() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return HexStr(ss.begin(), ss.end());
    }
};


//******************************************************************************
//******************************************************************************
/**
 * @brief The CTransactionSignatureSerializer class -class for serialization transaction signature
 */
class CTransactionSignatureSerializer {
private:
    /**
     * @brief txTo reference to the spending transaction (the one being serialized)
     */
    const CTransaction& txTo;
    /**
     * @brief scriptCode  output script being consumed
     */
    const CScript& scriptCode;
    /**
     * @brief nIn input index of txTo being signed
     */
    const unsigned int nIn;
    /**
     * @brief fAnyoneCanPay whether the hashtype has the SIGHASH_ANYONECANPAY flag set
     */
    const bool fAnyoneCanPay;
    /**
     * @brief fHashSingle whether the hashtype is SIGHASH_SINGLE
     */
    const bool fHashSingle;
    /**
     * @brief fHashNone whether the hashtype is SIGHASH_NONE
     */
    const bool fHashNone;

public:
    CTransactionSignatureSerializer(const CTransaction &txToIn, const CScript &scriptCodeIn, unsigned int nInIn, int nHashTypeIn) :
        txTo(txToIn), scriptCode(scriptCodeIn), nIn(nInIn),
        fAnyoneCanPay(!!(nHashTypeIn & SIGHASH_ANYONECANPAY)),
        fHashSingle((nHashTypeIn & 0x1f) == SIGHASH_SINGLE),
        fHashNone((nHashTypeIn & 0x1f) == SIGHASH_NONE) {}


    /**
     * @brief SerializeScriptCode  Serialize the passed scriptCode, skipping OP_CODESEPARATORs
     * @param s
     */
    template<typename S>
    void SerializeScriptCode(S &s, int /*nType*/, int /*nVersion*/) const {
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


    /**
     * @brief SerializeInput  Serialize an input of txTo
     * @param nInput
     * @param nType
     * @param nVersion
     */
    template<typename S>
    void SerializeInput(S &s, unsigned int nInput, int nType, int nVersion) const {
        // In case of SIGHASH_ANYONECANPAY, only the input being signed is serialized
        if (fAnyoneCanPay)
            nInput = nIn;
        // Serialize the prevout
        ::Serialize(s, txTo.vin[nInput].prevout, nType, nVersion);
        // Serialize the script
        if (nInput != nIn)
            // Blank out other inputs' signatures
            // ::Serialize(s, CScriptBase(), nType, nVersion);
            assert(!"CScriptBase not defined");
        else
            SerializeScriptCode(s, nType, nVersion);
        // Serialize the nSequence
        if (nInput != nIn && (fHashSingle || fHashNone))
            // let the others update at will
            ::Serialize(s, (int)0, nType, nVersion);
        else
            ::Serialize(s, txTo.vin[nInput].nSequence, nType, nVersion);
    }


    /**
     * @brief SerializeOutput  Serialize an output of txTo
     * @param nOutput
     * @param nType
     * @param nVersion
     */
    template<typename S>
    void SerializeOutput(S &s, unsigned int nOutput, int nType, int nVersion) const {
        if (fHashSingle && nOutput != nIn)
            // Do not lock-in the txout payee at other indices as txin
            ::Serialize(s, CTxOut(), nType, nVersion);
        else
            ::Serialize(s, txTo.vout[nOutput], nType, nVersion);
    }

    /**
     * @brief Serialize Serialize txTo
     * @param nType
     * @param nVersion
     */
    template<typename S>
    void Serialize(S &s, int nType, int nVersion) const {
        // Serialize nVersion
        ::Serialize(s, txTo.nVersion, nType, nVersion);
        // Serialize vin
        unsigned int nInputs = fAnyoneCanPay ? 1 : txTo.vin.size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++)
             SerializeInput(s, nInput, nType, nVersion);
        // Serialize vout
        unsigned int nOutputs = fHashNone ? 0 : (fHashSingle ? nIn+1 : txTo.vout.size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++)
             SerializeOutput(s, nOutput, nType, nVersion);
        // Serialize nLockTime
        ::Serialize(s, txTo.nLockTime, nType, nVersion);
    }
};

/**
 * @brief SignatureHash2  compute hash of transaction signature
 * @param scriptCode
 * @param txTo
 * @param nIn
 * @param nHashType
 * @return hash of transaction signature
 */
uint256 SignatureHash2(const CScript& scriptCode, const CTransactionPtr & txTo, unsigned int nIn, int nHashType/*, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache*/);

} // namespace xbridge

#endif // BITCOINTRANSACTION_H
