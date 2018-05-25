//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_ETH_H_
#define _XROUTER_CONNECTOR_ETH_H_

#include <vector>
#include <string>
#include <cstdint>
#include <sstream>
#include "json/json_spirit.h"
#include "xrouterconnector.h"
#include "streams.h"

using namespace json_spirit;

namespace xrouter
{

class EthWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    std::string getBlockCount() const;
    std::string getBlockHash(const std::string & blockId) const;
    Object      getBlock(const std::string & blockHash) const;
    Object      getTransaction(const std::string & trHash) const;
    Array       getAllBlocks(const int number) const;
    Array       getAllTransactions(const std::string & account, const int number) const;
    std::string getBalance(const std::string & account) const;
    std::string getBalanceUpdate(const std::string & account, const int number) const;
    Array       getTransactionsBloomFilter(const int, CDataStream &) const;
    Object      sendTransaction(const std::string &) const;
    Object      getPaymentAddress() const;
};


} // namespace xrouter

#endif
