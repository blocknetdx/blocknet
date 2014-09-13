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

BOOST_AUTO_TEST_CASE(set_collateral_address_bad)
{
	CDarkSendPool * dsp_ptr = new CDarkSendPool();

	string crappy = "badaddress";

	BOOST_CHECK( dsp_ptr->SetCollateralAddress(crappy) == false );
	delete dsp_ptr;
}

BOOST_AUTO_TEST_CASE(set_collateral_address_production)
{
	CDarkSendPool * dsp_ptr = new CDarkSendPool();

	string prod = "Xq19GqFvajRrEdDHYRKGYjTsQfpV5jyipF";

	BOOST_CHECK( dsp_ptr->SetCollateralAddress(prod) == true );
	delete dsp_ptr;
}

BOOST_AUTO_TEST_CASE(set_collateral_address_testnet)
{
	CDarkSendPool * dsp_ptr = new CDarkSendPool();

	string testnet = "mxE2Rp3oYpSEFdsN5TdHWhZvEHm3PJQQVm";

	BOOST_CHECK( dsp_ptr->SetCollateralAddress(testnet) == true );
	delete dsp_ptr;
}

BOOST_AUTO_TEST_CASE(darksend_denom)
{

    darkSendDenominations.push_back( (500   * COIN)+1 );
    darkSendDenominations.push_back( (100   * COIN)+1 );
    darkSendDenominations.push_back( (10    * COIN)+1 );
    darkSendDenominations.push_back( (1     * COIN)+1 );

    std::vector<CTxOut> vout1;
    std::vector<CTxOut> vout2;

    CTxOut d1 = CTxOut(); d1.nValue = (500   * COIN)+1;
    CTxOut d2 = CTxOut(); d2.nValue = (100   * COIN)+1;
    CTxOut d3 = CTxOut(); d3.nValue = (10   * COIN)+1;
    CTxOut d4 = CTxOut(); d4.nValue = (1   * COIN)+1;

    vout1.push_back(d1);
    vout1.push_back(d3);

    vout2.push_back(d1);
    vout2.push_back(d2);
    vout2.push_back(d3);

    BOOST_CHECK(darkSendPool.GetDenominations(vout1) != darkSendPool.GetDenominations(vout2));

    vout1.push_back(d2);

    BOOST_CHECK(darkSendPool.GetDenominations(vout1) == darkSendPool.GetDenominations(vout2));

    vout1.push_back(d2);

    BOOST_CHECK(darkSendPool.GetDenominations(vout1) == darkSendPool.GetDenominations(vout2));

    vout1.push_back(d4);

    BOOST_CHECK(darkSendPool.GetDenominations(vout1) != darkSendPool.GetDenominations(vout2));

}

BOOST_AUTO_TEST_CASE(darksend_session)
{
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(511*COIN));
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(131*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(31*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(151*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(751*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(531*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 3) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(551*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 4) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(571*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 5) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(514*COIN));
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(531*COIN) == false);

    darkSendPool.SetNull();

    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(12*COIN));
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(131*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(151*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(751*COIN) == false);
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(34*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 3) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(22*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 4) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(32*COIN));
    if(darkSendPool.GetMaxPoolTransactions() >= 5) BOOST_CHECK(darkSendPool.IsCompatibleWithSession(44*COIN));
    BOOST_CHECK(darkSendPool.IsCompatibleWithSession(33*COIN) == false);

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

    CScript payee = CScript();

    //setup a couple fake masternodes
    CMasterNode mn1(addr, testVin1, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn1);

    CMasterNode mn2(addr, testVin2, CPubKey(), vchSig, 0, CPubKey());
    darkSendMasterNodes.push_back(mn2);

    darkSendPool.unitTest = true;

    // return -1 if nothing present
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000, payee) == false);

    //block 1000
    for(i = 0; i <= 2; i++)
        darkSendPool.SubmitMasternodeVote(testVin1, fromMn1,1000);

    // not enough votes
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000, payee) == false); // 

    for(i = 0; i <= 4; i++)
        darkSendPool.SubmitMasternodeVote(testVin2, fromMn1, 1000);

    // not enough votes
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000, payee) == false); // 

    for(i = 0; i <= 4; i++)
        darkSendPool.SubmitMasternodeVote(testVin2, fromMn1, 1000);
    
    // should have 8 votes now
    BOOST_CHECK(darkSendPool.GetCurrentMasterNodeConsessus(1000, payee) == true); // vin2
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
    BOOST_CHECK(e.AddSig(CTxIn(1001, 0)) == true);
    BOOST_CHECK(e.AddSig(CTxIn(1001, 0)) == false);

    //sign non-existant
    BOOST_CHECK(e.AddSig(CTxIn(9999001, 0)) == false);

}

BOOST_AUTO_TEST_CASE(darksend_masternode_class)
{
    std::string strPubKey = "XpAy7r5RVdGLnnjWNKuB9EUDiJ5Tje9GZ8";
    CPubKey pubkey(ParseHex(strPubKey));

    std::vector<unsigned char> newSig;
    int64 newNow = GetTimeMicros();

    CMasterNode mn(CService("10.10.10.10:9999"), CTxIn(1000, 0), pubkey, newSig, newNow, pubkey);
    mn.unitTest = true;
    mn.UpdateLastSeen();
    mn.Check();
    BOOST_CHECK(mn.enabled == 1); // ok
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

    // add first entry
    darkSendPool.entries.push_back(e);
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 1);
    darkSendPool.Check();
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 1);
    
    // add second entry
    darkSendPool.entries.push_back(e);
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 2);
    darkSendPool.Check();
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 2);

    // add third entry
    darkSendPool.entries.push_back(e);
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_ACCEPTING_ENTRIES);
    BOOST_CHECK(darkSendPool.entries.size() == 3);
    darkSendPool.Check();
    BOOST_CHECK(darkSendPool.state == POOL_STATUS_SIGNING);
    BOOST_CHECK(darkSendPool.entries.size() == 3);

}

BOOST_AUTO_TEST_SUITE_END()
