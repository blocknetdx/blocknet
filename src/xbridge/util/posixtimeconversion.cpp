// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xbridge/util/posixtimeconversion.h>

#if BOOST_VERSION < 105800

namespace boost {

    namespace posix_time {

        time_t to_time_t(boost::posix_time::ptime t)
        {
            using namespace boost::posix_time;
            ptime epoch(boost::gregorian::date(1970,1,1));
            time_duration::sec_type x = (t - epoch).total_seconds();
            return time_t(x);
        }

    }

}

#endif
