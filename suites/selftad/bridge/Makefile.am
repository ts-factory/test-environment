# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.

#EXTRA_DIST = package.dox

tetestdir=$(prefix)/@PACKAGE_NAME@/$(subdir)

dist_tetest_DATA = package.xml


AM_CPPFLAGS = @TE_CPPFLAGS@ -I$(DESTDIR)/$(includedir)
AM_CFLAGS = @TE_CFLAGS@
AM_LDFLAGS = @TE_LDFLAGS@ -L$(DESTDIR)/$(libdir)


tetest_PROGRAMS = create send0 recv0 recv1

LDADD = -lrcfapi -lrpcxdr -llogger_ten -lipc -lpthread \
        -ltapi_tad -lndn -lasn -ltools -llogger_core

