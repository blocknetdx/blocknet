#ifndef _XROUTER_CONNECTOR_H_
#define _XROUTER_CONNECTOR_H_

#include <vector>
#include <string>
#include <cstdint>
#include "json/json_spirit.h"
#include "xbridge/xbridgewallet.h"
#include "streams.h"
#include "wallet.h"
#include "xrouterutils.h"
#include <boost/container/map.hpp>

using namespace json_spirit;

namespace xrouter
{

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
    virtual Object getBlockHash(const std::string & blockId) const = 0;
    virtual Object      getBlock(const std::string & blockHash) const = 0;
    virtual Object      getTransaction(const std::string & trHash) const = 0;
    virtual Array       getAllBlocks(const int number, int blocklimit=0) const = 0;
    virtual Array       getAllTransactions(const std::string & account, const int number, const int time=0, int blocklimit=0) const = 0;
    virtual std::string getBalance(const std::string & account, const int time=0, int blocklimit=0) const = 0;
    virtual std::string getBalanceUpdate(const std::string & account, const int number, const int time=0, int blocklimit=0) const = 0;
    virtual Array       getTransactionsBloomFilter(const int number, CDataStream & stream, int blocklimit=0) const = 0;
    virtual Object      sendTransaction(const std::string & transaction) const = 0;
    virtual std::string convertTimeToBlockCount(const std::string & timestamp) const = 0;
};


} // namespace xrouter


#endif
