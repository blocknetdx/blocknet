//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgetransactiondescr.h"
#include "xbridgewallet.h"
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
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class Session
        : public std::enable_shared_from_this<Session>
        , private boost::noncopyable
{
    class Impl;

public:
    Session();
    ~Session();

public:
    // helper functions
    /**
     * @brief sessionAddr
     * @return current session address
     */
    const std::vector<unsigned char> & sessionAddr() const;

public:
    // network
    /**
     * @brief checkXBridgePacketVersion Compares the package version to the version of the Protocol
     * @param packet -
     * @return true, if the versions are equal
     */
    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);
    /**
     * @brief processPacket - processes the packet in accordance with the received command
     * @param packet
     * @return true, if packet are valid and processing successful
     */
    bool processPacket(XBridgePacketPtr packet);

public:
    // service functions
    /**
     * @brief sendListOfTransactions - sends to the network a list of pending transactions
     */
    void sendListOfTransactions();
    /**
     * @brief checkFinishedTransactions checks the status of the transaction
     * if the transaction is completed/cancelled/expired - remove it from the list and notify the network
     */
    void checkFinishedTransactions();
    /**
     * @brief eraseExpiredPendingTransactions - removed expired transactions from pending
     */
    void eraseExpiredPendingTransactions();
    /**
     * @brief getAddressBook - gets the list of addresses
     */
    void getAddressBook();

private:
    std::unique_ptr<Impl> m_p;
};

} // namespace xbridge

#endif // XBRIDGESESSION_H
