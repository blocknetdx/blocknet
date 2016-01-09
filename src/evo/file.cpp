
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

boost::filesystem::path GetDataDirectory()
{
    string strDataDir2 = GetArg("-datadir2", "");
    if(strDataDir2 != "") return strDataDir2;

    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\Dash
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\Dash
    // Mac: ~/Library/Application Support/Dash
    // Unix: ~/.dash
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "Dash";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDirectory(pathRet);
    return pathRet / "DashData";
#else
    // Unix
    return pathRet / ".dash-data";
#endif
#endif
}


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
