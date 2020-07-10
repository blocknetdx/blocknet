// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/xbridgewalletconnectorbtg.h>

#include <xbridge/util/logger.h>
#include <xbridge/xbitcointransaction.h>

#include <primitives/transaction.h>


//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

namespace rpc
{

//*****************************************************************************
//*****************************************************************************
bool getinfo(const std::string & rpcuser, const std::string & rpcpasswd,
             const std::string & rpcip, const std::string & rpcport,
             WalletInfo & info);

bool getnetworkinfo(const std::string & rpcuser, const std::string & rpcpasswd,
                    const std::string & rpcip, const std::string & rpcport,
                    WalletInfo & info);

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
}

//******************************************************************************
//******************************************************************************
namespace
{

// https://github.com/BTCGPU/BTCGPU/blob/4dbe037384ce3f73f7b6edda99d6ef4119001695/src/script/interpreter.h#L21
/** Signature hash types/flags */
enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_FORKID = 0x40,
    SIGHASH_ANYONECANPAY = 0x80,
};

/** Fork IDs **/
enum
{
    FORKID_BCC = 0,
    FORKID_BTG = 79, // Atomic number AU
};

static const int FORKID_IN_USE = FORKID_BTG;

//******************************************************************************
//******************************************************************************
// https://github.com/BTCGPU/BTCGPU/blob/4dbe037384ce3f73f7b6edda99d6ef4119001695/src/script/interpreter.cpp#L1217
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

bool static UsesForkId(uint32_t nHashType) {
    return nHashType & SIGHASH_FORKID;
}

//******************************************************************************
//******************************************************************************
typedef struct { // helper for cache pointer
    uint256 hashPrevouts;
    uint256 hashSequence;
    uint256 hashOutputs;
    bool ready;
} cache_t;
// Reference: https://github.com/BTCGPU/BTCGPU/blob/4dbe037384ce3f73f7b6edda99d6ef4119001695/src/script/interpreter.cpp#L1270
uint256 SignatureHash(const CScript & scriptCode, const CTransactionPtr & tx,
                      unsigned int nIn, int nHashType,
                      const CAmount amount)
{
    // XBRIDGE
    auto & txTo = *tx;
    bool no_forkid{false};
    auto sigversion = SigVersion::BASE;
    auto forkid = FORKID_IN_USE;
    cache_t *cache = nullptr;
    // END XBRIDGE

    bool use_forkid = false;
    int nForkHashType = nHashType;
    if (!no_forkid) {
        use_forkid = UsesForkId(nHashType);
        if (use_forkid) {
            nForkHashType |= forkid << 8;
        }
    }

    // force new tx with FORKID to use bip143 transaction digest algorithm
    // see https://github.com/bitcoin/bips/blob/master/bip-0143.mediawiki
    if (sigversion == SigVersion::WITNESS_V0 || use_forkid) {
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
        ss << nForkHashType;

        return ss.GetHash();
    }

    // XBRIDGE should never end up here
    return {};
    // legacy code disabled below
    // XBRIDGE

    /*static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

    // Check for invalid use of SIGHASH_SINGLE
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo.vout.size()) {
            //  nOut out of range
            return one;
        }
    }

    // Wrapper to serialize only the necessary parts of the transaction being signed
    CTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn, nHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nForkHashType;
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
BTGWalletConnector::BTGWalletConnector() : BtcWalletConnector() {}

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
bool BTGWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
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

        int nHashType = SIGHASH_ALL | SIGHASH_FORKID;
        std::vector<unsigned char> signature;
        uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount*COIN);
        if (!m_cp.sign(mprivKey, hash, signature))
        {
            LOG() << "btg sign transaction error " << __FUNCTION__;
            return false;
        }

        signature.push_back(uint8_t(nHashType));

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "btg transaction not created " << __FUNCTION__;
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
        LOG() << "btg decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
bool BTGWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
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

    int nHashType = SIGHASH_ALL | SIGHASH_FORKID;
    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount*COIN);
    if (!m_cp.sign(mprivKey, hash, signature))
    {
        LOG() << "btg sign transaction error " << __FUNCTION__;
        return false;
    }

    signature.push_back(uint8_t(nHashType));

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << ToByteVector(inner);

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "btg transaction not created " << __FUNCTION__;
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
        LOG() << "btg decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
