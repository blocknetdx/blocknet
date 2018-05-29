//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEWALLETCONNECTORSYS_H
#define XBRIDGEWALLETCONNECTORSYS_H

#include "xbridgewalletconnectorbtc.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class SysWalletConnector : public BtcWalletConnector
{
public:
    SysWalletConnector();

public:
    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const bool withoutDust = true) const;

};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORSYS_H
