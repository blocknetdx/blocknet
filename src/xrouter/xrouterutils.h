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

static const std::string xr = "xr";

class UnknownChainAddress : public CBitcoinAddress {
public:
    explicit UnknownChainAddress(std::string & s) : CBitcoinAddress(s) { }
    bool IsValid() const { return vchData.size() == 20; }
    bool GetKeyID(CKeyID& keyID) const {
        uint160 id;
        memcpy(&id, &vchData[0], 20);
        keyID = CKeyID(id);
        return true;
    }
};


// Network and RPC interface
std::string CallCMD(std::string cmd);
std::string CallURL(std::string ip, std::string port, std::string url);
Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);


// Payment functions
bool createAndSignTransaction(std::string address, CAmount amount, std::string & raw_tx);
bool createAndSignTransaction(boost::container::map<std::string, CAmount> addrs, string & raw_tx);
bool createAndSignTransaction(Array txparams, std::string & raw_tx);
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
Object form_reply(const std::string & uuid, const std::string & reply);

} // namespace

#endif // XROUTERUTILS_H
