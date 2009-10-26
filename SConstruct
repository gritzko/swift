# Written by Victor Grishchenko, Arno Bakker 
# see LICENSE.txt for license information
#
# Requirements:
#  - scons: Cross-platform build system    http://www.scons.org/
#  - googletest: Google C++ Test Framework http://code.google.com/p/googletest/
#       * Install in ..\gtest-1.4.0
#  - google-glog: Google Logging Library for C++  http://code.google.com/p/google-glog/
#       * Install in ..\glog-0.3.0 
#       * I get ..\glog-0.3.0\src\windows\glog/log_severity.h(51) : error C2059: syntax error :
#         'constant' while running scons. Apparently the ERROR constant is already defined somewhere.
#         #undef ERROR before the def allows include.
#
#  - OpenSSL: http://www.slproweb.com/products/Win32OpenSSL.html
#       * Install non-light Win32 binary in \openssl
#       * Using a openssl-0.9.8k tar-ball doesn't work as the includes there
#         are symbolic links which get turned into 0 length files by 7Zip. 
#

import os
import re
import sys

DEBUG = True

TestDir='tests'

target = 'p2tp'
source = [ 'bin64.cpp','hashtree.cpp','datagram.cpp','bins.cpp',
    'transfer.cpp', 'p2tp.cpp', 'sendrecv.cpp', 
    'compat/hirestimeofday.cpp', 'compat/util.cpp']

env = Environment()
if sys.platform == "win32":
	# "MSVC works out of the box". Sure.
	# Make sure scons finds cl.exe, etc.
	env.Append ( ENV = { 'PATH' : os.environ['PATH'] } )

	# Make sure scons finds std MSVC include files
	if not 'INCLUDE' in os.environ:
		print "p2tp: Please run scons in a Visual Studio Command Prompt"
		sys.exit(-1)
		
	include = os.environ['INCLUDE']
	include += '..\\gtest-1.4.0\\include;'
	include += '..\\glog-0.3.0\\src\\windows;' # Funky
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
	libs = ['libglog','ws2_32']
	if DEBUG:
		libs += ['gtestd','libeay32MTd']
	else:
		libs += ['gtest','libeay32']
		
	# Update lib search path
	libpath = os.environ['LIBPATH']
	if DEBUG:
		libpath += '\\build\\gtest-1.4.0\\msvc\\gtest\\Debug;'
		libpath += '\\build\\glog-0.3.0\\vsprojects\\libglog\\Debug;'
		libpath += '\\openssl\\lib\\VC\\static;'
	else:
		libpath += '\\build\\gtest-1.4.0\\msvc\\gtest\\Release;'
		libpath += '\\build\\glog-0.3.0\\vsprojects\\libglog\\Release;'
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
	libs = ['stdc++','gtest','glog','pthread','crypto']
	if 'LIBPATH' in os.environ:
  	    libpath = os.environ['LIBPATH']
	else:
	    libpath = ""
	    print "To use external libs, set LIBPATH environment variable to list of colon-separated lib dirs"

env.StaticLibrary (
    target= target,
    source = source,
    LIBS=libs,
    LIBPATH=libpath )

Export("env")
Export("libs")
Export("libpath")
Export("DEBUG")
SConscript('tests/SConscript')
