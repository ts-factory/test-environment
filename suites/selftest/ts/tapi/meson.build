# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020-2022 OKTET Labs Ltd. All rights reserved.

packages = [
    'cache',
    'file',
    'fio',
    'job',
    'jobopt',
    'tool',
    'dns',
]

install_data([ 'package.xml' ], install_dir: package_dir)

tapi_mydir = package_dir
foreach package : packages
    package_dir = join_paths(tapi_mydir, package)
    package_tests_c = []
    subdir(package)
endforeach
