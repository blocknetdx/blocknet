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
/**
 * @brief The WalletConnector class
 */
class WalletConnector : public WalletParam
{
public:
    /**
     * @brief WalletConnector
     */
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
    /**
     * @brief fromXAddr
     * @param xaddr
     * @return
     */
    virtual std::string fromXAddr(const std::vector<unsigned char> & xaddr) const = 0;
    /**
     * @brief toXAddr
     * @param addr
     * @return
     */
    virtual std::vector<unsigned char> toXAddr(const std::string & addr) const = 0;

public:
    // wallet RPC

    /**
     * @brief getNewAddress
     * @param addr
     * @return
     */
    virtual bool getNewAddress(std::string & addr) = 0;

    /**
     * @brief requestAddressBook
     * @param entries
     * @return
     */
    virtual bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries) = 0;

    /**
     * \brief Return the wallet balance; optionally for the specified address.
     * \param addr Optional address to filter balance
     * \return returns the wallet balance for the address.
     *
     * The wallet balance for the specified address will be returned. Only utxo's associated with the address
     * are included.
     */
    double getWalletBalance(const std::string &addr = "") const;

    /**
     * @brief getUnspent
     * @param inputs
     * @return
     */
    virtual bool getUnspent(std::vector<wallet::UtxoEntry> & inputs) const = 0;

    /**
     * @brief lockCoins
     * @param inputs
     * @param lock
     * @return
     */
    virtual bool lockCoins(const std::vector<wallet::UtxoEntry> & inputs,
                             const bool lock = true) const = 0;

    /**
     * @brief getTxOut
     * @param entry
     * @return
     */
    virtual bool getTxOut(wallet::UtxoEntry & entry) = 0;

    /**
     * @brief sendRawTransaction
     * @param rawtx
     * @param txid
     * @param errorCode
     * @param message
     * @return
     */
    virtual bool sendRawTransaction(const std::string & rawtx,
                                    std::string & txid,
                                    int32_t & errorCode,
                                    std::string & message) = 0;

    /**
     * @brief signMessage
     * @param address
     * @param message
     * @param signature
     * @return
     */
    virtual bool signMessage(const std::string & address, const std::string & message, std::string & signature) = 0;
    /**
     * @brief verifyMessage
     * @param address
     * @param message
     * @param signature
     * @return
     */
    virtual bool verifyMessage(const std::string & address, const std::string & message, const std::string & signature) = 0;

public:
    // helper functions
    /**
     * \brief Checks if specified address has a valid prefix.
     * \param addr Address to check
     * \return returns true if address has a valid prefix, otherwise false.
     *
     * If the specified wallet address has a valid prefix the method returns true, otherwise false.
     */
    bool hasValidAddressPrefix(const std::string & addr) const;

    /**
     * @brief isDustAmount
     * @param amount
     * @return
     */
    virtual bool isDustAmount(const double & amount) const = 0;

    /**
     * @brief newKeyPair
     * @param pubkey
     * @param privkey
     * @return
     */
    virtual bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey) = 0;

    /**
     * @brief getKeyId
     * @param pubkey
     * @return
     */
    virtual std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey) = 0;
    /**
     * @brief getScriptId
     * @param script
     * @return
     */
    virtual std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script) = 0;
    /**
     * @brief scriptIdToString
     * @param id
     * @return
     */
    virtual std::string scriptIdToString(const std::vector<unsigned char> & id) const = 0;

    /**
     * @brief minTxFee1
     * @param inputCount
     * @param outputCount
     * @return
     */
    virtual double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) = 0;
    /**
     * @brief minTxFee2
     * @param inputCount
     * @param outputCount
     * @return
     */
    virtual double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) = 0;

    /**
     * @brief checkTransaction
     * @param depositTxId
     * @param isGood
     * @return
     */
    virtual bool checkTransaction(const std::string & depositTxId,
                                  const std::string & /*destination*/,
                                  const uint64_t & /*amount*/,
                                  bool & isGood) = 0;

    /**
     * @brief lockTime
     * @param role
     * @return
     */
    virtual uint32_t lockTime(const char role) const = 0;

    /**
     * @brief createDepositUnlockScript
     * @param myPubKey
     * @param otherPubKey
     * @param xdata
     * @param lockTime
     * @param resultSript
     * @return
     */
    virtual bool createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                           const std::vector<unsigned char> & otherPubKey,
                                           const std::vector<unsigned char> & xdata,
                                           const uint32_t lockTime,
                                           std::vector<unsigned char> & resultSript) = 0;

    /**
     * @brief createDepositTransaction
     * @param inputs
     * @param outputs
     * @param txId
     * @param rawTx
     * @return
     */
    virtual bool createDepositTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                          const std::vector<std::pair<std::string, double> > & outputs,
                                          std::string & txId,
                                          std::string & rawTx) = 0;

    /**
     * @brief createRefundTransaction
     * @param inputs
     * @param outputs
     * @param mpubKey
     * @param mprivKey
     * @param innerScript
     * @param lockTime
     * @param txId
     * @param rawTx
     * @return
     */
    virtual bool createRefundTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                         const std::vector<std::pair<std::string, double> > & outputs,
                                         const std::vector<unsigned char> & mpubKey,
                                         const std::vector<unsigned char> & mprivKey,
                                         const std::vector<unsigned char> & innerScript,
                                         const uint32_t lockTime,
                                         std::string & txId,
                                         std::string & rawTx) = 0;

    /**
     * @brief createPaymentTransaction
     * @param inputs
     * @param outputs
     * @param mpubKey
     * @param mprivKey
     * @param xpubKey
     * @param innerScript
     * @param txId
     * @param rawTx
     * @return
     */
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
