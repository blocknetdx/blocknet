#ifndef XUICONNECTOR_H
#define XUICONNECTOR_H

#include <string>
#include <memory>
#include <boost/signals2/signal.hpp>


class uint256;

namespace xbridge
{
struct TransactionDescr;
typedef boost::shared_ptr<TransactionDescr> TransactionDescrPtr;
}


class XUIConnector
{
public:
    boost::signals2::signal<void (const xbridge::TransactionDescrPtr & tx)> NotifyXBridgeTransactionReceived;

    boost::signals2::signal<void (const uint256 & id)> NotifyXBridgeTransactionChanged;

    boost::signals2::signal<void (const std::string & currency,
                                  const std::string & name,
                                  const std::string & address)> NotifyXBridgeAddressBookEntryReceived;
};

extern XUIConnector xuiConnector;

#endif // XUICONNECTOR_H

