//*****************************************************************************
//*****************************************************************************

#include "xrouterapp.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "script/standard.h"
#include "util/xutil.h"
#include "wallet.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

static const CAmount minBlock = 2;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
//*****************************************************************************
//*****************************************************************************
class App::Impl
{
    friend class App;

protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    Impl();

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();

    /**
     * @brief stop stopped service, timer, secp stop
     * @return true
     */
    bool stop();

    /**
     * @brief onSend  send packet to xrouter network to specified id,
     *  or broadcast, when id is empty
     * @param id
     * @param message
     */
    void onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message);
};

//*****************************************************************************
//*****************************************************************************
App::Impl::Impl()
{
}

//*****************************************************************************
//*****************************************************************************
App::App()
    : m_p(new Impl)
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    stop();
}

//*****************************************************************************
//*****************************************************************************
// static
App& App::instance()
{
    static App app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
bool App::isEnabled()
{
    // enabled by default
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    return m_p->start();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::start()
{
    return true;
}


//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    return m_p->stop();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::stop()
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const XRouterPacketPtr& packet)
{
    static std::vector<unsigned char> addr(20, 0);
    m_p->onSend(addr, packet->body());
}

//*****************************************************************************
// send packet to xrouter network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void App::Impl::onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message)
{
    std::vector<unsigned char> msg(id);
    if (msg.size() != 20) {
        std::cerr << "bad send address " << __FUNCTION__;
        return;
    }

    // timestamp
    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = xrouter::util::timeToInt(timestamp);
    unsigned char* ptr = reinterpret_cast<unsigned char*>(&timestampValue);
    msg.insert(msg.end(), ptr, ptr + sizeof(uint64_t));

    // body
    msg.insert(msg.end(), message.begin(), message.end());

    uint256 hash = Hash(msg.begin(), msg.end());

    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        // TODO: Need a better way to determine which peer to send to; for now
        // just send to the first.
        pnode->PushMessage("xrouter", msg);
        break;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const std::vector<unsigned char>& id, const XRouterPacketPtr& packet)
{
    m_p->onSend(id, packet->body());
}

//*****************************************************************************
//*****************************************************************************
static bool verifyBlockRequirement(const XRouterPacketPtr& packet)
{
    if (packet->size() < 36) {
        std::clog << "packet not big enough\n";
        return false;
    }

    uint256 txHash(packet->data());
    int offset = 32;
    uint32_t vout = *static_cast<uint32_t*>(static_cast<void*>(packet->data() + offset));

    CCoins coins;
    if (!pcoinsTip->GetCoins(txHash, coins)) {
        std::clog << "Could not find " << txHash.ToString() << "\n";
        return false;
    }

    if (vout > coins.vout.size()) {
        std::clog << "Invalid vout index " << vout << "\n";
        return false;
    }

    auto& txOut = coins.vout[vout];

    if (txOut.nValue < minBlock) {
        std::clog << "Insufficient BLOCK " << coins.vout[vout].nValue << "\n";
        return false;
    }

    CTxDestination destination;
    if (!ExtractDestination(txOut.scriptPubKey, destination)) {
        std::clog << "Unable to extract destination\n";
        return false;
    }

    auto txKeyID = boost::get<CKeyID>(&destination);
    if (!txKeyID) {
        std::clog << "destination must be a single address\n";
        return false;
    }

    CPubKey packetKey(packet->pubkey(),
        packet->pubkey() + XRouterPacket::pubkeySize);

    if (packetKey.GetID() != *txKeyID) {
        std::clog << "Public key provided doesn't match UTXO destination.\n";
        return false;
    }

    std::cout << "destination = " << txKeyID->ToString() << "\n";
    std::cout << "packet keyid = " << packetKey.GetID().ToString() << "\n";
    std::cout << "distnation.which() = " << destination.which() << "\n";
    std::cout << "txHash = " << txHash.ToString() << "\n";
    std::cout << "vout = " << vout << "\n";
    std::cout << "value = " << coins.vout[vout].nValue << "\n";
    return true;
}

//*****************************************************************************
//*****************************************************************************
static bool processGetBlocks(XRouterPacketPtr packet)
{
    std::cout << "Processing GetBlocks\n";
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(const std::vector<unsigned char>& id,
    const std::vector<unsigned char>& message,
    CValidationState& /*state*/)
{
    std::cerr << "Received xrouter packet\n";

    XRouterPacketPtr packet(new XRouterPacket);
    if (!packet->copyFrom(message)) {
        std::clog << "incorrect packet received " << __FUNCTION__;
        return;
    }

    if (!packet->verify()) {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return;
    }

    /* std::clog << "received message to " << util::base64_encode(std::string((char*)&id[0], 20)).c_str() */
    /*           << " command " << packet->command(); */

    switch (packet->command()) {
    case xrGetBlocks:
        processGetBlocks(packet);
        break;
    default:
        std::clog << "Unkown packet\n";
        break;
    }
}

//*****************************************************************************
//*****************************************************************************
static bool satisfyBlockRequirement(
    uint256& txHash,
    uint32_t& vout,
    CKey& key)
{
    if (!pwalletMain) {
        return false;
    }
    for (auto& addressCoins : pwalletMain->AvailableCoinsByAddress()) {
        for (auto& output : addressCoins.second) {
            if (output.Value() >= minBlock) {
                CKeyID keyID;
                if (!addressCoins.first.GetKeyID(keyID)) {
                    std::cerr << "GetKeyID failed\n";
                    continue;
                }
                if (!pwalletMain->GetKey(keyID, key)) {
                    std::cerr << "GetKey failed\n";
                    continue;
                }
                txHash = output.tx->GetHash();
                vout = output.i;
                return true;
            }
        }
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
Error App::getBlocks(uint256& id)
{
    XRouterPacketPtr packet{new XRouterPacket{xrGetBlocks}};

    uint256 txHash;
    uint32_t vout;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        std::cerr << "Minimum block requirement not satisfied\n";
        return UNKNOWN_ERROR;
    }
    std::cout << "txHash = " << txHash.ToString() << "\n";
    std::cout << "vout = " << vout << "\n";

    std::cout << "Sending xrGetBlock packet...\n";
    packet->append(txHash.begin(), 32);
    packet->append(vout);

    auto pubKey = key.GetPubKey();
    std::vector<unsigned char> pubKeyData{pubKey.begin(), pubKey.end()};

    auto privKey = key.GetPrivKey_256();
    std::vector<unsigned char> privKeyData{privKey.begin(), privKey.end()};

    packet->sign(pubKeyData, privKeyData);
    sendPacket(packet);
    return SUCCESS;
}
} // namespace xrouter
