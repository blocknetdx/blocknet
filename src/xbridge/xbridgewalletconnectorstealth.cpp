// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/xbridgewalletconnectorstealth.h>

#include <xbridge/util/logger.h>
#include <xbridge/xbitcointransaction.h>

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

namespace
{

// https://github.com/StealthSend/Stealth/blob/0deb8c429b449fd952ebfeff8b4a4ec4a7db64f1/src/script.h#L32
/** Signature hash types/flags */
enum {
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,
};

// Reference: https://github.com/StealthSend/Stealth/blob/746829d9ee85523b7ecd0ee8676f05cb6bfce596/src/script.cpp#L1283
uint256 SignatureHash(CScript &scriptCode, const CTransactionPtr & tx,
                      unsigned int nIn, int nHashType,
                      const CAmount amount)
{
    // XBRIDGE
    auto & txTo = *tx;
    // END XBRIDGE

    CTransaction txTmp(txTo);

    // In case concatenating two scripts ends up with two codeseparators,
    // or an extra one at the end, this prevents all those possible incompatibilities.
    FindAndDelete(scriptCode, CScript(OP_CODESEPARATOR));

    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        txTmp.vin[i].scriptSig = CScript();
    txTmp.vin[nIn].scriptSig = scriptCode;

    // Blank out some of the outputs
    if ((nHashType & 0x1f) == SIGHASH_NONE)
    {
        // Wildcard payee
        txTmp.vout.clear();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }
    else if ((nHashType & 0x1f) == SIGHASH_SINGLE)
    {
        // Only lock-in the txout payee at same index as txin
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size())
        {
            printf("ERROR: SignatureHash() : nOut=%d out of range\n", nOut);
            return uint256{};
        }
        txTmp.vout.resize(nOut+1);
        for (unsigned int i = 0; i < nOut; i++)
            txTmp.vout[i].SetNull();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }

    // Blank out other inputs completely, not recommended for open transactions
    if (nHashType & SIGHASH_ANYONECANPAY)
    {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    // Serialize and hash
    CDataStream ss(SER_GETHASH, 0);
    ss.reserve(10000);
    ss << txTmp << nHashType;
    return Hash(ss.begin(), ss.end());
}

} // namespace


xbridge::CTransactionPtr createTransaction(const bool txWithTimeField);
xbridge::CTransactionPtr createTransaction(const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> > & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField);

xbridge::CTransactionPtr createTransaction(const bool txWithTimeField = false);
xbridge::CTransactionPtr createTransaction(const WalletConnector & conn,
                                           const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField = false);


StealthWalletConnector::StealthWalletConnector() : BtcWalletConnector() { }

bool StealthWalletConnector::createRefundTransaction(const std::vector<XTxIn> & inputs,
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

        int nHashType = SIGHASH_ALL;
        std::vector<unsigned char> signature;
        uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount * COIN);
        if (!m_cp.sign(mprivKey, hash, signature))
        {
            LOG() << "stealth sign transaction error " << __FUNCTION__;
            return false;
        }

        signature.push_back(uint8_t(nHashType));

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "stealth transaction not created " << __FUNCTION__;
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
        LOG() << "stealth decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = reftxid;

    return true;
}

bool StealthWalletConnector::createPaymentTransaction(const std::vector<XTxIn> & inputs,
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

    int nHashType = SIGHASH_ALL;
    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, nHashType, inputs[0].amount*COIN);
    if (!m_cp.sign(mprivKey, hash, signature))
    {
        LOG() << "stealth sign transaction error " << __FUNCTION__;
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
        ERR() << "stealth transaction not created " << __FUNCTION__;
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
        LOG() << "stealth decode signed transaction error " << __FUNCTION__;
        return true;
    }

    txId  = paytxid;

    return true;
}

} // namespace xbridge
