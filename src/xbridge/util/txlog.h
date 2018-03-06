//******************************************************************************
//******************************************************************************

#ifndef TXLOG_H
#define TXLOG_H

#include <sstream>
#include <boost/pool/pool_alloc.hpp>

#define TXERR()   LOG('E')
// #define TXWARN()  LOG('W')
// #define TXTRACE() LOG('T')

//******************************************************************************
//******************************************************************************
class TXLOG : public std::basic_stringstream<char, std::char_traits<char>,
        boost::pool_allocator<char> > // std::stringstream
{
public:
    TXLOG();
    virtual ~TXLOG();

    static std::string logFileName();

private:
    static std::string makeFileName();

private:
    static std::string m_logFileName;
};

#endif // TXLOG_H
