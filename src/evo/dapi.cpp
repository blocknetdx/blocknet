

// Copyright (c) 2014-2015 The Dash developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "main.h"
#include "db.h"
#include "init.h"
#include "dapi.h"
#include "file.h"

/*

{ 
  "object" : "dash_budget_items",
  "data" : [
    {  "object" : "dash_budget",
    "name" : "ds-liquidity",
    ...
  },
    {  "object" : "dash_budget",
    "name" : "christmas-lottery",
    ...
  },
  ...
  ]
}
*/

bool CDAPI::Execute(Object& obj)
{
    std::string strObject = json_spirit::find_value(obj, "object").get_str();
    if(strObject != "dapi_command") return false;

    Object objData = json_spirit::find_value(obj, "data").get_obj();
    string strCommand = json_spirit::find_value(objData, "command").get_str();

    printf("%s\n", strCommand.c_str());

    if(strCommand == "get-profile") {
        GetProfile(obj);
        return true;
    }

    return true;
}

bool CDAPI::ValidateSignature(Object& obj)
{
    /*
        lookup pubkey for user
        remove signature
        hash object
        check signature against hash
    */

    return true;
}

bool CDAPI::GetProfile(Object& obj)
{
    /*
        {
            "command" : "get-profile",
            "my_uid" : 0,
            "target_uid" : 0,
            "signature" : "SIGNATURE",
        }
    */


    EventNotify("{\"name\":\"value\"}");
    return true;
}

bool CDAPI::SetProfile(Object& obj)
{
    /*
        {
            "command" : "set-profile",
            "my_uid" : 0,
            "target_uid" : 0,
            "signature" : "SIGNATURE",
            "fields" : ["fname", "lname"]
        }
    */

    return true;
}

bool CDAPI::GetProfileData(Object& obj)
{
    /*
        REQUIRED JSON
        {
            "command" : "get-private-data",
            "my_uid" : 0,
            "target_uid" : 0,
            "signature" : "SIGNATURE",
            "slot" : 1
        }
    */

    return true;
}

bool CDAPI::SetProfileData(Object& obj)
{
    /*
        REQUIRED JSON
        {
            "command" : "set-profile-data",
            "my_uid" : 0,
            "target_uid" : 0,
            "signature" : "SIGNATURE"
            "update" : {
                "email" : "evan@dash.org"
            }
        }
    */

    return true;
    
}

// send message from one user to another through T2
bool CDAPI::SendMessage(Object& obj)
{
    /*
        REQUIRED JSON
        {
            "command" : "send-message",
            "my_uid" : 0,
            "target_uid" : 0,
            "signature" : "SIGNATURE"
            "update" : {
                "email" : "evan@dash.org"
            }
        }
    */

    return true;
}

// broadcast any message on the network from T3
bool CDAPI::SendBroadcast(Object& obj)
{
    /*
        REQUIRED JSON
        {
            'command' : 'set-profile-data',
            'my_uid' : 0,
            'target_uid' : 0,
            'signature' : "SIGNATURE"
            'update' : {
                "email" : "evan@dash.org"
            }
        }
    */

    return true;
}

