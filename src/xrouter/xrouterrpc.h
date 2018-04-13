//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_

#include <vector>
#include <string>
#include <cstdint>
#include "xbridge/xbridgewalletconnector.h"
#include "xbridge/xbridgewalletconnectorbtc.h"

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
    Object executeRpcCall(const std::string command, const Array & params) {
        Object reply = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
        return reply;
    }
    
    Object executeAuthorizedRpcCall(const std::string auth, const std::string command, const Array & params) {
        Object reply = rpc::CallRPCAuthorized(auth, m_ip, m_port, command, params);
        return reply;
    }
};

} // namespace xrouter

#endif
