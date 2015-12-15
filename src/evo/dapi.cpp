

// Copyright (c) 2014-2015 The Dash developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "main.h"
#include "db.h"
#include "init.h"
#include "dapi.h"
#include "file.h"
#include "json/json_spirit.h"
#include "json/json_spirit_value.h"

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
            "object" : "dapi_command",
            "data" : {
                “command” : ”get-profile”,
                “my_uid” : INT64,
                “target_uid” : INT64, 
                “signature” : ‘’,
                “fields” : [“fname”, “lname”]
            }
        }
    */

    std::string strObject = json_spirit::find_value(obj, "object").get_str();
    if(strObject != "dapi_command") return false;

    Object objData = json_spirit::find_value(obj, "data").get_obj();
    string strUID = json_spirit::find_value(objData, "target_uid").get_str();

    CDriveFile file(GetProfileFile(strUID));
    //if(!file.Exists()) return false;
    file.Read();

    // Object ret;
    // ret.PushKV("object", "dapi_result");
    // ret.PushKV("data", "result");

    // string sret;
    // json_spirit::write( obj, sret );

    // EventNotify(sret);
    return true;
}

bool CDAPI::SetProfile(Object& obj)
{
    /*
    { 
        "object" : "dapi_command",
        "data" : {
            "command" = "set-profile",
            "my_uid" = INT64,
            "target_uid" = INT64, 
            "signature" = ""
        }
    }

    */


    CDriveFile file("/Users/evan/Desktop/dash/src/test/data/dapi-get-profile.js");
    file.obj = obj;
    file.WriteContents();


    return true;
}

bool CDAPI::GetPrivateData(Object& obj)
{
    /*
    {
        "object" : "dapi_command",
        "data" : {
            "command" = "get-private-data",
            "my_uid" = UID,
            "target_uid" = UID, 
            "signature" = ‘’,
            "slot" = 1
        }
    }
    */

    return true;
}

bool CDAPI::SetPrivateData(Object& obj)
{
    /*
    {
        "object" : "dapi_command",
        "data" : {
            "command" : "set-private-data",
            "my_uid" : INT64,
            "target_uid" : INT64, 
            "signature" : "SIGNATURE",
            "slot" : 1,
            "payload" : JSON_WEB_ENCRYPTION
        }
    }
    */

    return true;
    
}

// send message from one user to another through T2
bool CDAPI::SendMessage(Object& obj)
{
    /*
    { 
        "object" : "dapi_command",
        "data" : {
            "command" = "message",
            "subcommand" = "(available subcommands)",
            "my_uid" = UID,
            "target_uid" = UID, 
            "signature" = ‘’,
            "payload" = ENCRYPTED
        }
    }
    */

    return true;
}

// broadcast any message on the network from T3
bool CDAPI::SendBroadcast(Object& obj)
{
    /*
    { 
        "object" : "dapi_command",
        "data" : {
            "command" = "broadcast",
            "subcommand" = "tx", //can support multiple message commands
            "my_uid" = UID,
            "target_uid" = UID, 
            "signature" = ‘’,
            "payload" = SERIALIZED_BASE64_ENCODED
        }
    }
    */

    return true;
}

