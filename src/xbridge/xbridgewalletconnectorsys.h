//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEWALLETCONNECTORSYS_H
#define XBRIDGEWALLETCONNECTORSYS_H

#include "xbridgewalletconnectorbtc.h"
#include "xbridgecryptoproviderbtc.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class SysWalletConnector : public BtcWalletConnector<BtcCryptoProvider>
{
public:
    SysWalletConnector();

public:
    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs) const;

};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORSYS_H
