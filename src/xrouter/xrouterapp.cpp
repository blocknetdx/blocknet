//*****************************************************************************
//*****************************************************************************

#include "xrouterapp.h"
#include "util/xutil.h"
#include "net.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
//*****************************************************************************
//*****************************************************************************
class App::Impl
{
    friend class App;

protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    Impl();

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();

    /**
     * @brief stop stopped service, timer, secp stop
     * @return true
     */
    bool stop();

    /**
     * @brief onSend  send packet to xrouter network to specified id,
     *  or broadcast, when id is empty
     * @param id
     * @param message
     */
    void onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message);
};

//*****************************************************************************
//*****************************************************************************
App::Impl::Impl()
{
}

//*****************************************************************************
//*****************************************************************************
App::App()
    : m_p(new Impl)
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    stop();
}

//*****************************************************************************
//*****************************************************************************
// static
App& App::instance()
{
    static App app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
bool App::isEnabled()
{
    // enabled by default
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    return m_p->start();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::start()
{
    return true;
}


//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    return m_p->stop();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::stop()
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const XRouterPacketPtr & packet)
{
    static std::vector<unsigned char> addr(20, 0);
    m_p->onSend(addr, packet->body());
}

//*****************************************************************************
// send packet to xrouter network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void App::Impl::onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message)
{
    std::vector<unsigned char> msg(id);
    if (msg.size() != 20) {
        std::cerr << "bad send address " << __FUNCTION__;
        return;
    }

    // timestamp
    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = xrouter::util::timeToInt(timestamp);
    unsigned char* ptr = reinterpret_cast<unsigned char*>(&timestampValue);
    msg.insert(msg.end(), ptr, ptr + sizeof(uint64_t));

    // body
    msg.insert(msg.end(), message.begin(), message.end());

    uint256 hash = Hash(msg.begin(), msg.end());

    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        // TODO: Need a better way to determine which peer to send to; for now
        // just send to the first.
        pnode->PushMessage("xrouter", msg);
        break;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const std::vector<unsigned char> & id, const XRouterPacketPtr & packet)
{
    m_p->onSend(id, packet->body());
}

//*****************************************************************************
//*****************************************************************************
Error App::getBlocks(uint256& id)
{
    XRouterPacketPtr packet{new XRouterPacket{xrGetBlocks}};
    sendPacket(packet);
    return SUCCESS;
}
} // namespace xrouter
