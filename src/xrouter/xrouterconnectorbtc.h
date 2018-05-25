//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_BTC_H_
#define _XROUTER_CONNECTOR_BTC_H_

#include <vector>
#include <string>
#include <cstdint>
#include "json/json_spirit.h"
#include "xrouterconnector.h"
#include "xrouterpacket.h"
#include "keystore.h"
#include "main.h"
#include "net.h"

using namespace json_spirit;

namespace xrouter
{

class BtcWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    std::string getBlockCount() const;
    std::string getBlockHash(const std::string & blockId) const;
    Object      getBlock(const std::string & blockHash) const;
    Object      getTransaction(const std::string & trHash) const;
    Array       getAllBlocks(const int number) const;
    Array       getAllTransactions(const std::string & account, const int number) const;
    std::string getBalance(const std::string & account) const;
    std::string getBalanceUpdate(const std::string & account, const int number) const;
    Array       getTransactionsBloomFilter(const int number, CDataStream& stream) const;
    Object      sendTransaction(const std::string & transaction) const;
    Object      getPaymentAddress() const;

private:
    double getBalanceChange(Object tx, std::string account);
    bool checkFilterFit(Object tx, CBloomFilter filter);
};


} // namespace xrouter

#endif
