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
    BOOST_CHECK_THROW(CallRPC("dxGetLocalTokens SYS LTC"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxGetLocalTokens"));
}

BOOST_AUTO_TEST_CASE(dx_get_network_tokens)
{
    BOOST_CHECK_THROW(CallRPC("dxGetNetworkTokens SYS 1 []"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxGetNetworkTokens"));
}

BOOST_AUTO_TEST_CASE(dx_get_token_balances)
{
    BOOST_CHECK_THROW(CallRPC("dxGetTokenBalances txid"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxGetTokenBalances"));
}

BOOST_AUTO_TEST_CASE(dx_make_order)
{
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS LTC "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS 1 LTC 1 dryrun "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS LTC "), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp 1 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.01 "));
    BOOST_CHECK_NO_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp 2 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.02 dryrun"));
}

BOOST_AUTO_TEST_SUITE_END()