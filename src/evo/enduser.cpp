

#include "enduser.h"

#include "main.h"
#include "db.h"
#include "init.h"
#include "rpcserver.h"
#include "utilmoneystr.h"

#include "util.h"

#include <stdint.h>

#include "univalue/univalue.h"

#include <fstream>
using namespace json_spirit;
using namespace std;

bool CEndUser::Load()
{
    obj.clear();
    obj.push_back(Pair("name", "evan"));
    return true;
}