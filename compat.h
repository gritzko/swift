/*
 *  compat.h
 *  p2tp
 *
 *  Created by Arno Bakker, Victor Grishchenko
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef P2TP_COMPAT_H
#define P2TP_COMPAT_H

#ifdef _MSC_VER
#include "compat/stdint.h"
#else
#include <stdint.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <sys/stat.h>
#else
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define open(a,b,c)	_open(a,b,c)
#define S_IRUSR _S_IREAD
#define S_IWUSR	_S_IWRITE
#define S_IRGRP _S_IREAD
#define S_IROTH _S_IREAD
#endif

namespace p2tp {

typedef int64_t tint;
#define TINT_HOUR ((tint)1000000*60*60)
#define TINT_MIN ((tint)1000000*60)
#define TINT_SEC ((tint)1000000)
#define TINT_MSEC ((tint)1000)
#define TINT_uSEC ((tint)1)
#define TINT_NEVER ((tint)0x7fffffffffffffffLL)


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

tint    usec_time ();

};

#endif

