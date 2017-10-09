//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSIONBCC_H
#define XBRIDGESESSIONBCC_H

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

enum
{
    SIGHASH_FORKID = 0x40,
};

//*****************************************************************************
//*****************************************************************************
class XBridgeSessionBcc
        : public XBridgeSession
{
public:
    XBridgeSessionBcc();
    XBridgeSessionBcc(const WalletParam & wallet);
    virtual ~XBridgeSessionBcc();

public:
    std::shared_ptr<XBridgeSessionBcc> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeSessionBcc>(XBridgeSession::shared_from_this());
    }

protected:
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

    virtual uint32_t lockTime(const char role) const;
    virtual xbridge::CTransactionPtr createTransaction() const;
    virtual xbridge::CTransactionPtr createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                       const std::vector<std::pair<CScript, double> > & outputs,
                                                       const uint32_t lockTime = 0) const;

    virtual bool signTransaction(const xbridge::CKey & key,
                                 const xbridge::CTransactionPtr & transaction,
                                 const uint32_t inputIdx,
                                 const CScript & unlockScript,
                                 std::vector<unsigned char> & signature);
};

typedef std::shared_ptr<XBridgeSessionBcc> XBridgeSessionBccPtr;

#endif // XBRIDGESESSIONBCC_H
