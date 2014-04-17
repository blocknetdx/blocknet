// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "init.h"
#include "bitcoinrpc.h"

using namespace json_spirit;
using namespace std;

Value darksend(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "darksend <darkcoinaddress> <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    if(fMasterNode)
        return "DarkSend is not supported from masternodes";
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DarkCoin address");

    // Amount
    int64 nAmount = AmountFromValue(params[1]);

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    string strError = pwalletMain->DarkSendMoney(address.Get(), nAmount);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return "";
}

Value getpoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    Object obj;
    obj.push_back(Pair("connected_to_masternode",        darkSendPool.GetMasterNodeAddr()));
    obj.push_back(Pair("current_masternode",        darkSendPool.GetCurrentMasterNode()));
    obj.push_back(Pair("is_connected_to_masternode", darkSendPool.IsConnectedToMasterNode()));
    obj.push_back(Pair("state",        darkSendPool.GetState()));
    obj.push_back(Pair("entries",      darkSendPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted",      darkSendPool.GetCountEntriesAccepted()));
    obj.push_back(Pair("signatures",   darkSendPool.GetSignatureCount()));
    return obj;
}

Value darksendsub(const Array& params, bool fHelp)
{
    darkSendPool.SubscribeToMasterNode();
    return Value::null;
}

Value masternode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 1)
        strCommand = params[0].get_str();
    if (fHelp || params.size() != 1 ||
        (strCommand != "start" && strCommand != "stop" && strCommand != "list" && strCommand != "count" 
            && strCommand != "debug" && strCommand != "create" && strCommand != "current"))
        throw runtime_error(
            "masternode <start|stop|list|count|debug|create|current>\n");

    if (strCommand == "stop")
    {
        return "Not implemented yet";
    }

    if (strCommand == "list")
    {
        Object obj;
        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
            mn.Check();            
            obj.push_back(Pair(mn.addr.ToString().c_str(),       (int)mn.IsEnabled()));
        }
        return obj;
    }

    if (strCommand == "count") return (int)darkSendMasterNodes.size();

    if (strCommand == "debug")
    {
        if(darkSendPool.isCapableMasterNode) return "is masternode";

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = darkSendPool.GetMasterNodeVin(vin, pubkey, key);
        if(!found){
            return "Missing masternode input, try running masternode create";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "create")
    {
        return "Not implemented yet";
    }

    if (strCommand == "current")
    {
        int winner = darkSendPool.GetCurrentMasterNode();
        if(winner >= 0) {
            return darkSendMasterNodes[winner].addr.ToString().c_str();
        }

        return "unknown";
    }

    return Value::null;
}

