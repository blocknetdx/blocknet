//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"
#include "xrouterdef.h"
#include "xrouterlogger.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

std::string generateDomainRegistrationTx(std::string domain, std::string addr) {
    std::string raw_tx, txid;

    Array outputs;
    Object out;
    std::string domainstr = "blocknet://" + domain;
    vector<unsigned char> domain_enc(domainstr.begin(), domainstr.end());
    out.push_back(Pair("data", HexStr(domain_enc)));
    Object out2;
    out2.push_back(Pair("address", addr));
    out2.push_back(Pair("amount", XROUTER_DOMAIN_REGISTRATION_DEPOSIT));
    outputs.push_back(out);
    outputs.push_back(out2);
    Array inputs;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);    
    bool res = createAndSignTransaction(params, raw_tx);
    if (!res)
        return "";
    
    sendTransactionBlockchain(raw_tx, txid);
    return txid;
}

bool verifyDomain(std::string tx, std::string domain, std::string addr, int& blockNumber)
{
    uint256 txHash;
    txHash.SetHex(tx);
    CTransaction txval;
    uint256 hashBlock;
    CCoins coins;
    std::vector<CTxOut> vout_list;
    if (!pcoinsTip->GetCoins(txHash, coins)) {
        LOG() << "Output spent for tx: " << tx;
        return false;
    }
    
    GetTransaction(txHash, txval, hashBlock, true);
    vout_list = txval.vout;
    CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
    blockNumber = pblockindex->nHeight;

    bool has_domain = false;
    bool has_deposit = false;
    for (CTxOut txOut : vout_list) {
        std::string script = HexStr(txOut.scriptPubKey);

        // TODO 6a == OP_RETURN, maybe a more careful check is needed here?
        if (txOut.nValue == 0) {
            // This is the txout that stores the domain name
            if (script.substr(0, 2) != "6a") {
                return false;
            }
            vector<unsigned char> data = ParseHex(script.substr(4));
            std::string extracted_domain = std::string(data.begin(), data.end());
            if ("blocknet://" + domain != extracted_domain)
                return false;
            
            has_domain = true;
            LOG() << "Extracted domain: "  << extracted_domain;
        } else if (txOut.nValue == to_amount(XROUTER_DOMAIN_REGISTRATION_DEPOSIT)) {
            // This is the deposit txout, should be to service node payout address
            CTxDestination destination;
            if (!ExtractDestination(txOut.scriptPubKey, destination)) {
                LOG() << "Unable to extract destination";
                continue;
            }
            
            auto keyid = boost::get<CKeyID>(&destination);
            if (!keyid) {
                LOG() << "Destination must be a single address";
                continue;
            }
            
            std::string extracted_addr = CBitcoinAddress(*keyid).ToString();
            
            LOG() << "Extracted address: "  << extracted_addr;
            if (extracted_addr != addr)
                continue;
            
            has_deposit = true;
        }
    }
    
    return has_domain && has_deposit;
}

} // namespace xrouter
