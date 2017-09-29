//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESERVICESESSION_H
#define XBRIDGESERVICESESSION_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgewallet.h"
#include "FastDelegate.h"
#include "uint256.h"

#include <memory>
#include <set>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeServiceSession
        : public XBridgeSession
{
public:
    XBridgeServiceSession();
    XBridgeServiceSession(const WalletParam & wallet);
    virtual ~XBridgeServiceSession();

public:
    std::shared_ptr<XBridgeServiceSession> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeServiceSession>(XBridgeSession::shared_from_this());
    }

protected:
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const;
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

    virtual uint32_t lockTime(const char role) const;
    virtual xbridge::CTransactionPtr createTransaction() const;
    virtual xbridge::CTransactionPtr createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                       const std::vector<std::pair<CScript, double> > & outputs,
                                                       const uint32_t lockTime = 0) const;
};

typedef std::shared_ptr<XBridgeServiceSession> XBridgeSessionBtcPtr;

#endif // XBRIDGESERVICESESSION_H
