// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <arith_uint256.h>
#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <policy/feerate.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "decentralized consensus based democracy is the future";
    const CScript genesisOutputScript = CScript() << ParseHex("0452c91a00518fb8c6d38100341f88499554284d1ba75097cc25ae5a0d811835c63d2cb46c8855304bca81c452b63ce71fcb6897d06f8000450841f72602457f74") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.stakingAllowsMinDifficultyBlocks = false;
        consensus.nRuleChangeActivationThreshold = 1368; // 95% of 1440
        consensus.nMinerConfirmationWindow = 1440; // 1 day
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1584295200; // March 15, 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1589565600; // May 15, 2020

        // Deployment of staker network fees
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nStartTime = 1584295200; // March 15, 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nTimeout = 1589565600; // May 15, 2020

        // Deployment of staker p2pkh support
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].bit = 24;
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nStartTime = 1584295200; // March 15, 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nTimeout = 1589565600; // May 15, 2020

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("00000000000000000000000000000000000000000000000563b4a73316dfc193"); // block 1522597

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x1ee481216e8eef8ce5e98389dc1a95085748c4ae6b0c77bc60b456e482fca3a4"); // block 1522597

        // Last POW block
        consensus.lastPOWBlock = 2000;
        consensus.stakeMinAge = 3600;
        consensus.stakingModiferV2Block = 615800;
        consensus.coinMaturity = 100;
        consensus.stakingV05UpgradeTime = 1569261600; // Sep 23 '19 6pm UTC
        consensus.stakingV06UpgradeTime = 1586973600; // Apr 15, 2020 6pm UTC
        consensus.stakingV07UpgradeTime = 1591639200; // June 8, 2020 6pm UTC

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xa0;
        pchMessageStart[2] = 0xa2;
        pchMessageStart[3] = 0xa3;
        nDefaultPort = 41412;
        nPruneAfterHeight = std::numeric_limits<uint64_t>::max(); // Pruning not supported
        m_assumed_blockchain_size = 3;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock(1502214073, 734967, UintToArith256(consensus.powLimit).GetCompact(), 1, 250 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000eb7919102da5a07dc90905651664e6ebf0811c28f06573b9a0fd84ab7b8"));
        assert(genesis.hashMerkleRoot == uint256S("0xb1f0e93f6df55af4c23a0719ab33be2b8115e2b6127fc1d926a06c60a8b56bf2"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("seed1.blocknet.co");
        vSeeds.emplace_back("seed2.blocknet.co");
        vSeeds.emplace_back("seed3.blocknet.co");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,26);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,28);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,154);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "block";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {0, uint256S("0x00000eb7919102da5a07dc90905651664e6ebf0811c28f06573b9a0fd84ab7b8")},
                {70000, uint256S("0x4da13976c81dd150b97c5d05d1470f35176a2f7a8a7d329b4cb34810452d18a4")},
                {101651, uint256S("0xfb3a39c35bf3242f0bb283a128d0791ef91914c8a27260804101771cd8591a89")},
                {250000, uint256S("0x4a562d811edec011c36708ed4fa2215362cd89bce8a63c568e6d54f227c9f8b4")},
                {450000, uint256S("0xe6b2969308f015907b5dbfbca23076d06d4efbac3b768b939c941f3416c3210c")},
                {700000, uint256S("0xe2f5aa60986b6524e8c4c0983a7212ed8bef9fe0fa5c284277bb3329bc33d9af")},
                {900000, uint256S("0x40cf476ac051614b28a35ccbf55bcddb9691966a391ec5aa74bb4b8152e072b3")},
                {1110080, uint256S("0x566452bd3fd8608f83a6af45711af88877e54a137cac030ef3d9d134124c0c05")},
                {1212162, uint256S("0x3e16a50c22e23f7c6b93567a540e2e830f3ac774755896d12234dce30d1db06a")},
                {1301200, uint256S("0x016477330d5f04aeb7f93971ef7fb8c4c15aedc4281f43753aee4fa13a54ccd5")},
                {1367315, uint256S("0x28483eea85f9ddd028be0100e8930439bb7ff564d59f13a4eac2aea1036e7c9f")},
                {1522597, uint256S("0x1ee481216e8eef8ce5e98389dc1a95085748c4ae6b0c77bc60b456e482fca3a4")},
            }
        };
        // Assign last checkpoint height, used to estimate initial load time
        for (const auto & item : checkpointData.mapCheckpoints) {
            if (item.first > consensus.lastCheckpointHeight)
                consensus.lastCheckpointHeight = item.first;
        }

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 43200 [block_hash]
            /* nTime    */ 1593836305,
            /* nTxCount */ 3376960,
            /* dTxRate  */ 0.03353379319107036
        };

        /* enable fallback fee on mainnet */
        m_fallback_fee_enabled = true;
        consensus.defaultFallbackFee = CFeeRate(2000);

        // Governance
        consensus.superblock = 43200;
        consensus.proposalCutoff = 2880;
        consensus.votingCutoff = 60;
        consensus.proposalMaxAmount = 40000 * COIN;
        consensus.governanceBlock = 1296001;

        // subsidy func mainnet
        consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            CAmount baseReward = 1 * COIN;
            if (blockHeight == 1)                 return 4160024 * COIN; // from previous mainnet
            else if (blockHeight == 67629)        return 10    * COIN + baseReward; // expl amounts
            else if (blockHeight == 70247)        return 1000  * COIN + baseReward;
            else if (blockHeight == 70359)        return 49951 * COIN + baseReward;
            else if (blockHeight == 70406)        return 1000  * COIN + baseReward;
            else if (blockHeight == 70698)        return 49909 * COIN + baseReward;
            else if (blockHeight == 70716)        return 10000 * COIN + baseReward;
            else if (blockHeight == 70779)        return 49951 * COIN + baseReward;
            else if (blockHeight == 70808)        return 25001 * COIN + baseReward;
            else if (blockHeight == 71182)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71671)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71779)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71812)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71882)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71885)        return 50000 * COIN + baseReward;
            else if (blockHeight == 71897)        return 50000 * COIN + baseReward;
            else if (blockHeight == 72043)        return 48786 * COIN + baseReward; // expl amounts
            else if (blockHeight % consensusParams.superblock == 0) { // superblocks
               if (blockHeight >= 820800)         return consensusParams.proposalMaxAmount + baseReward; // phase 2 superblock
               else                               return 4320  * COIN + baseReward; // phase 1 superblock
            }
            else
                return 1 * COIN;
        };
    }
};

/**
 * Testnet (v5)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105");
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.powLimit = uint256S("0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.stakingAllowsMinDifficultyBlocks = false;
        consensus.nRuleChangeActivationThreshold = 45; // 75% for testchains
        consensus.nMinerConfirmationWindow = 60;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1573850059; // November 15, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1573936459; // November 16, 2019

        // Deployment of staker network fees
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nStartTime = 1559692800; // June 5, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nTimeout = 1577750400; // December 31, 2019

        // Deployment of staker p2pkh support
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nStartTime = 1559692800; // June 5, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nTimeout = 1577750400; // December 31, 2019

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000008da8d3d2e63fb960");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x07df8d108cf2bb7db773b84b99a9afc0d3f9c0f9c85ff9b0b87225cf0150b471"); // 663538

        consensus.lastPOWBlock = 31;
        consensus.stakeMinAge = 60;
        consensus.stakingModiferV2Block = 1;
        consensus.coinMaturity = 15;
        consensus.stakingV05UpgradeTime = 1566085343; // Aug 17, 2019
        consensus.stakingV06UpgradeTime = 1581628366; // Feb 13, 2020
        consensus.stakingV07UpgradeTime = 1587164819; // Apr 17, 2020 23:06:59 UTC

        pchMessageStart[0] = 0x45;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0x65;
        pchMessageStart[3] = 0xba;
        nDefaultPort = 41474;
        nPruneAfterHeight = std::numeric_limits<uint64_t>::max(); // Pruning not supported
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock(1548018283, 2, UintToArith256(consensus.powLimit).GetCompact(), 1, 250 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0fd62ae4f74c7ee0c11ef60fc5a2e69a5c02eaee2e77b21c3db70934b5a5c8b9"));
        assert(genesis.hashMerkleRoot == uint256S("0xb1f0e93f6df55af4c23a0719ab33be2b8115e2b6127fc1d926a06c60a8b56bf2"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("3.16.3.126"); // Blocknet update services or use dns
        vSeeds.emplace_back("18.224.130.185");
        vSeeds.emplace_back("18.213.44.27");
        vSeeds.emplace_back("34.196.102.239");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,139);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,19);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3a, 0x80, 0x61, 0xa0};
        base58Prefixes[EXT_SECRET_KEY] = {0x3a, 0x80, 0x58, 0x37};

        bech32_hrp = "tblock";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("0x0fd62ae4f74c7ee0c11ef60fc5a2e69a5c02eaee2e77b21c3db70934b5a5c8b9")},
                {302806, uint256S("0x54d0094600b90ae113cd54360058206b00eb1db2a5a9ae9a95e40f553edb2042")},
                {663538, uint256S("0x07df8d108cf2bb7db773b84b99a9afc0d3f9c0f9c85ff9b0b87225cf0150b471")},
            }
        };
        // Assign last checkpoint height, used to estimate initial load time
        for (const auto & item : checkpointData.mapCheckpoints) {
            if (item.first > consensus.lastCheckpointHeight)
                consensus.lastCheckpointHeight = item.first;
        }

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats
            /* nTime    */ 1593837804,
            /* nTxCount */ 1331560,
            /* dTxRate  */ 0.03318975611066963
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
        consensus.defaultFallbackFee = CFeeRate(2000);

        // Governance
        consensus.superblock = 144;
        consensus.proposalCutoff = 20;
        consensus.votingCutoff = 10;
        consensus.proposalMaxAmount = 20000 * COIN;
        consensus.governanceBlock = 267500;

        // subsidy func testnet
        consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= 32)
                return 5000 * COIN;
            else if (blockHeight > consensusParams.lastPOWBlock && blockHeight % consensusParams.superblock == 0)
                return consensusParams.proposalMaxAmount + 1 * COIN;
            return 1 * COIN;
        };
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 75; // BIP66 activated on regtest (Used in functional tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.stakingAllowsMinDifficultyBlocks = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_NETWORKFEES].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_STAKEP2PKH].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.lastPOWBlock = 125; // required for unit tests
        consensus.stakeMinAge = 60; // required for unit tests
        consensus.coinMaturity = 50; // required for unit tests

        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xcf;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xac;
        nDefaultPort = 41489;
        nPruneAfterHeight = std::numeric_limits<uint64_t>::max(); // Pruning not supported
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateVersionBitsParametersFromArgs(args);

        genesis = CreateGenesisBlock(1454124731, 2, UintToArith256(consensus.powLimit).GetCompact(), 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x6d23345907a2aeb4aecc08ad207cdea4823d2f3b0104414eeda95de1947f5eae"));
        assert(genesis.hashMerkleRoot == uint256S("0x23d75ed23b622cf66fec45862fa446551453dc596e4c122d8652cf461d4f8cb2"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0x6d23345907a2aeb4aecc08ad207cdea4823d2f3b0104414eeda95de1947f5eae")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,139);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,19);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3a, 0x80, 0x61, 0xa0};
        base58Prefixes[EXT_SECRET_KEY] = {0x3a, 0x80, 0x58, 0x37};

        bech32_hrp = "blockrt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
        consensus.defaultFallbackFee = CFeeRate(2000);

        // Governance
        consensus.superblock = 165;
        consensus.proposalCutoff = 20;
        consensus.votingCutoff = 10;
        consensus.proposalMaxAmount = 40000 * COIN;
        consensus.governanceBlock = 1;

        // subsidy func testnet
        consensus.GetBlockSubsidy = [](const int & blockHeight, const Consensus::Params & consensusParams) {
            if (blockHeight <= consensusParams.lastPOWBlock)
                return 50 * COIN;
            else if (blockHeight % consensusParams.superblock == 0)
                return consensusParams.proposalMaxAmount + 1 * COIN;
            return 1 * COIN;
        };
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateVersionBitsParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateVersionBitsParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }
        int64_t nStartTime, nTimeout;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
