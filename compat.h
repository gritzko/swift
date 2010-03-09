/*
 *  compat.h
 *  compatibility wrappers
 *
 *  Created by Arno Bakker, Victor Grishchenko
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef SWIFT_COMPAT_H
#define SWIFT_COMPAT_H

#ifdef _MSC_VER
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <sys/stat.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifndef _WIN32
typedef int SOCKET;
#endif

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _MSC_VER
#include "getopt_win.h"
#else
#include <getopt.h>
#endif

#ifdef _WIN32
#define open(a,b,c)    _open(a,b,c)
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IRGRP
#define S_IRGRP _S_IREAD
#endif
#ifndef S_IROTH
#define S_IROTH _S_IREAD
#endif

#ifdef _WIN32
#define setsockoptptr_t (char*)
#else
#define setsockoptptr_t void*
#endif


namespace swift {

/** tint is the time integer type; microsecond-precise. */
typedef int64_t tint;
#define TINT_HOUR ((tint)1000000*60*60)
#define TINT_MIN ((tint)1000000*60)
#define TINT_SEC ((tint)1000000)
#define TINT_MSEC ((tint)1000)
#define TINT_uSEC ((tint)1)
#define TINT_NEVER ((tint)0x3fffffffffffffffLL)


size_t  file_size (int fd);

int     file_seek (int fd, size_t offset);

int     file_resize (int fd, size_t new_size);

void*   memory_map (int fd, size_t size=0);
void    memory_unmap (int fd, void*, size_t size);

void    print_error (const char* msg);

#ifdef _WIN32

/** UNIX pread approximation. Does change file pointer. Is not thread-safe */
size_t  pread(int fildes, void *buf, size_t nbyte, long offset);

/** UNIX pwrite approximation. Does change file pointer. Is not thread-safe */
size_t  pwrite(int fildes, const void *buf, size_t nbyte, long offset);

int     inet_aton(const char *cp, struct in_addr *inp);

#endif

std::string gettmpdir(void);

tint    usec_time ();

bool    make_socket_nonblocking(SOCKET s);

bool    close_socket (SOCKET sock);


};

#endif

