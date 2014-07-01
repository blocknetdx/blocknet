#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "util.h"
#include "main.h"

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
    CTxIn testVin1 = CTxIn(n1, 0);
    CTxIn testVin2 = CTxIn(n2, 0);
    CService addr;
    std::vector<unsigned char> vchSig;

    //setup a couple fake masternodes
    CMasterNode mn1(addr, testVin1, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn1);

    CMasterNode mn2(addr, testVin2, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn2);

    // return -1 if nothing present
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == -1);


    darkSendPool.SubmitMasternodeVote(testVin1, 1000);
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == 0); // vin1

    darkSendPool.SubmitMasternodeVote(testVin2, 1000);
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == 1); // vin2 - prevout breaks ties
    darkSendPool.SubmitMasternodeVote(testVin2, 1000);
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == 1); // vin2

    darkSendPool.SubmitMasternodeVote(testVin2, 1000);
    darkSendPool.SubmitMasternodeVote(testVin2, 1000);

    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1001) == -1);

    darkSendPool.SubmitMasternodeVote(testVin2, 1001);
    darkSendPool.SubmitMasternodeVote(testVin1, 1001);
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1001) == 1); // vin2 - prevout breaks ties

    darkSendPool.SubmitMasternodeVote(testVin1, 1001);
    darkSendPool.SubmitMasternodeVote(testVin1, 1001);

    darkSendPool.SubmitMasternodeVote(testVin2, 1001);
    darkSendPool.SubmitMasternodeVote(testVin2, 1001);
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000) == 1); // vin2

    darkSendPool.SubmitMasternodeVote(testVin2, 1001);
    darkSendPool.SubmitMasternodeVote(testVin2, 1001);

    vecBlockVotes.push_back(make_pair(1001, make_pair(testVin1, 10)));
    vecBlockVotes.push_back(make_pair(1001, make_pair(testVin2, 4)));

    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1001) == 0); //vin1
}


BOOST_AUTO_TEST_CASE(darksend_masternode_rank)
{
    uint256 n1 = 10000;
    uint256 n2 = 10001;
    CTxIn testVin1 = CTxIn(n1, 0);
    CTxIn testVin2 = CTxIn(n2, 0);
    CService addr;
    std::vector<unsigned char> vchSig;

    //setup a couple fake masternodes
    CMasterNode mn1(addr, testVin1, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn1);

    CMasterNode mn2(addr, testVin2, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn2);

    printf("here\n");

    // return -1 if nothing present
    darkSendPool.GetMasternodeRank(testVin1, 1);
}


BOOST_AUTO_TEST_SUITE_END()
