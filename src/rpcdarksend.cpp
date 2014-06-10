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

    return darkSendPool.lastMessage;
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
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "start" && strCommand != "stop" && strCommand != "list" && strCommand != "count" 
            && strCommand != "debug" && strCommand != "create" && strCommand != "current" && strCommand != "votes" && strCommand != "genkey"))
        throw runtime_error(
            "masternode <start|stop|list|count|debug|create|current|votes|genkey> passphrase\n");

    if (strCommand == "stop")
    {
        if(!fMasterNode) return "you must set masternode=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }
            
            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        darkSendPool.RegisterAsMasterNode(true);
        pwalletMain->Lock();
        
        if(darkSendPool.isCapableMasterNode == MASTERNODE_STOPPED) return "successfully stopped masternode";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_NOT_CAPABLE) return "not capable masternode";
        
        return "unknown";
    }

if (strCommand == "list")
    {
        std::string strCommand = "active";

        if (params.size() == 2){
            strCommand = params[1].get_str().c_str();
        }

        if (strCommand != "active" && strCommand != "vin" && strCommand != "pubkey" && strCommand != "lastseen" && strCommand != "activeseconds"){
            throw runtime_error(
                "list supports 'active', 'vin', 'pubkey', 'lastseen', 'activeseconds'\n");
        }

        Object obj;
        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
            mn.Check();   

            if(strCommand == "active"){
                obj.push_back(Pair(mn.addr.ToString().c_str(),       (int)mn.IsEnabled()));
            } else if (strCommand == "vin") {
                obj.push_back(Pair(mn.addr.ToString().c_str(),       mn.vin.prevout.hash.ToString().c_str()));
            } else if (strCommand == "pubkey") {
                CScript pubkey;
                pubkey.SetDestination(mn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CBitcoinAddress address2(address1);

                obj.push_back(Pair(mn.addr.ToString().c_str(),       address2.ToString().c_str()));
            } else if (strCommand == "lastseen") {
                obj.push_back(Pair(mn.addr.ToString().c_str(),       (int64_t)mn.lastTimeSeen));
            } else if (strCommand == "activeseconds") {
                obj.push_back(Pair(mn.addr.ToString().c_str(),       (int64_t)(mn.lastTimeSeen - mn.now)/(1000*1000)));
            }
        }
        return obj;
    }
    if (strCommand == "count") return (int)darkSendMasterNodes.size();

    if (strCommand == "start")
    {
        if(!fMasterNode) return "you must set masternode=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }
            
            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        darkSendPool.RegisterAsMasterNode(false);
        pwalletMain->Lock();
        
        if(darkSendPool.isCapableMasterNode == MASTERNODE_INPUT_TOO_NEW) return "masternode input must have at least 6 confirmations";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_STOPPED) return "masternode is stopped";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_IS_CAPABLE) return "successfully started masternode";
        if(darkSendPool.masternodePortOpen == MASTERNODE_PORT_TEST_INPROGRESS) return "testing inbound connection, please try this command again in a few seconds";
        if(darkSendPool.masternodePortOpen == MASTERNODE_PORT_NOT_OPEN) return "inbound port is not open. Please open it and try again. (19999 for testnet and 9999 for mainnet)";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_NOT_CAPABLE) return "not capable masternode";

        return "unknown";
    }

    if (strCommand == "debug")
    {
        if(darkSendPool.isCapableMasterNode == MASTERNODE_INPUT_TOO_NEW) return "masternode input must have at least 6 confirmations";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_IS_CAPABLE) return "successfully started masternode";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_STOPPED) return "masternode is stopped";
        if(darkSendPool.masternodePortOpen == MASTERNODE_PORT_TEST_INPROGRESS) return "testing inbound connection, please try this command again in a few seconds";
        if(darkSendPool.masternodePortOpen == MASTERNODE_PORT_NOT_OPEN) return "inbound port is not open. Please open it and try again. (19999 for testnet and 9999 for mainnet)";
        if(darkSendPool.isCapableMasterNode == MASTERNODE_NOT_CAPABLE) return "not capable masternode";

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = darkSendPool.GetMasterNodeVin(vin, pubkey, key);
        if(!found){
            return "Missing masternode input, please look at the documentation for instructions on masternode creation";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "votes")
    {
        Object obj;
        BOOST_FOREACH(CMasterNodeVote& mv, darkSendMasterNodeVotes) {
            obj.push_back(Pair(boost::lexical_cast<std::string>((int)mv.blockHeight),      mv.GetPubKey().ToString().c_str()));
        }    
        return obj;
    }

    if (strCommand == "create")
    {
        return "Not implemented yet, please look at the documentation for instructions on masternode creation";
    }

    if (strCommand == "current")
    {
        int mod = 10;
        if (params.size() == 2){
            mod = 1;
        }

        int winner = darkSendPool.GetCurrentMasterNode(mod);
        if(winner >= 0) {
            return darkSendMasterNodes[winner].addr.ToString().c_str();
        }

        return "unknown";
    }

    if (strCommand == "genkey")
    {    
        CKey secret;
        secret.MakeNewKey(false);

        return CBitcoinSecret(secret).ToString();
    }

    return Value::null;
}

