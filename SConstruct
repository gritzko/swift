import os
import re

TestDir='tests'

env = Environment(CPPPATH = ['.'])

env.SharedLibrary (
    target='p2tp',
    source = [ 'bin64.cpp','hashtree.cpp','datagram.cpp',
               'bins.cpp', 'transfer.cpp' ],
    LIBS=['stdc++','gtest','glog','crypto'] )

SConscript('tests/SConscript')
