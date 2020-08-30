#
#  Makefile for libjade unit tests
#
#  Copyright 2010 MCN Technologies Inc.. All rights reserved.
#

AM_DEFAULT_SOURCE_EXT = .cpp
AM_CPPFLAGS = -I../include 
AM_LDFLAGS = -fexceptions -lpthread -ljdcommon -lsqlite3 -lupnp -lixml -lthreadutil -lcrypto

bin_PROGRAMS = \
	JdAwsS3UpnpHttpConnectionTest

LDADD = ../src/.libs/libjdaws.a