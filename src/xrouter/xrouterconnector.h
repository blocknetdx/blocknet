#ifndef _XROUTER_CONNECTOR_H_
#define _XROUTER_CONNECTOR_H_

#include <vector>
#include <string>
#include <cstdint>
#include "json/json_spirit.h"
#include "xbridge/xbridgewallet.h"
#include "streams.h"
#include "wallet.h"

using namespace json_spirit;

namespace xrouter
{

std::string CallCMD(std::string cmd);
std::string CallURL(std::string ip, std::string port, std::string url);

Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);

bool createAndSignTransaction(std::string address, const double amount, std::string & raw_tx);
bool sendTransactionBlockchain(std::string raw_tx, std::string & txid);
bool sendTransactionBlockchain(std::string address, const double amount, std::string & raw_tx);
bool createPaymentChannel(CPubKey address, double deposit, int date, std::string raw_tx);
    
class WalletConnectorXRouter : public xbridge::WalletParam
{
public:
    WalletConnectorXRouter();

    WalletConnectorXRouter & operator = (const WalletParam & other)
    {
        *(WalletParam *)this = other;
        return *this;
    }

    virtual std::string getBlockCount() const = 0;
    virtual std::string getBlockHash(const std::string & blockId) const = 0;
    virtual Object      getBlock(const std::string & blockHash) const = 0;
    virtual Object      getTransaction(const std::string & trHash) const = 0;
    virtual Array       getAllBlocks(const int number) const = 0;
    virtual Array       getAllTransactions(const std::string & account, const int number, const int time=0) const = 0;
    virtual std::string getBalance(const std::string & account, const int time=0) const = 0;
    virtual std::string getBalanceUpdate(const std::string & account, const int number, const int time=0) const = 0;
    virtual Array       getTransactionsBloomFilter(const int number, CDataStream & stream) const = 0;
    virtual Object      sendTransaction(const std::string & transaction) const = 0;
    virtual Object      getPaymentAddress() const = 0;
};


} // namespace xrouter


#endif
