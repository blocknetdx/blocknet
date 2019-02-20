//******************************************************************************
//******************************************************************************

#ifndef XROUTERPEER_H
#define XROUTERPEER_H

#include "xroutersettings.h"
#include "net.h"
#include "servicenode.h"
#include <chrono>
#include <boost/container/map.hpp>

//******************************************************************************
//******************************************************************************
namespace xrouter
{
    class XRouterPeer {
    public:
        CNode* node;
        CServicenode serviceNode;
        XRouterSettings config;
        double score;
        std::string domain;
        std::chrono::time_point<std::chrono::system_clock> lastConfigQuery;
        std::chrono::time_point<std::chrono::system_clock> lastConfigUpdate;
        PaymentChannel paymentChannel;
        std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > lastPacketsSent;
        XRouterPeer() { }
    };
} // namespace xrouter

#endif // XROUTERPEER_H
