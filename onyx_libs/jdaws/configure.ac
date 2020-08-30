# -*- Autoconf -*- 
#
# Top-level configure.ac file for libjade
#
# Process this file with autoconf to produce a configure script.
# 
# Copyright 2010 MCN Technologies Inc.. All rights reserved.
#      

AC_PREREQ(2.59)

AC_INIT([libjdaws], [0.5], [])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(src/JdAwsContext.cpp)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS(config/config.h)
AM_INIT_AUTOMAKE
AC_PROG_CXX
dnl next 4 lines are a hack to avoid libtool's
dnl needless checks for Fortran compilers
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile
                src/Makefile
                tests/Makefile])
AC_OUTPUT

