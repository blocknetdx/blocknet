//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"

#include "rpcprotocol.h"

#include <regex>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

#ifdef _WIN32
#include <objbase.h>

namespace xrouter {
std::string generateUUID()
{
    GUID guid;
	CoCreateGuid(&guid);
    char guid_string[37];
    sprintf(guid_string,
          "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
          guid.Data1, guid.Data2, guid.Data3,
          guid.Data4[0], guid.Data4[1], guid.Data4[2],
          guid.Data4[3], guid.Data4[4], guid.Data4[5],
          guid.Data4[6], guid.Data4[7]);
    return guid_string;
}
}
    
#else

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace xrouter {
std::string generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
}

#endif 

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

std::string walletCommandKey(const std::string & wallet, const std::string & command) {
    return wallet + "::" + command;
}
std::string walletCommandKey(const std::string & wallet) {
    return xr + "::" + wallet;
}
bool removeWalletNamespace(const std::string & wallet, std::string & result) {
    const std::string search{xr + "::"};
    auto s = wallet.find(search);
    if (s == std::string::npos) {
        result = wallet;
        return false;
    }
    result = std::string{wallet}.erase(s, search.size());
    return true;
}
bool hasWalletNamespace(const std::string & service) {
    const std::string s{xr + "::"};
    std::regex r("^"+s+"[a-zA-Z0-9\\-:\\$]+$");
    std::smatch m;
    return std::regex_match(service, m, r);
}
std::string pluginCommandKey(const std::string & service) {
    return xrs + "::" + service;
}
bool removePluginNamespace(const std::string & service, std::string & result) {
    const std::string search{xrs + "::"};
    auto s = service.find(search);
    if (s == std::string::npos) {
        result = service;
        return false;
    }
    result = std::string{service}.erase(s, search.size());
    return true;
}
bool hasPluginNamespace(const std::string & service) {
    const std::string s{xrs + "::"};
    std::regex r("^"+s+"[a-zA-Z0-9\\-:\\$]+$");
    std::smatch m;
    return std::regex_match(service, m, r);
}
bool commandFromNamespace(const std::string & fqService, std::string & command) {
    std::regex r(".*?::([a-zA-Z0-9\\-:\\$]+)$");
    std::smatch m;
    std::regex_search(fqService, m, r);
    if (m.size() > 1) {
        command = m[1];
        return true; // found!
    }
    return false; // no match
}

bool is_number(std::string s)
{
    try {
        std::string::size_type idx;
        int res = std::stoi(s, &idx);
        if (res < 0)
            throw "";
        if (idx != s.size())
            throw "";
    } catch(...) {
        return false;
    }
    
    return true;
}

bool is_hash(std::string s)
{
    std::string symbols = "0123456789abcdef";
    for (size_t i = 0; i < s.size(); i++)
        if (symbols.find(s[i]) == std::string::npos)
            return false;
    return true;
}

bool is_address(std::string s)
{
    for (size_t i = 0; i < s.size(); i++)
        if (!std::isalnum(s[i]))
            return false;
    if (s.size() < 30)
        return false;
    return true;
}

// We need this to allow zero CAmount in xrouter
CAmount to_amount(double val)
{
    if (val < 0.0 || val > 21000000.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    double tmp = val * COIN;
    auto nAmount = static_cast<CAmount>(tmp > 0 ? tmp + 0.5 : tmp - 0.5);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Object form_reply(const std::string & uuid, const Value & reply) {
    Object ret;

    if (reply.type() == array_type) {
        ret.emplace_back("reply", reply);
        if (!uuid.empty())
            ret.emplace_back("uuid", uuid);
        return ret;
    }

    if (reply.type() != obj_type) {
        ret.emplace_back("reply", reply);
        if (!uuid.empty())
            ret.emplace_back("uuid", uuid);
        return ret;
    }

    Object reply_obj = reply.get_obj();
    const Value & result = find_value(reply_obj, "result");
    const Value & error = find_value(reply_obj, "error");
    const Value & code = find_value(reply_obj, "code");

    if (error.type() != null_type) {
        ret.emplace_back("error", error);
        if (code.type() != null_type)
            ret.emplace_back("code", code);
        else
            ret.emplace_back("code", xrouter::INTERNAL_SERVER_ERROR);
    }
    else if (result.type() != null_type) {
        ret.emplace_back("reply", result);
        if (!uuid.empty())
            ret.emplace_back("uuid", uuid);
    }
    else {
        if (!uuid.empty())
            reply_obj.emplace_back("uuid", uuid);
        return reply_obj;
    }

    return ret;
}

Object form_reply(const std::string & uuid, const std::string & reply)
{
    Value reply_val;
    read_string(reply, reply_val);
    return form_reply(uuid, reply_val);
}

} // namespace xrouter
