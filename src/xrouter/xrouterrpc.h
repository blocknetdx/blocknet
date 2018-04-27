//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_

#include <vector>
#include <string>
#include <cstdint>
#include "xbridge/xbridgewalletconnector.h"
#include "xbridge/xbridgewalletconnectorbtc.h"
#include "xbridge/xbridgewalletconnectorbcc.h"
#include "xbridge/xbridgewalletconnectorsys.h"

//******************************************************************************
//******************************************************************************
namespace rpc
{
    Object CallRPCAuthorized(const std::string & auth,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);
               
    Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);
} // namespace rpc

namespace xrouter
{

class BtcWalletConnectorXRouter : public xbridge::BtcWalletConnector {
public:

};

class SysWalletConnectorXRouter : public xbridge::SysWalletConnector {
public:

};

class BccWalletConnectorXRouter : public xbridge::BccWalletConnector {
public:

};


} // namespace xrouter

#endif
