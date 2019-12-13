// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBITCOINTRANSACTION_H
#define BLOCKNET_XBRIDGE_XBITCOINTRANSACTION_H

#include <hash.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <streams.h>
#include <util/strencodings.h>

#include <cstring>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

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

    int          nVersion;
    unsigned int nTime;
    bool         serializeWithTimeField;

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
     * @brief CTransaction - defult constructor,
     * set all fields in default values
     */
    CTransaction(bool serializeWithTimeField = false)
    {
        SetNull();

        this->serializeWithTimeField = serializeWithTimeField;
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
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nVersion);
        if (serializeWithTimeField)
            READWRITE(nTime);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    }

    /**
     * @brief SetNull - reset all fields to default values
     */
    void SetNull()
    {
        nVersion               = CTransaction::CURRENT_VERSION;
        nTime                  = static_cast<unsigned int>(time(nullptr));
        serializeWithTimeField = false;
        nLockTime              = 0;

        vin.clear();
        vout.clear();
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


    /**
     * @brief SerializeInput  Serialize an input of txTo
     * @param nInput
     * @param nType
     * @param nVersion
     */
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
            // ::Serialize(s, CScriptBase());
            assert(!"CScriptBase not defined");
        else
            SerializeScriptCode(s);
        // Serialize the nSequence
        if (nInput != nIn && (fHashSingle || fHashNone))
            // let the others update at will
            ::Serialize(s, (int)0);
        else
            ::Serialize(s, txTo.vin[nInput].nSequence);
    }


    /**
     * @brief SerializeOutput  Serialize an output of txTo
     * @param nOutput
     * @param nType
     * @param nVersion
     */
    template<typename S>
    void SerializeOutput(S &s, unsigned int nOutput) const {
        if (fHashSingle && nOutput != nIn)
            // Do not lock-in the txout payee at other indices as txin
            ::Serialize(s, CTxOut());
        else
            ::Serialize(s, txTo.vout[nOutput]);
    }

    /**
     * @brief Serialize Serialize txTo
     * @param nType
     * @param nVersion
     */
    template<typename S>
    void Serialize(S &s) const
    {
        // Serialize nVersion
        ::Serialize(s, txTo.nVersion);
        if (txTo.serializeWithTimeField)
        {
            // Serialize nTime
            ::Serialize(s, txTo.nTime);
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

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBITCOINTRANSACTION_H
