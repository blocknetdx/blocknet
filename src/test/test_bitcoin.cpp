// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_bitcoin.h>

#include <banman.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <miner.h>
#include <net_processing.h>
#include <noui.h>
#include <pow.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <script/sigcache.h>
#include <streams.h>
#include <ui_interface.h>
#include <validation.h>

#ifndef USE_XROUTERCLIENT
const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;
#endif // USE_XROUTERCLIENT

FastRandomContext g_insecure_rand_ctx;

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    os << num.ToString();
    return os;
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
    : m_path_root(fs::temp_directory_path() / "test_blocknet" / strprintf("%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(1 << 30))))
{
    SHA256AutoDetect();
    ECC_Stop();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    // CreateAndProcessBlock() does not support building SegWit blocks, so don't activate in these tests.
    // TODO: fix the code to support SegWit blocks.
    gArgs.ForceSetArg("-vbparams", strprintf("segwit:0:%d", (int64_t)Consensus::BIP9Deployment::NO_TIMEOUT));
    SelectParams(chainName);
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    try { fs::remove_all(m_path_root); } catch(...) { }
    ECC_Stop();
}

fs::path BasicTestingSetup::SetDataDir(const std::string& name)
{
    fs::path ret = m_path_root / name;
    fs::create_directories(ret);
    gArgs.ForceSetArg("-datadir", ret.string());
    return ret;
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    SetDataDir("tempdir");
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.

        RegisterAllCoreRPCCommands(tableRPC);
        ClearDatadirCache();

        // We have to run a scheduler thread to prevent ActivateBestChain
        // from blocking due to queue overrun.
        threadGroup.create_thread(std::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

        mempool.setSanityCheck(1.0);
        pblocktree.reset(new CBlockTreeDB(1 << 20, true));
        pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
        pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));
        if (!LoadGenesisBlock(chainparams)) {
            throw std::runtime_error("LoadGenesisBlock failed.");
        }
        {
            CValidationState state;
            if (!ActivateBestChain(state, chainparams)) {
                throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", FormatStateMessage(state)));
            }
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);

        g_banman = MakeUnique<BanMan>(GetDataDir() / "banlist.dat", nullptr, DEFAULT_MISBEHAVING_BANTIME);
        g_connman = MakeUnique<CConnman>(0x1337, 0x1337); // Deterministic randomness for tests.
}

TestingSetup::~TestingSetup()
{
    threadGroup.interrupt_all();
    threadGroup.join_all();
    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    g_connman.reset();
    g_banman.reset();
    UnloadBlockIndex();
    pcoinsTip.reset();
    pcoinsdbview.reset();
    pblocktree.reset();
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < TESTCHAIN_BLOCK_COUNT; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        m_coinbase_txns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
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

TestChain100Setup::~TestChain100Setup()
{
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx) {
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx)
{
    return CTxMemPoolEntry(tx, nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}

/**
 * @returns a real block (0000006536eaba28d094da216be85369619c0fe60d03cde7fe9a7d552dfb7750)
 *      with 9 txs.
 */
CBlock getBlock13b8a()
{
    CBlock block;
    CDataStream stream(ParseHex("03000000e0c33d65df76bea7655e836ca9ebe3e0ced6f69e84c7e5b2ae07120d1d00000045bd36b6b591d2025d900062aa1d7cfe1c4f536555502e46e641ec87ece4b910f6678b59ff9d001e9c410400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000901000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0502dc05010affffffff026af6c404000000002321038652801ab54957eb94421271b7ff9d31dd2da44e5ab727f360003050c6a98c9dac9a3d3101000000001976a9147d7a3cb519a73f67d17ec26f50ba575a407c822e88ac0000000001000000012134fd572b79e2d3657862dde654a1ccb149887ee5d577600ee4d2986695d224010000006a473044022074bc105e27ab462ad7faf919f50be2d9aba66be4fbe5499ffcc75419d17c4e27022057389ea4bcc44855fd0c2049cfca5aeb8817ef26cc9c33a57fc741ef1183626c012102fdc596855e2fcdcdc23d798e00c555726aaece8e85e1178237564af3ae4eaafbffffffff02ae4ba97a6f0700001976a9141617f86512c72526c0cde953dc43bf5fd41ded2088ac00ca9a3b000000001976a9149c5781278b716c047ee60e5a3f29a98f55c5ef3f88ac000000000100000001b9bb0a81ffe82d17407de259d82ee0206cd828c4f8c458dfa16c46fbec1611a4000000006a473044022014f9d5501e5177594b44fe29b90d083904ab18fea72cf54a3ecbc1383d4def55022000d45c01410fb9e1f3a5663a355dd8cc391af7e8790326eaa43c4d2f038ceb5a01210367ee3e61177a8a300b822de4005d396ef7ff05a79b73854b00f308ecfb870823ffffffff0255770e3f6f0700001976a9147aa7846a9223d369c2c9b786347489390a7df88d88ac00ca9a3b000000001976a914ba7fc8f8ccaa75e078961c8888e98c89213d85a888ac0000000001000000011896a59c4565593e80666db098396ae6c88572a0ef5d377d0c974b482b3dc390000000006a47304402203d295735887ede766a7a99f9184f29a4c9fd78171e7743c4805140aac793492e02203d4243a1eaa583560cef11047ae85e871861b857ffc50cd3340845ff822c4cd301210292acdd3f85cc90095fb5eeb6bb40614e5791b44c677af6005437be73f19be26effffffff02f0a273036f0700001976a914af00cfd903fd3cdb6f4c131107a7ac0c6db8337c88ac00ca9a3b000000001976a914995f273daaa1494bbaacbe583f42d9972912829088ac0000000001000000016ba3831484856e7768e3dd709e41a4a7603ab0b7b89926618b0a1278672e7d36000000006a473044022007b2266d8935a7745017da3268b5861baba309a36d6b02bcfc0c2e95e412b94d02206b2caec9c9f13ffa5af629f95eb22fc46fe2cdb7b2f011b50196657ce6b0a8760121035228e0337656bd6162ffd44a07b815ec2e6a81efc2773a075750fa15507e7d12ffffffff0200ca9a3b000000001976a914ef5acbda3141f7f4cbb81775b1afa46d8dde6a6e88ac8bced8c76e0700001976a9144e8c27fb222a902a7d453f6d2c04941a5b25c6d988ac000000000100000001e08de50d4395a8817a1516713e0d9ef67de48bae9c7046fc61b30a045b87fd90010000006a4730440220638f8069cd983719d7a6800f3ff7ba09afcbdf826c77a58eb888a2aa7012bbf5022002d4e9d014dc2cb9fa8cc699c07d8d93b555befb846e5fde4a98f720915bdc6c012102f17b1663961589b87d7d78a0422f1fab164b9361881243324dd931a2428b1bdaffffffff0200ca9a3b000000001976a9149c5781278b716c047ee60e5a3f29a98f55c5ef3f88ac32fa3d8c6e0700001976a9148aba16d06cbc60989ed5cda489354c046332295788ac0000000001000000011a67810bf4f374ede3679afa84d92f2833cfb18a91a48b0e8adeb452cef16cdb010000006b48304502210094a78d4a771280bfaa0d7122ce6f90cca5fc38572468b3a5ed6dce5b3e1e98c002202565665cf9ac77a268dbd8fda9c59a9729039acbdfeadbde566c996f8a09f66101210385ed32f3e5dd5ec764f114399c500c92185f8f195fee9100feabdd55bf601d60ffffffff0200ca9a3b000000001976a914ba7fc8f8ccaa75e078961c8888e98c89213d85a888accd25a3506e0700001976a914ec4507c682ea5e406c9a5385ad4a4ad0f279e6b588ac0000000001000000017719849e084d3526aeed1bccfd1101331f49f32f055ca2f6a1582b105ec2d8ac010000006a473044022030807931517f501f4d12308e6cac3d750dbca5e26c322d9fbcff5d253378c9bf02200dd235102ef0d29922036c019b99913708e4f6837d7b9e0ea8720f54a861feb701210267e0500399ff2b84e4ea992d5bcd4bfbb9e32998fa39074091d9ab3a19b87a5dffffffff02745108156e0700001976a9140e7bb1885ce8de73a3efdd014b50e81a028762e788ac00ca9a3b000000001976a914995f273daaa1494bbaacbe583f42d9972912829088ac0000000001000000019cffd8e0840f25b07428d658d0e9945f968d43c6ea8d69330ae0c79cd880bd50000000006b483045022100f1aa36e5512608d0559802f9266f172d29c98b6c37a4fe8453a85a26f07509dc022022fff1e9aee89cd998851949e2a715997d79c27f024924e3c47ebf4d12094e950121030853004e0746a94fa0542ff918e6631d4c20381052ba71ef23385902d40df71affffffff020f7d6dd96d0700001976a9142df0f66f79be26def405525b98315f8b0dd84caa88ac00ca9a3b000000001976a914ef5acbda3141f7f4cbb81775b1afa46d8dde6a6e88ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;
    return block;
}
