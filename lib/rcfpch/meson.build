# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 OKTET Labs Ltd. All rights reserved.

rcf_sniffers_dummy = library('rcf_sniffers_dummy',
                             'rcf_sniffers_dummy.c', install: install_lib,
                             dependencies: dep_lib_conf_oid)
dep_lib_rcf_sniffers_dummy = declare_dependency(link_with: rcf_sniffers_dummy,
                                                dependencies: dep_lib_conf_oid)

if get_option('libs').split().contains('netconf')
    c_args += [ '-DWITH_NETCONF' ]
    te_libs += [ 'netconf' ]
endif

if get_option('sniffers')
    c_args += [ '-DWITH_SNIFFERS' ]
endif

f = 'pthread_atfork'
if dep_threads.found() and cc.has_function(f, dependencies: dep_threads)
    c_args += [ '-DHAVE_' + f.to_upper() ]
endif

headers += files(
    'rcf_ch_api.h',
    'rcf_pch.h',
    'rcf_pch_ta_cfg.h',
)
sources += files(
    'rcf_pch.c',
    'rcf_pch_conf.c',
    'rcf_pch_file.c',
    'rcf_pch_plugin.c',
    'rcf_pch_rpc.c',
    'rcf_pch_ta_cfg.c',
    'rcf_pch_var.c',
)
te_libs += [
    'tools',
    'conf_oid',
    'loggerta',
    'rpcxdr',
    'rpctransport',
    'agentlib',
]
