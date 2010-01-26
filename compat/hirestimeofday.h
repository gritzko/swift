/*
 * Written by Arno Bakker
 * see LICENSE.txt for license information
 *
 * Singleton class to retrieve a time-of-day in UTC in usec in a platform-
 * independent manner.
 */
#ifndef HIRESTIMEOFDAY_H
#define HIRESTIMEOFDAY_H

#ifdef _MSC_VER
#include "compat/stdint.h"
#else
#include <stdint.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

namespace swift {

typedef int64_t tint;
#define TINT_SEC ((tint)1000000)
#define TINT_MSEC ((tint)1000)
#define TINT_uSEC ((tint)1)
#define TINT_NEVER ((tint)0x7fffffffffffffffLL)


class HiResTimeOfDay
{
public:
    HiResTimeOfDay(void);
    tint getTimeUSec(void);
    static HiResTimeOfDay* Instance();

private:
#ifdef _WIN32
	tint     epochstart; // in usec
	LARGE_INTEGER epochcounter;
    LARGE_INTEGER last;
    LARGE_INTEGER frequency;

    tint HiResTimeOfDay::getFTime();
    LARGE_INTEGER getFrequency(void);
    LARGE_INTEGER getCounter(void);
#endif

    static HiResTimeOfDay* _instance;
};

};
#endif
