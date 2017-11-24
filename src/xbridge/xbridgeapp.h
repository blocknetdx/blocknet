//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "uint256.h"
#include "xbridgetransactiondescr.h"
#include "util/xbridgeerror.h"
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <queue>

#ifdef WIN32
// #include <Ws2tcpip.h>
#endif

namespace rpc
{
class AcceptedConnection;
}

//*****************************************************************************
//*****************************************************************************
class XBridgeApp
{
    typedef std::vector<unsigned char> UcharVector;
    typedef std::shared_ptr<boost::asio::io_service>       IoServicePtr;
    typedef std::shared_ptr<boost::asio::io_service::work> WorkPtr;

    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

private:
    XBridgeApp();
    virtual ~XBridgeApp();

public:
    static XBridgeApp & instance();

    static std::string version();

    static bool isEnabled();

    bool init(int argc, char *argv[]);
    bool start();

    xbridge::Error sendXBridgeTransaction(const std::string &from,
                                          const std::string &fromCurrency,
                                          const uint64_t &fromAmount,
                                          const std::string &to,
                                          const std::string &toCurrency,
                                          const uint64_t &toAmount,
                                          uint256 &id);

    bool sendPendingTransaction(XBridgeTransactionDescrPtr &ptr);

    xbridge::Error acceptXBridgeTransaction(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to, uint256 &result);
    bool sendAcceptingTransaction(XBridgeTransactionDescrPtr & ptr);

    xbridge::Error cancelXBridgeTransaction(const uint256 & id, const TxCancelReason & reason);
    bool sendCancelTransaction(const uint256 & txid, const TxCancelReason & reason);

    xbridge::Error rollbackXBridgeTransaction(const uint256 & id);
    bool sendRollbackTransaction(const uint256 & txid);

public:
    bool stop();

    XBridgeSessionPtr sessionByCurrency(const std::string & currency) const;
    std::vector<std::string> sessionsCurrencies() const;

    // store session
    void addSession(XBridgeSessionPtr session);
    // store session addresses in local table
    void storageStore(XBridgeSessionPtr session, const std::vector<unsigned char> & id);

    bool isLocalAddress(const std::vector<unsigned char> & id);
    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToKnown(const std::vector<unsigned char> & message);

    XBridgeSessionPtr serviceSession();

    void storeAddressBookEntry(const std::string & currency,
                               const std::string & name,
                               const std::string & address);
    void getAddressBook();

public:// slots:
    // send messave via xbridge
    void onSend(const XBridgePacketPtr & packet);
    void onSend(const UcharVector & id, const XBridgePacketPtr & packet);

    // call when message from xbridge network received
    void onMessageReceived(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);
    // broadcast message
    void onBroadcastReceived(const std::vector<unsigned char> & message);

private:
    void onSend(const UcharVector & id, const UcharVector & message);

public:
    static void sleep(const unsigned int umilliseconds);

private:
    boost::thread_group m_threads;

    XBridgePtr        m_bridge;

    mutable boost::mutex m_sessionsLock;
    typedef std::map<std::vector<unsigned char>, XBridgeSessionPtr> SessionAddrMap;
    SessionAddrMap m_sessionAddrs;
    typedef std::map<std::string, XBridgeSessionPtr> SessionIdMap;
    SessionIdMap m_sessionIds;
    typedef std::queue<XBridgeSessionPtr> SessionQueue;
    SessionQueue m_sessionQueue;

    // service session
    XBridgeSessionPtr m_serviceSession;

    boost::mutex m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages m_processedMessages;

    boost::mutex m_addressBookLock;
    typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
    typedef std::vector<AddressBookEntry> AddressBook;
    AddressBook m_addressBook;
    std::set<std::string> m_addresses;

public:
    static boost::mutex                                  m_txLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_pendingTransactions;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_transactions;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_historicTransactions;

    static boost::mutex                                  m_txUnconfirmedLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_unconfirmed;

    static boost::mutex                                  m_ppLocker;
    static std::map<uint256, std::pair<std::string, XBridgePacketPtr> > m_pendingPackets;

  private:
    /**
     * @brief m_historicTransactionsStates - the status list of historical transactions
     */
    std::list<XBridgeTransactionDescr::State>       m_historicTransactionsStates;

    /**
     * @brief m_lastErrorLock - mutex for locking only m_lastError
     */
    boost::mutex m_lastErrorLock;

    /**
     * @brief m_services
     */
    std::deque<IoServicePtr> m_services;

    /**
     * @brief m_works
     */
    std::deque<WorkPtr> m_works;

    /**
     * @brief m_timerIo
     */
    boost::asio::io_service m_timerIo;

    /**
     * @brief m_timerIoWork - update historical transactions list timer worker
     */
    std::shared_ptr<boost::asio::io_service::work>  m_timerIoWork;

    /**
     * @brief m_timerThread - timer thread
     */
    boost::thread m_timerThread;

    /**
     * @brief m_timer timer update historical transactions list
     */
    boost::asio::deadline_timer m_timer;

public:
    /**
     * @brief isHistoricState - checks the state of the transaction
     * @param state - current state of transaction
     * @return true, if the transaction is historical
     */
    bool isHistoricState(const XBridgeTransactionDescr::State state);
};

#endif // XBRIDGEAPP_H
