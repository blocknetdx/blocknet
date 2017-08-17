//*****************************************************************************
//*****************************************************************************

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgesessionbtc.h"
// #include "xbridgesessionethereum.h"
// #include "xbridgesessionrpccommon.h"
#include "xbridgeapp.h"
#include "util/logger.h"
#include "util/settings.h"

#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
XBridge::XBridge()
    : m_timerIoWork(new boost::asio::io_service::work(m_timerIo))
    , m_timerThread(boost::bind(&boost::asio::io_service::run, &m_timerIo))
    , m_timer(m_timerIo, boost::posix_time::seconds(TIMER_INTERVAL))
{
    try
    {
        // services and threas
        for (int i = 0; i < THREAD_COUNT; ++i)
        {
            IoServicePtr ios(new boost::asio::io_service);

            m_services.push_back(ios);
            m_works.push_back(WorkPtr(new boost::asio::io_service::work(*ios)));

            m_threads.create_thread(boost::bind(&boost::asio::io_service::run, ios));
        }

        m_timer.async_wait(boost::bind(&XBridge::onTimer, this));

        // sessions
        XBridgeApp & app = XBridgeApp::instance();
        {
            Settings & s = settings();
            std::vector<std::string> wallets = s.exchangeWallets();
            for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
            {
                WalletParam wp;
                wp.currency                    = *i;
                wp.title                       = s.get<std::string>(*i + ".Title");
                wp.address                     = s.get<std::string>(*i + ".Address");
                wp.ip                          = s.get<std::string>(*i + ".Ip");
                wp.port                        = s.get<std::string>(*i + ".Port");
                wp.user                        = s.get<std::string>(*i + ".Username");
                wp.passwd                      = s.get<std::string>(*i + ".Password");
                wp.addrPrefix[0]               = s.get<int>(*i + ".AddressPrefix", 0);
                wp.scriptPrefix[0]             = s.get<int>(*i + ".ScriptPrefix", 0);
                wp.secretPrefix[0]             = s.get<int>(*i + ".SecretPrefix", 0);
                wp.COIN                        = s.get<uint64_t>(*i + ".COIN", 0);
                wp.txVersion                   = s.get<uint32_t>(*i + ".TxVersion", 1);
                wp.minTxFee                    = s.get<uint64_t>(*i + ".MinTxFee", 0);
                wp.feePerByte                  = s.get<uint64_t>(*i + ".FeePerByte", 200);
                wp.minAmount                   = s.get<uint64_t>(*i + ".MinimumAmount", 0);
                wp.dustAmount                  = 3 * wp.minTxFee;
                wp.method                      = s.get<std::string>(*i + ".CreateTxMethod");
                wp.isGetNewPubKeySupported     = s.get<bool>(*i + ".GetNewKeySupported", false);
                wp.isImportWithNoScanSupported = s.get<bool>(*i + ".ImportWithNoScanSupported", false);
                wp.blockTime                   = s.get<int>(*i + ".BlockTime", 0);
                wp.requiredConfirmations       = s.get<int>(*i + ".Confirmations", 0);

                if (wp.ip.empty() || wp.port.empty() ||
                    wp.user.empty() || wp.passwd.empty() ||
                    wp.COIN == 0 || wp.blockTime == 0)
                {
                    LOG() << "read wallet " << *i << " with empty parameters>";
                    continue;
                }
                else
                {
                    LOG() << "read wallet " << *i << " [" << wp.title << "] " << wp.ip
                          << ":" << wp.port; // << " COIN=" << wp.COIN;
                }

                XBridgeSessionPtr session;
                if (wp.method == "ETHER")
                {
                    assert(!"not implemented");
                    // session.reset(new XBridgeSessionEthereum(wp));
                }
                else if (wp.method == "BTC")
                {
                    session.reset(new XBridgeSessionBtc(wp));
                }
                else if (wp.method == "RPC")
                {
                    assert(!"not implemented");
                    // session.reset(new XBridgeSessionRpc(wp));
                }
                else
                {
                    session.reset(new XBridgeSession(wp));
                }
                if (session)
                {
                    app.addSession(session);
                }
            }
        }
    }
    catch (std::exception & e)
    {
        ERR() << e.what();
        ERR() << __FUNCTION__;
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridge::run()
{
    m_threads.join_all();
}

//*****************************************************************************
//*****************************************************************************
void XBridge::stop()
{
    m_timer.cancel();
    m_timerIo.stop();
    m_timerIoWork.reset();
    m_timerThread.join();

//    for (IoServicePtr & i : m_services)
//    {
//        i->stop();
//    }
    for (WorkPtr & i : m_works)
    {
        i.reset();
    }

    m_threads.join_all();
}

//******************************************************************************
//******************************************************************************
void XBridge::onTimer()
{
    // DEBUG_TRACE();

    {
        m_services.push_back(m_services.front());
        m_services.pop_front();

        // XBridgeSessionPtr session(new XBridgeSession);
        XBridgeApp & app = XBridgeApp::instance();
        XBridgeSessionPtr session = app.serviceSession();

        IoServicePtr io = m_services.front();

        // call check expired transactions
        io->post(boost::bind(&XBridgeSession::checkFinishedTransactions, session));

        // send list of wallets (broadcast)
        // io->post(boost::bind(&XBridgeSession::sendListOfWallets, session));

        // send transactions list
        io->post(boost::bind(&XBridgeSession::sendListOfTransactions, session));

        // erase expired tx
        io->post(boost::bind(&XBridgeSession::eraseExpiredPendingTransactions, session));

        // check unconfirmed tx
        // io->post(boost::bind(&XBridgeSession::checkUnconfirmedTx, session));

        // resend addressbook
        // io->post(boost::bind(&XBridgeSession::resendAddressBook, session));
        io->post(boost::bind(&XBridgeSession::getAddressBook, session));

        // pending unprocessed packets
        {
            std::map<uint256, std::pair<std::string, XBridgePacketPtr> > map;
            {
                boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
                map = XBridgeApp::m_pendingPackets;
                XBridgeApp::m_pendingPackets.clear();
            }

            for (const std::pair<uint256, std::pair<std::string, XBridgePacketPtr> > & item : map)
            {
                std::string      currency = std::get<0>(item.second);
                XBridgeSessionPtr s = app.sessionByCurrency(currency);
                if (!s)
                {
                    // no session. packet dropped
                    WARN() << "no session for <" << currency << ">, packet dropped " << __FUNCTION__;
                    continue;
                }

                XBridgePacketPtr packet   = std::get<1>(item.second);
                io->post(boost::bind(&XBridgeSession::processPacket, s, packet));
            }
        }
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&XBridge::onTimer, this));
}
