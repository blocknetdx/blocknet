//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgetransactiondescr.h"
#include "xbridgewallet.h"
#include "uint256.h"
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
    /**
     * @brief Session - default constructor, init PIMPL
     */
    Session();

    ~Session();

    bool isWorking() const { return m_isWorking; }

public:
    // helper functions
    /**
     * @brief sessionAddr
     * @return session id (address)
     */
    const std::vector<unsigned char> & sessionAddr() const;

public:
    // network
    /**
     * @brief checkXBridgePacketVersion - equal packet version with current xbridge protocol version
     * @param message - data
     * @return true, packet version == current xbridge protocol version
     */
    static bool checkXBridgePacketVersion(const std::vector<unsigned char> & message);
    /**
     * @brief checkXBridgePacketVersion - equal packet version with current xbridge protocol version
     * @param packet - data
     * @return true, packet version == current xbridge protocol version
     */
    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);
    /**
     * @brief processPacket - decrypt packet, execute packet command
     * @param packet
     * @return true, if packet decrypted and packet command executed
     */
    bool processPacket(XBridgePacketPtr packet);

public:
    // service functions
    void sendListOfTransactions() const;
    void checkFinishedTransactions() const;
    void eraseExpiredPendingTransactions() const;
    void getAddressBook() const;

private:
    void setWorking() { m_isWorking = true; }
    void setNotWorking() { m_isWorking = false; }

private:
    std::unique_ptr<Impl> m_p;
    bool m_isWorking;
};

} // namespace xbridge

#endif // XBRIDGESESSION_H
