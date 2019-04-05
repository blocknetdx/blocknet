//******************************************************************************
//******************************************************************************
#ifndef XROUTERUTILS_H
#define XROUTERUTILS_H

#include "xrouterpacket.h"
#include "xroutererror.h"

#include "wallet.h"
#include "streams.h"

#include "json/json_spirit.h"

#include <vector>
#include <string>
#include <cstdint>
#include <boost/container/map.hpp>

using namespace json_spirit;

namespace xrouter
{

// Type definitions
typedef std::string NodeAddr;

static const std::string xr = "xr"; // XRouter SPV
static const std::string xrs = "xrs"; // XRouter services
static const std::string xrdelimiter = "::"; // XRouter namespace delimiter

/**
 * Helper to build key for use with lookups.
 * @param wallet
 * @param command
 * @return
 */
extern std::string walletCommandKey(const std::string & wallet, const std::string & command);
/**
 * Helper to build key for use with lookups.
 * @param wallet
 * @return
 */
extern std::string walletCommandKey(const std::string & wallet);
/**
 * Remove the top-level namespace from the service name. Returns true if successful, otherwise false.
 * @param service
 * @param result Stripped service name stored here.
 * @return
 */
extern bool removeNamespace(const std::string & service, std::string & result);

/**
 * Returns true if the specified service has the wallet namespace (xr::).
 * @return
 */
extern bool hasWalletNamespace(const std::string & service);

/**
 * Helper to build service key for use with lookups.
 * @param service
 * @return
 */
extern std::string pluginCommandKey(const std::string & service);

/**
 * Returns true if the specified service has the plugin namespace (xrs::).
 * @return
 */
extern bool hasPluginNamespace(const std::string & service);
/**
 * Returns true if successfully parsed service name from fully qualified service name.
 * @return
 */
extern bool commandFromNamespace(const std::string & fqService, std::string & command);
/**
 * Returns list of namespace parts.
 * @param s fully qualified service name
 * @param del Delimited (e.g. ::)
 * @param vout Return list of parts in order
 * @return
 */
extern bool xrsplit(const std::string & fqService, const std::string & del, std::vector<std::string> & vout);

class UnknownChainAddress : public CBitcoinAddress {
public:
    explicit UnknownChainAddress(const std::string & s) : CBitcoinAddress(s) { }
    bool IsValid() const { return vchData.size() == 20; }
    bool GetKeyID(CKeyID& keyID) const {
        uint160 id;
        memcpy(&id, &vchData[0], 20);
        keyID = CKeyID(id);
        return true;
    }
};


// Network and RPC interface
std::string CallCMD(const std::string & cmd, int & exit);
std::string CallURL(const std::string & ip, const std::string & port, const std::string & url);
std::string CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);


// Payment functions
bool createAndSignTransaction(const std::string & address, const CAmount & amount, std::string & raw_tx);
void unlockOutputs(const std::string & tx);
std::string signTransaction(std::string& raw_tx);
bool sendTransactionBlockchain(std::string raw_tx, std::string & txid);
bool sendTransactionBlockchain(std::string address, CAmount amount, std::string & raw_tx);
CMutableTransaction decodeTransaction(std::string tx);
double checkPayment(const std::string & rawtx, const std::string & address);


// Domains
std::string generateDomainRegistrationTx(std::string domain, std::string addr);
bool verifyDomain(std::string tx, std::string domain, std::string addr, int& blockNumber);


// Signing & Verification of packets
bool satisfyBlockRequirement(uint256& txHash, uint32_t& vout, CKey& key);
bool verifyBlockRequirement(const XRouterPacketPtr& packet);

// Miscellaneous functions
CAmount to_amount(double val);
bool is_number(std::string s);
bool is_hash(std::string s);
bool is_address(std::string s);
std::string generateUUID();
Object form_reply(const std::string & uuid, const Value & reply);
Object form_reply(const std::string & uuid, const std::string & reply);

} // namespace

#endif // XROUTERUTILS_H
