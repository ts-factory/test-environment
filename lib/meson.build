# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2023 OKTET Labs Ltd. All rights reserved.

pkg = import('pkgconfig')

tad_protocols = [
    'arp',
    'atm',
    'bridge',
    'cli',
    'dhcp',
    'eth',
    'flow',
    'forw',
    'geneve',
    'gre',
    'igmp',
    'ipstack',
    'iscsi',
    'pcap',
    'ppp',
    'rte_mbuf',
    'snmp',
    'socket',
    'vxlan',
]

#
# Lists of libraries in build/dependency order
#

# Common libraries
te_common_libs = [
    'logger_core',
    'tools',
    'conf_oid',
    'asn',
    'ndn',
    'rpcxdr',
    'rpc_dpdk',
    'rpc_types',
    'ovs_flow',
]

# Test Engine libraries
te_engine_libs = [
    'bsapi',
    'logic_expr',
    'ipc',
    'loggerten',
    'log_proc',
    'comm_net_engine',
    'rcfapi',
    'rcfunix',
    'confapi',
    'rcfrpc',
    'tapi',
    'tapi_rpc',
    'tapi_bpf',
    'tapi_env',
    'tapi_job',
    'tapi_netconf',
    'tapi_fio',
    'tapi_serial',
    'tapi_storage',
    'tapi_nvme',
    'tapi_tad',
    'tapi_tcp_states',
    'tapi_packetdrill',
    'tapi_performance',
    'tapi_tool',
    'trc',
    'rpcc_dpdk',
    'tapi_dpdk',
    'tapi_gtest',
    'tapi_upnp',
    'tapi_media',
    'tapi_wifi',
    'tapi_dns',
    'tapi_ovs',
]

# Test Agent libraries
te_agent_libs = [
    'comm_net_agent',
    'loggerta',
    'netconf',
    'rpctransport',
    'agentlib',
    'rpcserver',
    'rcfpch',
    'tad',
    'ta_job',
    'ta_restconf',
    'rpcs_bpf',
    'rpcs_dpdk',
    'rpcs_job',
    'rpcs_netconf',
    'rpcs_sai',
    'rpcs_serial',
    'upnp_cp',
]

all_libs = te_common_libs + te_engine_libs + te_agent_libs

libs = get_option('libs').split()

# Process list of libraries to build in order to:
#  - add external libraries to all_libs
foreach l : libs
    # Add external library at the end of the list, no TE libraries can
    # depend on it
    if not all_libs.contains(l)
        all_libs += [ l ]
    endif
endforeach

# Static version of common libraries is required for agents and agent apps
build_agents = get_option('agents').split().length() != 0
build_agent_apps = get_option('agent-apps').split().length() != 0

#
# Lists of options for libraries to be used as variables, this required to
# support build of the same library name times with different options and name.
#
lib_opts_agentlib = [
    'agentlib-dlfcn-lookup',
    'agentlib-ftp',
    'agentlib-power-sw',
    'agentlib-upnp',
]

lib_opts_rpcserver = [
    'rpcserver-agentlib',
    'rpcserver-idle-plugins',
    'rpcserver-smart-mem',
    'static-rpc-lookup',
    'typesafe-rpc',
]

lib_opts_tad = [
    'tad-cs',
    'tad-packet_mmap_rx_ring',
    'tad-protocols',
]

# Array of arrays of custom libraries options. Each value should look like:
# [ 'build-lib-name', 'real-te-lib-name', 'option-name', 'value' ]
custom_lib_opts = []

opts = get_option('custom-lib-opts')
foreach opt : opts
    o = opt.split(':')
    if o.length() != 4
        error('Wrong custom option: ' + opt)
    elif not all_libs.contains(o[0])
        error('Wrong library name ' + o[0] + ' in ' + opt)
    elif not is_variable('lib_opts_' + o[1])
        error('Custom options for library ' + o[1] + ' are not supported')
    elif (o[2]).strip() == ''
        error('Empty option name in ' + opt)
    else
        custom_lib_opts += [ o ]
    endif
endforeach

# Iterate over all libraries in order to follow build order
foreach l : all_libs
    if libs.contains(l)
        if te_engine_libs.contains(l)
            build_lib_shared = true
            build_lib_static = false
        elif te_common_libs.contains(l)
            build_lib_shared = true
            build_lib_static = build_agents or build_agent_apps
        elif te_agent_libs.contains(l)
            build_lib_shared = false
            build_lib_static = true
        else
            # External libraries are agent side only now
            build_lib_shared = false
            build_lib_static = true
        endif
        if te_common_libs.contains(l) or te_engine_libs.contains(l)
            install_lib = install_dev
        else
            install_lib = get_option('install-dev')
        endif
        # Library name to build, may be overridden in subdir
        libname = l
        # List of headers to install
        headers = []
        # List of scripts to install
        scripts = []
        # List of sources to build library
        sources = []
        # List of include directories required to build
        includes = [ te_include ]
        # C compiler arguments to be used
        c_args = []
        # List of dependencies to add include directories and libraries
        deps = []
        # List of dependencies in TE library name format
        te_libs = []
        # Whether this library needs to be linked wholly
        link_whole = false
        # logger_core is required dependency for everything else
        if l != 'logger_core'
            te_libs += [ 'logger_core' ]
        endif

        lrname=l
        foreach clo : custom_lib_opts
            if clo[0] == l
                lrname = clo[1]
            endif
        endforeach

        # Process options for library as variables
        lib_opts_name = 'lib_opts_' + lrname
        if is_variable(lib_opts_name)
            foreach opt_name : get_variable(lib_opts_name)
                custom_opt_value = get_option(opt_name)
                # Look to value in custom-lib-opts
                foreach clo : custom_lib_opts
                    if clo[0] == l and clo[1] == lrname and clo[2] == opt_name
                        if clo[3] == 'true'
                            custom_opt_value = true
                        elif clo[3] == 'false'
                            custom_opt_value = false
                        else
                            custom_opt_value = clo[3]
                        endif
                    endif
                endforeach
                # Note: prior to v0.46.1, the value parameter could not be an array type
                set_variable(('opt-' + opt_name).underscorify(), custom_opt_value)
            endforeach
        endif

        # Library will set this to true in case if it works with opt-* variables
        is_multi_build_library = false

        subdir(l)

        if is_variable(lib_opts_name) and not is_multi_build_library
            error('custom library options are not supported: ' + l)
        endif

        # Clenaup: meson does not allow to unset variable, so just set it to false
        if is_variable(lib_opts_name)
            foreach opt_name : get_variable(lib_opts_name)
                set_variable(('opt-' + opt_name).underscorify(), false)
            endforeach
        endif

        # If there are headers to be installed, do it
        if install_lib and headers.length() != 0
            install_headers(headers)
        endif

        if install_lib and scripts.length() != 0
            install_data(scripts, install_dir: get_option('bindir'))
        endif

        # If there are sources to be compiled as library, do it
        if sources.length() != 0
            # Automatically add the library itself to search for headers
            includes += include_directories(l)
            if build_lib_static
                deps_static = deps
                foreach te_lib : te_libs
                    deps_static += [ get_variable('dep_lib_static_' + te_lib) ]
                endforeach

                lib = static_library(libname, sources, install: install_lib,
                                     include_directories: includes,
                                     implicit_include_directories: false,
                                     c_args: c_args, dependencies: deps_static)
                # Libraries are used by agents build to generate symbol table
                set_variable('te_lib_' + libname, lib)
                lib_static = lib

                if link_whole
                    dep = declare_dependency(link_whole: lib,
                                             include_directories: includes,
                                             dependencies: deps_static)
                else
                    dep = declare_dependency(link_with: lib,
                                             include_directories: includes,
                                             dependencies: deps_static)
                endif
                set_variable('dep_lib_static_' + libname, dep)
            endif
            if build_lib_shared
                deps_shared = deps
                foreach te_lib : te_libs
                    deps_shared += [ get_variable('dep_lib_' + te_lib) ]
                endforeach
                lib = shared_library(libname, sources, install: install_lib,
                                     include_directories: includes,
                                     implicit_include_directories: false,
                                     c_args: c_args, dependencies: deps_shared)
                dep = declare_dependency(link_with: lib,
                                         include_directories: includes,
                                         dependencies: deps_shared)
                set_variable('dep_lib_' + libname, dep)
                lib_shared = lib
            endif
            if build_lib_static
                if build_lib_shared
                  # Bless the lib_static object with a reference to the pkg-config
                  # file so it could be used in "Requires" instead of "Libs".
                  # Doesn't seem to be needed with older meson versions.
                  pkg.generate(lib_static,
                               filebase: 'te-' + libname)
                  pkg.generate(lib_shared,
                               filebase: 'te-' + libname,
                               libraries_private: deps_static)
                else
                  pkg.generate(lib_static,
                               filebase: 'te-' + libname)
                endif
            else
              pkg.generate(lib_shared,
                           filebase: 'te-' + libname)
            endif
        endif
    endif
endforeach
