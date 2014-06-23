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

Value masternode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "list" && strCommand != "count" && strCommand != "current" && strCommand != "votes" && strCommand != "enforce"))
        throw runtime_error(
            "masternode list|count|current|votes|enforce> passphrase\n");

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

    if (strCommand == "votes")
    {
        Object obj;
        BOOST_FOREACH(CMasterNodeVote& mv, darkSendMasterNodeVotes) {
            obj.push_back(Pair(boost::lexical_cast<std::string>((int)mv.blockHeight),      mv.GetPubKey().ToString().c_str()));
        }    
        return obj;
    }

    if(strCommand == "enforce")
    {
        return (uint64_t)enforceMasternodePaymentsTime;
    }


    return Value::null;
}

