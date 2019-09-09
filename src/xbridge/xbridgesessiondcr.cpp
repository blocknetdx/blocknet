// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

////*****************************************************************************
////*****************************************************************************
//
//#include "xbridgesessiondcr.h"
//
//#include "xbridgeapp.h"
//#include "xbridgeexchange.h"
//#include "xuiconnector.h"
//#include "util/xutil.h"
//#include "util/logger.h"
//#include "util/txlog.h"
//#include "bitcoinrpcconnector.h"
//#include "xbitcointransaction.h"
//#include "base58.h"
//#include <boost/algorithm/string.hpp>
//
////*****************************************************************************
////*****************************************************************************
//XBridgeSessionDcr::XBridgeSessionDcr()
//    : XBridgeSession()
//{
//}
//
////*****************************************************************************
////*****************************************************************************
//XBridgeSessionDcr::XBridgeSessionDcr(const WalletParam & wallet)
//    : XBridgeSession(wallet)
//{
//}
//
////*****************************************************************************
////*****************************************************************************
//XBridgeSessionDcr::~XBridgeSessionDcr()
//{
//
//}
//
////*****************************************************************************
////*****************************************************************************
//std::vector<unsigned char> XBridgeSessionDcr::toXAddr(const std::string & addr) const
//{
//    std::vector<unsigned char> vaddr;
//    if (this->DecodeBase58Check(addr.c_str(), vaddr))
//    {
//        vaddr.erase(vaddr.begin());
//    }
//    return vaddr;
//}
//
////******************************************************************************
////******************************************************************************
//uint32_t XBridgeSessionDcr::lockTime(const char role) const
//{
//    rpc::Info info;
//    if (!rpc::getInfo(m_wallet.user, m_wallet.passwd,
//                     m_wallet.ip, m_wallet.port, info))
//    {
//        LOG() << "blockchain info not received " << __FUNCTION__;
//        return 0;
//    }
//
//    if (info.blocks == 0)
//    {
//        LOG() << "block count not defined in blockchain info " << __FUNCTION__;
//        return 0;
//    }
//
//    // lock time
//    uint32_t lt = 0;
//    if (role == 'A')
//    {
//        // 72h in seconds
//        // lt = info.blocks + 259200 / m_wallet.blockTime;
//
//        // 2h in seconds
//        lt = info.blocks + 120 / m_wallet.blockTime;
//    }
//    else if (role == 'B')
//    {
//        // 36h in seconds
//        // lt = info.blocks + 259200 / 2 / m_wallet.blockTime;
//
//        // 1h in seconds
//        lt = info.blocks + 36 / m_wallet.blockTime;
//    }
//
//    return lt;
//}
//
////******************************************************************************
////******************************************************************************
//xbridge::CTransactionPtr XBridgeSessionDcr::createTransaction() const
//{
//    return xbridge::CTransactionPtr(new xbridge::CBTCTransaction);
//}
//
////******************************************************************************
////******************************************************************************
//xbridge::CTransactionPtr XBridgeSessionDcr::createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
//                                                              const std::vector<std::pair<CScript, double> > &outputs,
//                                                              const uint32_t lockTime) const
//{
//    xbridge::CTransactionPtr tx(new xbridge::CBTCTransaction);
//    tx->nVersion  = m_wallet.txVersion;
//    tx->nLockTime = lockTime;
//
//    for (const std::pair<std::string, int> & in : inputs)
//    {
//        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second)));
//    }
//
//    for (const std::pair<CScript, double> & out : outputs)
//    {
//        tx->vout.push_back(CTxOut(out.second*m_wallet.COIN, out.first));
//    }
//
//    return tx;
//}
//
//bool XBridgeSessionDcr::signTransaction(const xbridge::CKey & key,
//                                        const xbridge::CTransactionPtr & transaction,
//                                        const uint32_t inputIdx,
//                                        const CScript & unlockScript,
//                                        std::vector<unsigned char> & signature)
//{
//    uint256 hash = xbridge::SignatureHash2(unlockScript, transaction, inputIdx, SIGHASH_ALL);
//    if (!key.Sign(hash, signature))
//    {
//        // cancel transaction
//        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//        return false;
//    }
//
//    signature.push_back((unsigned char)SIGHASH_ALL);
//
//    // TXLOG() << "signature " << HexStr(signature.begin(), signature.end());
//
//    return true;
//}
//
//bool XBridgeSessionDcr::DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet) const
//{
//    if (!DecodeBase58(psz, vchRet) ||
//        (vchRet.size() < 6)) {
//        vchRet.clear();
//        return false;
//    }
//
//    uint256 hash = HashBlake(vchRet.begin(), vchRet.end() - 4);
//    uint256 hash2 = HashBlake(hash.begin(), hash.end());
//
//    if (memcmp(&hash2, &vchRet.end()[-4], 4) != 0) {
//        vchRet.clear();
//        return false;
//    }
//
//    vchRet.resize(vchRet.size() - 4);
//    vchRet.erase(vchRet.begin(), vchRet.begin() + 2);
//    return true;
//}

