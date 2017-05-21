// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Zerocoin.h"
#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

BOOST_AUTO_TEST_SUITE(zerocoin_implementation_tests)

BOOST_AUTO_TEST_CASE(zcparams_test)
{
    cout << "Running zcparams_test...\n";

    bool fPassed = true;
    try{
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params *ZCParams = Params().Zerocoin_Params();
        (void)ZCParams;
    } catch(std::exception& e) {
        fPassed = false;
        std::cout << e.what() << "\n";
    }
    BOOST_CHECK(fPassed);
}

//translation from pivx quantity to zerocoin denomination
BOOST_AUTO_TEST_CASE(amount_to_denomination_test)
{
    cout << "Running amount_to_denomination_test...\n";

    //valid amount (min edge)
    CAmount amount = 1 * COIN;
    libzerocoin::CoinDenomination denomination;
    BOOST_CHECK(libzerocoin::AmountToZerocoinDenomination(amount, denomination));
    BOOST_CHECK(denomination == libzerocoin::ZQ_LOVELACE);

    //valid amount (max edge)
    CAmount amount1 = 100 * COIN;
    libzerocoin::CoinDenomination denomination1;
    BOOST_CHECK(libzerocoin::AmountToZerocoinDenomination(amount1, denomination1));
    BOOST_CHECK(denomination1 == libzerocoin::ZQ_WILLIAMSON);

    //invalid amount (too much)
    CAmount amount2 = 5000 * COIN;
    libzerocoin::CoinDenomination denomination2;
    BOOST_CHECK(!libzerocoin::AmountToZerocoinDenomination(amount2, denomination2));
    BOOST_CHECK(denomination2 == libzerocoin::ZQ_ERROR);

    //invalid amount (not enough)
    CAmount amount3 = 1;
    libzerocoin::CoinDenomination denomination3;
    BOOST_CHECK(!libzerocoin::AmountToZerocoinDenomination(amount3, denomination3));
    BOOST_CHECK(denomination3 == libzerocoin::ZQ_ERROR);

}

//ZQ_LOVELACE mints
std::string rawTx1 = "0100000001983d5fd91685bb726c0ebc3676f89101b16e663fd896fea53e19972b95054c49000000006a473044022010fbec3e78f9c46e58193d481caff715ceb984df44671d30a2c0bde95c54055f0220446a97d9340da690eaf2658e5b2bf6a0add06f1ae3f1b40f37614c7079ce450d012103cb666bd0f32b71cbf4f32e95fa58e05cd83869ac101435fcb8acee99123ccd1dffffffff0200e1f5050000000086c10280004c80c3a01f94e71662f2ae8bfcd88dfc5b5e717136facd6538829db0c7f01e5fd793cccae7aa1958564518e0223d6d9ce15b1e38e757583546e3b9a3f85bd14408120cd5192a901bb52152e8759fdd194df230d78477706d0e412a66398f330be38a23540d12ab147e9fb19224913f3fe552ae6a587fb30a68743e52577150ff73042c0f0d8f000000001976a914d6042025bd1fff4da5da5c432d85d82b3f26a01688ac00000000";
std::string rawTxpub1 = "473ff507157523e74680ab37f586aae52e53f3f912492b19f7e14ab120d54238ae30b338f39662a410e6d707784d730f24d19dd9f75e85221b51b902a19d50c120844d15bf8a3b9e346355857e7381e5be19c6d3d22e01845565819aae7cacc93d75f1ef0c7b09d823865cdfa3671715e5bfc8dd8fc8baef26216e7941fa0c3";
    std::string rawTxRand1 = "9fc222b16be09eb88affbdfbcc02d1c8b28f5e843c72eb06c89dd7aff0c60838";
std::string rawTxSerial1 = "b87754b165892c0f9634e3d03780ede24824125249cb8dfd4ad2c0be055cbead";

    std::string rawTx2 = "01000000018c52504b2822c39dd7f4bd93e30562dc9d246e0b0dd4ee401ec2c24e9378be12000000006b483045022100e2628dbcd284dd4858e2c2d8e2d2d31eb222773b3026d39c79c489f5daf4ae2302200e0b1cb9a6d534dc86ea33afb8153a5a4c7cd4fb497c889fb991fbac8bf86802012103836a4868020f52f2ab9e5ec3634d2cd38794677fab47ae7a7128ea8102972ae0ffffffff022c0f0d8f000000001976a914e2e8e36a1a35da051341775315b1168494921acd88ac00e1f5050000000086c10280004c809d49caa17c3f1fb8bc93eabf54462c8ad1717ab646c8130ca0863ca5613f34751445cd7bde8ef1dd833645c7c205dd9b36171dc25209f46b04a34b5e06caa655eea9bd95b46f7d03ae60a97961dd6632c1050090ec1b6748199f0721eeec0822dd288c663020dd88ecda7c8abf8a409fa5c500c4188e52bfbe2ca77ce7b2700700000000";
    std::string rawTxpub2 = "770b2e77ca72cbebf528e18c400c5a59f408abf8a7cdaec88dd2030668c28dd2208ecee21079f1948671bec900005c13266dd6179a960ae037d6fb495bda9ee55a6ca065e4ba3046bf40952c21d17369bdd05c2c7453683ddf18ede7bcd451475343f61a53c86a00c13c846b67a71d18a2c4654bfea93bcb81f3f7ca1ca499d";
    std::string rawTxRand2 = "23040b1d889ca4a41cf50b88a380f3f3acffac750e221a268fedf700f063a886";
    std::string rawTxSerial2 = "37393797cb39e5f22bdc4fba8108edb5ea497ba0d22aba0781e58a8555df285c";

    std::string rawTx3 = "01000000014651d7ed09c01d26679dd8ad1ee1f704f63167544ca48bdd3b4577444d540514010000006a47304402207995f8e30a87b74f36146d80ab02198319240a2eb3f93018c740e91b6812ff23022002677250aa9f9c7b6c1258647b0b0c03f89c7495b82b9c4dd2dcdb0ced82412801210236e3e30dbb1d62c8872413b2a771cd611b8042dfb5d06feb6805ba934ba534ffffffffff0200e1f5050000000086c10280004c803dac1997d38ee8650bb87fae490f4684a7b023744c95cd5ef025bc7f4d1414aff96947cebf342cfbfaf217ec0088e489d722d494409494a011a452af55a8cd4d2cef97f3b0307b66238623ab02b148a9e20f36782c8b7ea47c0c0b8226ddb91ee8f1f94c8c04df5c834993f27175b20b1da99d8338c674b1741a696c54def8012c0f0d8f000000001976a914c7f81b8e5650af548f5d56ef064da5c2d1ee09ae88ac00000000";
    std::string rawTxpub3 = "1f8de546c691a74b174c638839da91d0bb27571f29349835cdf048c4cf9f1e81eb9dd26820b0c7ca47e8b2c78360fe2a948b102ab238623667b30b0f397ef2c4dcda855af52a411a094944094d422d789e48800ec17f2fafb2c34bfce4769f9af14144d7fbc25f05ecd954c7423b0a784460f49ae7fb80b65e88ed39719ac3d";
    std::string rawTxRand3 = "1953c2919d658c3f654566400ace91563105ad5acc4e4151bca1e762c0877d7b";
    std::string rawTxSerial3 = "3abf349844720512325d129c95402edbc85d86fff89632a05dc18970560047a5";


    std::vector<std::pair<std::string, std::string> > vecRawMints = {std::make_pair(rawTx1, rawTxSerial1), std::make_pair(rawTx2, rawTxSerial2), std::make_pair(rawTx3, rawTxSerial3)};

//create a zerocoin mint from vecsend
BOOST_AUTO_TEST_CASE(checkzerocoinmint_test)
{
    cout << "Running check_zerocoinmint_test...\n";
    CTransaction tx;
    BOOST_CHECK(DecodeHexTx(tx, rawTx1));

    CValidationState state;
    bool fFoundMint = false;
    for(unsigned int i = 0; i < tx.vout.size(); i++){
        if(!tx.vout[i].scriptPubKey.empty() && tx.vout[i].scriptPubKey.IsZerocoinMint()) {
            BOOST_CHECK(CheckZerocoinMint(tx.vout[i], state, true));
            fFoundMint = true;
        }
    }

    BOOST_CHECK(fFoundMint);
}

bool CheckZerocoinSpendNoDB(uint256 hashTx, const CTxOut txout, vector<CTxIn> vin, const libzerocoin::Accumulator &accumulator,CValidationState& state)
{
    libzerocoin::CoinDenomination denomination;
    if(!libzerocoin::AmountToZerocoinDenomination(txout.nValue, denomination))
        return state.DoS(100, error("CheckZerocoinSpend(): Zerocoin spend does not have valid denomination"));

    // Check vIn
    bool fValidated = false;
    BOOST_FOREACH(const CTxIn& txin, vin) {

        //only check txin that is a zcspend
        if (!txin.scriptSig.IsZerocoinSpend())
            continue;

        libzerocoin::CoinSpend newSpend = TxInToZerocoinSpend(txin);
        if(!CheckZerocoinSpendProperties(txin, newSpend, accumulator, state))
            return state.DoS(100, error("Zerocoinspend properties are not valid"));

        fValidated = true;
    }

    return fValidated;
}

BOOST_AUTO_TEST_CASE(checkzerocoinspend_test)
{
    cout << "Running check_zerocoinspend_test...\n";

    //load our serialized pubcoin
    CBigNum bnpubcoin;
    BOOST_CHECK_MESSAGE(bnpubcoin.SetHexBool(rawTxpub1), "Failed to set CBigNum from hex string");
    libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(), bnpubcoin, libzerocoin::CoinDenomination::ZQ_LOVELACE);
    BOOST_CHECK_MESSAGE(pubCoin.validate(), "Failed to validate pubCoin created from hex string");

    //initialize and Accumulator and AccumulatorWitness
    libzerocoin::Accumulator accumulator(Params().Zerocoin_Params(), libzerocoin::CoinDenomination::ZQ_LOVELACE);
    libzerocoin::AccumulatorWitness witness(Params().Zerocoin_Params(), accumulator, pubCoin);

    //populate the witness and accumulators
    CValidationState state;
    for(pair<string, string> raw : vecRawMints) {
        CTransaction tx;
        BOOST_CHECK_MESSAGE(DecodeHexTx(tx, raw.first), "Failed to deserialize hex transaction");

        for(const CTxOut out : tx.vout){
            if(!out.scriptPubKey.empty() && out.scriptPubKey.IsZerocoinMint()) {
                libzerocoin::PublicCoin publicCoin(Params().Zerocoin_Params());
                BOOST_CHECK_MESSAGE(TxOutToPublicCoin(out, publicCoin, state), "Failed to convert CTxOut " << out.ToString() << " to PublicCoin");

                accumulator += publicCoin;
                witness += publicCoin;
            }
        }
    }

    //spend our minted Zerocoin
    CZerocoinMint zerocoinMint;
    zerocoinMint.SetRandomness(CBigNum(rawTxRand1));
    zerocoinMint.SetSerialNumber(CBigNum(rawTxSerial1));
    libzerocoin::SpendMetaData metaData(0, 0);
    // Create a New Zerocoin with specific denomination given by pubCoin
    libzerocoin::PrivateCoin privateCoin(Params().Zerocoin_Params(), pubCoin.getDenomination());
    privateCoin.setPublicCoin(pubCoin);
    privateCoin.setRandomness(zerocoinMint.GetRandomness());
    privateCoin.setSerialNumber(zerocoinMint.GetSerialNumber());
    libzerocoin::CoinSpend coinSpend(Params().Zerocoin_Params(), privateCoin, accumulator, witness, metaData);

    CBigNum serial = coinSpend.getCoinSerialNumber();
    BOOST_CHECK_MESSAGE(serial, "Serial Number can't be 0");

    libzerocoin::CoinDenomination denom = coinSpend.getDenomination();
    BOOST_CHECK_MESSAGE(denom == pubCoin.getDenomination(), "Spend denomination must match original pubCoin");

    
    BOOST_CHECK_MESSAGE(coinSpend.Verify(accumulator, metaData), "CoinSpend object failed to validate");

    //serialize the spend
    CDataStream serializedCoinSpend2(SER_NETWORK, PROTOCOL_VERSION);
    serializedCoinSpend2 << coinSpend;
    std::vector<unsigned char> data(serializedCoinSpend2.begin(), serializedCoinSpend2.end());

    /** Check valid spend */
    CTxIn newTxIn;
    newTxIn.nSequence = 1;
    newTxIn.scriptSig = CScript() << OP_ZEROCOINSPEND << data.size();
    newTxIn.scriptSig.insert(newTxIn.scriptSig.end(), data.begin(), data.end());
    newTxIn.prevout.SetNull();

    CScript script;
    CTxOut txOut(1 * COIN, script);

    CTransaction txNew;
    txNew.vin.push_back(newTxIn);
    txNew.vout.push_back(txOut);

    bool passedTest = true;
    for(const CTxOut& txout : txNew.vout) {
        if (!CheckZerocoinSpendNoDB(txNew.GetHash(), txout, txNew.vin, accumulator, state)) {
            passedTest = false;
            cout << state.GetRejectReason() << endl;
        }

    }
    BOOST_CHECK_MESSAGE(passedTest, "Valid zerocoinspend transaction did not pass checks");
    
    /**check an overspend*/
    CTxOut txOutOverSpend(100 * COIN, script);
    CTransaction txOverSpend;
    txOverSpend.vin.push_back(newTxIn);
    txOverSpend.vout.push_back(txOutOverSpend);
    bool detectedOverSpend = false;

    for(const CTxOut& txout : txOverSpend.vout) {
        if(!CheckZerocoinSpendNoDB(txOverSpend.GetHash(), txout, txOverSpend.vin, accumulator, state))
            detectedOverSpend = true;
    }

    BOOST_CHECK_MESSAGE(detectedOverSpend, "Zerocoinspend checks did not detect overspend");

}


BOOST_AUTO_TEST_CASE(setup_exceptions_test)
{
    cout << "Running check_unitialized parameters,etc for setup exceptions...\n";

    CBigNum bnpubcoin;
    BOOST_CHECK(bnpubcoin.SetHexBool(rawTxpub1));

    // Check Modulus > 1023 Exception
    try {
        libzerocoin::Params ZCParams(bnpubcoin);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception:  ZerocoinException: Modulus must be at least 1023 bit");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception: ZerocoinException: Modulus must be at least 1023 bit");
    }

    // Check Security Level < 80 Exception
    try {
        libzerocoin::Params ZCParams(bnpubcoin,1);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception:  Security Level >= 80");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception: ZerocoinException: Security Level >= 80");
    }

    // Check unitialized params Exception for PublicCoin
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PublicCoin pubCoin(ZCParams, libzerocoin::CoinDenomination::ZQ_LOVELACE);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    // Check unitialized params Exception for PublicCoin (alternate constructor)
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PublicCoin pubCoin(ZCParams);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    // Check unitialized params Exception for PrivateCoin
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PrivateCoin privCoin(ZCParams, libzerocoin::CoinDenomination::ZQ_LOVELACE);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    
}


BOOST_AUTO_TEST_CASE(setup_exceptions_test)
{
    cout << "Running check_unitialized parameters,etc for setup exceptions...\n";

    CBigNum bnpubcoin;
    BOOST_CHECK(bnpubcoin.SetHexBool(rawTxpub1));

    // Check Modulus > 1023 Exception
    try {
        libzerocoin::Params ZCParams(bnpubcoin);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception:  ZerocoinException: Modulus must be at least 1023 bit");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception: ZerocoinException: Modulus must be at least 1023 bit");
    }

    // Check Security Level < 80 Exception
    try {
        libzerocoin::Params ZCParams(bnpubcoin,1);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception:  Security Level >= 80");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception: ZerocoinException: Security Level >= 80");
    }

    // Check unitialized params Exception for PublicCoin
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PublicCoin pubCoin(ZCParams, libzerocoin::CoinDenomination::ZQ_LOVELACE);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    // Check unitialized params Exception for PublicCoin (alternate constructor)
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PublicCoin pubCoin(ZCParams);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    // Check unitialized params Exception for PrivateCoin
    try {
        SelectParams(CBaseChainParams::MAIN);
        libzerocoin::Params* ZCParams = Params().Zerocoin_Params();
        ZCParams->initialized = false;
        libzerocoin::PrivateCoin privCoin(ZCParams, libzerocoin::CoinDenomination::ZQ_LOVELACE);
        BOOST_CHECK_MESSAGE(false, "Didn't catch exception checking for uninitialized Params");
    }
    catch (...) {
        BOOST_CHECK_MESSAGE(true, "Caught exception checking for initalized Params");
    }

    
}

BOOST_AUTO_TEST_SUITE_END()
