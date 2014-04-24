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
        (strCommand != "list" && strCommand != "count" && strCommand != "current"))
        throw runtime_error(
            "masternode list|count|current> passphrase\n");

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

    return Value::null;
}

