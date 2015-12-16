

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

    printf("1 %s\n", strCommand.c_str());

    if(strCommand == "get-profile") {
        GetProfile(obj);
        return true;
    }

    printf("2 %d\n", strCommand == "set-profile");
    if (strCommand == "set-profile") {
        printf("here\n");
        SetProfile(obj);
        return true;
    }

    return false;
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

    printf("3 %s\n", strObject.c_str());

    // get the user we want to open
    Object objData = json_spirit::find_value(obj, "data").get_obj();
    string strUID = json_spirit::find_value(objData, "target_uid").get_str();


    printf("4 %s\n", strUID.c_str());

    // open the file and read it
    CDriveFile file(GetProfileFile(strUID));
    printf("5 %s\n", GetProfileFile(strUID).c_str());

    if(!file.Exists()) return false;
    file.Read();

    printf("6 %s\n", GetProfileFile(strUID).c_str());

    // send the user back the results of the query
    Object ret;
    ret.push_back(Pair("object", "dapi_result"));
    ret.push_back(Pair("data", file.obj));

    //TODO: this is terrible, we need to correctly clean and escape the json :) 
    std::stringstream ss;
    json_spirit::write( ret, ss );
    std::string strJson = escapeJsonString("'" + ss.str() + "'");
    strJson.replace(0,1,"\"");
    strJson.replace(strJson.size()-1,1,"\"");

    printf("7 %s\n", strJson.c_str());

    EventNotify(strJson);
    return true;
}

bool CDAPI::SetProfile(Object& obj)
{
    /*
    { 
        "object" : "dapi_command",
        "data" : {
            "command": "set-profile",
            "my_uid": INT64,
            "target_uid": INT64, 
            "signature": "",
            "update" : [
                {"field":"name","value":"newvalue"}
            ]
        }
    }

    */

    std::string strObject = json_spirit::find_value(obj, "object").get_str();
    if(strObject != "dapi_command") return false;

    printf("2 %s\n", strObject.c_str());
    // get the user we want to open
    Object objData = json_spirit::find_value(obj, "data").get_obj();
    const Array& arrDataUpdate = json_spirit::find_value(objData, "update").get_array();
    string strUID = json_spirit::find_value(objData, "target_uid").get_str();

    printf("3 %s\n", strUID.c_str());

    // open the file and read it
    CDriveFile file(GetProfileFile(strUID));
    if(!file.Exists()) return false;
    file.Read();

    std::map<std::string, Value> mapObj;
    json_spirit::obj_to_map(file.obj, mapObj);

    printf("4 %s\n", GetProfileFile(strUID).c_str());

    for( unsigned int i = 0; i < arrDataUpdate.size(); ++i )
    {
        Object tmp = arrDataUpdate[i].get_obj();
        string strField = json_spirit::find_value(tmp, "field").get_str();
        string strValue = json_spirit::find_value(tmp, "value").get_str();

        printf("5 %s %s\n", strField.c_str(), strValue.c_str());

        // string& strUpdate = json_spirit::find_value(obj, strField).get_str();
        // strUpdate = strValue;

        //update the users file, NOTE: this is completely insecure for the prototype (see the paper for security model!)
        //file.obj[strField] = strValue;
        mapObj[strField] = strValue;
    }

    json_spirit::map_to_obj(mapObj, file.obj);
    file.Write();

    // send the user back the results of the query
    Object ret;
    ret.push_back(Pair("object", "dapi_result"));
    ret.push_back(Pair("data", file.obj));

    std::stringstream ss;
    json_spirit::write( obj, ss );

    EventNotify(ss.str());

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

