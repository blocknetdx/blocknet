//******************************************************************************
//******************************************************************************
#ifndef XROUTERSERVER_H
#define XROUTERSERVER_H

#include <vector>
#include <string>
#include "xrouterconnector.h"
#include "xrouterconnectorbtc.h"
#include "xrouterconnectoreth.h"
#include "xrouterdef.h"

namespace xrouter
{

//*****************************************************************************
//*****************************************************************************
class XRouterServer
{
    friend class App;

    mutable boost::mutex m_connectorsLock;
    xrouter::Connectors m_connectors;
    xrouter::ConnectorsAddrMap m_connectorAddressMap;
    xrouter::ConnectorsCurrencyMap m_connectorCurrencyMap;

protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    XRouterServer() {}

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();

    /**
     * @brief onSend  send packet to xrouter network to specified id,
     *  or broadcast, when id is empty
     * @param message
     */
    void onSend(const std::vector<unsigned char>& message, CNode* pnode);
};

} // namespace

#endif // SERVER_H
