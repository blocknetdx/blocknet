#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "netbase.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

extern Value CallRPC(string args);

BOOST_AUTO_TEST_SUITE(dxapi_tests)


BOOST_AUTO_TEST_CASE(dx_get_local_tokens)
{
    BOOST_CHECK_THROW(CallRPC("dxGetLocalTokens"), runtime_error);
}

BOOST_AUTO_TEST_CASE(dx_get_network_tokens)
{
    BOOST_CHECK_THROW(CallRPC("dxGetNetworkTokens"), runtime_error);
}

BOOST_AUTO_TEST_CASE(dx_get_token_balances)
{
    BOOST_CHECK_THROW(CallRPC("dxGetTokenBalances"), runtime_error);
}


BOOST_AUTO_TEST_SUITE_END()