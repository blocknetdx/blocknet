//******************************************************************************
//******************************************************************************

#ifndef XROUTERDEF_H
#define XROUTERDEF_H

#include <vector>
#include <map>
#include <queue>
#include <memory>

#define MIN_BLOCK 200
#define XROUTER_DEFAULT_TIMEOUT 20000
#define XROUTER_DOMAIN_REGISTRATION_DEPOSIT 1.0
#define XROUTER_DEFAULT_CONFIRMATIONS 1

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
