#
#  Top level Makefile for libjdaws
#
#  Copyright 2011 MCN Technologies Inc.. All rights reserved.
#

lib_LTLIBRARIES 	= libjdaws.la

AM_CPPFLAGS = -I../include/

libjdaws_la_LDFLAGS = -version-info 0:5:0

libjdaws_la_SOURCES = \
	JdAwsContext.cpp \
	JdAwsS3.cpp \
	JdAwsS3UpnpHttpConnection.cpp

include_HEADERS = \
	../include/JdAwsContext.h 	\
	../include/JdAwsS3.h \
	../include/JdAwsS3Request.h \
	../include/JdAwsS3UpnpHttpConnection.h 

