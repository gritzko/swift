/*
 * Written by Arno Bakker
 * see LICENSE.txt for license information
 *
 * Defines UNIX like I/O calls and parameters for Win32
 */
#ifdef _WIN32

#ifndef UNIXIO_H_
#define UNIXIO_H_

#define open(a,b,c)	_open(a,b,c)
#define S_IRUSR _S_IREAD
#define S_IWUSR	_S_IWRITE
#define S_IRGRP _S_IREAD
#define S_IROTH _S_IREAD
#define ftruncate(a, b) _chsize(a,b)

size_t pread(int fildes, void *buf, size_t nbyte, long offset);
/** UNIX pread approximation. Does change file pointer. Is not thread-safe */
size_t pwrite(int fildes, const void *buf, size_t nbyte, long offset);
/** UNIX pwrite approximation. Does change file pointer. Is not thread-safe */

int inet_aton(const char *cp, struct in_addr *inp);

#endif /* UNIXIO_H_ */

#endif // WIN32