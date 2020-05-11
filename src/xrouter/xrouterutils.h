// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERUTILS_H
#define BLOCKNET_XROUTER_XROUTERUTILS_H

#include <xrouter/xrouterpacket.h>
#include <xrouter/xroutererror.h>

#include <streams.h>

#include <vector>
#include <string>
#include <cstdint>

#include <json/json_spirit.h>
#include <univalue.h>

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
 * @param withNamespace Optionally include the namespace. Defaults to false.
 * @return
 */
std::string walletCommandKey(const std::string & wallet, const std::string & command, const bool & withNamespace = false);
/**
 * Helper to build key for use with lookups.
 * @param wallet
 * @return
 */
std::string walletCommandKey(const std::string & wallet);
/**
 * Helper to transform a fully qualified service (e.g. xrs::CustomPlugin) to a url (e.g. xrs/CustomPlugin).
 * @param fqservice
 * @return
 */
std::string fqServiceToUrl(std::string fqservice);
/**
 * Remove the top-level namespace from the service name. Returns true if successful, otherwise false.
 * @param service
 * @param result Stripped service name stored here.
 * @return
 */
bool removeNamespace(const std::string & service, std::string & result);

/**
 * Returns true if the specified service has the wallet namespace (xr::).
 * @return
 */
bool hasWalletNamespace(const std::string & service);

/**
 * Helper to build service key for use with lookups.
 * @param service
 * @return
 */
std::string pluginCommandKey(const std::string & service);

/**
 * Returns true if the specified service has the plugin namespace (xrs::).
 * @return
 */
bool hasPluginNamespace(const std::string & service);
/**
 * Returns true if successfully parsed service name from fully qualified service name.
 * @return
 */
bool commandFromNamespace(const std::string & fqService, std::string & command);
/**
 * Returns list of namespace parts.
 * @param s fully qualified service name
 * @param del Delimited (e.g. ::)
 * @param vout Return list of parts in order
 * @return
 */
bool xrsplit(const std::string & fqService, const std::string & del, std::vector<std::string> & vout);

// XRouter client request
struct XRouterReply {
    int status;
    int error;
    CPubKey hdrpubkey;
    std::vector<unsigned char> hdrsignature;
    std::string result;
};
XRouterReply CallXRouterUrl(const std::string & host, const int & port, const std::string & url, const std::string & data,
                            const int & timeout, const CKey & signingkey, const CPubKey & serverkey,
                            const std::string & paymentrawtx);
XRouterReply CallXRouterUrlSSL(const std::string & host, const int & port, const std::string & url, const std::string & data,
                            const int & timeout, const CKey & signingkey, const CPubKey & serverkey,
                            const std::string & paymentrawtx);
// Network and RPC interface
std::string CallCMD(const std::string & cmd, int & exit);
std::string CallRPC(const std::string & rpcip, const std::string & rpcport,
                           const std::string & strMethod, const Array & params,
                           const std::string & jsonver="", const std::string & contenttype="");
std::string CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
                           const std::string & rpcip, const std::string & rpcport,
                           const std::string & strMethod, const Array & params,
                           const std::string & jsonver="", const std::string & contenttype="");

// Payment functions
bool createAndSignTransaction(const std::string & address, const CAmount & amount, std::string & raw_tx);
void unlockOutputs(const std::string & tx);
bool sendTransactionBlockchain(const std::string & rawtx, std::string & txid);
CMutableTransaction decodeTransaction(const std::string & tx);
double checkPayment(const std::string & rawtx, const std::string & address, const CAmount & expectedFee);

// Miscellaneous functions
CAmount to_amount(double val);
bool is_number(const std::string & s);
bool is_hash(const std::string & hash);
bool is_hex(const std::string & hex);
bool hextodec(const std::string & hex, unsigned int & n);
std::string generateUUID();
Object form_reply(const std::string & uuid, const Value & reply);
Object form_reply(const std::string & uuid, const std::string & reply);
UniValue form_reply(const std::string & uuid, const UniValue & reply);

} // namespace

#endif // BLOCKNET_XROUTER_XROUTERUTILS_H
