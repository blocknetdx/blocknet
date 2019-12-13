// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_UTIL_POSIXTIMECONVERSION_H
#define BLOCKNET_XBRIDGE_UTIL_POSIXTIMECONVERSION_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/version.hpp>

// old boost versions compatibility

#if BOOST_VERSION < 105800

namespace boost {

    namespace posix_time {

        time_t to_time_t(boost::posix_time::ptime t);

    }

}

#endif

#endif // BLOCKNET_XBRIDGE_UTIL_POSIXTIMECONVERSION_H
