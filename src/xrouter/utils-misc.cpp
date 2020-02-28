// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterutils.h>

#include <rpc/protocol.h>

#include <string>
#include <regex>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

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

std::string walletCommandKey(const std::string & wallet, const std::string & command, const bool & withNamespace) {
    return (withNamespace ? xr + xrdelimiter : "") + wallet + xrdelimiter + command;
}
std::string walletCommandKey(const std::string & wallet) {
    return xr + xrdelimiter + wallet;
}
std::string fqServiceToUrl(std::string fqservice) {
    boost::replace_all(fqservice, "::", "/");
    return std::move("/" + fqservice);
}
bool removeNamespace(const std::string & service, std::string & result) {
    auto namespaces = std::vector<std::string>{xr, xrs};

    std::vector<std::string> parts;
    if (!xrsplit(service, xrdelimiter, parts) || parts.empty())
        return false;

    for (const auto & ns : namespaces) {
        if (parts[0] == ns) {
            if (parts.size() > 2) // exclude namespace in result
                result = boost::algorithm::join(std::vector<std::string>{parts.begin()+1, parts.end()}, xrdelimiter);
            else // only 1 part here, no need to join
                result = parts[1];
            return true;
        }
    }

    result = service;
    return true;
}
bool hasWalletNamespace(const std::string & service) {
    const std::string s{xr + xrdelimiter};
    std::regex r("^"+s+"[a-zA-Z0-9_\\-:\\$]+$");
    std::smatch m;
    return std::regex_match(service, m, r);
}
std::string pluginCommandKey(const std::string & service) {
    return xrs + xrdelimiter + service;
}
bool hasPluginNamespace(const std::string & service) {
    const std::string s{xrs + xrdelimiter};
    std::regex r("^"+s+"[a-zA-Z0-9_\\-:\\$]+$");
    std::smatch m;
    return std::regex_match(service, m, r);
}
bool commandFromNamespace(const std::string & fqService, std::string & command) {
    std::regex r(".*?::([a-zA-Z0-9_\\-:\\$]+)$");
    std::smatch m;
    std::regex_search(fqService, m, r);
    if (m.size() > 1) {
        command = m[1];
        return true; // found!
    }
    return false; // no match
}
bool xrsplit(const std::string & fqService, const std::string & del, std::vector<std::string> & vout) {
    vout.clear();
    std::regex r("^[a-zA-Z0-9_\\-:\\$]+$");
    std::smatch m;
    std::vector<std::string> v;

    if (del.size() == 1) {
        boost::algorithm::split(v, fqService, boost::is_any_of(del));
    } else {
        auto t = fqService;
        size_t found{0};
        while (found != std::string::npos) {
            found = t.find(del);
            if (found != std::string::npos) {
                v.push_back(t.substr(0, found));
                t.erase(0, found+del.size());
            } else v.push_back(t);
        }
    }

    // Check
    std::vector<std::string> tmp;
    for (const auto & part : v) {
        if (!std::regex_match(part, m, r)) // check if part is valid
            return false;
        else
            tmp.push_back(part);
    }

    vout = tmp;
    return true;
}

bool is_number(const std::string & s)
{
    try {
        boost::lexical_cast<int>(s);
    } catch(...) {
        return false;
    }
    
    return true;
}

bool is_hash(const std::string & hash)
{
    if (hash.size() < 10)
        return false;
    std::regex r("^[a-zA-Z0-9]+$");
    std::smatch m;
    return std::regex_match(hash, m, r);
}

bool is_hex(const std::string & hex)
{
    std::regex r("^[a-fA-F0-9]{2,}$");
    std::smatch m;
    return std::regex_match(hex, m, r);
}

bool hextodec(const std::string & hex, unsigned int & n) {
    n = std::stoul(hex, nullptr, 16);
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

    ret = reply.get_obj();

    Value rply = find_value(ret, "reply");
    Value result = find_value(ret, "result");
    const Value error_val = find_value(ret, "error");
    const Value code_val = find_value(ret, "code");
    const Value uuid_val = find_value(ret, "uuid");

    if (rply.type() == null_type && result.type() == null_type && error_val.type() == null_type) {
        ret = Object();
        ret.emplace_back("reply", reply.get_obj());
        if (error_val.type() != null_type)
            ret.emplace_back("error", "Bad request");
        if (code_val.type() != null_type)
            ret.emplace_back("code", code_val);
        if (uuid_val.type() != null_type)
            ret.emplace_back("uuid", uuid_val);
        rply = find_value(ret, "reply");
        result = Value();
    }

    // Display result/reply
    if (result.type() != null_type && rply.type() == null_type) {
        for (int i = 0; i < ret.size(); ++i) {
            const auto & item = ret[i];
            if (item.name_ == std::string{"result"}) {
                ret.erase(ret.begin()+i);
                break;
            }
        }
        ret.insert(ret.begin(), Pair("reply", result));
    }

    // Display errors
    if (error_val.type() != null_type) {
        if (code_val.type() == null_type) {
            // insert after error
            for (int i = 0; i < ret.size(); ++i) {
                const auto & item = ret[i];
                if (item.name_ == std::string{"error"}) {
                    ret.insert(ret.begin()+i, Pair("code", xrouter::INTERNAL_SERVER_ERROR));
                    break;
                }
            }
        }
    }

    // Display uuid if necessary
    if (!uuid.empty()) {
        for (int i = 0; i < ret.size(); ++i) {
            const auto & item = ret[i];
            if (item.name_ == std::string{"uuid"}) {
                ret.erase(ret.begin()+i);
                break;
            }
        }
        ret.emplace_back("uuid", uuid);
    }

    return ret;
}

UniValue form_reply(const std::string & uuid, const UniValue & reply) {
    UniValue ret;

    if (reply.isArray() || !reply.isObject()) {
        ret.setObject();
        ret.pushKV("reply", reply);
        if (!uuid.empty())
            ret.pushKV("uuid", uuid);
        return ret;
    }

    ret = reply;

    UniValue rply = find_value(ret, "reply");
    UniValue result = find_value(ret, "result");
    const UniValue error_val = find_value(ret, "error");
    const UniValue code_val = find_value(ret, "code");
    const UniValue uuid_val = find_value(ret, "uuid");

    if (rply.isNull() && result.isNull() && error_val.isNull()) {
        ret = UniValue(UniValue::VOBJ);
        ret.pushKV("reply", reply.get_obj());
        if (!error_val.isNull())
            ret.pushKV("error", "Bad request");
        if (!code_val.isNull())
            ret.pushKV("code", code_val);
        if (!uuid_val.isNull())
            ret.pushKV("uuid", uuid_val);
        rply = find_value(ret, "reply");
        result = UniValue();
    }

    UniValue nret;
    // Display result/reply
    if (!result.isNull() && rply.isNull()) {
        nret.setObject();
        std::map<std::string, UniValue> uvmap;
        ret.getObjMap(uvmap);
        for (const auto & item : uvmap) {
            if (item.first != "result")
                nret.pushKV(item.first, item.second);
        }
        nret.pushKV("reply", result);
    } else
        nret = ret;

    // Display errors
    if (!error_val.isNull()) {
        if (code_val.isNull())
            nret.pushKV("code", xrouter::INTERNAL_SERVER_ERROR);
    }

    // Display uuid if necessary
    UniValue rret;
    if (!uuid.empty()) {
        rret.setObject();
        std::map<std::string, UniValue> uvmap;
        nret.getObjMap(uvmap);
        for (const auto & item : uvmap) {
            if (item.first != "uuid")
                rret.pushKV(item.first, item.second);
        }
        rret.pushKV("uuid", uuid);
    } else
        rret = nret;

    return rret;
}

Object form_reply(const std::string & uuid, const std::string & reply)
{
    Value reply_val;
    try {
        read_string(reply, reply_val);
    } catch (...) {
        reply_val = Value(reply);
    }
    if (reply_val.type() == null_type)
        reply_val = Value(reply);
    return form_reply(uuid, reply_val);
}

} // namespace xrouter
