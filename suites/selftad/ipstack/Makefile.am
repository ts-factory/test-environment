# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.

tetestdir=$(prefix)/@PACKAGE_NAME@/$(subdir)

dist_tetest_DATA = package.xml \
                   udp-data-csap.asn udp-filter.asn


noinst_HEADERS=ipstack-ts.h

AM_CPPFLAGS = @TE_CPPFLAGS@ -I$(DESTDIR)/$(includedir)
AM_CFLAGS = @TE_CFLAGS@ @NET_SNMP_CFLAGS@
AM_LDFLAGS = @TE_LDFLAGS@ -L$(DESTDIR)/$(libdir)


tetest_PROGRAMS = tcp_simple ip4_simple ip4_recv_send ip4_udp_dgm \
	ip4_fragments tcp_conn tcp_conn_reset tcp_data udp_socket \
	tcp_flood_func tcp_flood_iter

LDADD = -ltapi_rpc -lrpc_types -ltapi_env -ltapi_tad -lndn -lasn -ltapi \
	-lrcfrpc -lrcfapi -lrpcxdr -lconfapi -lconf_oid -llogger_ten -lipc \
	-ltools -llogger_core -lpthread @NET_SNMP_LIBS@

