// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xbridge/bitcoinrpcconnector.h>

#include <xbridge/util/logger.h>
#include <xbridge/util/txlog.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbridgewallet.h>
#include <xbridge/xbitcointransaction.h>

#include <base58.h>
#include <core_io.h>
#include <node/transaction.h>
#include <rpc/client.h>
#include <sync.h>
#include <validation.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <json/json_spirit_utils.h>

#define HTTP_DEBUG

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

const unsigned int MAX_SIZE = 0x02000000;

static CCriticalSection cs_rpcBlockchainStore;

/**
 * Returns all available coins across all wallets.
 * @param onlySafe
 * @param minDepth
 * @param maxDepth
 * @return
 */
static std::vector<std::pair<COutPoint,CTxOut>> availableCoins(const bool & onlySafe = true, const int & minDepth = 0,
        const int & maxDepth = 9999999)
{
    std::vector<std::pair<COutPoint,CTxOut>> r;
#ifdef ENABLE_WALLET
    const auto wallets = GetWallets();
    for (const auto & wallet : wallets) {
        auto locked_chain = wallet->chain().lock();
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> coins;
        wallet->AvailableCoins(*locked_chain, coins, onlySafe, nullptr, 1, MAX_MONEY, MAX_MONEY, 0, minDepth, maxDepth);
        if (coins.empty())
            continue;
        for (auto & coin : coins) {
            if (coin.fSpendable)
                r.emplace_back(coin.GetInputCoin().outpoint, coin.GetInputCoin().txout);
        }
    }
#endif // ENABLE_WALLET
    return std::move(r);
}

//*****************************************************************************
//*****************************************************************************
bool createFeeTransaction(const CScript & dstScript, const double amount, const double feePerByte,
        const std::vector<unsigned char> & data, std::vector<xbridge::wallet::UtxoEntry> & availUtxos,
        std::set<xbridge::wallet::UtxoEntry> & feeUtxos, std::string & rawTx)
{
#ifndef ENABLE_WALLET
    throw std::runtime_error("Cannot create fee transaction because it requires the wallet to be enabled");
#else
    if (availUtxos.empty())
        throw std::runtime_error("Create transaction command finished with error, not enough utxos to cover fee");

    LOCK(cs_rpcBlockchainStore);

    int errCode = 0;
    std::string errMessage;

    try
    {
        auto estFee = [feePerByte](const uint32_t inputs, const uint32_t outputs) -> double {
            return (192 * inputs + 34 * outputs) * feePerByte;
        };
        auto feeAmount = [&estFee](const double amt, const uint32_t inputs, const uint32_t outputs) -> double {
            return amt + estFee(inputs, outputs);
        };

        // Fee utxo selector
        auto selectFeeUtxos = [&estFee, &feeAmount](std::vector<xbridge::wallet::UtxoEntry> & a,
                                 std::vector<xbridge::wallet::UtxoEntry> & o,
                                 const double amt) -> void
        {
            bool done{false};
            std::vector<xbridge::wallet::UtxoEntry> gt;
            std::vector<xbridge::wallet::UtxoEntry> lt;

            // Check ideal, find input that is larger than min amount and within range
            double minAmount{feeAmount(amt, 1, 3)};
            for (const auto & utxo : a) {
                if (utxo.amount >= minAmount && utxo.amount < minAmount + estFee(1, 3) * 100) {
                    o.push_back(utxo);
                    done = true;
                    break;
                }
                else if (utxo.amount >= minAmount)
                    gt.push_back(utxo);
                else if (utxo.amount < minAmount)
                    lt.push_back(utxo);
            }

            if (done)
                return;

            // Find the smallest input > min amount
            // - or -
            // Find the biggest inputs smaller than min amount that when added is >= min amount
            // - otherwise fail -

            if (gt.size() == 1)
                o.push_back(gt[0]);
            else if (gt.size() > 1) {
                // sort utxos greater than amount (ascending) and pick first
                sort(gt.begin(), gt.end(),
                     [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                         return a.amount < b.amount;
                     });
                o.push_back(gt[0]);
            } else if (lt.size() < 2)
                return; // fail (not enough inputs)
            else {
                // sort inputs less than amount (descending)
                sort(lt.begin(), lt.end(),
                     [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                         return a.amount > b.amount;
                     });

                std::vector<xbridge::wallet::UtxoEntry> sel; // store all selected inputs
                for (const auto & utxo : lt) {
                    sel.push_back(utxo);

                    // Add amount and incorporate fee calc
                    double runningAmount{0};
                    for (auto & u : sel)
                        runningAmount += u.amount;
                    runningAmount -= estFee(sel.size(), 3); // subtract estimated fees

                    if (runningAmount >= minAmount) {
                        o.insert(o.end(), sel.begin(), sel.end()); // only add utxos if we pass threshold
                        break;
                    }
                }
            }
        };

        // Find inputs
        std::vector<xbridge::wallet::UtxoEntry> utxos(availUtxos.begin(), availUtxos.end());
        std::vector<xbridge::wallet::UtxoEntry> selUtxos;

        // Sort available utxos by amount (descending)
        sort(utxos.begin(), utxos.end(),
             [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                 return a.amount > b.amount;
             });

        selectFeeUtxos(utxos, selUtxos, amount);
        if (selUtxos.empty())
            throw std::runtime_error("Create transaction command finished with error, not enough utxos to cover fee");

        // Fee amount
        double inputAmt{0};
        double feeAmt{estFee(selUtxos.size(), 3)};
        std::string changeAddr{selUtxos[0].address};

        std::vector<COutPoint> vins;
        for (const auto & utxo : selUtxos)
        {
            vins.emplace_back(uint256S(utxo.txId), utxo.vout);
            feeUtxos.insert(utxo);
            inputAmt += utxo.amount;
        }

        // Total change
        const auto changeAmt = static_cast<CAmount>((inputAmt - amount - feeAmt)*COIN);

        std::vector<CTxOut> vouts;
        if (!data.empty())
            vouts.emplace_back(0, CScript() << OP_RETURN << ToByteVector(data));

        vouts.emplace_back(static_cast<CAmount>(amount*COIN), dstScript);

        if (changeAmt >= 5460) // BLOCK dust check
            vouts.emplace_back(changeAmt, GetScriptForDestination(DecodeDestination(changeAddr)));

        CMutableTransaction mtx;
        mtx.vin.resize(vins.size());
        mtx.vout.resize(vouts.size());
        for (int i = 0; i < vins.size(); ++i)
            mtx.vin[i] = CTxIn(vins[i]);
        for (int i = 0; i < vouts.size(); ++i)
            mtx.vout[i] = vouts[i];

        // Sign all the inputs
        auto wallets = GetWallets();
        for (int i = 0; i < mtx.vin.size(); ++i) {
            const auto & vin = mtx.vin[i];
            for (const auto & wallet : wallets) {
                const auto & wtx = wallet->GetWalletTx(vin.prevout.hash);
                if (!wtx)
                    continue;
                if (wallet->IsLocked())
                    throw std::runtime_error("Unable to send fee tx, wallet is locked");
                const auto & prevtxout = wtx->tx->vout[vin.prevout.n];
                SignatureData sigdata = DataFromTransaction(mtx, i, prevtxout);
                ProduceSignature(*wallet, MutableTransactionSignatureCreator(&mtx, i, prevtxout.nValue, SIGHASH_ALL),
                                 prevtxout.scriptPubKey, sigdata);
                UpdateInput(mtx.vin[i], sigdata);
            }
        }

        for (const auto & vin : mtx.vin) {
            if (vin.scriptSig.empty())
                throw std::runtime_error("Sign transaction error or not completed, failed to sign transaction");
        }

        rawTx = EncodeHexTx(::CTransaction(mtx));
    }
    catch (json_spirit::Object & obj)
    {
        errCode = find_value(obj, "code").get_int();
        errMessage = find_value(obj, "message").get_str();
    }
    catch (std::runtime_error & e)
    {
        // specified error
        errCode = -1;
        errMessage = e.what();
    }
    catch (...)
    {
        errCode = -1;
        errMessage = "unknown error";
    }

    if (errCode != 0)
    {
        errCode = -1;
        errMessage = "failed to create service node fee tx";
        return false;
    }

    return true;

#endif // ENABLE_WALLET
}

//*****************************************************************************
//*****************************************************************************
bool unspentP2PKH(std::vector<xbridge::wallet::UtxoEntry> & utxos)
{
    auto coins = availableCoins(true, 1);
    for (const std::pair<COutPoint, CTxOut> & out : coins) {
        wallet::UtxoEntry utxo;

        // Only support p2pkh (e.g. 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
        const CScript & pk = out.second.scriptPubKey;
        std::vector<unsigned char> script(pk.begin(), pk.end());
        if (script.size() == 25 &&
            script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
            script[23] == 0x88 && script[24] == 0xac)
        {
            CTxDestination address;
            if (!ExtractDestination(pk, address))
                continue;

            utxo.address = EncodeDestination(address);
            utxo.scriptPubKey = HexStr(pk.begin(), pk.end());

        } else continue; // ignore unsupported addresses (p2sh, p2pk, etc)

        utxo.txId = out.first.hash.GetHex();
        utxo.vout = out.first.n;
        utxo.amount = static_cast<double>(out.second.nValue) / static_cast<double>(COIN);

        utxos.push_back(utxo);
    }
    return !utxos.empty();
}

} // namespace rpc
} // namespace xbridge
