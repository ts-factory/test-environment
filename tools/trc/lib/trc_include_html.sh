#!/bin/sh
# Copyright (C) 2021-2022 OKTET Labs Ltd. All rights reserved.
include_path="$(dirname "$(which "$0")")"
cat $include_path/$1
