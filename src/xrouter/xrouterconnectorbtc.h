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
    std::string getBlockCount() const override;
    Object      getBlockHash(const std::string & blockId) const override;
    Object      getBlock(const std::string & blockHash) const override;
    Object      getTransaction(const std::string & trHash) const override;
    Array       getAllBlocks(const int number, int blocklimit=0) const override;
    Array       getAllTransactions(const std::string & account, const int number, const int time=0, int blocklimit=0) const override;
    std::string getBalance(const std::string & account, const int time=0, int blocklimit=0) const override;
    std::string getBalanceUpdate(const std::string & account, const int number, const int time=0, int blocklimit=0) const override;
    Array       getTransactionsBloomFilter(const int number, CDataStream& stream, int blocklimit=0) const override;
    Object      sendTransaction(const std::string & transaction) const override;
    std::string convertTimeToBlockCount(const std::string & timestamp) const override;

private:
    double getBalanceChange(Object tx, std::string account) const;
    bool checkFilterFit(Object tx, CBloomFilter filter) const;
};


} // namespace xrouter

#endif
