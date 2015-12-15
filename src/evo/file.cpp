
#include "main.h"
#include "file.h"
#include "util.h"
#include "init.h"
#include "base58.h"

#include <stdint.h>
#include <stdio.h>
#include <map>

#include "json/json_spirit.h"
#include "json/json_spirit_value.h"
#include "json/json_spirit_writer.h"

#include <fstream>
#include <string>
#include <streambuf>
#include <boost/filesystem.hpp>


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
