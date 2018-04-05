//*****************************************************************************
//*****************************************************************************

#include "xutil.h"

namespace xrouter {
namespace util
{
uint64_t timeToInt(const boost::posix_time::ptime& time)
{
    bpt::ptime start(boost::gregorian::date(1970,1,1));
    bpt::time_duration timeFromEpoch = time - start;
    boost::int64_t res = timeFromEpoch.total_microseconds();

    return static_cast<uint64_t>(res);
}
} // namespace util
} // namespace xrouter
