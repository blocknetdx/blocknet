#ifndef XBRIDGEWALLETCONNECTORETH_H
#define XBRIDGEWALLETCONNECTORETH_H

#include "xbridgewalletconnector.h"
#include "xbridgeethencoder.h"
#include "xbridgecryptoproviderbtc.h"

namespace xbridge
{

class EthWalletConnector : public WalletConnector
{
    class Impl;

public:
    EthWalletConnector() { m_cp = new BtcCryptoProvider; };
    ~EthWalletConnector() { delete m_cp; }

    bool init();
public:
    // reimplement for currency
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const;
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

public:
    bool getNewAddress(std::string & /*addr*/, const std::string & /*type*/ = "") { assert(false && "implementation"); return true; }
    bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries);
    double getWalletBalance(const std::string &addr = "") const;
    bool getInfo(rpc::WalletInfo & /*info*/) const { return true; }
    
    bool loadWallet(const std::string & /*walletName*/) const { assert(false && "implementation"); return true; }
    bool getUnspent(std::vector<wallet::UtxoEntry> & /*inputs*/, const std::set<wallet::UtxoEntry> & /*excluded*/) const { assert(false && "implementation"); return true; }
    bool getBlock(const std::string & /*blockHash*/, std::string & /*rawBlock*/) { assert(false && "implementation"); return true; }
    bool getBlockHash(const uint32_t & /*block*/, std::string & /*blockHash*/) { assert(false && "implementation"); return true; }

    bool getUnspent(std::vector<wallet::UtxoEntry> & /*inputs*/, const bool /*withoutDust*/ = true) const { return true; }
    bool lockCoins(const std::vector<wallet::UtxoEntry> & /*inputs*/, const bool /*lock*/ = true) { return true; }
    bool getTxOut(wallet::UtxoEntry & /*entry*/) { return true; }
    bool sendRawTransaction(const std::string & /*rawtx*/,
                            std::string & /*txid*/,
                            int32_t & /*errorCode*/,
                            std::string & /*message*/) { return true; }

    bool signMessage(const std::string & /*address*/, const std::string & /*message*/, std::string & /*signature*/) { return true; }
    bool verifyMessage(const std::string & /*address*/, const std::string & /*message*/, const std::string & /*signature*/) { return true; }

    bool getRawMempool(std::vector<std::string> & /*txids*/) { assert(false && "implementation");  return true; }
    bool getBlockCount(uint32_t & /*blockCount*/) { assert(false && "implementation");  return true; }

public:
    bool hasValidAddressPrefix(const std::string & /*addr*/) const { return true; }
    bool isValidAddress(const std::string & /*addr*/) const  { assert(false && "implementation"); return true; }

    bool isDustAmount(const double & /*amount*/) const { return false; }

    bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey);

    std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey);
    std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & /*script*/) { return std::vector<unsigned char>(); }
    std::string scriptIdToString(const std::vector<unsigned char> & /*id*/) const { return std::string(); }

    double minTxFee1(const uint32_t /*inputCount*/, const uint32_t /*outputCount*/) const { return 0; }
    double minTxFee2(const uint32_t /*inputCount*/, const uint32_t /*outputCount*/) const { return 0; }

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
                                         bool & isGood)
    {
        assert(false && "implementation"); 
        return false;
    }

    uint32_t lockTime(const char role) const;

    bool acceptableLockTimeDrift(const char /*role*/, const uint32_t /*lckTime*/) const
    {
        assert(false && "implementation"); 
        return false;
    }


    bool createDepositUnlockScript(const std::vector<unsigned char> & /*myPubKey*/,
                                   const std::vector<unsigned char> & /*otherPubKey*/,
                                   const std::vector<unsigned char> & /*xdata*/,
                                   const uint32_t /*lockTime*/,
                                   std::vector<unsigned char> & /*resultSript*/) 
    { 
        return false; 
    }

    bool createDepositTransaction(const std::vector<XTxIn> & /*inputs*/,
                                  const std::vector<XTxOut> & /*outputs*/,
                                  std::string & /*txId*/,
                                  uint32_t & /*txVout*/,
                                  std::string & /*rawTx*/) 
    { 
        return false; 
    }

    bool createRefundTransaction(const std::vector<XTxIn> & /*inputs*/,
                                 const std::vector<XTxOut> & /*outputs*/,
                                 const std::vector<unsigned char> & /*mpubKey*/,
                                 const std::vector<unsigned char> & /*mprivKey*/,
                                 const std::vector<unsigned char> & /*innerScript*/,
                                 const uint32_t /*lockTime*/,
                                 std::string & /*txId*/,
                                 std::string & /*rawTx*/) 
    { 
        return false; 
    }

    bool createPaymentTransaction(const std::vector<XTxIn> & /*inputs*/,
                                  const std::vector<XTxOut> & /*outputs*/,
                                  const std::vector<unsigned char> & /*mpubKey*/,
                                  const std::vector<unsigned char> & /*mprivKey*/,
                                  const std::vector<unsigned char> & /*xpubKey*/,
                                  const std::vector<unsigned char> & /*innerScript*/,
                                  std::string & /*txId*/,
                                  std::string & /*rawTx*/) 
    { 
        return false; 
    }

    bool createPartialTransaction(const std::vector<XTxIn>  & /*inputs*/,
                                  const std::vector<XTxOut> & /*outputs*/,
                                  std::string & /*txId*/,
                                  std::string & /*rawTx*/) 
    {
        return false;
    }

    bool splitUtxos(amount_t /*splitAmount*/, 
                    std::string /*addr*/, 
                    bool /*includeFees*/, 
                    std::set<wallet::UtxoEntry> /*excluded*/,
                    std::set<COutPoint> /*utxos*/, 
                    amount_t & /*totalSplit*/, 
                    amount_t & /*splitIncFees*/, 
                    int & /*splitCount*/,
                    std::string & /*txId*/, 
                    std::string & /*rawTx*/, 
                    std::string & /*failReason*/)
    {
        return false;
    }

    bool isUTXOSpentInTx(const std::string & /*txid*/, 
                         const std::string & /*utxoPrevTxId*/,
                         const uint32_t & /*utxoVoutN*/, 
                         bool & /*isSpent*/)
    {
        return false;
    }

    bool getTransactionsInBlock(const std::string & /*blockHash*/, 
                                std::vector<std::string> & /*txids*/)
    {
        return false;
    }

    bool getAccounts(std::vector<std::string> & accounts);
    bool getBalance(const bytes & account, uint256 & balance) const;
    bool getGasPrice(uint256 & gasPrice) const;
    bool getEstimateGas(const bytes & myAddress,
                        const bytes & data,
                        const uint256 & value,
                        uint256 & estimateGas) const;
    bool getLastBlockTime(uint256 & blockTime) const;

    bytes createInitiateData(const bytes & hashedSecret,
                             const bytes & responderAddress,
                             const uint256 & refundDuration) const;

    bytes createRespondData(const bytes & hashedSecret,
                            const bytes & initiatorAddress,
                            const uint256 & refundDuration) const;

    bytes createRefundData(const bytes & hashedSecret) const;
    bytes createRedeemData(const bytes & hashedSecret, const bytes & secret) const;

    bool callContractMethod(const bytes & myAddress,
                            const bytes & data,
                            const uint256 & value,
                            const uint256 & gas,
                            uint256 & transactionHash) const;

    bool isInitiated(const bytes& hashedSecret, bytes& initiatorAddress, const bytes & responderAddress, const uint256 value) const;
    bool isResponded(const bytes& hashedSecret, const bytes & initiatorAddress, bytes & responderAddress, const uint256 value) const;
    bool isRefunded(const bytes& hashedSecret, const bytes & recipientAddress, const uint256 value) const;
    bool isRedeemed(const bytes& hashedSecret, const bytes & recipientAddress, const uint256 value) const;

protected:
    BtcCryptoProvider* m_cp;

private:
    enum NetwortType
    {
        MAINNET,
        TESTNET
    };

    NetwortType m_networkType;
    uint256 m_fromBlock;
    const std::string m_contractAddress = "0x078fb70CA9A3077cDF1cA32b4a9Cb74898963DA8";
    const std::string m_contractAddressTestnet = "0x078fb70CA9A3077cDF1cA32b4a9Cb74898963DA8";
};

}

#endif // XBRIDGEWALLETCONNECTORETH_H
