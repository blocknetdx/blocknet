//*****************************************************************************
//*****************************************************************************

#ifndef XASSERT_H
#define XASSERT_H

#ifdef _XDEBUG
#define xassert(__expr) assert(__expr)
#else
#define xassert(__expr) void(0)
#endif

#endif // XASSERT_H
