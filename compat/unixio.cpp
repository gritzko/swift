/*
 * Written by Arno Bakker
 * see LICENSE.txt for license information
 */
#ifdef _WIN32

#include "unixio.h"
#include <stdio.h>
#include <io.h>
#include <winsock2.h>

size_t pread(int fildes, void *buf, size_t nbyte, long offset)
{
	_lseek(fildes,offset,SEEK_SET);
    return read(fildes,buf,nbyte);
}

size_t pwrite(int fildes, const void *buf, size_t nbyte, long offset)
{
    _lseek(fildes,offset,SEEK_SET);
    return write(fildes,buf,nbyte);
}


int inet_aton(const char *cp, struct in_addr *inp)
{
	inp->S_un.S_addr = inet_addr(cp);
	return 1;
}



#endif
