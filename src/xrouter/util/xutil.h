//*****************************************************************************
//*****************************************************************************

#ifndef XROUTERUTIL_H
#define XROUTERUTIL_H

#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xrouter {
namespace util
{
namespace bpt = boost::posix_time;

boost::uint64_t timeToInt(const bpt::ptime &time);

} // namespace util
} // namespace xrouter

#endif // XROUTERUTIL_H
