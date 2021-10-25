// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H

#include <xbridge/xbridgewalletconnector.h>
#include "xbridge/util/logger.h"

#include <event2/buffer.h>
#include <rpc/protocol.h>
#include <rpc/client.h>
#include <support/events.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <univalue.h>

#include <memory>

#include <json/json_spirit.h>
#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>

#include <boost/lexical_cast.hpp>

// #define XBRIDGE_LOG_RPC_CALLS

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
json_spirit::Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
                      const std::string & rpcip, const std::string & rpcport,
                      const std::string & strMethod, const json_spirit::Array & params,
                      const std::string & jsonver="", const std::string & contenttype="");

//*****************************************************************************
//*****************************************************************************
static json_spirit::Object CallRPC(const std::string & rpcip, const std::string & rpcport,
                            const std::string & strMethod, const json_spirit::Array & params)
{
    return CallRPC("", "", rpcip, rpcport, strMethod, params, "", "application/json");
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
class BtcWalletConnector : public WalletConnector
{
public:
    BtcWalletConnector()
        : m_isSignMessageWithWallet(true)
        , m_isSignMessageWithPrivKey(true)
    {}


    bool init();

public:
    // reimplement for currency
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const;
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

public:
    bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries);

    amount_t getWalletBalance(const std::set<wallet::UtxoEntry> & excluded, const std::string &addr = "") const;

    bool getInfo(rpc::WalletInfo & info) const;

    bool loadWallet(const std::string & walletName) const;

    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const std::set<wallet::UtxoEntry> & excluded) const;

    bool getNewAddress(std::string & addr, const std::string & type = "");

    bool getTxOut(wallet::UtxoEntry & entry);

    bool sendRawTransaction(const std::string & rawtx,
                            std::string & txid,
                            int32_t & errorCode,
                            std::string & message);

    bool signMessage(const std::string & address, const std::string & message, std::string & signature);
    bool verifyMessage(const std::string & address, const std::string & message, const std::string & signature);

    bool getRawMempool(std::vector<std::string> & txids);

    bool getBlock(const std::string & blockHash, std::string & rawBlock);

    bool getBlockHash(const uint32_t & block, std::string & blockHash);

    bool getBlockCount(uint32_t & blockCount) const;

public:
    bool hasValidAddressPrefix(const std::string & addr) const;
    bool isValidAddress(const std::string & addr) const;

    // amount > 0 && !isDustAmount
    bool isValidAmount(const amount_t & amount) const; 

    bool isDustAmount(const amount_t & amount) const;

    bool canAcceptTransactions() const;

    bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey);

    std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey);
    std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script);
    std::string scriptIdToString(const std::vector<unsigned char> & id) const;

    // minTxFee* are assumed to be fairly cheap calls (no rpc query).
    // Repeatedly called in loops during utxo selection.
    double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const;
    double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const;

    bool checkDepositTransaction(const std::string & depositTxId,
                                 const std::string & /*destination*/,
                                 amount_t & amount,
                                 amount_t & p2shAmount,
                                 uint32_t & depositTxVout,
                                 const std::string & expectedScript,
                                 amount_t & excessAmount,
                                 bool & isGood);

    bool getSecretFromPaymentTransaction(const std::string & paymentTxId,
                                         const std::string & depositTxId,
                                         const uint32_t & depositTxVOut,
                                         const std::vector<unsigned char> & hx,
                                         std::vector<unsigned char> & secret,
                                         bool & isGood);

    uint32_t lockTime(const char role) const;

    bool acceptableLockTimeDrift(const char role, const uint32_t lckTime) const;

    bool createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                   const std::vector<unsigned char> & otherPubKey,
                                   const std::vector<unsigned char> & secretHash,
                                   const uint32_t lockTime,
                                   std::vector<unsigned char> & resultSript);

    bool createDepositTransaction(const std::vector<XTxIn>  & inputs,
                                  const std::vector<XTxOut> & outputs,
                                  std::string & txId,
                                  uint32_t & txVout,
                                  std::string & rawTx);

    bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                 const std::vector<XTxOut> & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx);

    bool createPaymentTransaction(const std::vector<XTxIn>  & inputs,
                                  const std::vector<XTxOut> & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx);

    bool createPartialTransaction(const std::vector<XTxIn>  & inputs,
                                  const std::vector<XTxOut> & outputs,
                                  std::string & txId,
                                  std::string & rawTx) override;

    bool splitUtxos(amount_t splitAmount, std::string addr, bool includeFees, std::set<wallet::UtxoEntry> excluded,
                    std::set<COutPoint> utxos, amount_t & totalSplit, amount_t & splitIncFees, int & splitCount,
                    std::string & txId, std::string & rawTx, std::string & failReason) override;

    bool isUTXOSpentInTx(const std::string & txid, const std::string & utxoPrevTxId,
                         const uint32_t & utxoVoutN, bool & isSpent);

    bool getTransactionsInBlock(const std::string & blockHash, std::vector<std::string> & txids);

protected:
    CryptoProvider m_cp;

  private:
    bool m_isSignMessageWithWallet;
    bool m_isSignMessageWithPrivKey;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H
