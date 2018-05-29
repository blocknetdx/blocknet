//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLETCONNECTOR_H
#define XBRIDGEWALLETCONNECTOR_H

#include "xbridgewallet.h"

#include <vector>
#include <string>
#include <memory>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{
struct WalletInfo
{
    double   relayFee;
    uint32_t blocks;

    WalletInfo()
        : relayFee(0)
        , blocks(0)
    {

    }
};
} //namespace rpc

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

    double getWalletBalance(const std::string &addr = "") const;

    virtual bool getInfo(rpc::WalletInfo & info) const = 0;

    virtual bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const bool withoutDust = true) const = 0;

    virtual bool lockCoins(const std::vector<wallet::UtxoEntry> & inputs,
                             const bool lock = true) const = 0;

    virtual bool getTxOut(wallet::UtxoEntry & entry) = 0;

    virtual bool sendRawTransaction(const std::string & rawtx,
                                    std::string & txid,
                                    int32_t & errorCode,
                                    std::string & message) = 0;

    virtual bool signMessage(const std::string & address, const std::string & message, std::string & signature) = 0;
    virtual bool verifyMessage(const std::string & address, const std::string & message, const std::string & signature) = 0;

public:
    // helper functions
    bool hasValidAddressPrefix(const std::string & addr) const;

    virtual bool isDustAmount(const double & amount) const = 0;

    virtual bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey) = 0;

    virtual std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey) = 0;
    virtual std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script) = 0;
    virtual std::string scriptIdToString(const std::vector<unsigned char> & id) const = 0;

    virtual double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const = 0;
    virtual double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const = 0;

    virtual bool checkTransaction(const std::string & depositTxId,
                                  const std::string & /*destination*/,
                                  const uint64_t & /*amount*/,
                                  bool & isGood) = 0;

    virtual uint32_t lockTime(const char role) const = 0;

    virtual bool createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                           const std::vector<unsigned char> & otherPubKey,
                                           const std::vector<unsigned char> & xdata,
                                           const uint32_t lockTime,
                                           std::vector<unsigned char> & resultSript) = 0;

    virtual bool createDepositTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                          const std::vector<std::pair<std::string, double> > & outputs,
                                          std::string & txId,
                                          std::string & rawTx) = 0;

    virtual bool createRefundTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                         const std::vector<std::pair<std::string, double> > & outputs,
                                         const std::vector<unsigned char> & mpubKey,
                                         const std::vector<unsigned char> & mprivKey,
                                         const std::vector<unsigned char> & innerScript,
                                         const uint32_t lockTime,
                                         std::string & txId,
                                         std::string & rawTx) = 0;

    virtual bool createPaymentTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                          const std::vector<std::pair<std::string, double> > & outputs,
                                          const std::vector<unsigned char> & mpubKey,
                                          const std::vector<unsigned char> & mprivKey,
                                          const std::vector<unsigned char> & xpubKey,
                                          const std::vector<unsigned char> & innerScript,
                                          std::string & txId,
                                          std::string & rawTx) = 0;
};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTOR_H
