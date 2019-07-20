// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_bitcoin.h>

#define protected public // for overridding protected fields in CChainParams
#include <chainparams.h>
#include <coins.h>
#undef protected
#include <index/txindex.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <servicenode.h>
#include <timedata.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

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
        for (int i = 0; i <= Params().GetConsensus().coinMaturity; ++i) { // we'll need to spend coinbases, make sure we have enough blocks
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
    };

    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
    std::vector<CTransactionRef> m_coinbase_txns; // For convenience, coinbase transactions
};

ServiceNode snodeNetwork(const std::vector<unsigned char> & snodePubKey,
        const uint32_t & tier,
        const std::vector<COutPoint> & collateral,
        const std::vector<unsigned char> & sig)
{
    auto ss = CDataStream(SER_NETWORK, PROTOCOL_VERSION);
    ss << snodePubKey << tier << collateral << sig;
    ServiceNode snode; ss >> snode;
    return snode;
}

BOOST_FIXTURE_TEST_SUITE(servicenode_tests, ServicenodeChainSetup)

// Check case where servicenode is properly validated under normal circumstances
BOOST_AUTO_TEST_CASE(servicenode_tests_isvalid)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    const auto tier = ServiceNode::Tier::SPV;

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : m_coinbase_txns) {
        if (totalAmount >= ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->GetValueOut();
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // Generate the signature from sig hash
    const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
    BOOST_CHECK(snode.IsValid(GetTxFunc));
}

// Check open tier case
BOOST_AUTO_TEST_CASE(servicenode_tests_opentier)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    const auto tier = ServiceNode::Tier::OPEN;
    const auto collateral = std::vector<COutPoint>();

    // Valid check
    {
        // Generate the signature from sig hash
        const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
        BOOST_CHECK_MESSAGE(snode.IsValid(GetTxFunc), "Failed on valid snode key sig");
    }

    // Case where wrong key is used to generate sig. For the open tier the snode private key
    // must be used to generate the signature. In this test we use another key.
    {
        // Generate the signature from sig hash
        const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));  // use invalid coinbase key (invalid for open tier)
        // Deserialize servicenode obj from network stream
        ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
        BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Failed on invalid snode key sig");
    }
}

// Check case where duplicate collateral utxos are used
BOOST_AUTO_TEST_CASE(servicenode_tests_duplicate_collateral)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    const auto tier = ServiceNode::Tier::SPV;

    // Assumes total input amounts below adds up to ServiceNode::COLLATERAL_SPV
    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    while (totalAmount < ServiceNode::COLLATERAL_SPV) {
        collateral.emplace_back(m_coinbase_txns[0]->GetHash(), 0);
        totalAmount += m_coinbase_txns[0]->GetValueOut();
    }

    // Generate the signature from sig hash
    const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
    BOOST_CHECK(!snode.IsValid(GetTxFunc));
}

// Check case where there's not enough snode inputs
BOOST_AUTO_TEST_CASE(servicenode_tests_insufficient_collateral)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    const auto tier = ServiceNode::Tier::SPV;

    // Assumes total input amounts below adds up to ServiceNode::COLLATERAL_SPV
    std::vector<COutPoint> collateral;
    collateral.emplace_back(m_coinbase_txns[0]->GetHash(), 0);
    BOOST_CHECK(m_coinbase_txns[0]->GetValueOut() < ServiceNode::COLLATERAL_SPV);

    // Generate the signature from sig hash
    const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
    BOOST_CHECK(!snode.IsValid(GetTxFunc));
}

// Check case where collateral inputs are spent
BOOST_AUTO_TEST_CASE(servicenode_tests_spent_collateral)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    const auto tier = ServiceNode::Tier::SPV;

    // Spend inputs that would be used in snode collateral
    {
        CBasicKeyStore keystore; // temp used to spend inputs
        keystore.AddKey(coinbaseKey);
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
    }

    Coin coin;
    BOOST_CHECK_MESSAGE(!pcoinsTip->GetCoin(COutPoint(m_coinbase_txns[0]->GetHash(), 0), coin), "Coin should be spent here"); // should be spent

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : m_coinbase_txns) {
        if (totalAmount >= ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->GetValueOut();
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // Generate the signature from sig hash
    const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
    std::vector<unsigned char> sig;
    BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));

    // Deserialize servicenode obj from network stream
    ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
    BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Should fail on spent collateral");
}

// Check misc cases
BOOST_AUTO_TEST_CASE(servicenode_tests_misc_checks)
{
    CKey key; key.MakeNewKey(true);
    const auto & pubkey = key.GetPubKey();
    const auto snodePubKey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());

    CAmount totalAmount{0};
    std::vector<COutPoint> collateral;
    for (const auto & tx : m_coinbase_txns) {
        if (totalAmount >= ServiceNode::COLLATERAL_SPV)
            break;
        totalAmount += tx->GetValueOut();
        collateral.emplace_back(tx->GetHash(), 0);
    }

    // Fail on bad tier
    {
        const uint32_t tier = 999;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << tier << collateral;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, sig);
        BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Fail on bad tier");
    }

    // Fail on empty collateral
    {
        const uint32_t tier = ServiceNode::Tier::SPV;
        std::vector<COutPoint> collateral2;
        // Generate the signature from sig hash
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << tier << collateral2;
        const auto & sighash = ss.GetHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        // Deserialize servicenode obj from network stream
        ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral2, sig);
        BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Fail on empty collateral");
    }

    // Fail on empty snode pubkey
    {
        const auto tier = ServiceNode::Tier::SPV;
        const auto & sighash = ServiceNode::CreateSigHash(snodePubKey, tier, collateral);
        std::vector<unsigned char> sig;
        BOOST_CHECK(coinbaseKey.SignCompact(sighash, sig));
        ServiceNode snode = snodeNetwork(std::vector<unsigned char>(), tier, collateral, sig);
        BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Fail on empty snode pubkey");
    }

    // Fail on empty sighash
    {
        const auto tier = ServiceNode::Tier::SPV;
        ServiceNode snode = snodeNetwork(snodePubKey, tier, collateral, std::vector<unsigned char>());
        BOOST_CHECK_MESSAGE(!snode.IsValid(GetTxFunc), "Fail on empty sighash");
    }
}


BOOST_AUTO_TEST_SUITE_END()