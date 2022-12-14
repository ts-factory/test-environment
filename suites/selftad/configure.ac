# SPDX-License-Identifier: Apache-2.0
# Process this file with autoconf to produce a configure script.
#
# Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.

AC_PREREQ([2.53])
AC_INIT([selftad],[0.1])

AC_CONFIG_SRCDIR([configure.ac])

AC_CONFIG_AUX_DIR([../../auxdir])

AM_INIT_AUTOMAKE([1.8.5 foreign -Wall])
TE_SETUP_AR

AC_CANONICAL_HOST
# Checks for programs.
AC_PROG_CC
AC_PROG_YACC
AC_PROG_LEX
AC_PROG_RANLIB
AC_PROG_INSTALL

AC_SUBST([TE_CPPFLAGS])
AC_SUBST([TE_CFLAGS])
AC_SUBST([TE_LDFLAGS])
AC_SUBST([TE_PATH])
AC_SUBST([TE_PLATFORM])

AC_ARG_VAR([TE_CFLAGS])
AC_ARG_VAR([TE_LDFLAGS])
AC_ARG_VAR([TE_INSTALL])
AC_ARG_VAR([TE_PATH])
AC_ARG_VAR([TE_PLATFORM])


AC_CONFIG_HEADERS([config.h])

# It's assumed that stdio.h always presents.
# Check for stdlib.h, stdarg.h, string.h and float.h.
AC_HEADER_STDC
AC_CHECK_HEADERS([\
unistd.h \
pthread.h \
semaphore.h \
signal.h \
assert.h \
time.h \
sys/time.h \
sys/types.h \
sys/socket.h \
sys/select.h \
sys/poll.h \
sys/ioctl.h \
sys/queue.h \
sys/sendfile.h \
netinet/in.h \
netinet/ip.h \
netinet/tcp.h \
netinet/ether.h \
net/if.h \
net/ethernet.h \
arpa/inet.h \
netdb.h \
setjmp.h \
])

AC_CHECK_HEADERS(ucd-snmp/snmp_impl.h, , ,
#include<sys/types.h>
#include<ucd-snmp/asn1.h>
)

AC_CHECK_HEADERS(net-snmp/definitions.h, , ,
#include<net-snmp/net-snmp-config.h>
)

AC_PATH_PROG(NET_SNMP_CONFIG, net-snmp-config, no)
if test "$NET_SNMP_CONFIG" = "no" ; then
    AC_MSG_ERROR([net-snmp-config is required but not found])
else
    NET_SNMP_CFLAGS=`$NET_SNMP_CONFIG --cflags`
    NET_SNMP_LIBS="`$NET_SNMP_CONFIG --libs` -lcrypto"
fi

SELFTAD_TE_DEPS="\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libipc.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librcfapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libconfapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libconf_oid.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libconf_types.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librcfrpc.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librpc_types.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librpcxdr.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/liblogger_ten.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libndn.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_rpc.a"

AC_SUBST([NET_SNMP_CFLAGS])
AC_SUBST([NET_SNMP_LIBS])
AC_SUBST([SELFTAD_TE_DEPS])

AC_DEFINE_UNQUOTED(SUITE_SRCDIR, "$srcdir", [The suite source directory])


AC_CONFIG_FILES([\
Makefile \
eth/Makefile \
ipstack/Makefile \
dhcp/Makefile \
snmp/Makefile \
cli/Makefile \
iscsi/Makefile \
])


AC_OUTPUT
