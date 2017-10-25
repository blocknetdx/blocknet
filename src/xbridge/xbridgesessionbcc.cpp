//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesessionbcc.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xuiconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"
// #include "dht/dht.h"
#include "bitcoinrpcconnector.h"
#include "xbitcointransaction.h"
#include "base58.h"

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBcc::XBridgeSessionBcc()
    : XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBcc::XBridgeSessionBcc(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBcc::~XBridgeSessionBcc()
{

}

//*****************************************************************************
//*****************************************************************************
//std::string XBridgeSessionBtc::fromXAddr(const std::vector<unsigned char> & xaddr) const
//{
//    return EncodeBase58Check(xaddr);
//}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> XBridgeSessionBcc::toXAddr(const std::string & addr) const
{
    std::vector<unsigned char> vaddr;
    if (DecodeBase58Check(addr.c_str(), vaddr))
    {
        vaddr.erase(vaddr.begin());
    }
    return vaddr;
}

//******************************************************************************
//******************************************************************************
uint32_t XBridgeSessionBcc::lockTime(const char role) const
{
    rpc::Info info;
    if (!rpc::getInfo(m_wallet.m_user, m_wallet.m_passwd,
                     m_wallet.m_ip, m_wallet.m_port, info))
    {
        LOG() << "blockchain info not received " << __FUNCTION__;
        return 0;
    }

    if (info.blocks == 0)
    {
        LOG() << "block count not defined in blockchain info " << __FUNCTION__;
        return 0;
    }

    // lock time
    uint32_t lt = 0;
    if (role == 'A')
    {
        // 72h in seconds
        // lt = info.blocks + 259200 / m_wallet.blockTime;

        // 2h in seconds
        lt = info.blocks + 120 / m_wallet.blockTime;
    }
    else if (role == 'B')
    {
        // 36h in seconds
        // lt = info.blocks + 259200 / 2 / m_wallet.blockTime;

        // 1h in seconds
        lt = info.blocks + 36 / m_wallet.blockTime;
    }

    return lt;
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeSessionBcc::createTransaction() const
{
    return xbridge::CTransactionPtr(new xbridge::CBTCTransaction);
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeSessionBcc::createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                              const std::vector<std::pair<CScript, double> > &outputs,
                                                              const uint32_t lockTime) const
{
    xbridge::CTransactionPtr tx(new xbridge::CBTCTransaction);
    tx->nVersion  = m_wallet.txVersion;
    tx->nLockTime = lockTime;

//    uint32_t sequence = lockTime ? std::numeric_limits<uint32_t>::max() - 1 : std::numeric_limits<uint32_t>::max();

//    for (const std::pair<std::string, int> & in : inputs)
//    {
//        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second),
//                                CScript(), sequence));
//    }
    for (const std::pair<std::string, int> & in : inputs)
    {
        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second)));
    }

    for (const std::pair<CScript, double> & out : outputs)
    {
        tx->vout.push_back(CTxOut(out.second*m_wallet.COIN, out.first));
    }

    return tx;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionBcc::signTransaction(const xbridge::CKey & key,
                                        const xbridge::CTransactionPtr & transaction,
                                        const uint32_t inputIdx,
                                        const CScript & unlockScript,
                                        std::vector<unsigned char> & signature)
{
    uint256 hash = xbridge::SignatureHash2(unlockScript, transaction, inputIdx, SIGHASH_ALL | SIGHASH_FORKID);
    if (!key.Sign(hash, signature))
    {
        // cancel transaction
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    signature.push_back((unsigned char)SIGHASH_ALL | SIGHASH_FORKID);

    // TXLOG() << "signature " << HexStr(signature.begin(), signature.end());

    return true;
}
