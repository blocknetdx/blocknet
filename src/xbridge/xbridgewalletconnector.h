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
namespace wallet
{
typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;

struct UtxoEntry
{
    std::string txId;
    uint32_t    vout;
    double      amount;

    friend bool operator < (const UtxoEntry & l, const UtxoEntry & r);
    friend bool operator == (const UtxoEntry & l, const UtxoEntry & r);
};

} // namespace wallet

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

public:
    // reimplement for currency
    virtual std::string fromXAddr(const std::vector<unsigned char> & xaddr) const = 0;
    virtual std::vector<unsigned char> toXAddr(const std::string & addr) const = 0;

public:
    // wallet RPC

    virtual bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries) = 0;

    virtual bool getUnspent(std::vector<wallet::UtxoEntry> & inputs) const = 0;

    bool checkAmount(const uint64_t amount) const;
    double getWalletBalance() const;

    virtual bool lockUnspent(const std::vector<wallet::UtxoEntry> & inputs,
                             const bool lock = true) const = 0;

    virtual bool getRawTransaction(const std::string & txid, const bool verbose, std::string & tx) = 0;

    virtual bool getNewAddress(std::string & addr) = 0;

    virtual bool createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                      const std::vector<std::pair<std::string, double> > & outputs,
                                      const uint32_t lockTime,
                                      std::string & tx) = 0;

    virtual bool signRawTransaction(std::string & rawtx, bool & complete) = 0;

    virtual bool decodeRawTransaction(const std::string & rawtx,
                                      std::string & txid,
                                      std::string & tx) = 0;

    virtual bool sendRawTransaction(const std::string & rawtx,
                                    std::string & txid,
                                    int32_t & errorCode) = 0;

public:
    // helper functions

    virtual bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey) = 0;

    virtual std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey) = 0;
    virtual std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script) = 0;
    virtual std::string scriptIdToString(const std::vector<unsigned char> & id) const = 0;

    virtual double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) = 0;
    virtual double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) = 0;

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
