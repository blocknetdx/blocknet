//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"
#include "xrouterlogger.h"
#include "xrouterdef.h"
#include "xroutererror.h"

#include "init.h"
#include "wallet.h"

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

bool satisfyBlockRequirement(uint256& txHash, uint32_t& vout, CKey& key)
{
    if (!pwalletMain) {
        return false;
    }
    for (auto& addressCoins : pwalletMain->AvailableCoinsByAddress()) {
        for (auto& output : addressCoins.second) {
            if (output.Value() >= to_amount(MIN_BLOCK)) {
                CKeyID keyID;
                if (!addressCoins.first.GetKeyID(keyID)) {
                    //std::cerr << "GetKeyID failed\n";
                    continue;
                }
                if (!pwalletMain->GetKey(keyID, key)) {
                    //std::cerr << "GetKey failed\n";
                    continue;
                }
                txHash = output.tx->GetHash();
                vout = output.i;
                return true;
            }
        }
    }
    return false;
}

bool verifyBlockRequirement(const XRouterPacketPtr& packet)
{
    if (packet->size() < 36) {
        LOG() << "Packet not big enough";
        return false;
    }

    uint256 txHash(packet->data());
    CTransaction txval;
    uint256 hashBlock;
    int offset = 32;
    uint32_t vout = *static_cast<uint32_t*>(static_cast<void*>(packet->data() + offset));

    CCoins coins;
    CTxOut txOut;
    if (pcoinsTip->GetCoins(txHash, coins)) {
        if (vout > coins.vout.size()) {
            LOG() << "Invalid vout index " << vout;
            return false;
        }

        txOut = coins.vout[vout];
    } else if (GetTransaction(txHash, txval, hashBlock, true)) {
        txOut = txval.vout[vout];
    } else {
        LOG() << "Could not find " << txHash.ToString();
        return false;
    }

    if (txOut.nValue < to_amount(MIN_BLOCK)) {
        LOG() << "Insufficient BLOCK " << txOut.nValue;
        return false;
    }
    
    CTxDestination destination;
    if (!ExtractDestination(txOut.scriptPubKey, destination)) {
        LOG() << "Unable to extract destination";
        return false;
    }

    auto txKeyID = boost::get<CKeyID>(&destination);
    if (!txKeyID) {
        LOG() << "destination must be a single address";
        return false;
    }

    CPubKey packetKey(packet->pubkey(),
        packet->pubkey() + XRouterPacket::pubkeySize);

    if (packetKey.GetID() != *txKeyID) {
        LOG() << "Public key provided doesn't match UTXO destination.";
        return false;
    }

    return true;
}
    
} // namespace xrouter
