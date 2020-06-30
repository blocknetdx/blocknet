// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xbridge/xbridgedb.h>

#include <chainparams.h>
#include <clientversion.h>
#include <hash.h>
#include <random.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/system.h>

namespace xbridge {

template <typename Stream, typename Data>
bool SerializeDB(Stream& stream, const Data& data)
{
    // Write and commit header, data
    try {
        CHashWriter hasher(SER_DISK, CLIENT_VERSION);
        stream << Params().MessageStart() << data;
        hasher << Params().MessageStart() << data;
        stream << hasher.GetHash();
    } catch (const std::exception& e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool SerializeFileDB(const std::string& prefix, const fs::path& path, const Data& data)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("%s.%04x", prefix, randv);

    // open temp output file, and associate with CAutoFile
    fs::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fsbridge::fopen(pathTmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: Failed to open file %s", __func__, pathTmp.string());

    // Serialize
    if (!SerializeDB(fileout, data)) return false;
    if (!FileCommit(fileout.Get()))
        return error("%s: Failed to flush file %s", __func__, pathTmp.string());
    fileout.fclose();

    // replace existing file, if any, with new file
    if (!RenameOver(pathTmp, path))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

template <typename Stream, typename Data>
bool DeserializeDB(Stream& stream, Data& data, bool fCheckSum = true)
{
    try {
        CHashVerifier<Stream> verifier(&stream);
        // de-serialize file header (network specific magic number) and ..
        unsigned char pchMsgTmp[4];
        verifier >> pchMsgTmp;
        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
            return error("%s: Invalid network magic number", __func__);

        // de-serialize data
        verifier >> data;

        // verify checksum
        if (fCheckSum) {
            uint256 hashTmp;
            stream >> hashTmp;
            if (hashTmp != verifier.GetHash()) {
                return error("%s: Checksum mismatch, data corrupted", __func__);
            }
        }
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool DeserializeFileDB(const fs::path& path, Data& data)
{
    // open input file, and associate with CAutoFile
    FILE *file = fsbridge::fopen(path, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: Failed to open file %s", __func__, path.string());

    return DeserializeDB(filein, data);
}


XBridgeDB::XBridgeDB() : pathDB(GetDataDir() / "orders.dat") { }

bool XBridgeDB::Write(const XOrderSet & orderSet, bool force) {
    if (orderSet.empty() && lastOrdersEmpty) // prevent writing empty set
        return false;
    if (!force && !ShouldSave()) // prevent saving too soon
        return false;
    auto saved = SerializeFileDB("orders", pathDB, orderSet);
    if (saved) {
        lastOrdersEmpty = orderSet.empty();
        lastsave = boost::posix_time::microsec_clock::universal_time();
    }
    return saved;
}

bool XBridgeDB::Read(XOrderSet & orderSet) {
    return DeserializeFileDB(pathDB, orderSet);
}

bool XBridgeDB::Exists() {
    return fs::exists(pathDB);
}

bool XBridgeDB::Create() {
    return Write(XOrderSet{});
}

bool XBridgeDB::ShouldSave() {
    const auto currentTime = boost::posix_time::microsec_clock::universal_time();
    const auto duration = currentTime - lastsave;
    return duration.total_seconds() >= 30;
}

}
