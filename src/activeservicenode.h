// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2015-2016 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ACTIVESERVICENODE_H
#define ACTIVESERVICENODE_H

#include "init.h"
#include "key.h"
#include "servicenode.h"
#include "net.h"
#include "obfuscation.h"
#include "sync.h"
#include "wallet.h"

#define SERVICENODE_REQUIRED_AMOUNT                 5000
#define SERVICENODE_ACCEPTABLE_INPUTS_CHECK_AMOUNT  4999.99

#define ACTIVE_SERVICENODE_INITIAL 0 // initial state
#define ACTIVE_SERVICENODE_SYNC_IN_PROCESS 1
#define ACTIVE_SERVICENODE_INPUT_TOO_NEW 2
#define ACTIVE_SERVICENODE_NOT_CAPABLE 3
#define ACTIVE_SERVICENODE_STARTED 4

// Responsible for activating the Servicenode and pinging the network
class CActiveServicenode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    /// Ping Servicenode with the option to force ping
    bool SendServicenodePing(std::string& errorMessage, bool force = false);

    /// Register any Servicenode
    bool Register(CTxIn vin, CService service, CKey key, CPubKey pubKey, CKey keyServicenode, CPubKey pubKeyServicenode, std::string& errorMessage);

    /// Get 5000 BLOCK input that can be used for the Servicenode
    bool GetServiceNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    bool GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey);

public:
    // Initialized by init.cpp
    // Keys for the main Servicenode
    CPubKey pubKeyServicenode;

    // Initialized while registering Servicenode
    CTxIn vin;
    CService service;

    int status;
    std::string notCapableReason;

    CActiveServicenode()
    {
        status = ACTIVE_SERVICENODE_INITIAL;
    }

    /// Manage status of main Servicenode
    void ManageStatus();
    std::string GetStatus();

    /// Register remote Servicenode
    bool Register(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string& errorMessage);

    /// Get 5000 BLOCK input that can be used for the Servicenode
    bool GetServiceNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    vector<COutput> SelectCoinsServicenode();

    /// Enable cold wallet mode (run a Servicenode with no funds)
    bool EnableHotColdServiceNode(CTxIn& vin, CService& addr);
};

#endif
