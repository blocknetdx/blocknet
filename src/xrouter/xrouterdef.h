//******************************************************************************
//******************************************************************************

#ifndef XROUTERDEF_H
#define XROUTERDEF_H

#include <vector>
#include <map>
#include <queue>
#include <memory>

#define MIN_BLOCK 200

//******************************************************************************
//******************************************************************************
namespace xrouter
{

class WalletConnectorXRouter;
typedef std::shared_ptr<WalletConnectorXRouter> WalletConnectorXRouterPtr;

typedef std::vector<WalletConnectorXRouterPtr> Connectors;
typedef std::map<std::vector<unsigned char>, WalletConnectorXRouterPtr> ConnectorsAddrMap;
typedef std::map<std::string, WalletConnectorXRouterPtr> ConnectorsCurrencyMap;

} // namespace xrouter

#endif // XROUTERDEF_H
