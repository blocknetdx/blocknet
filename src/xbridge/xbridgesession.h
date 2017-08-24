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
#include "script/script.h"

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
    XBridgeSession(const WalletParam & wallet);
    virtual ~XBridgeSession();

    const std::vector<unsigned char> & sessionAddr() const { return m_myid; }

    std::string currency() const  { return m_wallet.currency; }
    double      minAmount() const { return (double)m_wallet.minAmount / 100000; }

    void start(XBridge::SocketPtr socket);

    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);

    bool sendXBridgeMessage(XBridgePacketPtr packet);
    bool takeXBridgeMessage(const std::vector<unsigned char> & message);

    bool processPacket(XBridgePacketPtr packet);

public:
    // service functions
    void sendListOfWallets();
    void sendListOfTransactions();
    void checkFinishedTransactions();
    void eraseExpiredPendingTransactions();

    void resendAddressBook();
    void sendAddressbookEntry(const std::string & currency,
                              const std::string & name,
                              const std::string & address);

    void getAddressBook();
    void requestAddressBook();

    void checkUnconfirmedTx();
    void requestUnconfirmedTx();

    bool checkAmount(const uint64_t amount) const;
    double getAccountBalance() const;

    bool revertXBridgeTransaction(const uint256 & id);

private:
    virtual void init();

    void disconnect();

    void doReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset = 0);
    void onReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset,
                      const boost::system::error_code & error,
                      std::size_t transferred);

    void doReadBody(XBridgePacketPtr packet,
                    const std::size_t offset = 0);
    void onReadBody(XBridgePacketPtr packet,
                    const std::size_t offset,
                    const boost::system::error_code & error,
                    std::size_t transferred);

    // const unsigned char * myaddr() const;

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

protected:
    const unsigned char * myaddr() const;

    void sendPacket(const std::vector<unsigned char> & to, const XBridgePacketPtr & packet);
    void sendPacket(const std::string & to, const XBridgePacketPtr & packet);
    void sendPacketBroadcast(XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool checkPacketAddress(XBridgePacketPtr packet);

    virtual std::string currencyToLog() const { return std::string("[") + m_wallet.currency + std::string("]"); }

    bool makeNewPubKey(xbridge::CPubKey & newPKey) const;

    double minTxFee1(const uint32_t inputCount, const uint32_t outputCount);
    double minTxFee2(const uint32_t inputCount, const uint32_t outputCount);

    std::string round_x(const long double val, uint32_t prec);

    virtual uint32_t lockTime(const char role) const;
    virtual xbridge::CTransactionPtr createTransaction();
    virtual xbridge::CTransactionPtr createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                       const std::vector<std::pair<CScript, double> > & outputs,
                                                       const uint32_t lockTime = 0);
    virtual std::string createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                             const std::vector<std::pair<CScript, double> > & outputs,
                                             const uint32_t lockTime = 0);

    bool checkDepositTx(const XBridgeTransactionDescrPtr & xtx,
                        const std::string & depositTxId,
                        const uint32_t & confirmations,
                        const uint64_t & neededAmount,
                        bool & isGood);

protected:
    virtual bool processInvalid(XBridgePacketPtr packet);
    virtual bool processZero(XBridgePacketPtr packet);
    virtual bool processAnnounceAddresses(XBridgePacketPtr packet);
    virtual bool processXChatMessage(XBridgePacketPtr packet);

    virtual bool processTransaction(XBridgePacketPtr packet);
    virtual bool processPendingTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionAccepting(XBridgePacketPtr packet);

    virtual bool processBitcoinTransactionHash(XBridgePacketPtr packet);

    virtual bool processAddressBookEntry(XBridgePacketPtr packet);

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
    virtual bool processTransactionFinished(XBridgePacketPtr packet);
    virtual bool processTransactionRollback(XBridgePacketPtr packet);
    virtual bool processTransactionDropped(XBridgePacketPtr packet);

protected:
    std::vector<unsigned char> m_myid;

    XBridge::SocketPtr m_socket;

    typedef fastdelegate::FastDelegate1<XBridgePacketPtr, bool> PacketHandler;
    typedef std::map<const int, PacketHandler> PacketHandlersMap;
    PacketHandlersMap m_handlers;

protected:
    std::set<std::vector<unsigned char> > m_addressBook;

    WalletParam       m_wallet;
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
