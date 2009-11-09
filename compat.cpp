/*
 *  compat.cpp
 *  p2tp
 *
 *  Created by Arno Bakker, Victor Grishchenko
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "compat.h"
#include "util.h"
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <Tchar.h>
#include <io.h>
#include <winsock2.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

namespace p2tp {

size_t file_size (int fd) {
    struct stat st;
    fstat(fd, &st);
    return st.st_size;
}

int     file_seek (int fd, size_t offset) {
#ifndef _WIN32
    return lseek(fd,offset,SEEK_SET);
#else
	return _lseek(fd,offset,SEEK_SET);
#endif
}

int     file_resize (int fd, size_t new_size) {
#ifndef _WIN32
    return ftruncate(fd, new_size);
#else
    return _chsize(fd,new_size);
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

void*   memory_map (int fd, size_t size) {
    if (!size)
        size = file_size(fd);
    void *mapping;
#ifndef _WIN32
    mapping = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping==MAP_FAILED)
        return NULL;
    return mapping;
#else
    HANDLE fhandle = (HANDLE)_get_osfhandle(fd);
    assert(fd<1024);
    static HANDLE map_handles[1024];
    maphandle = CreateFileMapping(     fhandle,
                                       NULL,
                                       PAGE_READWRITE,
                                       0,
                                       0,
                                       NULL    );
	if (maphandle == NULL)
        return NULL;
    map_handles[fd] = maphandle;
    
    mapping = MapViewOfFile         (  maphandle,
                                       FILE_MAP_WRITE,
                                       0,
                                       0,
                                       0  );
        
    return mapping;
#endif
}

void    memory_unmap (int fd, void* mapping, size_t size) {
#ifndef _WIN32
    munmap(mapping,size);
    close(fd);
#else
	UnmapViewOfFile(mapping);
	CloseHandle(map_handles[fd]);
#endif
}
    
#ifdef _WIN32
    
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
 
tint usec_time () {
    struct timeval t;
	gettimeofday(&t,NULL);
	tint ret;
	ret = t.tv_sec;
	ret *= 1000000;
	ret += t.tv_usec;
	return ret;
}    
    
}
