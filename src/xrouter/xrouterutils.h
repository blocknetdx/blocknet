//******************************************************************************
//******************************************************************************
#ifndef XROUTERUTILS_H
#define XROUTERUTILS_H

#include <vector>
#include <string>
#include <cstdint>
#include "json/json_spirit.h"
#include "streams.h"
#include "wallet.h"
#include "xrouterpacket.h"
#include <boost/container/map.hpp>

using namespace json_spirit;

namespace xrouter
{
    
struct PaymentChannel
{
    CKey key;
    CKeyID keyid;
    std::string raw_tx;
    std::string txid;
    CScript redeemScript;
    int vout;
    CAmount value;
    CAmount deposit;
    std::string latest_tx;
    int deadline;
    bool isNull() { return deposit == 0; }
    void setNull() { deposit = 0; }
};

class UnknownChainAddress : public CBitcoinAddress {
public:
    UnknownChainAddress(std::string s) : CBitcoinAddress(s) { }
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
void unlockOutputs(std::string tx);
std::string signTransaction(std::string& raw_tx);
bool sendTransactionBlockchain(std::string raw_tx, std::string & txid);
bool sendTransactionBlockchain(std::string address, CAmount amount, std::string & raw_tx);
CMutableTransaction decodeTransaction(std::string tx);


// Payment channels
PaymentChannel createPaymentChannel(CPubKey address, CAmount deposit, int date);
bool createAndSignChannelTransaction(PaymentChannel channel, std::string address, CAmount deposit, CAmount amount, std::string & raw_tx);
bool verifyChannelTransaction(std::string transaction);
bool finalizeChannelTransaction(PaymentChannel channel, CKey snodekey, std::string latest_tx, std::string & raw_tx);
std::string createRefundTransaction(PaymentChannel channel);
double getTxValue(std::string rawtx, std::string address, std::string type="address");
int getChannelExpiryTime(std::string rawtx);


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

} // namespace

#endif // XROUTERUTILS_H
