//
// <sys/time.h> for MSVC, which does not ship one.
//
// linux/i_system.c uses exactly one thing from it: gettimeofday(), to drive
// I_GetTime_RealTime().
//
// Deliberately implemented on C11 timespec_get() rather than
// GetSystemTimeAsFileTime(), because pulling in <windows.h> here would drag
// winnt.h's SHORT/LONG typedefs into the engine, where they collide with the
// macros of the same names in m_swap.h. This keeps the shim pure CRT.
// Resolution is the Windows system clock either way, which is the same source
// mingw-w64's gettimeofday() uses -- so the two Windows builds tick alike.
//
// MSVC build only -- see msvc\compat\unistd.h for why.
//
#ifndef __SMACK_MSVC_SYS_TIME_H__
#define __SMACK_MSVC_SYS_TIME_H__

#include <time.h>

// Same tag/guard winsock2.h uses, so including both is harmless.
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval
{
    long tv_sec;
    long tv_usec;
};
#endif

static __inline int gettimeofday(struct timeval *tv, void *tz)
{
    struct timespec ts;

    (void)tz;
    if (!tv)
        return 0;

    if (!timespec_get(&ts, TIME_UTC))
        return -1;

    tv->tv_sec  = (long)ts.tv_sec;
    tv->tv_usec = (long)(ts.tv_nsec / 1000);
    return 0;
}

#endif // __SMACK_MSVC_SYS_TIME_H__
