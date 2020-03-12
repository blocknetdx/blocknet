// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEDEF_H
#define BLOCKNET_XBRIDGE_XBRIDGEDEF_H

#include <vector>
#include <map>
#include <queue>
#include <memory>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

class WalletConnector;
typedef std::shared_ptr<WalletConnector> WalletConnectorPtr;

typedef std::vector<WalletConnectorPtr> Connectors;
typedef std::map<std::vector<unsigned char>, WalletConnectorPtr> ConnectorsAddrMap;
typedef std::map<std::string, WalletConnectorPtr> ConnectorsCurrencyMap;

class Session;
typedef std::shared_ptr<Session> SessionPtr;

typedef std::queue<SessionPtr> SessionQueue;
typedef std::map<std::vector<unsigned char>, SessionPtr> SessionsAddrMap;

typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
typedef std::vector<AddressBookEntry> AddressBook;

typedef std::shared_ptr<boost::asio::io_service>       IoServicePtr;
typedef std::shared_ptr<boost::asio::io_service::work> WorkPtr;

struct TransactionDescr;
typedef std::shared_ptr<TransactionDescr> TransactionDescrPtr;

class Transaction;
typedef std::shared_ptr<Transaction> TransactionPtr;

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEDEF_H
