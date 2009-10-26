/*
 * Inspired by
 * - http://msdn.microsoft.com/en-us/library/ms644904%28VS.85%29.aspx
 * - Python-2.6.3/Modules/timemodule.c
 */

#include <iostream>
#include "hirestimeofday.h"

#ifndef _WIN32
#include <sys/time.h>
#endif

namespace p2tp {

HiResTimeOfDay* HiResTimeOfDay::_instance = 0;

HiResTimeOfDay* HiResTimeOfDay::Instance()
{
	if (_instance == 0)
		_instance = new HiResTimeOfDay();
	return _instance;
}


#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>


HiResTimeOfDay::HiResTimeOfDay(void)
{
	frequency = getFrequency();
	epochstart = getFTime();
	epochcounter = getCounter();
}


tint HiResTimeOfDay::getTimeUSec(void)
{
	LARGE_INTEGER currentcounter;
	tint	  currentstart;

	currentstart = getFTime();
    currentcounter = getCounter();

    if (currentcounter.QuadPart < epochcounter.QuadPart)
    {

    	// Wrap around detected, reestablish baseline
    	epochstart = currentstart;
    	epochcounter = currentcounter;
    }
    return epochstart + (1000000*(currentcounter.QuadPart-epochcounter.QuadPart))/frequency.QuadPart;
}


// Private
tint HiResTimeOfDay::getFTime()
{
	struct timeb t;
	ftime(&t);
	tint usec;
	usec =  t.time * 1000000;
	usec += t.millitm * 1000;
	return usec;
}



LARGE_INTEGER HiResTimeOfDay::getFrequency(void)
{
    LARGE_INTEGER proc_freq;

    if (!::QueryPerformanceFrequency(&proc_freq))
    	std::cerr << "HiResTimeOfDay: QueryPerformanceFrequency() failed";

    return proc_freq;
}

LARGE_INTEGER HiResTimeOfDay::getCounter()
{
    LARGE_INTEGER counter;

    DWORD_PTR oldmask = ::SetThreadAffinityMask(::GetCurrentThread(), 0);
    if (!::QueryPerformanceCounter(&counter))
    	std::cerr << "HiResTimeOfDay: QueryPerformanceCounter() failed";
    ::SetThreadAffinityMask(::GetCurrentThread(), oldmask);

    return counter;
}

#else

HiResTimeOfDay::HiResTimeOfDay(void)
{
}


tint HiResTimeOfDay::getTimeUSec(void)
{
	struct timeval t;
	gettimeofday(&t,NULL);
	tint ret;
	ret = t.tv_sec;
	ret *= 1000000;
	ret += t.tv_usec;
	return ret;
}
#endif

  
  
  
// ARNOTODO: move to p2tp.cpp

#ifdef _WIN32
static WSADATA _WSAData;
#endif
  
void LibraryInit(void)
{
#ifdef _WIN32
	// win32 requires you to initialize the Winsock DLL with the desired
	// specification version
	WORD wVersionRequested;
    wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &_WSAData);
#endif
}
  
  
} // end of namespace




#ifdef TEST
#include <iostream>

using namespace p2tp;

int main()
{
	HiResTimeOfDay *t = HiResTimeOfDay::Instance();
	for (int i=0; i<100; i++)
	{
		tint st = t->getTimeUSec();
		Sleep(1000);
		tint et = t->getTimeUSec();
		tint diff = et - st;
		std::cout << "diffxTime is " << diff << "\n";
	}
	return 0;
}
#endif

