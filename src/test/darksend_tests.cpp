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
<<<<<<< HEAD
=======


>>>>>>> nightlydarkcoin-gitian
BOOST_AUTO_TEST_CASE(darksend_vote)
{
    CPubKey key;
    CMasterNodeVote mnv;
    mnv.Set(key, 1);
<<<<<<< HEAD
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
=======
    mnv.Vote(false);
    BOOST_CHECK(mnv.GetVotes() == 0);
    mnv.Vote(false);
    BOOST_CHECK(mnv.GetVotes() == -1);
}

>>>>>>> nightlydarkcoin-gitian

BOOST_AUTO_TEST_SUITE_END()
