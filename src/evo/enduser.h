

// Copyright (c) 2014-2015 The Dash developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef DASHDRIVE_USER_H
#define DASHDRIVE_USER_H

#include "main.h"
#include "db.h"
#include "init.h"
#include "rpcserver.h"

#include "util.h"

#include <stdint.h>

#include "json/json_spirit_value.h"
#include "univalue/univalue.h"

using namespace std;
using namespace json_spirit;

class CEndUser
{
private:
    Object obj;
    std::string strPath;
    CEndUser(std::string strPathIn) {strPath = strPathIn;}

public:

    bool Load();
    bool Save() {return true;};
};

#endif