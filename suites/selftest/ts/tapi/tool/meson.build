# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020-2023 OKTET Labs Ltd. All rights reserved.

packages = [
    'haproxy',
    'memcached',
    'nptcp',
    'perf',
    'redis',
    'torrent',
    'trex',
]

install_data([ 'package.xml' ], install_dir: package_dir)

tapi_tool_mydir = package_dir
foreach package : packages
    package_dir = join_paths(tapi_tool_mydir, package)
    package_tests_c = []
    subdir(package)
endforeach
