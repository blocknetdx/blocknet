#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "util.h"
#include "main.h"
#include "key.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(darksend_tests)

BOOST_AUTO_TEST_CASE(darksend_sign)
{

    std::string errorMessage = "";
    std::string strBase64;
    CKey key;
    CPubKey pubkey;
    std::vector<unsigned char> vchSig;

    CDarkSendSigner dss;
    dss.SetKey("XDPugk3QgxVpQ4BubgzKaXhQudtaBnjuos9w6ZTojYx68EipNnt7", errorMessage, key, pubkey);
    BOOST_CHECK(dss.SignMessage("hello", errorMessage, vchSig, key) == true);
    BOOST_CHECK(dss.VerifyMessage(pubkey, vchSig, "hello", errorMessage) == true);
    BOOST_CHECK(dss.VerifyMessage(pubkey, vchSig, "hello2", errorMessage) == false);

}
BOOST_AUTO_TEST_CASE(darksend_vote)
{
    CPubKey key;
    CMasterNodeVote mnv;
    mnv.Set(key, 1);
    mnv.Vote();
    BOOST_CHECK(mnv.GetVotes() == 2);
    mnv.Vote();
    BOOST_CHECK(mnv.GetVotes() == 3);
}

BOOST_AUTO_TEST_CASE(darksend_masternode_voting)
{
    uint256 n1 = 10000;
    uint256 n2 = 10001;
    CTxIn fromMn1 = CTxIn(n2, 0);
    CTxIn testVin1 = CTxIn(n1, 0);
    CTxIn testVin2 = CTxIn(n2, 0);
    CService addr;
    int i = 0;
    std::vector<unsigned char> vchSig;

    //setup a couple fake masternodes
    CMasterNode mn1(addr, testVin1, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn1);

    CMasterNode mn2(addr, testVin2, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn2);

    // return -1 if nothing present
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == -1);

    //block 1000
    for(i = 0; i <= 2; i++)
        darkSendPool.SubmitMasternodeVote(testVin1, fromMn1,1000);

    // not enough votes
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == -1); // 

    for(i = 0; i <= 4; i++)
        darkSendPool.SubmitMasternodeVote(testVin2, fromMn1, 1000);

    // not enough votes
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == -1); // 

    for(i = 0; i <= 4; i++)
        darkSendPool.SubmitMasternodeVote(testVin2, fromMn1, 1000);
    
    // should have 8 votes now
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == 1); // vin2
}


BOOST_AUTO_TEST_CASE(darksend_masternode_search_by_vin)
{
    uint256 n1 = 10000;
    uint256 n2 = 10001;
    uint256 n3 = 99999;
    CTxIn testVin1 = CTxIn(n1, 0);
    CTxIn testVin2 = CTxIn(n2, 0);
    CTxIn testVinNotFound = CTxIn(n3, 0);
    CService addr;
    std::vector<unsigned char> vchSig;

    //setup a couple fake masternodes
    CMasterNode mn1(addr, testVin1, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn1);

    CMasterNode mn2(addr, testVin2, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn2);

    BOOST_CHECK(darkSendPool.GetMasternodeByVin(testVinNotFound) == -1);
    BOOST_CHECK(darkSendPool.GetMasternodeByVin(testVin1) == 0);
    BOOST_CHECK(darkSendPool.GetMasternodeByVin(testVin2) == 1);
}

BOOST_AUTO_TEST_CASE(darksend_add_entry)
{
    std::vector<CTxIn> vin;
    vin.push_back( CTxIn(1000, 0) );
    vin.push_back( CTxIn(1001, 0) );

    std::vector<CTxOut> vout;
    vout.push_back( CTxOut(1, CScript()) );
    vout.push_back( CTxOut(1, CScript()) );

    //try added entries
    CDarkSendEntry e;
    BOOST_CHECK(e.sev.size() == 0);
    BOOST_CHECK(e.Add(vin, 1, CTransaction(), vout) == true);
    BOOST_CHECK(e.sev.size() == 2);
    BOOST_CHECK(e.Add(vin, 1, CTransaction(), vout) == false);
    BOOST_CHECK(e.sev.size() == 2);

    //sign one of the vin
    BOOST_CHECK(e.AddSig(CScript(), CScript(), CTxIn(1001, 0)) == true);
    BOOST_CHECK(e.AddSig(CScript(), CScript(), CTxIn(1001, 0)) == false);

    //sign non-existant
    BOOST_CHECK(e.AddSig(CScript(), CScript(), CTxIn(9999001, 0)) == false);

}

BOOST_AUTO_TEST_CASE(darksend_masternode_class)
{
    std::string strPubKey = "XpAy7r5RVdGLnnjWNKuB9EUDiJ5Tje9GZ8";
    CPubKey pubkey(ParseHex(strPubKey));

    std::vector<unsigned char> newSig;
    int64 newNow = GetTimeMicros();

    CMasterNode mn(CService("10.10.10.10:9999"), CTxIn(1000, 0), pubkey, newSig, newNow, pubkey);
    mn.UpdateLastSeen();
    mn.Check();
    BOOST_CHECK(mn.enabled == 3); // bad vin
    mn.lastTimeSeen -= MASTERNODE_EXPIRATION_MICROSECONDS;
    mn.Check();
    BOOST_CHECK(mn.enabled == 2); // hasn't pinged
    mn.lastTimeSeen -= MASTERNODE_EXPIRATION_MICROSECONDS;
    mn.Check();
    BOOST_CHECK(mn.enabled == 4); // expired
}

BOOST_AUTO_TEST_CASE(darksend_pool_add_entry)
{
    std::vector<CTxIn> vin;
    vin.push_back( CTxIn(1000, 0) );
    vin.push_back( CTxIn(1001, 0) );

    std::vector<CTxOut> vout;
    vout.push_back( CTxOut(1, CScript()) );
    vout.push_back( CTxOut(1, CScript()) );

    //try added entries
    CDarkSendEntry e;
    BOOST_CHECK(e.Add(vin, 1, CTransaction(), vout) == true);
    darkSendPool.entries.push_back(e);

    // add first entry
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 1);
    darkSendPool.Check();
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 1);

    
}


BOOST_AUTO_TEST_SUITE_END()
