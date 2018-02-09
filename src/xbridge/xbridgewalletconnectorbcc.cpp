//******************************************************************************
//******************************************************************************

#include "xbridgewalletconnectorbcc.h"

#include "xkey.h"
#include "xbitcoinsecret.h"
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
xbridge::CTransactionPtr createTransaction();
xbridge::CTransactionPtr createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const uint64_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime);

//******************************************************************************
//******************************************************************************
BccWalletConnector::BccWalletConnector()
    : BtcWalletConnector()
{

}

//******************************************************************************
//******************************************************************************
bool BccWalletConnector::createRefundTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                        const std::vector<std::pair<std::string, double> > & outputs,
                                                        const std::vector<unsigned char> & mpubKey,
                                                        const std::vector<unsigned char> & mprivKey,
                                                        const std::vector<unsigned char> & innerScript,
                                                        const uint32_t lockTime,
                                                        std::string & txId,
                                                        std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(inputs, outputs, COIN, txVersion, lockTime);
    txUnsigned->vin[0].nSequence = std::numeric_limits<uint32_t>::max()-1;

    CScript inner(innerScript.begin(), innerScript.end());

    xbridge::CKey m;
    m.Set(mprivKey.begin(), mprivKey.end(), true);
    if (!m.IsValid())
    {
        // cancel transaction
        LOG() << "sign transaction error, restore private key failed, transaction canceled " << __FUNCTION__;
//            sendCancelTransaction(xtx, crNotSigned);
        return false;
    }

    CScript redeem;
    {
        CScript tmp;
        std::vector<unsigned char> raw(mpubKey.begin(), mpubKey.end());
        tmp << raw << OP_TRUE << inner;

        std::vector<unsigned char> signature;
        uint256 hash = xbridge::SignatureHash2(inner, txUnsigned, 0, SIGHASH_ALL);
        if (!m.Sign(hash, signature))
        {
            // cancel transaction
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
            return false;
        }

        signature.push_back((unsigned char)SIGHASH_ALL);

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction());
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
bool BccWalletConnector::createPaymentTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                         const std::vector<std::pair<std::string, double> > & outputs,
                                                         const std::vector<unsigned char> & mpubKey,
                                                         const std::vector<unsigned char> & mprivKey,
                                                         const std::vector<unsigned char> & xpubKey,
                                                         const std::vector<unsigned char> & innerScript,
                                                         std::string & txId,
                                                         std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(inputs, outputs, COIN, txVersion, 0);

    CScript inner(innerScript.begin(), innerScript.end());

    xbridge::CKey m;
    m.Set(mprivKey.begin(), mprivKey.end(), true);
    if (!m.IsValid())
    {
        // cancel transaction
        LOG() << "sign transaction error (SetSecret failed), transaction canceled " << __FUNCTION__;
//            sendCancelTransaction(xtx, crNotSigned);
        return false;
    }

    std::vector<unsigned char> signature;
    uint256 hash = xbridge::SignatureHash2(inner, txUnsigned, 0, SIGHASH_ALL);
    if (!m.Sign(hash, signature))
    {
        // cancel transaction
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
        return false;
    }

    signature.push_back((unsigned char)SIGHASH_ALL);

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << inner;

    xbridge::CTransactionPtr tx(createTransaction());
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
