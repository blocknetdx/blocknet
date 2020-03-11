// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_XUICONNECTOR_H
#define BLOCKNET_XBRIDGE_XUICONNECTOR_H

#include <uint256.h>

#include <string>
#include <memory>

#include <boost/signals2/signal.hpp>

namespace xbridge
{
struct TransactionDescr;
typedef std::shared_ptr<TransactionDescr> TransactionDescrPtr;
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

#endif // BLOCKNET_XBRIDGE_XUICONNECTOR_H

