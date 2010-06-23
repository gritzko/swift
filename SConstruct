# Written by Victor Grishchenko, Arno Bakker 
# see LICENSE.txt for license information
#
# Requirements:
#  - scons: Cross-platform build system    http://www.scons.org/
#  - googletest: Google C++ Test Framework http://code.google.com/p/googletest/
#       * Install in ..\gtest-1.4.0
#

import os
import re
import sys

DEBUG = True

TestDir='tests'

target = 'swift'
source = [ 'bin64.cpp','sha1.cpp','hashtree.cpp','datagram.cpp','bins.cpp',
    'transfer.cpp', 'channel.cpp', 'sendrecv.cpp', 'send_control.cpp',
    'compat.cpp']

env = Environment()
if sys.platform == "win32":
	# "MSVC works out of the box". Sure.
	# Make sure scons finds cl.exe, etc.
	env.Append ( ENV = { 'PATH' : os.environ['PATH'] } )

	# Make sure scons finds std MSVC include files
	if not 'INCLUDE' in os.environ:
		print "swift: Please run scons in a Visual Studio Command Prompt"
		sys.exit(-1)
		
	include = os.environ['INCLUDE']
	include += '..\\gtest-1.4.0\\include;'
	include += '\\openssl\\include;' 
	
	env.Append ( ENV = { 'INCLUDE' : include } )
	
	# Other compiler flags
	env.Append(CPPPATH=".")
	if DEBUG:
		env.Append(CXXFLAGS="/Zi /Yd /MTd")
		env.Append(LINKFLAGS="/DEBUG")

	# Add simulated pread/write
	source += ['compat/unixio.cpp']
 
 	# Set libs to link to
	libs = ['ws2_32']
	if DEBUG:
		libs += ['gtestd','libeay32MTd']
	else:
		libs += ['gtest','libeay32']
		
	# Update lib search path
	libpath = os.environ['LIBPATH']
	if DEBUG:
		libpath += '\\build\\gtest-1.4.0\\msvc\\gtest\\Debug;'
		libpath += '\\openssl\\lib\\VC\\static;'
	else:
		libpath += '\\build\\gtest-1.4.0\\msvc\\gtest\\Release;'
		libpath += '\\openssl\\lib;'
	# Somehow linker can't find uuid.lib
	libpath += 'C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\Lib;'
	
else:
	# Enable the user defining external includes
        if 'CPPPATH' in os.environ:
	    cpppath = os.environ['CPPPATH']
	else:
	    cpppath = ""
	    print "To use external libs, set CPPPATH environment variable to list of colon-separated include dirs"
	env.Append(CPPPATH=".:"+cpppath)
    #env.Append(LINKFLAGS="--static")

	#if DEBUG:
	#	env.Append(CXXFLAGS="-g")

	# Set libs to link to
	libs = ['stdc++','pthread']
	if 'LIBPATH' in os.environ:
  	    libpath = os.environ['LIBPATH']
	else:
	    libpath = ""
	    print "To use external libs, set LIBPATH environment variable to list of colon-separated lib dirs"

if DEBUG:
	env.Append(CXXFLAGS="-DDEBUG")

env.StaticLibrary (
    target='libswift',
    source = source,
    LIBS=libs,
    LIBPATH=libpath )

env.Program(
   target='swift',
   source=['swift.cpp','httpgw.cpp'],
   CPPPATH=cpppath,
   LIBS=[libs,'libswift'],
   LIBPATH=libpath+':.' )

Export("env")
Export("libs")
Export("libpath")
Export("DEBUG")
SConscript('tests/SConscript')
