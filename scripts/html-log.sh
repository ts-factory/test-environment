#! /bin/bash
#
# Script to generate html test log.
#
# Copyright (C) 2012 OKTET Labs, St.-Petersburg, Russia
#
# Author Roman Kolobov <Roman.Kolobov@oktetlabs.ru>
#
# $Id: $

HTML_OPTION=false OUTPUT_HTML=true EXEC_NAME=$0 `dirname \`which $0\``/log.sh "$@"
