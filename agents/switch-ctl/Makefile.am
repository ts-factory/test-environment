# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.

ACLOCAL_AMFLAGS = -I @TE_M4@

AM_CPPFLAGS = @TE_CPPFLAGS@ $(TE_CPPFLAGS_VAR) \
            -I$(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/include
AM_CFLAGS = @TE_CFLAGS@ $(TE_CFLAGS_VAR)
AM_LDFLAGS = @TE_LDFLAGS@ -L$(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib


bin_PROGRAMS = taswitch-ctl

#
# -lrcfpch must be before @TE_LDADD@ since the last may contain
# -ltad_ch with names used in -lrcfpch.
#
taswitch_ctl_LDADD = symtbl.o -lrcfpch -l@TAD@ @TE_LDADD@ -lconf_oid \
                     -lcomm_net_agent -lloggerta -ltools -llogger_core \
                     -lpthread -lstdc++

taswitch_ctl_SOURCES = main.c conf.c

taswitch_ctl_DEPENDENCIES = @TE_DEPENDENCIES@ symtbl.o \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/librcfpch.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/lib@TAD@.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/libconf_oid.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/libcomm_net_agent.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/libloggerta.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/libtools.a \
    $(DESTDIR)/$(prefix)/../$(TE_PLATFORM)/lib/liblogger_core.a

if CFG_SNIFFERS_DUMMY
taswitch_ctl_LDADD+= -lrcf_sniffers_dummy
endif

symtbl.o: main.o conf.o @TE_DEPENDENCIES@
	te_generate_symtbl ${NM} empty empty symtbl.c $^
	${CC} -c -o $@ symtbl.c
