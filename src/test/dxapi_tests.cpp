#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "netbase.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;
BOOST_AUTO_TEST_SUITE(dxapi_tests)
BOOST_AUTO_TEST_CASE(dxapi_test)
{
    BOOST_CHECK_EQUAL(1,2);
}
BOOST_AUTO_TEST_SUITE_END()