//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridge.h"
#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgetransactiondescr.h"
#include "xbridgewallet.h"
#include "FastDelegate.h"
#include "uint256.h"
#include "xkey.h"
#include "xbitcointransaction.h"
#include "bitcoinrpcconnector.h"
#include "script/script.h"
#include "xbridgewalletconnector.h"

#include <memory>
#include <set>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

extern const unsigned int LOCKTIME_THRESHOLD;

//*****************************************************************************
//*****************************************************************************
class XBridgeSession
        : public std::enable_shared_from_this<XBridgeSession>
        , private boost::noncopyable
{
public:
    XBridgeSession();
    virtual ~XBridgeSession();

    const std::vector<unsigned char> & sessionAddr() const { return m_myid; }

    std::string currency() const  { return m_wallet.currency; }

    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);

    bool processPacket(XBridgePacketPtr packet);

public:
    // service functions
    void sendListOfTransactions();
    void checkFinishedTransactions();
    void eraseExpiredPendingTransactions();

    void getAddressBook();

    bool rollbacktXBridgeTransaction(const uint256 & id);

private:
    virtual void init();

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

protected:
    void sendPacket(const std::vector<unsigned char> & to, const XBridgePacketPtr & packet);
    void sendPacketBroadcast(XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool checkPacketAddress(XBridgePacketPtr packet);

    bool makeNewPubKey(xbridge::CPubKey & newPKey) const;

    // std::string round_x(const long double val, uint32_t prec);

    bool checkDepositTx(const XBridgeTransactionDescrPtr & xtx,
                        const std::string & depositTxId,
                        const uint32_t & confirmations,
                        const uint64_t & neededAmount,
                        bool & isGood);

    // fn search xaddress in transaction and restore full 'coin' address as string
    bool isAddressInTransaction(const std::vector<unsigned char> & address,
                                const XBridgeTransactionPtr & tx,
                                std::string & fullAddress);

protected:
    virtual bool processInvalid(XBridgePacketPtr packet);
    virtual bool processZero(XBridgePacketPtr packet);
    virtual bool processXChatMessage(XBridgePacketPtr packet);

    virtual bool processTransaction(XBridgePacketPtr packet);
    virtual bool processPendingTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionAccepting(XBridgePacketPtr packet);

    virtual bool processTransactionHold(XBridgePacketPtr packet);
    virtual bool processTransactionHoldApply(XBridgePacketPtr packet);

    virtual bool processTransactionInit(XBridgePacketPtr packet);
    virtual bool processTransactionInitialized(XBridgePacketPtr packet);

    virtual bool processTransactionCreate(XBridgePacketPtr packet);
    virtual bool processTransactionCreatedA(XBridgePacketPtr packet);
    virtual bool processTransactionCreatedB(XBridgePacketPtr packet);

    virtual bool processTransactionConfirmA(XBridgePacketPtr packet);
    virtual bool processTransactionConfirmedA(XBridgePacketPtr packet);

    virtual bool processTransactionConfirmB(XBridgePacketPtr packet);
    virtual bool processTransactionConfirmedB(XBridgePacketPtr packet);

    virtual bool finishTransaction(XBridgeTransactionPtr tr);
    virtual bool sendCancelTransaction(const uint256 & txid,
                                       const TxCancelReason & reason);
    virtual bool sendCancelTransaction(const XBridgeTransactionDescrPtr & tx,
                                       const TxCancelReason & reason);
    virtual bool rollbackTransaction(XBridgeTransactionPtr tr);

    virtual bool processTransactionCancel(XBridgePacketPtr packet);
    bool cancelOrRollbackTransaction(const uint256 & txid, const TxCancelReason & reason);

    virtual bool processTransactionFinished(XBridgePacketPtr packet);
    virtual bool processTransactionRollback(XBridgePacketPtr packet);

protected:
    std::vector<unsigned char> m_myid;

    typedef fastdelegate::FastDelegate1<XBridgePacketPtr, bool> PacketHandler;
    typedef std::map<const int, PacketHandler> PacketHandlersMap;
    PacketHandlersMap m_handlers;
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
