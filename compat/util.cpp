/*
 * Written by Arno Bakker
 * see LICENSE.txt for license information
 */

#include "util.h"
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <Tchar.h>
#endif

namespace p2tp
{

std::string gettmpdir(void)
{
#ifdef _WIN32
  DWORD result = ::GetTempPath(0, _T(""));
  if (result == 0)
	throw std::runtime_error("Could not get system temp path");

  std::vector<TCHAR> tempPath(result + 1);
  result = ::GetTempPath(static_cast<DWORD>(tempPath.size()), &tempPath[0]);
  if((result == 0) || (result >= tempPath.size()))
	throw std::runtime_error("Could not get system temp path");

  return std::string(tempPath.begin(), tempPath.begin() + static_cast<std::size_t>(result));
#else
	  return std::string("/tmp/");
#endif
}


    void print_error(const char* msg) {
        perror(msg);
#ifdef _WIN32
        int e = WSAGetLastError();
        if (e)
            fprintf(stderr,"network error #%i\n",e);
#endif
    }
    
    
}; // namespace

