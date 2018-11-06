//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLETCONNECTORBTC_H
#define XBRIDGEWALLETCONNECTORBTC_H

#include "xbridgewalletconnector.h"

#include <memory>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
class BtcWalletConnector : public WalletConnector
{
    class Impl;

public:
    BtcWalletConnector() {}

    bool init();

public:
    // reimplement for currency
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const;
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

public:
    bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries);

    bool getInfo(rpc::WalletInfo & info) const;

    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const bool withLocked = false) const;

    bool lockCoins(const std::vector<wallet::UtxoEntry> & inputs, const bool lock = true);

    bool getNewAddress(std::string & addr);

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

public:
    bool hasValidAddressPrefix(const std::string & addr) const;
    bool isValidAddress(const std::string & addr) const;

    bool isDustAmount(const double & amount) const;

    bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey);

    std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey);
    std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script);
    std::string scriptIdToString(const std::vector<unsigned char> & id) const;

    double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const;
    double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const;

    bool checkDepositTransaction(const std::string & depositTxId,
                                 const std::string & /*destination*/,
                                 double & amount,
                                 uint32_t & depositTxVout,
                                 const std::string & expectedScript,
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

    bool createDepositTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  std::string & txId,
                                  uint32_t & txVout,
                                  std::string & rawTx);

    bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                 const std::vector<std::pair<std::string, double> > & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx);

    bool createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx);

    bool isUTXOSpentInTx(const std::string & txid, const std::string & utxoPrevTxId,
                         const uint32_t & utxoVoutN, bool & isSpent);

    bool getTransactionsInBlock(const std::string & blockHash, std::vector<std::string> & txids);

protected:
    CryptoProvider m_cp;
};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORBTC_H
