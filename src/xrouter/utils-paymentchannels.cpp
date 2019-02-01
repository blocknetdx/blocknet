//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"
#include "xrouterlogger.h"
#include "xrouterdef.h"
#include "xroutererror.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "base58.h"
#include "wallet.h"
#include "init.h"
#include "key.h"
#include "core_io.h"

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
    
PaymentChannel createPaymentChannel(CPubKey address, CAmount deposit, int date)
{
    PaymentChannel channel;
    CScript inner;
    std::string raw_tx, txid;
    
    int locktime = std::time(0) + date; 
    CPubKey my_pubkey; 
    {
        LOCK(pwalletMain->cs_wallet);
        my_pubkey = pwalletMain->GenerateNewKey();
    }
    CKey mykey;
    CKeyID mykeyID = my_pubkey.GetID();
    pwalletMain->GetKey(mykeyID, mykey);
          
    inner << OP_IF
                << address << OP_CHECKSIGVERIFY
          << OP_ELSE
                << locktime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
          << OP_ENDIF
          << my_pubkey << OP_CHECKSIG;

    CScriptID scriptid = CScriptID(inner);
    CScript p2shScript = GetScriptForDestination(scriptid);
    //p2shScript << OP_HASH160 << id << OP_EQUAL;
    std::string resultScript = scriptid.ToString();
    resultScript = HexStr(p2shScript.begin(), p2shScript.end());
    
    
    Array outputs;
    Object out;
    std::stringstream sstream;
    sstream << std::hex << locktime;
    std::string hexdate = sstream.str();
    out.push_back(Pair("data", hexdate));
    Object out2;
    out2.push_back(Pair("script", resultScript));
    out2.push_back(Pair("amount", ValueFromAmount(deposit)));
    outputs.push_back(out);
    outputs.push_back(out2);
    Array inputs;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);    
    bool res = createAndSignTransaction(params, raw_tx);
    if (!res)
        return channel;
    
    res = sendTransactionBlockchain(raw_tx, txid);
    
    // Get the CLTV vout number
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> dparams;
    dparams.push_back(raw_tx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, dparams));
    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    int i = 0;
    int voutn = 0;
    for (Value vout : vouts) {
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (vouttype == "scripthash") {
            voutn = i;
            break;
        }
        
        i++;
    }
    
    channel.raw_tx = raw_tx;
    channel.txid = txid;
    channel.value = 0.0;
    channel.key = mykey;
    channel.keyid = mykeyID;
    channel.vout = voutn;
    channel.redeemScript = inner;
    channel.deposit = deposit;
    channel.deadline = locktime;
    
    return channel;
}

bool createAndSignChannelTransaction(PaymentChannel channel, std::string address, CAmount deposit, CAmount amount, std::string& raw_tx)
{
    CPubKey my_pubkey; 
    {
        LOCK(pwalletMain->cs_wallet);
        my_pubkey = pwalletMain->GenerateNewKey();
    }
    CKey mykey;
    pwalletMain->GetKey(my_pubkey.GetID(), mykey);
    CKeyID mykeyID = my_pubkey.GetID();

    CMutableTransaction unsigned_tx;

    COutPoint outp(ParseHashV(Value(channel.txid), "txin"), channel.vout);
    CTxIn in(outp);
    unsigned_tx.vin.push_back(in);
    // TODO: get minimal fee from wallet
    unsigned_tx.vout.push_back(CTxOut(deposit - amount - to_amount(0.001), GetScriptForDestination(CBitcoinAddress(mykeyID).Get())));
    unsigned_tx.vout.push_back(CTxOut(amount, GetScriptForDestination(CBitcoinAddress(address).Get())));

    std::vector<unsigned char> signature;
    uint256 sighash = SignatureHash(channel.redeemScript, unsigned_tx, 0, SIGHASH_ALL);
    channel.key.Sign(sighash, signature);
    signature.push_back((unsigned char)SIGHASH_ALL);
    CScript sigscript;
    sigscript << signature;

    CMutableTransaction signed_tx;
    signed_tx.vout = unsigned_tx.vout;
    signed_tx.vin.push_back(CTxIn(outp, sigscript));
    
    raw_tx = EncodeHexTx(signed_tx);
    
    return true;
}

bool verifyChannelTransaction(std::string transaction) {
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, transaction))
        throw XRouterError("Channel TX decode failed", xrouter::INSUFFICIENT_FEE);
    uint256 hashTx = tx.GetHash();
    
    LOCK(cs_main);

    CCoinsViewCache& view = *pcoinsTip;
    const CCoins* existingCoins = view.AccessCoins(hashTx);
    bool fHaveMempool = mempool.exists(hashTx);
    bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;
    if (!fHaveMempool && !fHaveChain) {
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, state, tx, false, NULL, false)) {
            if (state.IsInvalid())
                throw XRouterError("Invalid channel transaction: " + strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()), xrouter::INSUFFICIENT_FEE);
            else
                throw XRouterError("Invalid channel transaction: " + state.GetRejectReason(), xrouter::INSUFFICIENT_FEE);
        }
    } else if (fHaveChain) {
        throw XRouterError("Invalid channel transaction: already in blockchain", xrouter::INSUFFICIENT_FEE);
    }
    
    return true;
}

bool finalizeChannelTransaction(PaymentChannel channel, CKey snodekey, std::string latest_tx, std::string & raw_tx)
{
    CMutableTransaction tx = decodeTransaction(latest_tx);
    std::vector<unsigned char> signature;
    uint256 sighash = SignatureHash(channel.redeemScript, tx, 0, SIGHASH_ALL);
    snodekey.Sign(sighash, signature);
    signature.push_back((unsigned char)SIGHASH_ALL);

    CScript finalScript;
    // TODO: script is removed and pushed again because the first byte (size) must be written inside the << operator implementations
    finalScript << vector<unsigned char>(tx.vin[0].scriptSig.begin()+1, tx.vin[0].scriptSig.end());
    finalScript << signature << OP_1 << std::vector<unsigned char>(channel.redeemScript);
    tx.vin[0].scriptSig = finalScript;
    //tx.vin[0].scriptSig << signature << OP_TRUE << std::vector<unsigned char>(channel.redeemScript);
    raw_tx = EncodeHexTx(tx);
    return true;
}

std::string createRefundTransaction(PaymentChannel channel)
{        
    CPubKey my_pubkey; 
    {
        LOCK(pwalletMain->cs_wallet);
        my_pubkey = pwalletMain->GenerateNewKey();
    }
    CKey mykey;
    pwalletMain->GetKey(my_pubkey.GetID(), mykey);
    CKeyID mykeyID = my_pubkey.GetID();

    CMutableTransaction unsigned_tx;

    COutPoint outp(ParseHashV(Value(channel.txid), "txin"), channel.vout);
    CTxIn in(outp);
    unsigned_tx.vin.push_back(in);
    // TODO: get minimal fee from wallet
    unsigned_tx.vout.push_back(CTxOut(channel.deposit - to_amount(0.001), GetScriptForDestination(CBitcoinAddress(mykeyID).Get())));
    
    std::vector<unsigned char> signature;
    uint256 sighash = SignatureHash(channel.redeemScript, unsigned_tx, 0, SIGHASH_ALL);
    channel.key.Sign(sighash, signature);
    signature.push_back((unsigned char)SIGHASH_ALL);
    CScript sigscript;
    sigscript << signature << OP_FALSE << std::vector<unsigned char>(channel.redeemScript);
    
    CMutableTransaction signed_tx;
    signed_tx.vout = unsigned_tx.vout;
    signed_tx.vin.push_back(CTxIn(outp, sigscript));
    
    return EncodeHexTx(signed_tx);
}

double getTxValue(std::string rawtx, std::string address, std::string type)
{
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> params;
    params.push_back(rawtx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Decode transaction command finished with error");
    }

    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    for (Value vout : vouts) {
        double val = find_value(vout.get_obj(), "value").get_real();        
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (type == "nulldata")
            if (vouttype == "nulldata")
                return val;
            
        if (type == "scripthash")
            if (vouttype == "scripthash")
                return val;
            
        const Value & addr_val = find_value(script, "addresses");
        if (addr_val.is_null())
            continue;
        Array addr = addr_val.get_array();

        for (unsigned int k = 0; k != addr.size(); k++ ) {
            std::string cur_addr = Value(addr[k]).get_str();
            if (cur_addr == address)
                return val;
        }
    }
    
    return 0.0;
}

int getChannelExpiryTime(std::string rawtx)
{
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> params;
    params.push_back(rawtx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Decode transaction command finished with error");
    }

    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    for (Value vout : vouts) {            
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (vouttype != "nulldata")
            continue;
        std::string asmscript = find_value(script, "hex").get_str();
        if (asmscript.substr(0, 4) != "6a04") {
            // TODO: a better check of the script?
            return -1;
        }
        
        std::string hexdate = "0x" + asmscript.substr(4);
        int res = stoi(hexdate, 0, 16);
        return res;    
    }
    
    return -1;
}

} // namespace xrouter
