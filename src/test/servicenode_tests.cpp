// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/staking_tests.h>

#include <rpc/server.h>
#include <servicenode/servicenode.h>
#include <servicenode/servicenodemgr.h>

/**
 * Setup a chain suitable for testing servicenode inputs. The larger coinbase payout will allow us
 * to setup snode inputs.
 */
struct ServicenodeChainSetup : public TestingSetup {
    ServicenodeChainSetup() : TestingSetup(CBaseChainParams::REGTEST) {
        auto *params = (CChainParams*)&Params();
        params->consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 1250 * COIN;
            return 1 * COIN;
        };

        coinbaseKey.MakeNewKey(true); // default privkey for coinbase spends

        CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        for (int i = 0; i <= Params().GetConsensus().coinMaturity + 10; ++i) { // we'll need to spend coinbases, make sure we have enough blocks
            SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing*20); // prevent difficulty from increasing too rapidly
            CBlock b = CreateAndProcessBlock(std::vector<CMutableTransaction>(), scriptPubKey);
            m_coinbase_txns.push_back(b.vtx[0]);
        }

        // Turn on index for transaction lookups
        g_txindex = MakeUnique<TxIndex>(1 << 20, true);
        g_txindex->Start();
        g_txindex->Sync();
    }

    CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey) {
        const CChainParams& chainparams = Params();
        std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
        CBlock& block = pblocktemplate->block;

        // Replace mempool-selected txns with just coinbase plus passed-in txns:
        block.vtx.resize(1);
        for (const CMutableTransaction& tx : txns)
            block.vtx.push_back(MakeTransactionRef(tx));
        // IncrementExtraNonce creates a valid coinbase and merkleRoot
        {
            LOCK(cs_main);
            unsigned int extraNonce = 0;
            IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);
        }

        while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
        ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

        CBlock result = block;
        return result;
    }

    ~ServicenodeChainSetup() {
        sn::ServiceNodeMgr::instance().reset();
    };

    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
    std::vector<CTransactionRef> m_coinbase_txns; // For convenience, coinbase transactions
};

sn::ServiceNode snodeNetwork(const CPubKey & snodePubKey, const uint8_t & tier,
                         const std::vector<COutPoint> & collateral, const uint32_t & blockNumber,
                         const uint256 & blockHash, const std::vector<unsigned char> & sig)
{
    auto ss = CDataStream(SER_NETWORK, PROTOCOL_VERSION);
    ss << snodePubKey << tier << collateral << blockNumber << blockHash << sig;
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

BOOST_FIXTURE_TEST_SUITE(servicenode_tests, ServicenodeChainSetup)

// Check case where servicenode is properly validated under normal circumstances
BOOST_AUTO_TEST_CASE(servicenode_tests_isvalid)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : m_coinbase_txns) {
        if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->GetValueOut();
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // Generate the signature from sig hash
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
    BOOST_CHECK(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));
}

// Check open tier case
BOOST_AUTO_TEST_CASE(servicenode_tests_opentier)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::OPEN;
    const auto collateral = std::vector<COutPoint>();

    // Valid check
    {
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Failed on valid snode key sig");
    }

    // Case where wrong key is used to generate sig. For the open tier the snode private key
    // must be used to generate the signature. In this test we use another key.
    {
        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));  // use invalid coinbase key (invalid for open tier)
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Failed on invalid snode key sig");
    }
}

// Check case where duplicate collateral utxos are used
BOOST_AUTO_TEST_CASE(servicenode_tests_duplicate_collateral)
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
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
    BOOST_CHECK(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));
}

// Check case where there's not enough snode inputs
BOOST_AUTO_TEST_CASE(servicenode_tests_insufficient_collateral)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    // Assumes total input amounts below adds up to ServiceNode::COLLATERAL_SPV
    std::vector<COutPoint> collateral;
    collateral.emplace_back(m_coinbase_txns[0]->GetHash(), 0);
    BOOST_CHECK(m_coinbase_txns[0]->GetValueOut() < sn::ServiceNode::COLLATERAL_SPV);

    // Generate the signature from sig hash
    const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
    BOOST_CHECK(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc));
}

// Check case where collateral inputs are spent
BOOST_AUTO_TEST_CASE(servicenode_tests_spent_collateral)
{
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();
    const auto tier = sn::ServiceNode::Tier::SPV;

    CBasicKeyStore keystore; // temp used to spend inputs
    keystore.AddKey(coinbaseKey);

    // Spend inputs that would be used in snode collateral
    {
        // Spend inputs somewhere
        CScript scriptPubKey = CScript() << ToByteVector(key.GetPubKey()) << OP_CHECKSIG;
        CTransactionRef tx = m_coinbase_txns[0];
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(COutPoint(tx->GetHash(), 0));
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = scriptPubKey;
        mtx.vout[0].nValue = tx->GetValueOut() - CENT;
        const CTransaction txConst(mtx);
        SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[0]);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, mtx.vout[0].nValue, SIGHASH_ALL), tx->vout[0].scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);
        SetMockTime(GetAdjustedTime() + Params().GetConsensus().nPowTargetSpacing*3); // add some time to make it easier to mine
        CBlock block = CreateAndProcessBlock(std::vector<CMutableTransaction>({mtx}), scriptPubKey); // spend inputs
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash()); // make sure block was added to the chaintip

        Coin coin;
        BOOST_CHECK_MESSAGE(!pcoinsTip->GetCoin(COutPoint(m_coinbase_txns[0]->GetHash(), 0), coin), "Coin should be spent here"); // should be spent

        CAmount totalAmount{0};
        std::vector<COutPoint> collateral;
        for (const auto & txn : m_coinbase_txns) {
            if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
                break;
            totalAmount += txn->GetValueOut();
            collateral.emplace_back(txn->GetHash(), 0);
        }

        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Should fail on spent collateral");
    }

    // Check case where spent collateral is in mempool
    {
        // Spend one of the collateral inputs (spend the 2nd coinbase input, b/c first was spent above)
        CScript scriptPubKey = CScript() << ToByteVector(key.GetPubKey()) << OP_CHECKSIG;
        CTransactionRef tx = m_coinbase_txns[1];
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0] = CTxIn(COutPoint(tx->GetHash(), 0));
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = scriptPubKey;
        mtx.vout[0].nValue = tx->GetValueOut() - CENT;
        const CTransaction txConst(mtx);
        SignatureData sigdata = DataFromTransaction(mtx, 0, tx->vout[0]);
        ProduceSignature(keystore, MutableTransactionSignatureCreator(&mtx, 0, mtx.vout[0].nValue, SIGHASH_ALL), tx->vout[0].scriptPubKey, sigdata);
        UpdateInput(mtx.vin[0], sigdata);

        CValidationState state;
        LOCK(cs_main);
        BOOST_CHECK(AcceptToMemoryPool(mempool, state, MakeTransactionRef(mtx),
                                                    nullptr, // pfMissingInputs
                                                    nullptr, // plTxnReplaced
                                                    false,   // bypass_limits
                                                    0));     // nAbsurdFee
        CAmount totalAmount{0};
        std::vector<COutPoint> collateral;
        for (int i = 1; i < m_coinbase_txns.size(); ++i) { // start at 1 (ignore first spent coinbase)
            const auto & txn = m_coinbase_txns[i];
            if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
                break;
            totalAmount += txn->GetValueOut();
            collateral.emplace_back(txn->GetHash(), 0);
        }

        // Generate the signature from sig hash
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Should fail on spent collateral in mempool");
    }
}

// Servicenode registration and ping tests
BOOST_AUTO_TEST_CASE(servicenode_tests_registration_pings)
{
    auto chain = interfaces::MakeChain();
    auto locked_chain = chain->assumeLocked();
    auto wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
    bool firstRun; wallet->LoadWallet(firstRun);
    {
        LOCK(wallet->cs_wallet);
        wallet->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
    }
    WalletRescanReserver reserver(wallet.get());
    reserver.reserve();
    wallet->ScanForWalletTransactions(chainActive.Genesis()->GetBlockHash(), {}, reserver, false);
    BOOST_CHECK(AddWallet(wallet));

    // Turn on index for staking
    g_txindex = MakeUnique<TxIndex>(1 << 20, true);
    g_txindex->Start();
    g_txindex->Sync();

    CTxDestination dest(coinbaseKey.GetPubKey().GetID());
    int addedSnodes{0};

    // Snode registration and ping w/ uncompressed key
    {
        CKey key; key.MakeNewKey(false);
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get()), "Register snode w/ uncompressed key");
        // Snode ping w/ uncompressed key
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().sendPing(key, g_connman.get()), "Snode ping w/ uncompressed key");
        ++addedSnodes;
    }

    // Snode registration and ping w/ compressed key
    {
        CKey key; key.MakeNewKey(true);
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().registerSn(key, sn::ServiceNode::SPV, EncodeDestination(dest), g_connman.get()), "Register snode w/ compressed key");
        // Snode ping w/ compressed key
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().sendPing(key, g_connman.get()), "Snode ping w/ compressed key");
        ++addedSnodes;
    }

    // Check snode count matches number added above
    BOOST_CHECK(sn::ServiceNodeMgr::instance().list().size() == addedSnodes);
    sn::ServiceNodeMgr::instance().reset();

    // Check servicenoderegister all rpc
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 2, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count should match expected");
        params = UniValue(UniValue::VARR);
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", params));
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Check servicenoderegister by alias rpc
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 2, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count should match expected");
        params = UniValue(UniValue::VARR);
        params.push_backV({ "snode1" });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenoderegister", params));
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Check servicenoderegister rpc result data
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 2, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count should match expected");
        params = UniValue(UniValue::VARR);
        try {
            auto result = CallRPC2("servicenoderegister", params);
            BOOST_CHECK_EQUAL(result.isArray(), true);
            UniValue o = result[1];
            BOOST_CHECK_EQUAL(find_value(o, "alias").get_str(), "snode1");
            BOOST_CHECK_EQUAL(find_value(o, "tier").get_int(), sn::ServiceNode::SPV);
            BOOST_CHECK_EQUAL(find_value(o, "servicenodeprivkey").get_str().empty(), false); // check not empty
            BOOST_CHECK_EQUAL(find_value(o, "address").get_str(), saddr);
        } catch (std::exception & e) {
            BOOST_CHECK_MESSAGE(false, strprintf("servicenoderegister failed: %s", e.what()));
        }

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Check servicenoderegister bad alias
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 2, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count should match expected");
        params = UniValue(UniValue::VARR);
        params.push_backV({ "bad_alias" });
        BOOST_CHECK_THROW(CallRPC2("servicenoderegister", params), std::runtime_error);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Check servicenoderegister no configs
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        BOOST_CHECK_THROW(CallRPC2("servicenoderegister", params), std::runtime_error);
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    RemoveWallet(wallet);
}

// Check misc cases
BOOST_AUTO_TEST_CASE(servicenode_tests_misc_checks)
{
    auto & smgr = sn::ServiceNodeMgr::instance();
    CKey key; key.MakeNewKey(true);
    const auto snodePubKey = key.GetPubKey();

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : m_coinbase_txns) {
        if (totalAmount >= sn::ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->GetValueOut();
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // NOTE** This test must be first!
    BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().list().empty(), "Fail on non-empty snode list");

    // Fail on bad tier
    {
        const uint8_t tier = 0xff;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << tier << collateral;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on bad tier");
    }

    // Fail on empty collateral
    {
        const uint8_t tier = sn::ServiceNode::Tier::SPV;
        std::vector<COutPoint> collateral2;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << tier << collateral2;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral2, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty collateral");
    }

    // Fail on empty snode pubkey
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        sn::ServiceNode snode = snodeNetwork(CPubKey(), tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty snode pubkey");
    }

    // Fail on empty sighash
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), std::vector<unsigned char>());
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on empty sighash");
    }

    // Fail on bad best block
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, 0, uint256());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, 0, uint256(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on bad best block");
    }

    // Fail on stale best block (valid but stale block number)
    {
        const int staleBlockNumber = chainActive.Height()-SNODE_STALE_BLOCKS - 1;
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on stale best block");
    }

    // Fail on best block number being too far into future
    {
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, chainActive.Height()+5, chainActive[5]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, chainActive.Height()+5, chainActive[5]->GetBlockHash(), sig);
        BOOST_CHECK_MESSAGE(!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc), "Fail on best block, unknown block, too far in future");
    }

    // Test disabling the stale check on the servicenode validation
    {
        const int staleBlockNumber = chainActive.Height()-SNODE_STALE_BLOCKS - 1;
        const auto tier = sn::ServiceNode::Tier::SPV;
        const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash());
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        sn::ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, staleBlockNumber, chainActive[staleBlockNumber]->GetBlockHash(), sig);
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

    // Test rpc servicenode setup
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 1, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Service node config count should match");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
        UniValue params2(UniValue::VARR);
        params2.push_backV({ "auto", 10, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params2));
        UniValue entries2 = CallRPC2("servicenodesetup", params2);
        BOOST_CHECK_MESSAGE(entries2.size() == 10, "Service node config count should match");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenode.conf formatting
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 10, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        std::set<sn::ServiceNodeConfigEntry> entries;
        BOOST_CHECK_MESSAGE(smgr.loadSnConfig(entries), "Should load config");
        BOOST_CHECK_MESSAGE(entries.size() == 10, "Should load exactly 10 snode config entries");
        // Check servicenode.conf formatting
        for (const auto & entry : entries) {
            const auto & sentry = sn::ServiceNodeMgr::configEntryToString(entry);
            BOOST_CHECK_EQUAL(sentry, strprintf("%s %s %s %s", entry.alias, "SPV", EncodeSecret(entry.key),
                                                EncodeDestination(entry.address)));
        }
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test the servicenodesetup list option
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        UniValue list(UniValue::VARR);
        UniValue snode1(UniValue::VOBJ); snode1.pushKV("alias", "snode1"), snode1.pushKV("tier", "SPV"), snode1.pushKV("address", saddr);
        UniValue snode2(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 2, "Service node config count on list option should match");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodesetup list option data checks
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params;
        UniValue list;
        UniValue snode1 = UniValue(UniValue::VOBJ); snode1.pushKV("alias", "snode1"), snode1.pushKV("tier", "SPV"), snode1.pushKV("address", saddr);
        UniValue snode2;

        // Should fail on missing alias
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should fail if spaces in alias
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode 2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should fail on missing tier
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should fail on bad tier
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "BAD"), snode2.pushKV("address", saddr);
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should fail on missing address in non-free tier
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV");
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should fail on empty address in non-free tier
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "SPV"), snode2.pushKV("address", "");
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_THROW(CallRPC2("servicenodesetup", params), std::runtime_error);

        // Should not fail on empty address in free tier
        params = UniValue(UniValue::VARR);
        list = UniValue(UniValue::VARR);
        snode2 = UniValue(UniValue::VOBJ); snode2.pushKV("alias", "snode2"), snode2.pushKV("tier", "OPEN"), snode2.pushKV("address", "");
        list.push_back(snode1), list.push_back(snode2);
        params.push_backV({ "list", list });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test the servicenodesetup remove option
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 10, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 10, "Service node config count should match expected");

        params = UniValue(UniValue::VARR);
        params.push_backV({ "remove" });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        std::set<sn::ServiceNodeConfigEntry> ent;
        sn::ServiceNodeMgr::instance().loadSnConfig(ent);
        BOOST_CHECK_MESSAGE(ent.empty(), "Service node setup remove option should result in 0 snode entries");

        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }

    // Test servicenodegenkey rpc
    {
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodegenkey", UniValue(UniValue::VARR)));
        const auto & result = CallRPC2("servicenodegenkey", UniValue(UniValue::VARR));
        BOOST_CHECK_MESSAGE(result.isStr(), "servicenodegenkey should return a string");
        CKey ckey = DecodeSecret(result.get_str());
        BOOST_CHECK_MESSAGE(ckey.IsValid(), "servicenodegenkey should return a valid private key");
    }

    // Test servicenodeexport and servicenodeimport rpc
    {
        const auto & saddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY));
        UniValue params(UniValue::VARR);
        params.push_backV({ "auto", 1, saddr });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodesetup", params));
        UniValue entries = CallRPC2("servicenodesetup", params);
        BOOST_CHECK_MESSAGE(entries.size() == 1, "Service node config count should match expected");

        const std::string & passphrase = "password";
        params = UniValue(UniValue::VARR);
        params.push_backV({ "snode0", passphrase });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodeexport", params));
        const auto & result = CallRPC2("servicenodeexport", params);
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
        const std::string & str = entries[0].write();
        BOOST_CHECK_EQUAL(strtext, str);

        // Check servicenodeimport
        params = UniValue(UniValue::VARR);
        params.push_backV({ result.get_str(), passphrase });
        BOOST_CHECK_NO_THROW(CallRPC2("servicenodeimport", params));
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().getSnEntries().size() == 1, "servicenodeimport should have imported snode data");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();

        // Check servicenodeimport bad passphrase
        params = UniValue(UniValue::VARR);
        params.push_backV({ result.get_str(), "bad passphrase" });
        BOOST_CHECK_THROW(CallRPC2("servicenodeimport", params), std::runtime_error);
        BOOST_CHECK_MESSAGE(sn::ServiceNodeMgr::instance().getSnEntries().empty(), "servicenodeimport should fail due to bad passphrase");
        sn::ServiceNodeMgr::writeSnConfig(std::vector<sn::ServiceNodeConfigEntry>(), false); // reset
        sn::ServiceNodeMgr::instance().reset();
    }
}

BOOST_AUTO_TEST_SUITE_END()