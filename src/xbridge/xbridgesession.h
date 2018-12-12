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
    bool processPacket(XBridgePacketPtr packet, CValidationState * state = nullptr);

public:
    // service functions
    void sendListOfTransactions() const;
    void checkFinishedTransactions() const;
    void getAddressBook() const;

    /**
     * Redeems the specified order's deposit.
     * @param xtx
     * @param errCode
     */
    bool redeemOrderDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const;

    /**
     * Redeems the counterparty's deposit.
     * @param xtx
     * @param errCode
     */
    bool redeemOrderCounterpartyDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const;

    /**
     * Submits a trader's refund transaction on their behalf.
     * @param orderId
     * @param currency
     * @param lockTime
     * @param refTx
     * @param errCode
     * @return
     */
    bool refundTraderDeposit(const std::string & orderId, const std::string & currency, const uint32_t & lockTime,
                             const std::string & refTx, int32_t & errCode) const;

private:
    void setWorking() { m_isWorking = true; }
    void setNotWorking() { m_isWorking = false; }

private:
    std::unique_ptr<Impl> m_p;
    bool m_isWorking;
};

} // namespace xbridge

#endif // XBRIDGESESSION_H
