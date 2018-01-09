//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "uint256.h"
#include "xbridgetransactiondescr.h"
#include "util/xbridgeerror.h"
#include "xbridgewalletconnector.h"
#include "xbridgedef.h"

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

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class App
{
    class Impl;

private:
    App();
    virtual ~App();

public:
    static App & instance();

    static std::string version();

    static bool isEnabled();

    bool init(int argc, char *argv[]);

    bool start();
    bool stop();

public:
    // transactions

    TransactionDescrPtr transaction(const uint256 & id) const;
    std::map<uint256, xbridge::TransactionDescrPtr> transactions() const;
    std::map<uint256, xbridge::TransactionDescrPtr> history() const;

    void appendTransaction(const TransactionDescrPtr & ptr);

    void moveTransactionToHistory(const uint256 & id);

    Error sendXBridgeTransaction(const std::string & from,
                                 const std::string & fromCurrency,
                                 const uint64_t & fromAmount,
                                 const std::string & to,
                                 const std::string & toCurrency,
                                 const uint64_t & toAmount,
                                 uint256 & id);
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);

    Error acceptXBridgeTransaction(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to);
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr);

    xbridge::Error cancelXBridgeTransaction(const uint256 &id, const TxCancelReason &reason);
    bool sendCancelTransaction(const uint256 &txid, const TxCancelReason &reason);

    xbridge::Error rollbackXBridgeTransaction(const uint256 &id);
    bool sendRollbackTransaction(const uint256 &txid);

    /**
     * @brief isValidAddress checks the correctness of the address
     * @param address checked address
     * @return true, if address valid
     */
    bool isValidAddress(const std::string &address) const;

    /**
     * @brief checkAcceptParams checks the correctness of the parameters
     * @param id - id accepted transaction
     * @param ptr - smart pointer to accepted transaction
     * @return xbridge::SUCCESS, if all parameters valid
     */
    xbridge::Error checkAcceptParams(const uint256 &id, TransactionDescrPtr &ptr);

    /**
     * @brief checkCreateParams - checks parameter needs to success created transaction
     * @param fromCurrency - from currency
     * @param toCurrency - to currency
     * @param fromAmount -  amount
     * @return xbridge::SUCCES, if all parameters valid
     */
    xbridge::Error checkCreateParams(const std::string &fromCurrency, const std::string &toCurrency, const uint64_t &fromAmount);

    /**
     * @brief checkAmount - checks wallet balance
     * @param currency - currency name
     * @param amount - amount
     * @return xbridge::SUCCES, if  the session currency is open and
     * on account has sufficient funds for operations
     */
    xbridge::Error checkAmount(const std::string &currency, const uint64_t &amount);
public:
    // connectors

    std::vector<std::string> availableCurrencies() const;
    bool hasCurrency(const std::string & currency) const;

    void addConnector(const WalletConnectorPtr & conn);
    void updateConnector(const WalletConnectorPtr & conn,
                         const std::vector<unsigned char> addr,
                         const std::string & currency);
    WalletConnectorPtr connectorByCurrency(const std::string & currency) const;
    std::vector<WalletConnectorPtr> connectors() const;

public:
    // network

    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToKnown(const std::vector<unsigned char> & message);

    // send messave via xbridge
    void sendPacket(const XBridgePacketPtr & packet);
    void sendPacket(const std::vector<unsigned char> & id, const XBridgePacketPtr & packet);

    // call when message from xbridge network received
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);
    // broadcast message
    void onBroadcastReceived(const std::vector<unsigned char> & message,
                             CValidationState & state);

    bool processLater(const uint256 & txid, const XBridgePacketPtr & packet);
    bool removePackets(const uint256 & txid);

private:
    std::unique_ptr<Impl> m_p;
};

} // namespace xbridge

#endif // XBRIDGEAPP_H
