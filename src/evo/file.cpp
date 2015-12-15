
#include "util.h"
#include "init.h"

#include "addrman.h"
#include "amount.h"
#include "checkpoints.h"
#include "compat/sanity.h"
#include "key.h"
#include "main.h"
#include "file.h"

#include <stdint.h>
#include <stdio.h>
#include <map>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

#include "main.h"
#include "base58.h"

#include <fstream>
#include "json/json_spirit.h"
#include "json/json_spirit_value.h"

#include <string>
#include <streambuf>


using namespace boost;
using namespace std;
using namespace json_spirit;


// CDriveFile --- 

CDriveFile::CDriveFile()
{
    LOCK(cs);
    strMagicMessage = "CDriveFile";
    fDirty = false;
}

CDriveFile::CDriveFile(const std::string strPathIn)
{
    LOCK(cs);
    strPath = strPathIn;
    Read();
    fDirty = false;
}


CDriveFile::ReadResult CDriveFile::Read()
{
    std::ifstream t(strPath);
    std::string str((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());

    json_spirit::Value val;

    bool fSuccess = json_spirit::read(str, val);
    if (fSuccess) {
        obj = val.get_obj();
        return Ok;
    }

    return FileError;
}

bool CDriveFile::Write()
{
    LOCK(cs);

    ofstream os( strPath );
    write( obj, os );
    os.close();
   

    return false;
}
