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
    Object      getBlockHash(const int & block) const override;
    Object      getBlock(const std::string & hash) const override;
    Array       getBlocks(const std::vector<std::string> & blockHashes) const override;
    Object      getTransaction(const std::string & hash) const override;
    Array       getTransactions(const std::vector<std::string> & txHashes) const override;
    Array       getTransactionsBloomFilter(const int & number, CDataStream & stream, const int & fetchlimit) const override;
    Object      sendTransaction(const std::string & transaction) const override;
    Object      decodeRawTransaction(const std::string & hex) const override;
    std::string convertTimeToBlockCount(const std::string & timestamp) const override;
    std::string getBalance(const std::string & address) const override;
};

} // namespace xrouter

#endif
