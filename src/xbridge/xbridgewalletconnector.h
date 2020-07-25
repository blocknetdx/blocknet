// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTOR_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTOR_H

#include <xbridge/xbridgewallet.h>

#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <vector>
#include <string>
#include <memory>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

struct XTxIn
{
    std::string txid;
    uint32_t    n;
    double      amount;

    XTxIn(std::string _txid, uint32_t _n, double _amount)
        : txid(_txid)
        , n(_n)
        , amount(_amount)
    {}
};

//*****************************************************************************
//*****************************************************************************
namespace rpc
{
struct WalletInfo
{
    double   relayFee;
    uint32_t blocks;
    int64_t mediantime;
    uint256 bestblockhash;

    WalletInfo()
        : relayFee(0)
        , blocks(0)
        , mediantime(0)
        , bestblockhash(uint256())
    {

    }
};
} //namespace rpc

static const uint32_t SEQUENCE_FINAL = 0xffffffff;

//*****************************************************************************
//*****************************************************************************
class WalletConnector : public WalletParam
{
public:
    WalletConnector();

public:
    WalletConnector & operator = (const WalletParam & other)
    {
        *(WalletParam *)this = other;
        return *this;
    }

    virtual bool init() = 0;

public:
    // reimplement for currency
    virtual std::string fromXAddr(const std::vector<unsigned char> & xaddr) const = 0;
    virtual std::vector<unsigned char> toXAddr(const std::string & addr) const = 0;

public:
    // wallet RPC

    virtual bool getNewAddress(std::string & addr) = 0;

    virtual bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries) = 0;

    double getWalletBalance(const std::set<wallet::UtxoEntry> & excluded, const std::string &addr = "") const;

    std::string getNewTokenAddress();

    virtual bool getInfo(rpc::WalletInfo & info) const = 0;

    virtual bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const std::set<wallet::UtxoEntry> & excluded) const = 0;

    virtual bool getBlock(const std::string & blockHash, std::string & rawBlock) = 0;

    virtual bool getBlockHash(const uint32_t & block, std::string & blockHash) = 0;

    virtual bool getTxOut(wallet::UtxoEntry & entry) = 0;

    virtual bool sendRawTransaction(const std::string & rawtx,
                                    std::string & txid,
                                    int32_t & errorCode,
                                    std::string & message) = 0;

    virtual bool signMessage(const std::string & address, const std::string & message, std::string & signature) = 0;
    virtual bool verifyMessage(const std::string & address, const std::string & message, const std::string & signature) = 0;

    virtual bool getRawMempool(std::vector<std::string> & txids) = 0;

    virtual bool getBlockCount(uint32_t & blockCount) = 0;

public:
    // helper functions
    virtual bool hasValidAddressPrefix(const std::string & addr) const = 0;
    virtual bool isValidAddress(const std::string & addr) const = 0;

    virtual bool isDustAmount(const double & amount) const = 0;

    virtual bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey) = 0;

    virtual std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey) = 0;
    virtual std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script) = 0;
    virtual std::string scriptIdToString(const std::vector<unsigned char> & id) const = 0;

    virtual double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const = 0;
    virtual double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const = 0;

    virtual bool checkDepositTransaction(const std::string & depositTxId,
                                         const std::string & /*destination*/,
                                         double & amount,
                                         uint64_t & p2shAmount,
                                         uint32_t & depositTxVout,
                                         const std::string & expectedScript,
                                         double & excessAmount,
                                         bool & isGood) = 0;

    virtual bool getSecretFromPaymentTransaction(const std::string & paymentTxId,
                                                 const std::string & depositTxId,
                                                 const uint32_t & depositTxVOut,
                                                 const std::vector<unsigned char> & hx,
                                                 std::vector<unsigned char> & secret,
                                                 bool & isGood) = 0;

    virtual uint32_t lockTime(const char role) const = 0;

    virtual bool acceptableLockTimeDrift(const char role, const uint32_t lckTime) const = 0;

    virtual bool createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                           const std::vector<unsigned char> & otherPubKey,
                                           const std::vector<unsigned char> & xdata,
                                           const uint32_t lockTime,
                                           std::vector<unsigned char> & resultSript) = 0;

    virtual bool createDepositTransaction(const std::vector<XTxIn> & inputs,
                                          const std::vector<std::pair<std::string, double> > & outputs,
                                          std::string & txId,
                                          uint32_t & txVout,
                                          std::string & rawTx) = 0;

    virtual bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                         const std::vector<std::pair<std::string, double> > & outputs,
                                         const std::vector<unsigned char> & mpubKey,
                                         const std::vector<unsigned char> & mprivKey,
                                         const std::vector<unsigned char> & innerScript,
                                         const uint32_t lockTime,
                                         std::string & txId,
                                         std::string & rawTx) = 0;

    virtual bool createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                          const std::vector<std::pair<std::string, double> > & outputs,
                                          const std::vector<unsigned char> & mpubKey,
                                          const std::vector<unsigned char> & mprivKey,
                                          const std::vector<unsigned char> & xpubKey,
                                          const std::vector<unsigned char> & innerScript,
                                          std::string & txId,
                                          std::string & rawTx) = 0;

    virtual bool createPartialTransaction(const std::vector<XTxIn> inputs,
                                          const std::vector<std::pair<std::string, double> > outputs,
                                          std::string & txId,
                                          std::string & rawTx) = 0;

    virtual bool splitUtxos(CAmount splitAmount, std::string addr, bool includeFees, std::set<wallet::UtxoEntry> excluded,
                            std::set<COutPoint> utxos, CAmount & totalSplit, CAmount & splitIncFees, int & splitCount,
                            std::string & txId, std::string & rawTx, std::string & failReason) = 0;

    virtual bool isUTXOSpentInTx(const std::string & txid, const std::string & utxoPrevTxId,
                                 const uint32_t & utxoVoutN, bool & isSpent) = 0;

    virtual bool getTransactionsInBlock(const std::string & blockHash, std::vector<std::string> & txids) = 0;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTOR_H
