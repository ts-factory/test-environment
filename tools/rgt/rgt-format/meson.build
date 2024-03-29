# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 OKTET Labs Ltd. All rights reserved.

subdir('xml2html')
subdir('xml2text')
subdir('xml2html-multi')

tool_deps = [dep_libxml2, dep_glib, dep_lib_tools, dep_lib_logger_file, dep_lib_logger_core]

libcapture = static_library(
    'libcapture',
    ['capture.c', 'capture.h'],
    include_directories: inc,
    c_args: c_args,
    dependencies: [dep_libxml2, dep_glib, dep_lib_tools],
    link_with: libtmpls,
)

libxml2fmt_core = static_library(
    'libxml2fmt_core',
    'xml2fmt_core.c',
    include_directories: inc,
    c_args: c_args,
    dependencies: [dep_libxml2, dep_glib, dep_popt, dep_lib_tools,
                   dep_lib_logger_file, dep_lib_logger_core],
)

libxml2multi = static_library(
    'libxml2multi',
    'xml2multi_common.c',
    include_directories: inc,
    c_args: c_args,
    dependencies: [dep_libxml2, dep_glib, dep_popt, dep_lib_tools],
)

dep_jansson = dependency('jansson', required: false)
if dep_jansson.found() and cc.has_function('json_loadb',
                                           args: c_args,
                                           dependencies: dep_jansson)
    tool_deps += [ dep_jansson ]
    c_args += [ '-DHAVE_LIBJANSSON=1' ]
endif

libmi_msg = static_library(
    'libmi_msg',
    ['mi_msg.c'],
    include_directories: inc,
    c_args: c_args,
    dependencies: [dep_lib_tools, dep_jansson],
)

common_util_path = join_paths('..', get_option('datadir'), 'rgt-format')

rgt_xml2_tools = [
    ['xml2text', xml2text_tmpls],
    ['xml2html', xml2html_tmpls],
    ['xml2html-multi', xml2html_multi_tmpls],
]

foreach tool: rgt_xml2_tools
    header = custom_target(
        tool.get(0) + '_header',
        output: tool.get(0) + '.h',
        input: [tool.get(1)],
        capture: true,
        command: [
            header_gen_script,
            '@INPUT@',
        ],
    )
    executable(
        'rgt-' + tool.get(0),
        files(join_paths(tool.get(0), tool.get(0) + '.c'), 'capture.h'),
        header,
        include_directories: inc,
        dependencies: tool_deps,
        c_args: c_args,
        link_with: [libtmpls, libcapture, libxml2multi, libxml2fmt_core,
                    libmi_msg],
        install: true,
    )
endforeach

executable(
    'rgt-xml2json',
    files('xml2json/xml2json.c'),
    include_directories: inc,
    dependencies: tool_deps,
    c_args: c_args,
    link_with: [libtmpls, libxml2multi, libxml2fmt_core, libmi_msg],
    install: true,
)
