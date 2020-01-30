// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterutils.h>

#include <xbridge/xbridgeapp.h>
#include <xbridge/xbridgewallet.h>
#include <xrouter/xrouterdef.h>
#include <xrouter/xrouterlogger.h>

#include <base58.h>
#include <core_io.h>
#include <key.h>
#include <node/transaction.h>
#include <rpc/client.h>
#include <validation.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <boost/lexical_cast.hpp>

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

CMutableTransaction decodeTransaction(const std::string & tx)
{
    std::vector<unsigned char> txData(ParseHex(tx));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction result;
    ssData >> result;
    return result;
}

// TODO: check that this variable is static across xbridge and xrouter
static CCriticalSection cs_rpcBlockchainStore;

bool createAndSignTransaction(const std::string & toaddress, const CAmount & toamount, std::string & raw_tx) {
#ifndef ENABLE_WALLET
    return false;
#else
    LOCK(cs_rpcBlockchainStore);

    raw_tx.clear(); // clean ret transaction tx
    // Exclude the used uxtos
    const auto excludedUtxos = xbridge::App::instance().getAllLockedUtxos("BLOCK");

    // Available utxos from from wallet
    std::vector<xbridge::wallet::UtxoEntry> inputs;
    std::vector<xbridge::wallet::UtxoEntry> outputsForUse;
    std::map<COutPoint, std::pair<CTxOut, std::shared_ptr<CWallet>>> coinLookup;

    try {
        std::vector<COutput> coins;
        auto wallets = GetWallets();
        for (const auto & wallet : wallets) {
            LOCK2(cs_main, wallet->cs_wallet);
            if (wallet->IsLocked())
                continue;
            auto lockedChain = wallet->chain().lock();
            std::vector<COutput> coi;
            wallet->AvailableCoins(*lockedChain, coi, true, nullptr);
            coins.insert(coins.end(), coi.begin(), coi.end());
            for (const auto & coin : coi)
                coinLookup[coin.GetInputCoin().outpoint] = std::make_pair(coin.GetInputCoin().txout, wallet);
        }
        if (coins.empty())
            return false; // not enough inputs
        for (const auto & coin : coins) {
            xbridge::wallet::UtxoEntry entry;
            entry.txId = coin.tx->GetHash().ToString();
            entry.vout = coin.i;
            entry.amount = static_cast<double>(coin.GetInputCoin().txout.nValue)/static_cast<double>(COIN);
            CTxDestination destination;
            if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, destination))
                continue; // skip incompatible addresses
            entry.address = EncodeDestination(destination);
            entry.scriptPubKey = HexStr(coin.GetInputCoin().txout.scriptPubKey);
            entry.confirmations = coin.nDepth;
            inputs.emplace_back(entry);
        }
    } catch (...) {
        ERR() << "Failed to created feetx, listunspent returned error";
        return false;
    }

    // Remove all the excluded utxos
    inputs.erase(
        std::remove_if(inputs.begin(), inputs.end(), [&excludedUtxos](xbridge::wallet::UtxoEntry & u) {
            if (excludedUtxos.count(u))
                return true; // remove if in excluded list

            // Only accept p2pkh (like 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
            std::vector<unsigned char> script = ParseHex(u.scriptPubKey);
            if (script.size() == 25 &&
                script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
                script[23] == 0x88 && script[24] == 0xac)
            {
                return false; // keep
            }

            return true; // remove if script invalid
        }),
        inputs.end()
    );

    // Select utxos
    uint64_t utxoAmount{0};
    uint64_t fee1{0};
    uint64_t fee2{0};
    auto minTxFee1 = [](const uint32_t & inputs, const uint32_t & outputs) -> double {
        uint64_t fee = (192*inputs + 34*2) * 20;
        return static_cast<double>(fee) / COIN;
    };
    auto minTxFee2 = [](const uint32_t & inputs, const uint32_t & outputs) -> double {
        return 0;
    };
    if (!xbridge::App::instance().selectUtxos("", inputs, minTxFee1, minTxFee2, toamount,
                                              COIN, outputsForUse, utxoAmount, fee1, fee2))
    {
        ERR() << "Insufficient funds for fee tx";
        return false;
    }

    std::vector<xbridge::wallet::UtxoEntry> inputs_o;
    CAmount change = utxoAmount - toamount - fee1;
    std::string largestInputAddress;
    double largestInput{0};
    for (const auto & a : outputsForUse) {
        if (a.amount > largestInput) {
            largestInputAddress = a.address;
            largestInput = a.amount;
        }
        inputs_o.push_back(a);
    }

    std::vector<CTxOut> outputs_o;
    outputs_o.emplace_back(toamount, GetScriptForDestination(DecodeDestination(toaddress))); // Payment
    outputs_o.emplace_back(change, GetScriptForDestination(DecodeDestination(largestInputAddress))); // Change

    // Create the transaction
    std::string rawtx;
    CMutableTransaction mtx;
    mtx.vin.resize(inputs_o.size());
    mtx.vout.resize(outputs_o.size());
    for (int i = 0; i < (int)inputs_o.size(); ++i)
        mtx.vin[i] = CTxIn(COutPoint(uint256S(inputs_o[i].txId), inputs_o[i].vout));
    for (int i = 0; i < (int)outputs_o.size(); ++i)
        mtx.vout[i] = outputs_o[i];
    // Sign transaction
    for (int i = 0; i < (int)mtx.vin.size(); ++i) {
        const auto & item = coinLookup[mtx.vin[i].prevout];
        const auto & txout = item.first;
        SignatureData sigdata = DataFromTransaction(mtx, i, txout);
        ProduceSignature(*item.second, MutableTransactionSignatureCreator(&mtx, i, txout.nValue, SIGHASH_ALL),
                txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[i], sigdata);
    }
    const CTransaction txConst(mtx);
    raw_tx = EncodeHexTx(txConst);

    // lock used coins
    std::set<xbridge::wallet::UtxoEntry> feeUtxos{outputsForUse.begin(), outputsForUse.end()};
    xbridge::App::instance().lockFeeUtxos(feeUtxos);

    return true;
#endif // ENABLE_WALLET
}

void unlockOutputs(const std::string & tx) {
    if (tx.empty())
        return;
    try {
        CMutableTransaction txobj = decodeTransaction(tx);
        std::set<xbridge::wallet::UtxoEntry> coins;
        for (const auto & vin : txobj.vin) {
            xbridge::wallet::UtxoEntry entry;
            entry.txId = vin.prevout.hash.ToString();
            entry.vout = vin.prevout.n;
            coins.insert(entry);
        }
        xbridge::App::instance().unlockFeeUtxos(coins);
    } catch (...) {
        ERR() << "Failed to unlock fee utxos for tx: " + tx;
    }
}

bool sendTransactionBlockchain(const std::string & rawtx, std::string & txid)
{
    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, rawtx, true, false))
        return false;
    // Send transaction
    uint256 txhash; std::string errstr;
    const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txhash, errstr, 0);
    txid = txhash.ToString();
    return err == TransactionError::OK;
}

double checkPayment(const std::string & rawtx, const std::string & address, const CAmount & expectedFee)
{
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, rawtx) || tx.vin.empty() || tx.vout.empty())
        throw std::runtime_error("Bad fee payment");

    for (const auto & input : tx.vin) {
        CTransactionRef t;
        uint256 hashBlock;
        if (!GetTransaction(input.prevout.hash, t, Params().GetConsensus(), hashBlock))
            throw std::runtime_error("Bad fee payment, failed to find fee inputs");
    }

    CAmount payment{0};
    for (const auto & output : tx.vout) {
        std::vector<CTxDestination> addresses;
        txnouttype whichType;
        int nRequired;
        ExtractDestinations(output.scriptPubKey, whichType, addresses, nRequired);
        for (const CTxDestination & addr : addresses) {
            if (EncodeDestination(addr) == address) {
                payment += output.nValue;
                break;
            }
        }
    }

    if (payment == 0)
        throw std::runtime_error("Bad fee payment, payment address is missing");

    if (payment < expectedFee)
        throw std::runtime_error("Bad fee payment, fee is too low");

    return payment;

//    const static std::string decodeCommand("decoderawtransaction");
//    std::vector<std::string> params;
//    params.push_back(rawtx);
//
//    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
//    if (result.type() != obj_type)
//        throw std::runtime_error("Check payment failed: Decode transaction command finished with error");
//
//    Object obj = result.get_obj();
//    Array vouts = find_value(obj, "vout").get_array();
//    for (const auto & vout : vouts) {
//        // Validate tx type
//        auto & scriptPubKey = find_value(vout.get_obj(), "scriptPubKey").get_obj();
//        const auto & vouttype = find_value(scriptPubKey, "type").get_str();
//        if (vouttype != "pubkeyhash")
//            throw std::runtime_error("Check payment failed: Only pubkeyhash payments are accepted");
//
//        // Validate payment address
//        const auto & addr_val = find_value(scriptPubKey, "addresses");
//        if (addr_val.type() != array_type)
//            continue;
//
//        auto & addrs = addr_val.get_array();
//        if (addrs.size() <= 0)
//            continue;
//
//        if (addrs[0].get_str() != address) // check address
//            continue;
//
//        return find_value(vout.get_obj(), "value").get_real();
//    }
//
//    return 0.0;
}
    
} // namespace xrouter
