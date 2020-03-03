// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <node/transaction.h>
#include <rpc/server.h>
#include <servicenode/servicenode.h>
#define protected public
#include <servicenode/servicenodemgr.h>
#undef protected
#include <wallet/coincontrol.h>
#include <xbridge/xbridgeapp.h>

sn::ServiceNode snodeNetwork(const CPubKey & snodePubKey, const uint8_t & tier, const CKeyID & paymentAddr,
                         const std::vector<COutPoint> & collateral, const uint32_t & blockNumber,
                         const uint256 & blockHash, const std::vector<unsigned char> & sig)
{
    auto ss = CDataStream(SER_NETWORK, PROTOCOL_VERSION);
    ss << snodePubKey << tier << paymentAddr << collateral << blockNumber << blockHash << sig;
    sn::ServiceNode snode; ss >> snode;
    return snode;
}

/**
 * Save configuration files to the specified path.
 */
void saveFile(const boost::filesystem::path& p, const std::string& str) {
    boost::filesystem::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.open(p, std::ios_base::binary);
    file.write(str.c_str(), str.size());
}

void cleanupSn() {
    sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
    sn::ServiceNodeMgr::instance().reset();
    mempool.clear();
}

bool ServiceNodeSetupFixtureSetup{false};
struct ServiceNodeSetupFixture {
    explicit ServiceNodeSetupFixture() {
        if (ServiceNodeSetupFixtureSetup) return; ServiceNodeSetupFixtureSetup = true;
        chain_1000_50();
        chain_1250_50();
    }
    void chain_1000_50() {
        auto pos = std::make_shared<TestChainPoS>(false);
        auto *params = (CChainParams*)&Params();
        params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 1000 * COIN;
            return 50 * COIN;
        };
        pos->Init("1000,50");
        pos.reset();
    }
    void chain_1250_50() {
        auto pos = std::make_shared<TestChainPoS>(false);
        auto *params = (CChainParams*)&Params();
        params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 1250 * COIN;
            return 50 * COIN;
        };
        pos->Init("1250,50");
        pos.reset();
    }
};

BOOST_FIXTURE_TEST_SUITE(servicenode_tests, ServiceNodeSetupFixture)

/// Check case where servicenode is properly validated under normal circumstances
BOOST_AUTO_TEST_CASE(servicenode_tests_isvalid)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    pos.Init("1000,50");

    const auto snodePubKey = pos.coinbaseKey.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : pos.m_coinbase_txns) {
        CTransactionRef txx;
        if (!GetTxFunc({tx->GetHash(), 0}, txx)) // make sure tx exists
            continue;
        totalAmount += tx->vout[0].nValue;
        collateral.emplace_back(tx->GetHash(), 0);
        if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
            break;
    }

    // Generate the signature from sig hash
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode;
    BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
    BOOST_CHECK(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));

    cleanupSn();
    pos_ptr.reset();
}

/// Check open tier case
BOOST_FIXTURE_TEST_CASE(servicenode_tests_opentier, TestChainPoS)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::OPEN;
    const auto collateral = std::vector<COutPoint>();

    // Valid check
    {
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        // TODO Blocknet OPEN tier snodes, support non-SPV snode tiers (invert the isValid check below)
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "OPEN tier should not be supported at this time");
    }

    // Case where wrong key is used to generate sig. For the open tier the snode private key
    // must be used to generate the signature. In this test we use another key.
    {
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));  // use invalid coinbase key (invalid for open tier)
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Failed on invalid snode key sig");
    }

    cleanupSn();
}

/// Check case where duplicate collateral utxos are used
BOOST_FIXTURE_TEST_CASE(servicenode_tests_duplicate_collateral, TestChainPoS)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    // Assumes total input amounts below adds up to ServiceNode::COLLATERAL_SPV
    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    while (totalAmount < sn::ServiceNode::COLLATERAL_SPV) {
        collateral.emplace_back(m_coinbase_txns[0]->GetHash(), 0);
        totalAmount += m_coinbase_txns[0]->GetValueOut();
    }

    // Generate the signature from sig hash
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode;
    BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
    BOOST_CHECK(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));

    cleanupSn();
}

/// Check case where there's not enough snode inputs
BOOST_FIXTURE_TEST_CASE(servicenode_tests_insufficient_collateral, TestChainPoS)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    // Assumes total input amounts below adds up to ServiceNode::COLLATERAL_SPV
    std::vector<COutPoint> collateral;
    collateral.emplace_back(m_coinbase_txns[0]->GetHash(), 0);
    BOOST_CHECK(m_coinbase_txns[0]->GetValueOut() < sn::ServiceNode::COLLATERAL_SPV);

    // Generate the signature from sig hash
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode;
    BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
    BOOST_CHECK(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));

    cleanupSn();
}

/// Check case where collateral inputs are spent
BOOST_AUTO_TEST_CASE(servicenode_tests_spent_collateral)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    pos.Init("1000,50");
    pos.StakeBlocks(5), SyncWithValidationInterfaceQueue();

    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    CBasicKeyStore keystore; // temp used to spend inputs
    keystore.AddKey(pos.coinbaseKey);

    // Spend inputs that would be used in snode collateral
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 500 * COIN);
        }
        // Spend the first available input in "coins"
        auto c = coins[0];
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(c.GetInputCoin().outpoint);
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = GetScriptForRawPubKey(snodePubKey);
        mtx.vout[0].nValue = c.GetInputCoin().txout.nValue - CENT;
        SignatureData sigdata = DataFromTransaction(mtx, 0, c.GetInputCoin().txout);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, c.GetInputCoin().txout.nValue, SIGHASH_ALL), c.GetInputCoin().txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send snode collateral spent tx: %s", errstr));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, chainActive.Tip(), params->GetConsensus()));
        BOOST_CHECK_MESSAGE(block.vtx.size() >= 3 && block.vtx[2]->GetHash() == mtx.GetHash(), "Expected transaction to be included in latest block");
        Coin cn;
        BOOST_CHECK_MESSAGE(!pcoinsTip->GetCoin(c.GetInputCoin().outpoint, cn), "Coin should be spent here");

        CAmount totalAmount{0};
        std::vector<COutPoint> collateral;
        for (int i = 0; i < coins.size(); ++i) {
            const auto & coin = coins[i];
            const auto txn = coin.tx->tx;
            totalAmount += coin.GetInputCoin().txout.nValue;
            collateral.emplace_back(txn->GetHash(), coin.i);
            if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
                break;
        }

        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Should fail on spent collateral");

        cleanupSn();
    }

    // Check case where spent collateral is in mempool
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 500 * COIN);
        }
        // Spend one of the collateral inputs (spend the 2nd coinbase input, b/c first was spent above)
        COutput c = coins[0];
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(c.GetInputCoin().outpoint);
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = GetScriptForRawPubKey(snodePubKey);
        mtx.vout[0].nValue = c.GetInputCoin().txout.nValue - CENT;
        SignatureData sigdata = DataFromTransaction(mtx, 0, c.GetInputCoin().txout);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, c.GetInputCoin().txout.nValue, SIGHASH_ALL), c.GetInputCoin().txout.scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);

        {
            CValidationState state;
            LOCK(cs_main);
            BOOST_CHECK(AcceptToMemoryPool(mempool, state, MakeTransactionRef(mtx), nullptr, nullptr, false, 0));
        }
        CAmount totalAmount{0};
        std::vector<COutPoint> collateral;
        for (int i = 0; i < coins.size(); ++i) { // start at 1 (ignore first spent coinbase)
            const auto & coin = coins[i];
            const auto txn = coin.tx->tx;
            totalAmount += coin.GetInputCoin().txout.nValue;
            collateral.emplace_back(txn->GetHash(), coin.i);
            if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
                break;
        }

        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Should not fail on spent collateral in mempool");

        cleanupSn();
    }

    // Servicenode should be marked invalid if collateral is spent
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 500 * COIN);
        }
        CAmount totalAmount{0};
        std::vector<COutPoint> collateral;
        for (int i = 1; i < coins.size(); ++i) { // start at 1 (ignore first spent coinbase)
            const auto & coin = coins[i];
            const auto txn = coin.tx->tx;
            totalAmount += coin.GetInputCoin().txout.nValue;
            collateral.emplace_back(txn->GetHash(), coin.i);
            if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
                break;
        }
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode s;
        BOOST_CHECK_NO_THROW(s = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << s;
        sn::ServiceNode s2;
        auto success = sn::ServiceNodeMgr::instance().processRegistration(ss, s2);
        BOOST_CHECK_MESSAGE(success, "snode registration should succeed");

        const auto snode = sn::ServiceNodeMgr::instance().getSn(snodePubKey);
        BOOST_CHECK_MESSAGE(!snode.isNull(), "snode registration should succeed");
        if (!snode.isNull()) {
            RegisterValidationInterface(&sn::ServiceNodeMgr::instance());
            const auto firstUtxo = snode.getCollateral().front();
            CTransactionRef tx; uint256 hashBlock;
            BOOST_CHECK_MESSAGE(GetTransaction(firstUtxo.hash, tx, Params().GetConsensus(), hashBlock), "failed to get snode collateral");
            CMutableTransaction mtx;
            mtx.vin.resize(1); mtx.vout.resize(1);
            mtx.vin[0] = CTxIn(firstUtxo);
            mtx.vout[0] = CTxOut(tx->vout[firstUtxo.n].nValue - CENT, tx->vout[firstUtxo.n].scriptPubKey);
            SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[firstUtxo.n]);
            ProduceSignature(*pos.wallet, MutableTransactionSignatureCreator(&mtx, 0, tx->vout[firstUtxo.n].nValue, SIGHASH_ALL), tx->vout[firstUtxo.n].scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            // Send transaction
            uint256 txid; std::string errstr; const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
            BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to spend snode collateral: %s", errstr));
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            const auto checkSnode = sn::ServiceNodeMgr::instance().getSn(snodePubKey);
            BOOST_CHECK_MESSAGE(checkSnode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "snode should be valid because collateral was spent but we're still in grace period");
            BOOST_CHECK_MESSAGE(checkSnode.getInvalid(), "snode should be marked invalid in the validation interface event (connect block)");
            BOOST_CHECK_MESSAGE(checkSnode.getInvalidBlockNumber() == chainActive.Height(), "snode invalid block number should match chain tip");
            pos.StakeBlocks(sn::ServiceNode::VALID_GRACEPERIOD_BLOCKS), SyncWithValidationInterfaceQueue(); // make sure snode grace period expires
            BOOST_CHECK_MESSAGE(!checkSnode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "snode should be invalid because collateral was spent and grace period expired");
            UnregisterValidationInterface(&sn::ServiceNodeMgr::instance());
        }

        cleanupSn();
    }
    pos_ptr.reset();
}

/// Check case where servicenode is re-registered on spent collateral
BOOST_AUTO_TEST_CASE(servicenode_tests_reregister_onspend)
{
    gArgs.SoftSetBoolArg("-servicenode", true);
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    params->consensus.coinMaturity = 10;
    pos.Init("1000,50");
    sn::ServiceNodeMgr::instance().reset();
    RegisterValidationInterface(&sn::ServiceNodeMgr::instance());

    CKey key; key.MakeNewKey(true);
    const auto saddr = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);

    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    otherwallet->SetBroadcastTransactions(true);
    AddKey(*otherwallet, key);
    AddWallet(otherwallet); // add wallet to global mgr
    RegisterValidationInterface(otherwallet.get());

    CBasicKeyStore keystore; // temp used to spend inputs
    keystore.AddKey(key);
    keystore.AddKey(pos.coinbaseKey);

    // Check that snode registration automatically happens after spent utxo detected
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 1000 * COIN);
        }
        std::vector<COutPoint> collateral;
        CAmount collateralTotal{0};
        for (auto & c : coins) {
            if (collateralTotal >= sn::ServiceNode::COLLATERAL_SPV)
                break;
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(c.GetInputCoin().outpoint);
            mtx.vout.resize(1);
            mtx.vout[0].scriptPubKey = GetScriptForDestination(saddr);
            mtx.vout[0].nValue = c.GetInputCoin().txout.nValue - CENT;
            SignatureData sigdata = DataFromTransaction(mtx, 0, c.GetInputCoin().txout);
            ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, c.GetInputCoin().txout.nValue, SIGHASH_ALL), c.GetInputCoin().txout.scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            // Send transaction
            uint256 txid; std::string errstr;
            const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
            BOOST_REQUIRE_MESSAGE(err == TransactionError::OK, strprintf("Failed to send snode collateral tx: %s", errstr));
            collateral.emplace_back(mtx.GetHash(), 0);
            collateralTotal += mtx.vout[0].nValue;
        }
        pos.StakeBlocks(params->GetConsensus().coinMaturity), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());

        // Setup snode
        UniValue rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ EncodeDestination(saddr), "snode0" });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));
        const auto snodeEntry = sn::ServiceNodeMgr::instance().getActiveSn();
        const auto snode = sn::ServiceNodeMgr::instance().getSn(snodeEntry.key.GetPubKey());
        // Find collateral that is spendable
        COutPoint selUtxo;
        for (const auto & col : snode.getCollateral()) {
            LOCK(cs_main);
            Coin c;
            if (pcoinsTip->GetCoin(col, c) && c.nHeight >= params->GetConsensus().coinMaturity) {
                selUtxo = col;
                break;
            }
        }
        BOOST_REQUIRE_MESSAGE(!selUtxo.IsNull(), "Failed to find collateral utxo");

        // Spend one of the collateral utxos
        CTransactionRef tx; uint256 hashBlock;
        BOOST_CHECK_MESSAGE(GetTransaction(selUtxo.hash, tx, Params().GetConsensus(), hashBlock), "failed to get snode collateral");
        CMutableTransaction mtx;
        mtx.vin.resize(1); mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(selUtxo);
        mtx.vout[0] = CTxOut(tx->vout[selUtxo.n].nValue - CENT, tx->vout[selUtxo.n].scriptPubKey);
        SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[selUtxo.n]);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, tx->vout[selUtxo.n].nValue, SIGHASH_ALL), tx->vout[selUtxo.n].scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        uint256 txid; std::string errstr; const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to spend snode collateral: %s", errstr));
        pos.StakeBlocks(2), SyncWithValidationInterfaceQueue();

        const auto checkSnode = sn::ServiceNodeMgr::instance().getSn(snodeEntry.key.GetPubKey());
        BOOST_CHECK_MESSAGE(checkSnode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "snode should be auto-registered after spent utxo detected (2 confirmations)");
        // make sure spent collateral not in the new registration
        for (const auto & utxo : checkSnode.getCollateral())
            BOOST_CHECK_MESSAGE(utxo != selUtxo, "snode spent utxo should not exist after new registration");
    }

    UnregisterValidationInterface(otherwallet.get());
    RemoveWallet(otherwallet);
    UnregisterValidationInterface(&sn::ServiceNodeMgr::instance());
    gArgs.SoftSetBoolArg("-servicenode", false);
    cleanupSn();
    pos_ptr.reset();
}

/// Check case where servicenode is valid on reorg (block disconnect)
BOOST_AUTO_TEST_CASE(servicenode_tests_valid_onreorg)
{
    gArgs.SoftSetBoolArg("-servicenode", true);
    TestChainPoS pos(false);
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    params->consensus.coinMaturity = 10;
    pos.Init("1000,50");
    sn::ServiceNodeMgr::instance().reset();
    RegisterValidationInterface(&sn::ServiceNodeMgr::instance());

    CKey key; key.MakeNewKey(true);
    const auto saddr = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);

    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    otherwallet->SetBroadcastTransactions(true);
    AddKey(*otherwallet, key);
    AddWallet(otherwallet); // add wallet to global mgr
    RegisterValidationInterface(otherwallet.get());

    CBasicKeyStore keystore; // temp used to spend inputs
    keystore.AddKey(key);
    keystore.AddKey(pos.coinbaseKey);

    // Check that snode is valid after spent collateral is orphaned
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 1000 * COIN);
        }
        std::vector<COutPoint> collateral;
        CAmount collateralTotal{0};
        for (auto & c : coins) {
            if (collateralTotal >= sn::ServiceNode::COLLATERAL_SPV)
                break;
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(c.GetInputCoin().outpoint);
            mtx.vout.resize(1);
            mtx.vout[0].scriptPubKey = GetScriptForDestination(saddr);
            mtx.vout[0].nValue = c.GetInputCoin().txout.nValue - CENT;
            SignatureData sigdata = DataFromTransaction(mtx, 0, c.GetInputCoin().txout);
            ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, c.GetInputCoin().txout.nValue, SIGHASH_ALL), c.GetInputCoin().txout.scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            // Send transaction
            uint256 txid; std::string errstr;
            const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
            BOOST_REQUIRE_MESSAGE(err == TransactionError::OK, strprintf("Failed to send snode collateral tx: %s", errstr));
            collateral.emplace_back(mtx.GetHash(), 0);
            collateralTotal += mtx.vout[0].nValue;
        }
        pos.StakeBlocks(params->GetConsensus().coinMaturity), SyncWithValidationInterfaceQueue();
        rescanWallet(otherwallet.get());

        // Setup snode
        UniValue rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ EncodeDestination(saddr), "snode0" });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));
        const auto snodeEntry = sn::ServiceNodeMgr::instance().getActiveSn();
        const auto snode = sn::ServiceNodeMgr::instance().getSn(snodeEntry.key.GetPubKey());
        // Find collateral that is spendable
        COutPoint selUtxo;
        for (const auto & col : snode.getCollateral()) {
            Coin c;
            if (pcoinsTip->GetCoin(col, c) && c.nHeight >= params->GetConsensus().coinMaturity) {
                selUtxo = col;
                break;
            }
        }
        BOOST_REQUIRE_MESSAGE(!selUtxo.IsNull(), "Failed to find collateral utxo");

        // Simulate that the snode is someone elses, remove it from our local state
        sn::ServiceNodeMgr::instance().removeSnEntry(snodeEntry);
        auto ss = CDataStream(SER_NETWORK, PROTOCOL_VERSION);
        ss << snode;
        sn::ServiceNode snodetmp;
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().processRegistration(ss, snodetmp), "failed to process snode registration");

        // Spend one of the collateral utxos
        CTransactionRef tx; uint256 hashBlock;
        BOOST_CHECK_MESSAGE(GetTransaction(selUtxo.hash, tx, params->GetConsensus(), hashBlock), "failed to get snode collateral");
        CMutableTransaction mtx;
        mtx.vin.resize(1); mtx.vout.resize(1);
        mtx.vin[0] = CTxIn(selUtxo);
        mtx.vout[0] = CTxOut(tx->vout[selUtxo.n].nValue - CENT, tx->vout[selUtxo.n].scriptPubKey);
        SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[selUtxo.n]);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, tx->vout[selUtxo.n].nValue, SIGHASH_ALL), tx->vout[selUtxo.n].scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        uint256 txid; std::string errstr; const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to spend snode collateral: %s", errstr));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        pos.StakeBlocks(sn::ServiceNode::VALID_GRACEPERIOD_BLOCKS), SyncWithValidationInterfaceQueue();
        const auto checkSnode = sn::ServiceNodeMgr::instance().getSn(snodeEntry.key.GetPubKey());
        BOOST_CHECK_MESSAGE(checkSnode.getInvalid(), "snode should be marked invalid since collateral was spent");
        BOOST_CHECK_MESSAGE(!checkSnode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "snode should be invalid");

        // Now disconnect spent collateral blocks and verify that snode is still valid
        CValidationState state;
        for (int i = 0; i <= sn::ServiceNode::VALID_GRACEPERIOD_BLOCKS; ++i)
            InvalidateBlock(state, *params, chainActive.Tip(), false);
        SyncWithValidationInterfaceQueue();
        const auto checkSnode2 = sn::ServiceNodeMgr::instance().getSn(snodeEntry.key.GetPubKey());
        BOOST_CHECK_MESSAGE(checkSnode2.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "snode should still be valid after block disconnects");
    }

    UnregisterValidationInterface(otherwallet.get());
    RemoveWallet(otherwallet);
    UnregisterValidationInterface(&sn::ServiceNodeMgr::instance());
    gArgs.SoftSetBoolArg("-servicenode", false);
    cleanupSn();
}

/// Check case where collateral inputs are immature
BOOST_AUTO_TEST_CASE(servicenode_tests_immature_collateral)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    params->consensus.coinMaturity = 10;
    pos.Init("1000,50");

    gArgs.SoftSetBoolArg("-servicenode", true);

    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;
    CTxDestination sdest = GetDestinationForKey(snodePubKey, OutputType::LEGACY);

    CBasicKeyStore keystore; // temp used to spend inputs
    keystore.AddKey(pos.coinbaseKey);
    keystore.AddKey(key);

    bool firstRun;
    auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
    otherwallet->LoadWallet(firstRun);
    otherwallet->SetBroadcastTransactions(true);
    AddKey(*otherwallet, key);
    AddWallet(otherwallet); // add wallet to global mgr
    RegisterValidationInterface(otherwallet.get());

    // Test registering snode with immature inputs
    {
        std::vector<COutput> coins;
        {
            LOCK2(cs_main, pos.wallet->cs_wallet);
            pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 1000 * COIN);
        }

        std::vector<COutPoint> collateral;
        CAmount collateralTotal{0};
        for (auto & c : coins) {
            if (collateralTotal >= sn::ServiceNode::COLLATERAL_SPV)
                break;
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0] = CTxIn(c.GetInputCoin().outpoint);
            mtx.vout.resize(1);
            mtx.vout[0].scriptPubKey = GetScriptForDestination(sdest);
            mtx.vout[0].nValue = c.GetInputCoin().txout.nValue - CENT;
            SignatureData sigdata = DataFromTransaction(mtx, 0, c.GetInputCoin().txout);
            ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, c.GetInputCoin().txout.nValue, SIGHASH_ALL), c.GetInputCoin().txout.scriptPubKey, sigdata);
            UpdateInput(mtx.vin[0], sigdata);
            // Send transaction
            uint256 txid; std::string errstr;
            const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
            BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send snode collateral tx: %s", errstr));
            collateral.emplace_back(mtx.GetHash(), 0);
            collateralTotal += mtx.vout[0].nValue;
        }
        // Stake 2 blocks since that's minimum snode collateral confirmations
        pos.StakeBlocks(2), SyncWithValidationInterfaceQueue(), rescanWallet(otherwallet.get());
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Service node should be valid with 1 confirmation on collateral");
        // Register the snode
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().registerSn(key, sn::ServiceNode::SPV, EncodeDestination(sdest), g_connman.get(), {otherwallet}), "Service node should register on immature collateral");
        sn::ServiceNodeConfigEntry entry("snode0", sn::ServiceNode::SPV, key, sdest);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>{entry});
        std::set<sn::ServiceNodeConfigEntry> entries;
        sn::ServiceNodeMgr::instance().loadSnConfig(entries);
    }

    // Test that snode auto-registration occurs when one of the collateral inputs is spent/staked
    {
        // make sure coin isn't immature so we can spend it
        pos.StakeBlocks(params->GetConsensus().coinMaturity), SyncWithValidationInterfaceQueue();

        RegisterValidationInterface(&sn::ServiceNodeMgr::instance());
        auto snode = sn::ServiceNodeMgr::instance().getSn(snodePubKey);
        BOOST_CHECK_MESSAGE(!snode.isNull(), "Service node should not be null");
        const auto collateral0 = snode.getCollateral()[0];
        CTransactionRef tx; uint256 block;
        BOOST_CHECK(GetTransaction(collateral0.hash, tx, params->GetConsensus(), block));

        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(collateral0);
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = tx->vout[0].scriptPubKey; // must spend back to same snode collateral address
        mtx.vout[0].nValue = tx->vout[0].nValue - CENT;
        SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[0]);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, tx->vout[0].nValue, SIGHASH_ALL), tx->vout[0].scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        // Send transaction
        uint256 txid; std::string errstr;
        const TransactionError err = BroadcastTransaction(MakeTransactionRef(mtx), txid, errstr, 0);
        BOOST_CHECK_MESSAGE(err == TransactionError::OK, strprintf("Failed to send snode collateral tx: %s", errstr));
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
        xbridge::App::instance().utAddXWallets({"BLOCK","BTC","LTC"});
        const auto & jservices = xbridge::App::instance().myServicesJSON();
        auto success = sn::ServiceNodeMgr::instance().sendPing(50, jservices, g_connman.get());
        BOOST_CHECK_MESSAGE(success, "Refresh snode ping before running state check");
        auto running = sn::ServiceNodeMgr::instance().getSn(snodePubKey).running();
        BOOST_CHECK_MESSAGE(running, "Service node with recently spent collateral in grace period should still be in running state");
        pos.StakeBlocks(sn::ServiceNode::VALID_GRACEPERIOD_BLOCKS), SyncWithValidationInterfaceQueue();
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().getSn(snodePubKey).isValid(GetTxFunc, IsServiceNodeBlockValidFunc),  "Service node with recently staked collateral should be valid");
        UnregisterValidationInterface(&sn::ServiceNodeMgr::instance());
    }

    UnregisterValidationInterface(otherwallet.get());
    RemoveWallet(otherwallet);
    cleanupSn();
    gArgs.SoftSetBoolArg("-servicenode", false);
    pos_ptr.reset();
}

/// Servicenode registration and ping tests
BOOST_AUTO_TEST_CASE(servicenode_tests_registration_pings)
{
    gArgs.SoftSetBoolArg("-servicenode", true);
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    params->consensus.coinMaturity = 10;
    pos.Init("1000,50");

    CTxDestination dest(pos.coinbaseKey.GetPubKey().GetID());
    auto & smgr = sn::ServiceNodeMgr::instance();

    // Snode registration and ping w/ uncompressed key
    {
        CKey key; key.MakeNewKey(false);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register snode w/ uncompressed key");
        // Snode ping w/ uncompressed key
        sn::ServiceNodeConfigEntry entry("snode0", sn::ServiceNode::SPV, key, dest);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>{entry});
        std::set<sn::ServiceNodeConfigEntry> entries;
        smgr.loadSnConfig(entries);
        xbridge::App::instance().utAddXWallets({"BLOCK","BTC","LTC"});
        const auto & jservices = xbridge::App::instance().myServicesJSON();
        auto success = smgr.sendPing(50, jservices, g_connman.get());
        BOOST_CHECK_MESSAGE(success, "Snode ping w/ uncompressed key");
        BOOST_CHECK(smgr.list().size() == 1);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Snode registration and ping w/ compressed key
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register snode w/ compressed key");
        // Snode ping w/ compressed key
        sn::ServiceNodeConfigEntry entry("snode1", sn::ServiceNode::SPV, key, dest);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>{entry});
        std::set<sn::ServiceNodeConfigEntry> entries;
        smgr.loadSnConfig(entries);
        xbridge::App::instance().utAddXWallets({"BLOCK","BTC","LTC"});
        const auto & jservices = xbridge::App::instance().myServicesJSON();
        auto success = smgr.sendPing(50, jservices, g_connman.get());
        BOOST_CHECK_MESSAGE(success, "Snode ping w/ compressed key");
        BOOST_CHECK(smgr.list().size() == 1);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check servicenoderegister all rpc
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(pos.coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr, "snode0" });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check servicenoderegister by alias rpc
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(pos.coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr, "snode1" });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ "snode1" });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check servicenoderegister rpc result data
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(pos.coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr, "snode1" });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        try {
            auto result = CallRPC2("servicenoderegister", rpcparams);
            BOOST_CHECK_EQUAL(result.isArray(), true);
            UniValue o = result[0];
            BOOST_CHECK_EQUAL(find_value(o, "alias").get_str(), "snode1");
            BOOST_CHECK_EQUAL(find_value(o, "tier").get_str(), sn::ServiceNodeMgr::tierString(sn::ServiceNode::SPV));
            BOOST_CHECK_EQUAL(find_value(o, "snodekey").get_str().empty(), false); // check not empty
            BOOST_CHECK_EQUAL(find_value(o, "snodeprivkey").get_str().empty(), false); // check not empty
            BOOST_CHECK(DecodeSecret(find_value(o, "snodeprivkey").get_str()).IsValid()); // check validity
            BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        } catch (std::exception & e) {
            BOOST_CHECK_MESSAGE(false, strprintf("servicenoderegister failed: %s", e.what()));
        }

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check servicenoderegister bad alias
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(pos.coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ "bad_alias" });
        BOOST_CHECK_THROW(CallRPC2("servicenoderegister", rpcparams), std::runtime_error);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check servicenoderegister no configs
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(pos.coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue rpcparams(UniValue::VARR);
        BOOST_CHECK_THROW(CallRPC2("servicenoderegister", rpcparams), std::runtime_error);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check valid snode ping
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register SPV tier snode");
        const auto bestBlock = chainActive.Height();
        const auto bestBlockHash = chainActive[bestBlock]->GetBlockHash();
        auto snode = smgr.getSn(key.GetPubKey());
        sn::ServiceNodePing pingValid(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()),
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        pingValid.sign(key);
        BOOST_CHECK_MESSAGE(pingValid.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Service node ping should be valid for open tier xrs services");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check invalid snode ping (empty/missing config)
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register SPV tier snode");
        const auto bestBlock = chainActive.Height();
        const auto bestBlockHash = chainActive[bestBlock]->GetBlockHash();
        auto snode = smgr.getSn(key.GetPubKey());
        sn::ServiceNodePing pingInvalid(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()), "", snode);
        pingInvalid.sign(key);
        BOOST_CHECK_MESSAGE(!pingInvalid.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Service node ping should be invalid for missing config");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check snode addping
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register SPV tier snode");
        const auto bestBlock = chainActive.Height();
        const auto bestBlockHash = chainActive[bestBlock]->GetBlockHash();
        auto snode = smgr.getSn(key.GetPubKey());
        // Normal add ping should succeed
        sn::ServiceNodePing ping(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()),
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping.sign(key);
        BOOST_CHECK_MESSAGE(smgr.addPing(ping), "addPing should succeed");
        // Ping in past should fail
        sn::ServiceNodePing ping2(key.GetPubKey(), bestBlock, bestBlockHash, ping.getPingTime() - 1000,
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping2.sign(key);
        BOOST_CHECK_MESSAGE(!smgr.addPing(ping2), "addPing should fail on ping with time prior to latest known ping");
        // Ping with future time should succeed
        sn::ServiceNodePing ping3(key.GetPubKey(), bestBlock, bestBlockHash, ping.getPingTime() + 10000,
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping3.sign(key);
        BOOST_CHECK_MESSAGE(smgr.addPing(ping3), "addPing should succeed for a future time");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // Check snode processPing
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get(), {pos.wallet}), "Register SPV tier snode");
        const auto bestBlock = chainActive.Height();
        const auto bestBlockHash = chainActive[bestBlock]->GetBlockHash();
        auto snode = smgr.getSn(key.GetPubKey());
        // Normal add ping should succeed
        sn::ServiceNodePing ping(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()),
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping.sign(key);
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION); ss << ping;
        sn::ServiceNodePing pping;
        BOOST_CHECK_MESSAGE(smgr.processPing(ss, pping), "processPing should succeed");
        // Ping in past should fail
        sn::ServiceNodePing ping2(key.GetPubKey(), bestBlock, bestBlockHash, ping.getPingTime() - 1000,
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping2.sign(key);
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION); ss2 << ping2;
        sn::ServiceNodePing pping2;
        BOOST_CHECK_MESSAGE(!smgr.processPing(ss2, pping2), "processPing should fail on ping with time prior to latest known ping");
        // Ping with future time should succeed
        sn::ServiceNodePing ping3(key.GetPubKey(), bestBlock, bestBlockHash, ping.getPingTime() + 10000,
                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
        ping3.sign(key);
        CDataStream ss3(SER_NETWORK, PROTOCOL_VERSION); ss3 << ping3;
        sn::ServiceNodePing pping3;
        BOOST_CHECK_MESSAGE(smgr.processPing(ss3, pping3), "processPing should succeed for a future time");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        smgr.reset();
    }

    // TODO Blocknet OPEN tier snodes, support non-SPV snode tiers (enable unit tests)
//    // Snode ping should fail on open tier with xr:: namespace
//    {
//        CKey key; key.MakeNewKey(true);
//        BOOST_CHECK_MESSAGE(smgr.registerSn(key, sn::ServiceNode::OPEN, EncodeDestination(dest), g_connman.get(), {}), "Register OPEN tier snode");
//        const auto bestBlock = chainActive.Height();
//        const auto bestBlockHash = chainActive[bestBlock]->GetBlockHash();
//        auto snode = smgr.getSn(key.GetPubKey());
//        sn::ServiceNodePing pingValid(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()),
//                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
//        pingValid.sign(key);
//        BOOST_CHECK_MESSAGE(pingValid.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Service node ping should be valid for open tier xrs services");
//        sn::ServiceNodePing pingInvalid(key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()),
//                R"({"xbridgeversion":50,"xrouterversion":50,"xrouter":{"config":"[Main]\nwallets=BLOCK,LTC\nplugins=CustomPlugin1,CustomPlugin2\nhost=127.0.0.1", "plugins":{"CustomPlugin1":"","CustomPlugin2":""}}})", snode);
//        pingInvalid.sign(key);
//        BOOST_CHECK_MESSAGE(!pingInvalid.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Service node ping should be invalid for open tier non-xrs services");
//        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
//    }

    gArgs.SoftSetBoolArg("-servicenode", false);
    cleanupSn();
    pos_ptr.reset();
}

/// Check misc cases
BOOST_AUTO_TEST_CASE(servicenode_tests_misc_checks)
{
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1000 * COIN;
        return 50 * COIN;
    };
    pos.Init("1000,50");

    auto & smgr = sn::ServiceNodeMgr::instance();
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();

    std::vector<COutput> coins;
    {
        LOCK2(cs_main, pos.wallet->cs_wallet);
        pos.wallet->AvailableCoins(*pos.locked_chain, coins, true, nullptr, 500 * COIN);
    }
    // sort largest coins first
    std::sort(coins.begin(), coins.end(), [](const COutput & a, const COutput & b) {
        return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
    });
    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & coin : coins) {
        totalAmount += coin.GetInputCoin().txout.nValue;
        collateral.push_back(coin.GetInputCoin().outpoint);
        if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
            break;
    }

    // NOTE** This test must be first!
    BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().list().empty(), "Fail on non-empty snode list");

    // Fail on bad tier
    {
        const uint8_t tier = 0xff;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << tier << collateral;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on bad tier");
    }

    // Fail on empty collateral
    {
        const uint8_t tier = sn::ServiceNode::Tier::SPV;
        std::vector<COutPoint> collateral2;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << tier << collateral2;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral2, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty collateral");
    }

    // Fail on empty snode pubkey
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(CPubKey(), tier, CPubKey().GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty snode pubkey");
    }

    // Fail on empty sighash
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), std::vector<unsigned char>()));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty sighash");
    }

    // Fail on bad best block
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, 0, uint256());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, 0, uint256(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on bad best block");
    }

    // Fail on stale best block (valid but stale block number)
    {
        const int staleBlockNumber = chainActive.Height()-SNODE_STALE_BLOCKS - 1;
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on stale best block");
    }

    // Fail on best block number being too far into future
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height()+5, chainActive[5]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, chainActive.Height()+5, chainActive[5]->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on best block, unknown block, too far in future");
    }

    // Test disabling the stale check on the servicenode validation
    {
        const int staleBlockNumber = chainActive.Height()-SNODE_STALE_BLOCKS - 1;
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, snodePubKey.GetID(), collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(pos.coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode;
        BOOST_CHECK_NO_THROW(snode = snodeNetwork(snodePubKey, tier, snodePubKey.GetID(), collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash(), sig));
        BOOST_CHECK_MESSAGE(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc, false), "Fail on disabled stale check");
    }

    // Test case where snode config doesn't exist on disk
    {
        boost::filesystem::remove(sn::ServiceNodeMgr::getServiceNodeConf());
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load default config");
        BOOST_CHECK_MESSAGE(entries.empty(), "Snode configs should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test snode config for OPEN tier
    {
        const auto & skey = EncodeSecret(key);
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 OPEN " + skey);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load OPEN tier config");
        BOOST_CHECK_MESSAGE(entries.size() == 1, "OPEN tier config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test snode config for SPV tier
    {
        const auto & skey = EncodeSecret(key);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 SPV " + skey + " " + saddr);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load SPV tier config");
        BOOST_CHECK_MESSAGE(entries.size() == 1, "SPV tier config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test snode config for multiple tiers
    {
        const auto & skey = EncodeSecret(key);
        CKey key2; key2.MakeNewKey(true);
        const auto & skey2 = EncodeSecret(key2);
        const auto & saddr2 = EncodeDestination(GetDestinationForKey(key2.GetPubKey(), OutputType::LEGACY));
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 OPEN " + skey + "\n"
                                                           "mn2 SPV " + skey2 + " " + saddr2);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load multi-entry config");
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Multi-entry config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test lowercase tiers
    {
        const auto & skey = EncodeSecret(key);
        CKey key2; key2.MakeNewKey(true);
        const auto & skey2 = EncodeSecret(key2);
        const auto & saddr2 = EncodeDestination(GetDestinationForKey(key2.GetPubKey(), OutputType::LEGACY));
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 open " + skey + "\n"
                                                           "mn2 spv " + skey2 + " " + saddr2);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load lowercase tiers");
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Lowercase tiers config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test bad snode configs
    {
        const auto & skey = EncodeSecret(key);
        CKey key2; key2.MakeNewKey(true);
        const auto & skey2 = EncodeSecret(key2);
        const auto & saddr2 = EncodeDestination(GetDestinationForKey(key2.GetPubKey(), OutputType::LEGACY));

        // Test bad tiers
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 CUSTOM " + skey + "\n"
                                                           "mn2 SPVV " + skey2 + " " + saddr2);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should not load bad tiers");
        BOOST_CHECK_MESSAGE(entries.empty(), "Bad tiers config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();

        // Test bad keys
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 OPEN fkjdsakfjdsakfjksadjfkasjk\n"
                                                           "mn2 SPV djfksadjfkdasjkfajsk " + saddr2);
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should not load bad keys");
        BOOST_CHECK_MESSAGE(entries.empty(), "Bad keys config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();

        // Test bad address
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 OPEN " + skey + " jdfksjkfajsdkfjaksdfjaksdjk\n"
                                                           "mn2 SPV " + skey2 + " dsjfksdjkfdsjkfdsjkfjskdjfksdsjk");
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should not load bad addresses");
        BOOST_CHECK_MESSAGE(entries.empty(), "Bad addresses config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test optional address on OPEN tier
    {
        const auto & skey = EncodeSecret(key);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 OPEN " + skey + " " + saddr);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load optional address");
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Optional address config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test missing address on SPV tier
    {
        const auto & skey = EncodeSecret(key);
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        saveFile(sn::ServiceNodeMgr::getServiceNodeConf(), "mn1 SPV " + skey);
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should not load missing address");
        BOOST_CHECK_MESSAGE(entries.empty(), "Missing address config should match expected size");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    cleanupSn();
    pos_ptr.reset();
}

/// Check rpc cases
BOOST_AUTO_TEST_CASE(servicenode_tests_rpc)
{
    gArgs.SoftSetBoolArg("-servicenode", true);
    auto pos_ptr = std::make_shared<TestChainPoS>(false);
    auto & pos = *pos_ptr;
    auto *params = (CChainParams*)&Params();
    params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
        if (blockHeight <= consensusParams.lastPOWBlock)
            return 1250 * COIN;
        return 50 * COIN;
    };
    pos.Init("1250,50");

    auto & smgr = sn::ServiceNodeMgr::instance();
    const auto snodePubKey = pos.coinbaseKey.GetPubKey();
    const auto & saddr = EncodeDestination(GetDestinationForKey(snodePubKey, OutputType::LEGACY));

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : pos.m_coinbase_txns) {
        if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->vout[0].nValue;
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // Test servicenodesetup rpc
    {
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node should be returned");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node should be returned");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Check servicenode.conf formatting
    {
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", rpcparams));
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load config");
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Should load exactly 1 snode config entry");
        // Check servicenode.conf formatting
        for (const auto & entry : entries) {
            const auto & sentry = sn::ServiceNodeMgr::configEntryToString(entry);
            BOOST_CHECK_EQUAL(sentry, strprintf("%s %s %s %s", entry.alias, "SPV", EncodeSecret(entry.key),
                                                EncodeDestination(entry.address)));
        }
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test the servicenodesetuplist rpc command
    {
        UniValue rpcparams(UniValue::VARR);
        UniValue list(UniValue::VARR);
        UniValue snode1(UniValue::VOBJ); snode1.pushKV("alias", "snode1"), snode1.pushKV("tier", "SPV"), snode1.pushKV("address", saddr);
        UniValue snode2(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        UniValue entries;
        BOOST_CHECK_NO_THROW(entries = CallRPC2("servicenodesetuplist", rpcparams));
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count on list option should match");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodesetuplist rpc command data checks
    {
        UniValue rpcparams;
        UniValue list;
        UniValue snode1 = UniValue(UniValue::VOBJ); snode1.pushKV("alias", "snode1"), snode1.pushKV("tier", "SPV"), snode1.pushKV("address", saddr);
        UniValue snode2;

        // Should fail on missing alias
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should fail if spaces in alias
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode 2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should fail on missing tier
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should fail on bad tier
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "BAD"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should fail on missing address in non-free tier
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV");
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should fail on empty address in non-free tier
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", "");
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_THROW(CallRPC2("servicenodesetuplist", rpcparams), std::runtime_error);

        // Should not fail on empty address in free tier
        rpcparams = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "OPEN"), snode2.pushKV("address", "");
        list.push_back(snode1), list.push_back(snode2);
        rpcparams.push_back(list);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetuplist", rpcparams));

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test the servicenoderemove rpc
    {
        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        UniValue entry;
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry should be returned");

        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ "snode0" });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderemove", rpcparams));
        std::set<sn::ServiceNodeConfigEntry> ent;
        sn::ServiceNodeMgr::instance().loadSnConfig(ent);
        BOOST_CHECK_MESSAGE(ent.empty(), "servicenoderemove should remove the snode0 entry");

        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        entry.clear();
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry should be returned");

        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderemove", rpcparams));
        ent.clear();
        sn::ServiceNodeMgr::instance().loadSnConfig(ent);
        BOOST_CHECK_MESSAGE(ent.empty(), "servicenoderemove should remove all entry if no alias is specified");

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodegenkey rpc
    {
        UniValue result;
        BOOST_CHECK_NO_THROW(result = CallRPC2("servicenodegenkey", UniValue(UniValue::VARR)));
        BOOST_CHECK_MESSAGE(result.isStr(), "servicenodegenkey should return a string");
        CKey ckey = DecodeSecret(result.get_str());
        BOOST_CHECK_MESSAGE(ckey.IsValid(), "servicenodegenkey should return a valid private key");
    }

    // Test servicenodeexport and servicenodeimport rpc
    {
        UniValue entry;
        UniValue result;

        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry should be returned");

        const std::string & passphrase = "password";
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ "snode0", passphrase });
        BOOST_CHECK_NO_THROW(result = CallRPC2("servicenodeexport", rpcparams));
        BOOST_CHECK_MESSAGE(result.isStr(), "servicenodeexport should return a string");

        // Check that the encrypted hex matches the expected output
        SecureString spassphrase; spassphrase.reserve(100);
        spassphrase = passphrase.c_str();
        CCrypter crypt;
        const std::vector<unsigned char> vchSalt = ParseHex("0000aabbccee0000"); // note* this salt is fixed (i.e. it's not being used)
        crypt.SetKeyFromPassphrase(spassphrase, vchSalt, 100, 0);
        CKeyingMaterial plaintext;
        BOOST_CHECK_MESSAGE(crypt.Decrypt(ParseHex(result.get_str()), plaintext), "servicenodeexport failed to decrypt plaintext");
        std::string strtext(plaintext.begin(), plaintext.end());
        UniValue uexport; uexport.read(strtext);
        BOOST_CHECK_EQUAL(find_value(uexport, "alias").get_str(), find_value(entry, "alias").get_str());
        BOOST_CHECK_EQUAL(find_value(uexport, "tier").get_str(), find_value(entry, "tier").get_str());
        BOOST_CHECK_EQUAL(find_value(uexport, "snodekey").get_str(), find_value(entry, "snodeprivkey").get_str());
        BOOST_CHECK_EQUAL(find_value(uexport, "address").get_str(), find_value(entry, "address").get_str());

        // Check servicenodeimport
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ result.get_str(), passphrase });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodeimport", rpcparams));
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().getSnEntries().size() == 1, "servicenodeimport should have imported snode data");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();

        // Check servicenodeimport bad passphrase
        rpcparams = UniValue(UniValue::VARR);
        rpcparams.push_backV({ result.get_str(), "bad passphrase" });
        BOOST_CHECK_THROW(CallRPC2("servicenodeimport", rpcparams), std::runtime_error);
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().getSnEntries().empty(), "servicenodeimport should fail due to bad passphrase");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodestatus and servicenodelist rpc
    {
        const auto tt = GetAdjustedTime();
        UniValue entries, entry, o;

        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        o = entry;
        const auto snodekey = find_value(o, "snodeprivkey").get_str();
        const auto sk = DecodeSecret(snodekey);

        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(entries = CallRPC2("servicenodestatus", rpcparams));
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Service node status count should match expected");
        BOOST_CHECK_EQUAL(entries.isArray(), true);
        o = entries[0];
        BOOST_CHECK_EQUAL(find_value(o, "alias").get_str(), "snode0");
        BOOST_CHECK_EQUAL(find_value(o, "tier").get_str(), sn::ServiceNodeMgr::tierString(sn::ServiceNode::SPV));
        BOOST_CHECK_EQUAL(find_value(o, "snodekey").get_str(), HexStr(sk.GetPubKey()));
        BOOST_CHECK_EQUAL(find_value(o, "snodeprivkey").get_str(), EncodeSecret(sk));
        BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseen").get_int(), 0);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseenstr").get_str(), "1970-01-01T00:00:00.000Z");
        BOOST_CHECK_EQUAL(find_value(o, "status").get_str(), "offline"); // hasn't been started, expecting offline
        BOOST_CHECK_EQUAL(find_value(o, "services").isArray(), true);

        // Start the snode to add to list
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));

        // Check the status
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(entries = CallRPC2("servicenodestatus", rpcparams));
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Service node status count should match expected");
        BOOST_CHECK_EQUAL(entries.isArray(), true);
        o = entries[0];
        BOOST_CHECK_EQUAL(find_value(o, "alias").get_str(), "snode0");
        BOOST_CHECK_EQUAL(find_value(o, "tier").get_str(), sn::ServiceNodeMgr::tierString(sn::ServiceNode::SPV));
        BOOST_CHECK_EQUAL(find_value(o, "snodekey").get_str(), HexStr(sk.GetPubKey()));
        BOOST_CHECK_EQUAL(find_value(o, "snodeprivkey").get_str(), EncodeSecret(sk));
        BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseen").get_int(), 0);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseenstr").get_str(), "1970-01-01T00:00:00.000Z");
        BOOST_CHECK_EQUAL(find_value(o, "status").get_str(), "offline"); // snode is offline until ping
        BOOST_CHECK_EQUAL(find_value(o, "services").isArray(), true);

        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(entries = CallRPC2("servicenodelist", rpcparams));
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Service node list count should match expected");
        BOOST_CHECK_EQUAL(entries.isArray(), true);
        o = entries[0];
        BOOST_CHECK_EQUAL(find_value(o, "snodekey").get_str(), HexStr(sk.GetPubKey()));
        BOOST_CHECK_EQUAL(find_value(o, "tier").get_str(), sn::ServiceNodeMgr::tierString(sn::ServiceNode::SPV));
        BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseen").get_int(), 0);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseenstr").get_str(), "1970-01-01T00:00:00.000Z");
        BOOST_CHECK_EQUAL(find_value(o, "status").get_str(), "offline"); // snode is offline until ping
        BOOST_CHECK_EQUAL(find_value(o, "score").get_int(), 0);
        BOOST_CHECK_EQUAL(find_value(o, "services").isArray(), true);

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodesendping rpc
    {
        UniValue entries, entry, o;

        UniValue rpcparams(UniValue::VARR);
        rpcparams.push_backV({ saddr });
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesetup", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node entry expected");
        o = entry;
        const auto snodekey = find_value(o, "snodeprivkey").get_str();
        const auto sk = DecodeSecret(snodekey);

        // First check error since snode is not started
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_THROW(CallRPC2("servicenodesendping", rpcparams), std::runtime_error);

        // Start snode
        const auto tt2 = GetAdjustedTime();
        rpcparams = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", rpcparams));

        // Start snode and send ping
        rpcparams = UniValue(UniValue::VARR);
        xbridge::App::instance().utAddXWallets({"BLOCK","BTC","LTC"});
        BOOST_CHECK_NO_THROW(entry = CallRPC2("servicenodesendping", rpcparams));
        BOOST_CHECK_MESSAGE(entry.isObject(), "Service node ping should return the snode");
        o = entry;
        BOOST_CHECK_EQUAL(find_value(o, "alias").get_str(), "snode0");
        BOOST_CHECK_EQUAL(find_value(o, "tier").get_str(), sn::ServiceNodeMgr::tierString(sn::ServiceNode::SPV));
        BOOST_CHECK_EQUAL(find_value(o, "snodekey").get_str(), HexStr(sk.GetPubKey()));
        BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        BOOST_CHECK      (find_value(o, "timelastseen").get_int() >= tt2);
        BOOST_CHECK_EQUAL(find_value(o, "timelastseenstr").get_str().empty(), false);
        BOOST_CHECK_EQUAL(find_value(o, "status").get_str(), "running");
        BOOST_CHECK_EQUAL(find_value(o, "services").isArray(), true);

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodecreateinputs rpc
    {
        CKey key; key.MakeNewKey(true);
        CTxDestination dest(key.GetPubKey().GetID());

        // Send to other wallet key
        CReserveKey reservekey(pos.wallet.get());
        CAmount nFeeRequired;
        std::string strError;
        std::vector<CRecipient> vecSend;
        int nChangePosRet = -1;
        for (int i = 0; i < params->GetConsensus().snMaxCollateralCount*2; ++i) {
            CRecipient recipient = {GetScriptForDestination(dest),
                                    sn::ServiceNode::COLLATERAL_SPV/(params->GetConsensus().snMaxCollateralCount*2),
                                    false};
            vecSend.push_back(recipient);
        }
        vecSend.push_back({GetScriptForDestination(dest), COIN, false}); // For voting input
        vecSend.push_back({GetScriptForDestination(dest), COIN, false}); // For fee
        CCoinControl cc;
        CTransactionRef tx;
        {
            auto locked_chain = pos.wallet->chain().lock();
            LOCK(pos.wallet->cs_wallet);
            BOOST_CHECK_MESSAGE(pos.wallet->CreateTransaction(*locked_chain, vecSend, tx, reservekey, nFeeRequired,
                                nChangePosRet, strError, cc), "Failed to send coin to other wallet");
            CValidationState state;
            BOOST_CHECK_MESSAGE(pos.wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state),
                                strprintf("Failed to send coin to other wallet: %s", state.GetRejectReason()));
        }
        pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();

        // Create otherwallet to test create inputs rpc
        bool firstRun;
        auto otherwallet = std::make_shared<CWallet>(*pos.chain, WalletLocation(), WalletDatabase::CreateMock());
        otherwallet->LoadWallet(firstRun);
        AddKey(*otherwallet, key);
        AddWallet(otherwallet);
        otherwallet->SetBroadcastTransactions(true);
        rescanWallet(otherwallet.get());

        UniValue entries, o, rpcparams;

        {
            // Should fail on bad nodecount
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ EncodeDestination(dest), -1 });
            BOOST_CHECK_THROW(CallRPC2("servicenodecreateinputs", rpcparams), std::runtime_error);

            // Should fail on bad address
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ "kfdjsaklfjksdlajfkdsjfkldsjkfla", 1 });
            BOOST_CHECK_THROW(CallRPC2("servicenodecreateinputs", rpcparams), std::runtime_error);

            // Should fail on good address not in wallets
            CKey nk; nk.MakeNewKey(true);
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ EncodeDestination(CTxDestination(nk.GetPubKey().GetID())), 1 });
            BOOST_CHECK_THROW(CallRPC2("servicenodecreateinputs", rpcparams), std::runtime_error);

            // Should fail on bad input size
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ EncodeDestination(dest), 1, -1000 });
            BOOST_CHECK_THROW(CallRPC2("servicenodecreateinputs", rpcparams), std::runtime_error);

            // Should fail on bad input size
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ EncodeDestination(dest), 1, 1000.123 });
            BOOST_CHECK_THROW(CallRPC2("servicenodecreateinputs", rpcparams), std::runtime_error);
        }

        // Test normal case (should succeed)
        {
            const int inputSize = static_cast<int>(sn::ServiceNode::COLLATERAL_SPV / COIN / params->GetConsensus().snMaxCollateralCount);
            rpcparams = UniValue(UniValue::VARR);
            rpcparams.push_backV({ EncodeDestination(dest), 1, inputSize });
            BOOST_CHECK_NO_THROW(entries = CallRPC2("servicenodecreateinputs", rpcparams));
            BOOST_CHECK_MESSAGE(entries.isObject(), "Bad result object");
            o = entries.get_obj();
            BOOST_CHECK_EQUAL(find_value(o, "nodeaddress").get_str(), EncodeDestination(dest));
            BOOST_CHECK_EQUAL(find_value(o, "nodecount").get_int(), 1);
            BOOST_CHECK_EQUAL(find_value(o, "collateral").get_int(), sn::ServiceNode::COLLATERAL_SPV / COIN);
            BOOST_CHECK_EQUAL(find_value(o, "inputsize").get_int(), inputSize);
            BOOST_CHECK_EQUAL(find_value(o, "txid").get_str().empty(), false);
            pos.StakeBlocks(1), SyncWithValidationInterfaceQueue();
            rescanWallet(otherwallet.get());

            // Check that tx was created
            const auto txid = uint256S(find_value(o, "txid").get_str());
            CTransactionRef txn;
            uint256 hashBlock;
            BOOST_CHECK_MESSAGE(GetTransaction(txid, txn, params->GetConsensus(), hashBlock), "Failed to find inputs tx");
            std::set<COutPoint> txvouts;
            CAmount txAmount{0};
            for (int i = 0; i < txn->vout.size(); ++i) {
                const auto & out = txn->vout[i];
                if (out.nValue != inputSize * COIN)
                    continue;
                txvouts.insert({txn->GetHash(), (uint32_t)i});
                txAmount += out.nValue;
            }
            // Check that coins in wallet match expected
            std::vector<COutput> coins;
            {
                LOCK2(cs_main, otherwallet->cs_wallet);
                otherwallet->AvailableCoins(*pos.locked_chain, coins);
            }
            std::vector<COutput> filtered;
            CAmount filteredAmount{0};
            for (const auto & coin : coins) {
                if (coin.GetInputCoin().txout.nValue != inputSize * COIN)
                    continue;
                filtered.push_back(coin);
                filteredAmount += coin.GetInputCoin().txout.nValue;
            }
            BOOST_CHECK_EQUAL(txvouts.size(), filtered.size());
            BOOST_CHECK_EQUAL(filtered.size(), params->GetConsensus().snMaxCollateralCount);
            BOOST_CHECK_EQUAL(txAmount, filteredAmount);
            BOOST_CHECK_EQUAL(filteredAmount, (CAmount)sn::ServiceNode::COLLATERAL_SPV);

            for (const auto & coin : filtered) {
                if (txvouts.count(coin.GetInputCoin().outpoint))
                    txvouts.erase(coin.GetInputCoin().outpoint);
            }
            BOOST_CHECK_EQUAL(txvouts.size(), 0); // expecting coinsdb to match transaction utxos
        }

        RemoveWallet(otherwallet);
    }

    gArgs.SoftSetBoolArg("-servicenode", false);
    cleanupSn();
    pos_ptr.reset();
}

BOOST_AUTO_TEST_SUITE_END()