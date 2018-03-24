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
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS LTC "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp -1 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.01 "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp -1 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.0 "), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp 1 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.01 "));
    BOOST_CHECK_NO_THROW(CallRPC("dxMakeOrder SYS 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp 2 LTC LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP 0.02 dryrun"));
}

BOOST_AUTO_TEST_CASE(dx_take_order)
{
    BOOST_CHECK_THROW(CallRPC("dxTakeOrder SYS LTC "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxTakeOrder SYS 06cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a5965 1 LTC 1 dryrun "), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxTakeOrder 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp 06cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a5965 LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxTakeOrder 06cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a5965 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp  LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP"));
    BOOST_CHECK_NO_THROW(CallRPC("dxTakeOrder 5eed087e8ef3b3c91a5c6e302d1f8b037543a40beb5d69a3158be4a5181608cb LZwAVhrTUkYspqRjcCGGiFHMcWNxxsgnqP  1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp"));
}

BOOST_AUTO_TEST_CASE(dx_get_my_orders)
{
    BOOST_CHECK_THROW(CallRPC("dxGetMyOrders txid"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("dxGetMyOrders"));
}

BOOST_AUTO_TEST_CASE(dx_cancel_order)
{
    BOOST_CHECK_THROW(CallRPC("dxCancelOrder txid"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxCancelOrder"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxCancelOrder 06cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a5965"), runtime_error);
}

BOOST_AUTO_TEST_CASE(dx_get_order)
{
    BOOST_CHECK_THROW(CallRPC("dxGetOrder txid"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrder"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrder 01cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a6559"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrder 01cdb308781f2729052d926868a6559"), runtime_error);
}


BOOST_AUTO_TEST_CASE(dx_get_order_fills)
{
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderFills LTC SYS"));
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderFills LTC SYS true"));
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderFills LTC SYS false"));
    BOOST_CHECK_THROW(CallRPC("dxGetOrderFills LTC SYS no_bool"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrderFills LTC SYS 1"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrderFills 01cdb308781f2729052d926868a6559"), runtime_error);
}


BOOST_AUTO_TEST_CASE(dx_get_order_book)
{
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderBook 1 LTC SYS 10"));
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderBook 2 LTC SYS "));
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderBook 3 LTC SYS 50 "));
    BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderBook 4 LTC SYS "));
    BOOST_CHECK_THROW(CallRPC("dxGetOrderBook LTC SYS false"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrderBook -1 LTC SYS no_bool"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrderBook 1 LTC SYS -1"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("dxGetOrderBook SYS LTC"), runtime_error);
}

BOOST_AUTO_TEST_CASE(dx_get_locked_utxos)
{
	BOOST_CHECK_NO_THROW(CallRPC("dxGetLockedUtxos 01cdb308781f2729052d9d2ed4ee2ea1ee5ad0d4ef9c978796d49826868a6559"));
	BOOST_CHECK_THROW(CallRPC("dxGetLockedUtxos uint256()"), runtime_error);
	BOOST_CHECK_THROW(CallRPC("dxGetLockedUtxos 01cdb308781f978796d49826868a6559"), runtime_error);
}


BOOST_AUTO_TEST_CASE(dx_get_order_history)
{
	BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderHistory SYS LTC 1519540000 1529540000 86400"));
	BOOST_CHECK_NO_THROW(CallRPC("dxGetOrderHistory SYS LTC 1519540000 1529540000 3600"));
	BOOST_CHECK_THROW(CallRPC("dxGetOrderHistory SYS LTC 22.03.18-0:00:00 22.03.18-0:01:00 86400"), runtime_error);
	BOOST_CHECK_THROW(CallRPC("dxGetOrderHistory SYS LTC 1519540000 1529540000 -200"), runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()