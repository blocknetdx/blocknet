#ifndef POSIXTIMECONVERSION_H
#define POSIXTIMECONVERSION_H

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

#endif
