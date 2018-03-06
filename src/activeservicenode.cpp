
#include "activeservicenode.h"
#include "addrman.h"
#include "servicenode.h"
#include "servicenodeconfig.h"
#include "servicenodeman.h"
#include "protocol.h"
#include "spork.h"
#include "xbridge/xbridgeexchange.h"

//
// Bootup the Servicenode, look for a 5000 BlocknetDX input and register on the network
//
void CActiveServicenode::ManageStatus()
{
    std::string errorMessage;

    if (!fServiceNode) return;

    if (fDebug) LogPrintf("CActiveServicenode::ManageStatus() - Begin\n");

    // If state is set to true, servicenode ping will be sent regardless of last occurrence.
    bool forceSendPing = false;

    //need correct blocks to send ping
    if (Params().NetworkID() != CBaseChainParams::REGTEST && !servicenodeSync.IsBlockchainSynced()) {
        status = ACTIVE_SERVICENODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveServicenode::ManageStatus() - %s\n", GetStatus());
        return;
    }

    if (status == ACTIVE_SERVICENODE_SYNC_IN_PROCESS) status = ACTIVE_SERVICENODE_INITIAL;

    if (status == ACTIVE_SERVICENODE_INITIAL) {
        CServicenode* pmn;
        pmn = mnodeman.Find(pubKeyServicenode);
        if (pmn != NULL) {
            pmn->Check();
            if (pmn->IsEnabled() && pmn->protocolVersion == PROTOCOL_VERSION) {
                forceSendPing = true;
                EnableHotColdServiceNode(pmn->vin, pmn->addr);
            }
        }
    }

    if (status != ACTIVE_SERVICENODE_STARTED) {
        // Set defaults
        status = ACTIVE_SERVICENODE_NOT_CAPABLE;
        notCapableReason = "";

        if (pwalletMain->IsLocked()) {
            notCapableReason = "Wallet is locked.";
            LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (pwalletMain->GetBalance() == 0) {
            notCapableReason = "Hot node, waiting for remote activation.";
            LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (strServiceNodeAddr.empty()) {
            if (!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the servicenodeaddr configuration option.";
                LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
                return;
            }
        } else {
            service = CService(strServiceNodeAddr);
        }

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (service.GetPort() != 41412) {
                notCapableReason = strprintf("Invalid port: %u - only 41412 is supported on mainnet.", service.GetPort());
                LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
                return;
            }
        } else if (service.GetPort() == 41412) {
            notCapableReason = strprintf("Invalid port: %u - 41412 is only supported on mainnet.", service.GetPort());
            LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        LogPrintf("CActiveServicenode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString());

        CNode* pnode = ConnectNode((CAddress)service, NULL, false);
        if (!pnode) {
            notCapableReason = "Could not connect to " + service.ToString();
            LogPrintf("CActiveServicenode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }
        pnode->Release();

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;

        if (GetServiceNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress)) {
            if (GetInputAge(vin) < SERVICENODE_MIN_CONFIRMATIONS) {
                status = ACTIVE_SERVICENODE_INPUT_TOO_NEW;
                notCapableReason = strprintf("%s - %d confirmations", GetStatus(), GetInputAge(vin));
                LogPrintf("CActiveServicenode::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);

            // send to all nodes
            CPubKey pubKeyServicenode;
            CKey keyServicenode;

            if (!obfuScationSigner.SetKey(strServiceNodePrivKey, errorMessage, keyServicenode, pubKeyServicenode)) {
                notCapableReason = "Error upon calling SetKey: " + errorMessage;
                LogPrintf("Register::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            if (!Register(vin, service, keyCollateralAddress, pubKeyCollateralAddress, keyServicenode, pubKeyServicenode, errorMessage)) {
                notCapableReason = "Error on Register: " + errorMessage;
                LogPrintf("Register::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            LogPrintf("CActiveServicenode::ManageStatus() - Is capable master node!\n");
            status = ACTIVE_SERVICENODE_STARTED;

            return;
        } else {
            notCapableReason = "Could not find suitable coins!";
            LogPrintf("CActiveServicenode::ManageStatus() - %s\n", notCapableReason);
            return;
        }
    }

    //send to all peers
    if (!SendServicenodePing(errorMessage, forceSendPing)) {
        LogPrintf("CActiveServicenode::ManageStatus() - Error on Ping: %s\n", errorMessage);
    }
}

std::string CActiveServicenode::GetStatus()
{
    switch (status) {
    case ACTIVE_SERVICENODE_INITIAL:
        return "Servicenode started, activation in progress...";
    case ACTIVE_SERVICENODE_SYNC_IN_PROCESS:
        return "Sync in progress. Must wait until sync is complete to start Servicenode";
    case ACTIVE_SERVICENODE_INPUT_TOO_NEW:
        return strprintf("Servicenode input must have at least %d confirmations", SERVICENODE_MIN_CONFIRMATIONS);
    case ACTIVE_SERVICENODE_NOT_CAPABLE:
        return "Not capable servicenode: " + notCapableReason;
    case ACTIVE_SERVICENODE_STARTED:
        return "Servicenode successfully started";
    default:
        return "unknown";
    }
}

bool CActiveServicenode::SendServicenodePing(std::string& errorMessage, bool force)
{
    if (status != ACTIVE_SERVICENODE_STARTED) {
        errorMessage = "Servicenode is not in a running status";
        return false;
    }

    CPubKey pubKeyServicenode;
    CKey keyServicenode;

    if (!obfuScationSigner.SetKey(strServiceNodePrivKey, errorMessage, keyServicenode, pubKeyServicenode)) {
        errorMessage = strprintf("Error upon calling SetKey: %s\n", errorMessage);
        return false;
    }

    LogPrintf("CActiveServicenode::SendServicenodePing() - Relay Servicenode Ping vin = %s\n", vin.ToString());

    CServicenodePing mnp(vin);
    if (!mnp.Sign(keyServicenode, pubKeyServicenode)) {
        errorMessage = "Couldn't sign Servicenode Ping";
        return false;
    }

    // Update lastPing for our servicenode in Servicenode list
    CServicenode* pmn = mnodeman.Find(vin);
    if (pmn != NULL) {
        // If we have a force send ping request, skip the time check here
        if (!force && pmn->IsPingedWithin(SERVICENODE_PING_SECONDS, mnp.sigTime)) {
            errorMessage = "Too early to send Servicenode Ping";
            return false;
        }

        pmn->lastPing = mnp;
        mnodeman.mapSeenServicenodePing.insert(make_pair(mnp.GetHash(), mnp));

        //mnodeman.mapSeenServicenodeBroadcast.lastPing is probably outdated, so we'll update it
        CServicenodeBroadcast mnb(*pmn);
        uint256 hash = mnb.GetHash();
        if (mnodeman.mapSeenServicenodeBroadcast.count(hash))
        {
            mnodeman.mapSeenServicenodeBroadcast[hash].lastPing = mnp;
        }

        mnp.Relay();

        /*
         * IT'S SAFE TO REMOVE THIS IN FURTHER VERSIONS
         * AFTER MIGRATION TO V12 IS DONE
         */

        if (IsSporkActive(SPORK_10_SERVICENODE_PAY_UPDATED_NODES)) return true;
        // for migration purposes ping our node on old servicenodes network too
        std::string retErrorMessage;
        std::vector<unsigned char> vchServiceNodeSignature;
        int64_t serviceNodeSignatureTime = GetAdjustedTime();

        std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(serviceNodeSignatureTime) + boost::lexical_cast<std::string>(false);

        if (!obfuScationSigner.SignMessage(strMessage, retErrorMessage, vchServiceNodeSignature, keyServicenode)) {
            errorMessage = "dseep sign message failed: " + retErrorMessage;
            return false;
        }

        if (!obfuScationSigner.VerifyMessage(pubKeyServicenode, vchServiceNodeSignature, strMessage, retErrorMessage)) {
            errorMessage = "dseep verify message failed: " + retErrorMessage;
            return false;
        }

        LogPrint("servicenode", "dseep - relaying from active mn, %s \n", vin.ToString().c_str());
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            pnode->PushMessage("dseep", vin, vchServiceNodeSignature, serviceNodeSignatureTime, false);

        /*
         * END OF "REMOVE"
         */

        return true;
    } else {
        // Seems like we are trying to send a ping while the Servicenode is not registered in the network
        errorMessage = "Obfuscation Servicenode List doesn't include our Servicenode, shutting down Servicenode pinging service! " + vin.ToString();
        status = ACTIVE_SERVICENODE_NOT_CAPABLE;
        notCapableReason = errorMessage;
        return false;
    }
}

bool CActiveServicenode::Register(std::string strService, std::string strKeyServicenode, std::string strTxHash, std::string strOutputIndex, std::string& errorMessage)
{
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyServicenode;
    CKey keyServicenode;

    //need correct blocks to send ping
    if (!servicenodeSync.IsBlockchainSynced()) {
        errorMessage = GetStatus();
        LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SetKey(strKeyServicenode, errorMessage, keyServicenode, pubKeyServicenode)) {
        errorMessage = strprintf("Can't find keys for servicenode %s - %s", strService, errorMessage);
        LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
        return false;
    }

    if (!GetServiceNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress, strTxHash, strOutputIndex)) {
        errorMessage = strprintf("Could not allocate vin %s:%s for servicenode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
        return false;
    }

    CService service = CService(strService);
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (service.GetPort() != 41412) {
            errorMessage = strprintf("Invalid port %u for servicenode %s - only 41412 is supported on mainnet.", service.GetPort(), strService);
            LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
            return false;
        }
    } else if (service.GetPort() == 41412) {
        errorMessage = strprintf("Invalid port %u for servicenode %s - 41412 is only supported on mainnet.", service.GetPort(), strService);
        LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
        return false;
    }

    addrman.Add(CAddress(service), CNetAddr("127.0.0.1"), 2 * 60 * 60);

    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyServicenode, pubKeyServicenode, errorMessage);
}

bool CActiveServicenode::Register(CTxIn vin, CService service, CKey keyCollateralAddress, CPubKey pubKeyCollateralAddress, CKey keyServicenode, CPubKey pubKeyServicenode, std::string& errorMessage)
{
    CServicenodeBroadcast mnb;
    CServicenodePing mnp(vin);
    if (!mnp.Sign(keyServicenode, pubKeyServicenode)) {
        errorMessage = strprintf("Failed to sign ping, vin: %s", vin.ToString());
        LogPrintf("CActiveServicenode::Register() -  %s\n", errorMessage);
        return false;
    }
    mnodeman.mapSeenServicenodePing.insert(make_pair(mnp.GetHash(), mnp));

    LogPrintf("CActiveServicenode::Register() - Adding to Servicenode list\n    service: %s\n    vin: %s\n", service.ToString(), vin.ToString());

    xbridge::Exchange & e = xbridge::Exchange::instance();

    mnb = CServicenodeBroadcast(service, vin,
                                pubKeyCollateralAddress, pubKeyServicenode,
                                PROTOCOL_VERSION,
                                e.isEnabled() ? e.connectedWallets() : std::vector<std::string>());
    mnb.lastPing = mnp;
    if (!mnb.Sign(keyCollateralAddress)) {
        errorMessage = strprintf("Failed to sign broadcast, vin: %s", vin.ToString());
        LogPrintf("CActiveServicenode::Register() - %s\n", errorMessage);
        return false;
    }

    // Assign the new vin
    this->vin = vin;

    mnodeman.mapSeenServicenodeBroadcast.insert(make_pair(mnb.GetHash(), mnb));
    servicenodeSync.AddedServicenodeList(mnb.GetHash());

    CServicenode* pmn = mnodeman.Find(vin);
    if (pmn == NULL) {
        CServicenode mn(mnb);
        mnodeman.Add(mn);
    } else {
        pmn->UpdateFromNewBroadcast(mnb);
    }

    //send to all peers
    LogPrintf("CActiveServicenode::Register() - RelayElectionEntry vin = %s\n", vin.ToString());
    mnb.Relay();

    /*
     * IT'S SAFE TO REMOVE THIS IN FURTHER VERSIONS
     * AFTER MIGRATION TO V12 IS DONE
     */

    if (IsSporkActive(SPORK_10_SERVICENODE_PAY_UPDATED_NODES)) return true;
    // for migration purposes inject our node in old servicenodes' list too
    std::string retErrorMessage;
    std::vector<unsigned char> vchServiceNodeSignature;
    int64_t serviceNodeSignatureTime = GetAdjustedTime();
    std::string donationAddress = "";
    int donationPercantage = 0;

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyServicenode.begin(), pubKeyServicenode.end());

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(serviceNodeSignatureTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(PROTOCOL_VERSION) + donationAddress + boost::lexical_cast<std::string>(donationPercantage);

    if (!obfuScationSigner.SignMessage(strMessage, retErrorMessage, vchServiceNodeSignature, keyCollateralAddress)) {
        errorMessage = "dsee sign message failed: " + retErrorMessage;
        LogPrintf("CActiveServicenode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, vchServiceNodeSignature, strMessage, retErrorMessage)) {
        errorMessage = "dsee verify message failed: " + retErrorMessage;
        LogPrintf("CActiveServicenode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    LOCK(cs_vNodes);
    BOOST_FOREACH (CNode* pnode, vNodes)
        pnode->PushMessage("dsee", vin, service, vchServiceNodeSignature, serviceNodeSignatureTime, pubKeyCollateralAddress, pubKeyServicenode, -1, -1, serviceNodeSignatureTime, PROTOCOL_VERSION, donationAddress, donationPercantage);

    /*
     * END OF "REMOVE"
     */

    return true;
}

bool CActiveServicenode::GetServiceNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey)
{
    return GetServiceNodeVin(vin, pubkey, secretKey, "", "");
}

bool CActiveServicenode::GetServiceNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex)
{
    // Get servicenode coins
    vector<COutput> possibleCoins = SelectCoinsServicenode();
    // If no coins return
    if (possibleCoins.empty()) {
        LogPrintf("CActiveServicenode::GetServiceNodeVin - No coins, could not locate valid vin\n");
        return false;
    }

    COutput* selectedOutput;

    // Find the vin
    if (!strTxHash.empty()) {
        // Let's find it
        uint256 txHash(strTxHash);
        int outputIndex;
        try {
            outputIndex = std::stoi(strOutputIndex.c_str());
        } catch (const std::exception& e) {
            LogPrintf("%s: %s on strOutputIndex\n", __func__, e.what());
            return false;
        }

        bool found = false;
        BOOST_FOREACH (COutput& out, possibleCoins) {
            if (out.tx->GetHash() == txHash && out.i == outputIndex) {
                selectedOutput = &out;
                found = true;
                break;
            }
        }
        if (!found) {
            LogPrintf("CActiveServicenode::GetServiceNodeVin - Could not locate valid vin\n");
            return false;
        }
    } else {
        // No output specified,  Select the first one
        if (!possibleCoins.empty()) {
            selectedOutput = &possibleCoins[0];
        } else {
            LogPrintf("CActiveServicenode::GetServiceNodeVin - Could not locate specified vin from possible list\n");
            return false;
        }
    }

    // At this point we have a selected output, retrieve the associated info
    return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}


// Extract Servicenode vin information from output
bool CActiveServicenode::GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey)
{
    CScript pubScript;

    vin = CTxIn(out.tx->GetHash(), out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CBitcoinAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CActiveServicenode::GetServiceNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf("CActiveServicenode::GetServiceNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running Servicenode
vector<COutput> CActiveServicenode::SelectCoinsServicenode()
{
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;
    vector<COutPoint> confLockedCoins;

    // Temporary unlock MN coins from servicenode.conf
    if (GetBoolArg("-mnconflock", true)) {
        LOCK(pwalletMain->cs_wallet);
        uint256 mnTxHash;
        BOOST_FOREACH (CServicenodeConfig::CServicenodeEntry mne, servicenodeConfig.getEntries()) {
            mnTxHash.SetHex(mne.getTxHash());

            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;

            COutPoint outpoint = COutPoint(mnTxHash, nIndex);
            confLockedCoins.push_back(outpoint);
            pwalletMain->UnlockCoin(outpoint);
        }
    }

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(vCoins);

    // Lock MN coins from servicenode.conf back if they where temporary unlocked
    if (!confLockedCoins.empty()) {
        LOCK(pwalletMain->cs_wallet);
        BOOST_FOREACH (COutPoint outpoint, confLockedCoins)
            pwalletMain->LockCoin(outpoint);
    }

    // Filter
    BOOST_FOREACH (const COutput& out, vCoins) {
        if (out.tx->vout[out.i].nValue == SERVICENODE_REQUIRED_AMOUNT * COIN) { //exactly
            filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}

// when starting a Servicenode, this can enable to run as a hot wallet with no funds
bool CActiveServicenode::EnableHotColdServiceNode(CTxIn& newVin, CService& newService)
{
    if (!fServiceNode) return false;

    status = ACTIVE_SERVICENODE_STARTED;

    //The values below are needed for signing mnping messages going forward
    vin = newVin;
    service = newService;

    // update xbridge info for my servicenode
    CServicenode * mn = mnodeman.Find(vin);
    if (mn)
    {
        xbridge::Exchange & e = xbridge::Exchange::instance();
        if (e.isEnabled())
        {
            mn->connectedWallets.clear();
            for(const std::string & walletName : e.connectedWallets())
                mn->connectedWallets.push_back(CServicenodeXWallet(walletName));

            CServicenodeBroadcast mnb(*mn);
            uint256 hash = mnb.GetHash();
            if (mnodeman.mapSeenServicenodeBroadcast.count(hash))
            {
                mnodeman.mapSeenServicenodeBroadcast[hash].connectedWallets = mn->connectedWallets;
            }
        }
    }

    LogPrintf("CActiveServicenode::EnableHotColdServiceNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}
