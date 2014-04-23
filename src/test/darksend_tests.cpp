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
    mnv.Vote(false);
    BOOST_CHECK(mnv.GetVotes() == 0);
    mnv.Vote(false);
    BOOST_CHECK(mnv.GetVotes() == -1);
}


BOOST_AUTO_TEST_SUITE_END()
