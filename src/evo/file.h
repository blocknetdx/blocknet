

#ifndef DASHDRIVE_FILE_H
#define DASHDRIVE_FILE_H

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


#include "sync.h"
#include "net.h"
#include "util.h"
#include "base58.h"


#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

#include "json/json_spirit_value.h"
#include "univalue/univalue.h"

using namespace json_spirit;
using namespace std;


// TODO: What include is required for this?
#define CLIENT_VERSION 1

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

class CDriveFile
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    string strMagicMessage;
    string strPath;
    Object obj;
    bool fDirty;

public:

    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CDriveFile();
    CDriveFile(const string strPathIn);

    static CDriveFile GetNextIncrementalFileInFolder(const string strFolderIn);

    bool Exists();
    ReadResult Read();
    bool Write();

};


#endif

