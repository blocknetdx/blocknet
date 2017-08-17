#include "xbitcointransaction.h"

namespace xbridge
{

uint256 SignatureHash2(const CScript& scriptCode, const CTransactionPtr & txTo, unsigned int nIn, int nHashType/*, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache*/)
{
//    if (sigversion == SIGVERSION_WITNESS_V0) {
//        uint256 hashPrevouts;
//        uint256 hashSequence;
//        uint256 hashOutputs;

//        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
//            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(txTo);
//        }

//        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
//            hashSequence = cache ? cache->hashSequence : GetSequenceHash(txTo);
//        }


//        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
//            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(txTo);
//        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.vout.size()) {
//            CHashWriter ss(SER_GETHASH, 0);
//            ss << txTo.vout[nIn];
//            hashOutputs = ss.GetHash();
//        }

//        CHashWriter ss(SER_GETHASH, 0);
//        // Version
//        ss << txTo.nVersion;
//        // Input prevouts/nSequence (none/all, depending on flags)
//        ss << hashPrevouts;
//        ss << hashSequence;
//        // The input being signed (replacing the scriptSig with scriptCode + amount)
//        // The prevout may already be contained in hashPrevout, and the nSequence
//        // may already be contain in hashSequence.
//        ss << txTo.vin[nIn].prevout;
//        ss << static_cast<const CScriptBase&>(scriptCode);
//        ss << amount;
//        ss << txTo.vin[nIn].nSequence;
//        // Outputs (none/one/all, depending on flags)
//        ss << hashOutputs;
//        // Locktime
//        ss << txTo.nLockTime;
//        // Sighash type
//        ss << nHashType;

//        return ss.GetHash();
//    }

    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nIn >= txTo->vin.size()) {
        //  nIn out of range
        return one;
    }

    // Check for invalid use of SIGHASH_SINGLE
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo->vout.size()) {
            //  nOut out of range
            return one;
        }
    }

    // Wrapper to serialize only the necessary parts of the transaction being signed
    CTransactionSignatureSerializer txTmp(*txTo, scriptCode, nIn, nHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

}
