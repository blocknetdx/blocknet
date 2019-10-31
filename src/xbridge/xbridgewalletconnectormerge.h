#ifndef XBRIDGEWALLETCONNECTORMERGE_H
#define XBRIDGEWALLETCONNECTORMERGE_H

#include "xbridgewalletconnectorbtc.h"
#include "xbridgecryptoproviderbtc.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class MergeWalletConnector : public BtcWalletConnector<BtcCryptoProvider>

{
public:
    MergeWalletConnector();

    bool init();
};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORMERGE_H
