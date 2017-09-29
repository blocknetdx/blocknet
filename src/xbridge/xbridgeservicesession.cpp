//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgeservicesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xuiconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"
// #include "dht/dht.h"
#include "bitcoinrpcconnector.h"
#include "xbitcointransaction.h"
#include "base58.h"

//*****************************************************************************
//*****************************************************************************
XBridgeServiceSession::XBridgeServiceSession()
    : XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeServiceSession::XBridgeServiceSession(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeServiceSession::~XBridgeServiceSession()
{

}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeServiceSession::fromXAddr(const std::vector<unsigned char> & /*xaddr*/) const
{
    assert(!"not implemented in service session");
    return std::string();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> XBridgeServiceSession::toXAddr(const std::string & /*addr*/) const
{
    assert(!"not implemented in service session");
    return std::vector<unsigned char>();
}

//******************************************************************************
//******************************************************************************
uint32_t XBridgeServiceSession::lockTime(const char /*role*/) const
{
    assert(!"not implemented in service session");
    return 0;
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeServiceSession::createTransaction() const
{
    assert(!"not implemented in service session");
    return xbridge::CTransactionPtr();
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeServiceSession::createTransaction(const std::vector<std::pair<std::string, int> > & /*inputs*/,
                                                              const std::vector<std::pair<CScript, double> > & /*outputs*/,
                                                              const uint32_t /*lockTime*/) const
{
    assert(!"not implemented in service session");
    return xbridge::CTransactionPtr();
}
