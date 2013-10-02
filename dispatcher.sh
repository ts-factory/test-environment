#! /bin/bash
#
# Test Environment: Dispatcher 
#
# Script to build and start Test Environment.
#
# Copyright (C) 2003-2012 Test Environment authors (see file AUTHORS
# in the root directory of the distribution).
#
# Test Environment is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License as 
# published by the Free Software Foundation; either version 2 of 
# the License, or (at your option) any later version.
# 
# Test Environment is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
# MA  02111-1307  USA
#
# Author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
# Author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
#
# $Id$
#

bin_sh=`ls -l /bin/sh | sed "s/^.*-> //g"`
if (test x$bin_sh != x"/bin/bash") && (test x$bin_sh != x"bash") ; then
    echo "#############################################"
    echo "#### /bin/sh is not bash on your system. ####"
    echo "#### This can make TE build and usage    ####"
    echo "#### impossible.                         ####"
    echo "#############################################"
fi

TE_RUN_DIR="${PWD}"

DISPATCHER="$(which "$0")"
while test -L "$DISPATCHER" ; do
    DISPATCHER="$(dirname "$DISPATCHER")/$(readlink "${DISPATCHER}")"
done
pushd "$(dirname "$DISPATCHER")" >/dev/null
DISPATCHER_DIR="${PWD}"
popd >/dev/null

usage()
{
cat <<EOF
Usage: dispatcher.sh [<generic options>] [[<test options> tests ]...
Generic options:
  -q                            Suppress part of output messages
  --force                       Never prompt                                

  --daemon[=<PID>]              Run/use TE engine daemons
  --shutdown[=<PID>]            Shut down TE engine daemons on exit

  --conf-dir=<directory>        specify configuration file directory
                                (\${TE_BASE}/conf or . by default)

    In configuration files options below <filename> is full name of the
    configuration file or name of the file in the configuration directory.

  --conf-builder=<filename>     Builder config file (${CONF_BUILDER} by default)
  --conf-cs=<filename>          Configurator config file (${CONF_CS} by default)
  --conf-logger=<filename>      Logger config file (${CONF_LOGGER} by default)
  --conf-rcf=<filename>         RCF config file (${CONF_RCF} by default)
  --conf-rgt=<filename>         RGT config file (rgt.conf by default)
  --conf-tester=<filename>      Tester config file (${CONF_TESTER} by default)
  --conf-nut=<filename>         NUT config file (${CONF_NUT} by default)

  --script=<filename>           Name of the file with shell script to be
                                included as source

  --live-log                    Run RGT in live mode

  --log-html=<dirname>          Name of the directory with structured HTML logs
                                to be generated (do not generate by default)
  --log-plain-html=<filename>   Name of the file with plain HTML logs
                                to be generated (do not generate by default)
  --log-txt=<filename>          Name of the file with logs in text format
                                to be generated (log.txt by default)
  --log-txt-detailed-packets    Include detailed packet dumps in text log.

  --no-builder                  Do not build TE
  --no-nuts-build               Do not build NUTs
  --no-tester                   Do not run Tester
  --no-cs                       Do not run Configurator
  --no-rcf                      Do not run RCF
  --no-run                      Do not run Logger, RCF, Configurator and Tester
  --no-autotool                 Do not try to perform autoconf/automake after
                                package configure failure

  --opts=<filename>             Get additional command-line options from file

  --cs-print-trees              Print configurator trees.
  --cs-log-diff                 Log backup diff unconditionally.

  --build-only                  Build TE, do not run RCF and Configurator,
                                build but do not run Test Suites
                            
  --build=path                  Build package specified in the path.
  --build-log=path              Build package with log level 0xFFFF.
  --build-nolog=path            Build package with undefined log level.
  --build-cs                    Build Configurator.
  --build-logger                Build Logger.
  --build-rcf                   Build RCF.
  --build-tester                Build Tester.
  --build-lib-xxx               Build host library xxx.
  --build-ta-xxx                Build Test Agent xxx.
  --build-log-xxx               Build package with log level 0xFFFF.
  --build-nolog-xxx             Build package with undefined log level.
  --build-parallel[=num]        Enable parallel build using num threads.

  --tester-suite=<name>:<path>  Specify path to the Test Suite.
  --tester-no-run               Don't run any tests.
  --tester-no-build             Don't build any Test Suites.
  --tester-no-trc               Don't use Testing Results Comparator.
  --tester-no-cs                Don't interact with Configurator.
  --tester-no-cfg-track         Don't track configuration changes.
  --tester-no-logues            Disable prologues and epilogues globally.
  --tester-req=<reqs-expr>      Requirements to be tested (logical expression).
  --tester-no-reqs              Ignore requirements, run all possible
                                iterations.
  --tester-quietskip            Quietly skip tests which do not meet specified
                                requirements (default).
  --tester-verbskip             Force Tester to log skipped iterations.

    The following Tester options get test path as a value:
        <testpath>      :=  / | <path-item> | <testpath>/<path-item>
        <path-item>     := <test-name>[:<args>][%<iter-select>][*<repeat>]
        <args>          := <arg>[,<args>]
        <arg>           := <param-name>=<values> | <param-name>~=<values>
        <values>        := <value> | { <values-list> }
        <values-list>   := <value>[,<values-list>]
        <iter-select>   := <iter-number>[+<step>] | <hash>
    For example,
        --tester-run=mysuite/mypkg/mytest:p1={a1,a2}
    requests to run all iterations of the test 'mytest' when its parameter
    'p1' is equal to 'a1' or 'a2';
        --tester-run=mysuite/mypkg/mytest%3*10
    requests to run 10 times third iteration of the same test.

  --tester-fake=<testpath>      Don't run any test scripts, just emulate test
                                scenario.
  --tester-run=<testpath>       Run test under the path.
  --tester-run-from=<testpath>  Run tests starting from the test path.
  --tester-run-to=<testpath>    Run tests up to the test path.
  --tester-exclude=<testpath>   Exclude specified tests from campaign.
  --tester-vg=<testpath>        Run test scripts under specified path using
                                valgrind.
  --tester-gdb=<testpath>       Run test scripts under specified path using
                                gdb.

  --tester-random-seed=<number> Random seed to initialize pseudo-random number
                                generator
  --tester-verbose              Increase verbosity of the Tester (the first
                                level is set by default).
  --tester-quiet                Decrease verbosity of the Tester.
  --tester-out-tin              Output Test Identification Numbers to terminal
  --tester-out-expected         If result is expected (in accordance with TRC),
                                output the result together with OK
  --tester-interactive          Interactive ask user for tests to run.

  --test-sigusr2-verdict        Handle the SIGUSR2 signal in test and stop it by TEST_VERDICT.
                                By default the SIGUSR2 handled like SIGINT, it stops testing.

  --trc-log=<filename>          Generate bzip2-ed TRC log
  --trc-db=<filename>           TRC database to be used
  --trc-tag=<TAG>               Tag to get specific expected results
  --trc-key2html=<filename>     File with key substitutions when output to HTML
                                report
  --trc-ignore-log-tags         Ignore tags from log
  --trc-html=<filename>         Name of the file for HTML report
  --trc-brief-html=<filename>   Name of the file for brief HTML report
  --trc-html-header=<filename>  Name of the file with header for all HTML
                                reports.
  --trc-txt=<filename>          Name of the file for text report
                                (by default, it is generated to stdout)
  --trc-quiet                   Do not output total statistics to stdout
  --trc-comparison=<method>     Specify the method to match parameter values in TRC
                                - exact (the default)
                                - casefold 
                                - normalised (XML-style space normalization)
                                - tokens (the values are split into tokens which are
                                either sequences of XML name characters or single characters;
                                the matching is done on these lists; in additional, numeric
                                tokens are compared as numbers (so e.g. 10 and 0xa render equal)
  --trc-update                  Update TRC database
  --trc-init                    Initialize TRC database (be careful,
                                TRC database file will be rewritten)

  --vg-engine                   Run RCF, Configurator, Logger and Tester under
                                valgrind (without by default)
  --vg-cs                       Run Configurator under valgrind
  --vg-logger                   Run Logger under valgrind (without by default)
  --vg-rcf                      Run RCF under valgrind (without by default)
                                (without by default)
  --vg-tester                   Run Tester under valgrind (without by default)
  --gdb-tester                  Run Tester under gdb.
  --tce                         Do TCE processing

 --sniff-not-feed-conf          Do not feed the sniffer configuration file
                                to Configurator.
 --sniff=<TA/iface>             Run sniffer on *iface* of the *TA*.
 --sniff-filter=<filter>        Add for the sniffer filter(tcpdump-like
                                syntax). See 'man 7 pcap-filter'.
 --sniff-name=<name>            Add for the sniffer a human-readable name.
 --sniff-snaplen=<val>          Add for the sniffer restriction on maximum
                                number of bytes to capture for one packet.
                                By default: ${TE_SNIFF_SNAPLEN:+unlimited}.
 --sniff-space=<val>            Add for the sniffer restriction on maximum
                                overall size of temporary files, Mb.
                                By default: 64 Mb.
 --sniff-fsize=<val>            Add for the sniffer restriction on maximum
                                size of the one temporary file, Mb.
                                By default: 16 Mb.
 --sniff-rotation=<x>           Add for the sniffer restriction on number of
                                temporary files. This option excluded by
                                the *--sniff-ta-log-ofill-drop* option.
                                By default: 4.
 --sniff-ofill-drop             Change overfill handle method of temporary
                                files for the sniffer to tail drop.
                                By default overfill handle method is rotation.
 --sniff-log-dir=<path>         Path to the *TEN* side capture files.
                                By default used: ${TE_SNIFF_DEF_LOG_DIR}.
 --sniff-log-name=<pattern>     *TEN* side log file naming pattern, the
                                following format specifies are supported:
                                - %a : agent name
                                - %u : user name
                                - %i : iface name
                                - %s : sniffer name
                                - %n : sniffer session sequence number
                                By default '%a_%i_%s_%n' is used. The pcap
                                extension will be added automatically.
 --sniff-log-osize=<val>        Maximum *TEN* side logs cumulative size for all
                                sniffers, Mb.
                                By default: ${TE_SNIFF_LOG_OSIZE:+unlimited}.
 --sniff-log-space=<val>        Maximum *TEN* side logs cumulative size for one
                                sniffer, Mb. By default: ${TE_SNIFF_LOG_SPACE} Mb.
 --sniff-log-fsize=<val>        Maximum *TEN* side capture file size for each
                                sniffer in Mb.
                                By default: ${TE_SNIFF_LOG_FSIZE} Mb.
 --sniff-log-ofill-drop         Change overfill handle method to tail drop.
                                By default overfill handle method is rotation.
 --sniff-log-period=<val>       Period of taken logs from agents, milliseconds.
                                By default: ${TE_SNIFF_LOG_PERIOD} msec.
 --sniff-log-conv-disable       Option to disable capture logs conversion
                                and merge with the main log.

    The script exits with a status of zero if everything does smoothly and
    all tests, if any tests are run, give expected results. A status of two
    is returned, if some tests are run and give unexpected results.
    A status of one indicates start up or any internal failure.

EOF
}

exit_with_log()
{
    rm -rf "${TE_TMP}"
    cd "${TE_RUN_DIR}"
    exit 1
}

# Parse options

EXT_OPTS_PROCESSED=

QUIET=
DAEMON=
SHUTDOWN=yes

# No additional Tester options by default
TESTER_OPTS=
# No additional TRC options and tags by default
TRC_OPTS=
TRC_TAGS=
# No additional option for rgt-xml2html-multi tool
RGT_X2HM_OPTS=
# Configurator options
CS_OPTS=
# Building options
BUILDER_OPTS=
BUILD_MAKEFLAGS=

LIVE_LOG=

VG_OPTIONS="--tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=32"
VG_RCF=
VG_CS=
VG_LOGGER=
VG_TESTER=
GDB_TESTER=

# Subsystems to be initialized
BUILDER=yes
TESTER=yes
RCF=yes
CS=yes
LOGGER=yes

# Whether Test Suite should be built
BUILD_TS=yes

# Configuration directory
CONF_DIR=

# Directory for raw log file
TE_LOG_DIR="${TE_RUN_DIR}"

# Default directory for capture log files
TE_SNIFF_DEF_LOG_DIR="${TE_RUN_DIR}/caps"

# Sniffer polling setting
TE_SNIFF_LOG_OSIZE=0     # Megabytes (unlimited)
TE_SNIFF_LOG_SPACE=256   # Megabytes
TE_SNIFF_LOG_NAME=""     # Pattern
TE_SNIFF_LOG_FSIZE=64    # Megabytes
TE_SNIFF_LOG_OFILL=0     # Rotation
TE_SNIFF_LOG_PERIOD=200  # Milliseconds

TE_SNIFF_LOC=            # The variable contains an agent name and iface.
                         # It gets a string from command line.
TE_SNIFF_NAME=
TE_SNIFF_FILTER=
TE_SNIFF_SNAPLEN=

# Configuration files
CONF_BUILDER=builder.conf
CONF_LOGGER=logger.conf
CONF_TESTER=tester.conf
CONF_CS=cs.conf
CONF_RCF=rcf.conf
CONF_RGT=
CONF_NUT=nut.conf

# Whether NUTs processing is requested
CONF_NUT_SET=
# Whether NUTs should be built
BUILD_NUTS=yes

# If yes, generate on-line log in the logging directory
LOG_ONLINE=

# Name of the file with test logs to be genetated
RGT_LOG_TXT=log.txt
# Name of the file with plain HTML logs to be genetated
RGT_LOG_HTML_PLAIN=
# Name of the directory for structured HTML logs to be genetated
RGT_LOG_HTML=

#
# Process Dispatcher options.
#
process_opts()
{
    while test -n "$1" ; do
        opt="$1"
        case "$1" in 
            --help ) usage ; exit 0 ;;

            --daemon=* )    DAEMON="${1#--daemon=}" ; SHUTDOWN= ;;
            --daemon   )    SHUTDOWN= ;;
            --shutdown=* )  DAEMON="${1#--shutdown=}" ; SHUTDOWN=yes ;;
            --shutdown )    SHUTDOWN=yes ;;

            --opts=* )
                EXT_OPTS_PROCESSED=yes
                OPTS="${1#--opts=}"
                if test "${OPTS:0:1}" != "/" ; then 
                    OPTS="${CONF_DIR}/${OPTS}"
                fi
                if test -f ${OPTS} ; then
                    process_opts $(cat ${OPTS})
                    # Don't want to see the option after expansion
                    opt=
                else
                    echo "File with options ${OPTS} not found" >&2
                    exit 1
                fi
                ;;

            --script=* )
                script_req="${1#--script=}"
                script_file=
                script_opts=
                for i in ${script_req//:/ } ; do
                    if test -z "${script_file}" ; then
                        script_file="$i"
                    else
                        script_opts="$script_opts $i"
                    fi
                done
                if test "${script_file:0:1}" != "/" ; then 
                    script_file="${CONF_DIR}/${script_file}"
                fi
                if test -f "${script_file}" ; then
                    TE_EXTRA_OPTS=
                    . "${script_file}"
                    if test -n "${TE_EXTRA_OPTS}" ; then
                        process_opts ${TE_EXTRA_OPTS}
                        TE_EXTRA_OPTS=
                    fi
                else
                    echo "File with shell script ${script_file} not found" >&2
                    exit 1
                fi
                ;;

            -q) QUIET=yes ;;
            -n) BUILDER= ; TESTER_OPTS="${TESTER_OPTS} --nobuild" ;;
            --live-log)  LIVE_LOG=yes ; TESTER_OPTS="-q ${TESTER_OPTS}" ;;

            --log-txt=*)        RGT_LOG_TXT="${1#--log-txt=}" ;;
            --log-html=*)       RGT_LOG_HTML="${1#--log-html=}" ;;
            --log-plain-html=*) RGT_LOG_HTML_PLAIN="${1#--log-plain-html=}" ;;
            --log-txt-detailed-packets) RGT_LOG_TXT_DETAILED_PACKETS=true ;;

            --gdb-tester)   GDB_TESTER=yes ;;

            --vg-rcf)    VG_RCF=yes ;;
            --vg-cs)     VG_CS=yes ;;
            --vg-logger) VG_LOGGER=yes ;;
            --vg-tester) VG_TESTER=yes ;;
            --vg-engine) VG_RCF=yes
                         VG_CS=yes
                         VG_LOGGER=yes
                         VG_TESTER=yes ;;
        
            --no-builder) BUILDER= ;;
            --no-nuts-build) BUILD_NUTS= ;;
            --no-tester) TESTER= ;;
            --no-cs) CS= ;;
            --no-rcf) RCF= ;;
            --no-run) RCF= ; CS= ; TESTER= ; LOGGER= ;;
            --no-autotool) TE_NO_AUTOTOOL=yes ;;
            
            --conf-dir=*) CONF_DIR="${1#--conf-dir=}" ;;
            
            --conf-builder=*) CONF_BUILDER_SET=1; CONF_BUILDER="${1#--conf-builder=}" ;;
            --conf-logger=*) CONF_LOGGER_SET=1; CONF_LOGGER="${1#--conf-logger=}" ;;
            --conf-tester=*) CONF_TESTER_SET=1; CONF_TESTER="${1#--conf-tester=}" ;;
            --conf-cs=*) CONF_CS_SET=1; CONF_CS="${1#--conf-cs=}" ;;
            --conf-rcf=*) CONF_RCF_SET=1; CONF_RCF=${1#--conf-rcf=} ;;
            --conf-rgt=*) CONF_RGT_SET=1; CONF_RGT=${1#--conf-rgt=} ;;
            --conf-nut=*) CONF_NUT_SET=1; CONF_NUT="${1#--conf-nut=}" ;;
            --force) TE_NO_PROMPT=yes ;;

            --tce) DO_TCE=yes ;;
            
            --log-dir=*) TE_LOG_DIR="${1#--log-dir=}" ;;
            --log-online) LOG_ONLINE=yes ;;

            --sniff-log-conv-disable) TE_SNIFF_LOG_CONV_DISABLE=true ;;
            --sniff-log-dir=*) TE_SNIFF_LOG_DIR="${1#--sniff-log-dir=}"
                 ;;
            --sniff-log-osize=*) TE_SNIFF_LOG_OSIZE="${1#--sniff-log-osize=}"
                export TE_SNIFF_LOG_OSIZE ;;
            --sniff-log-space=*) TE_SNIFF_LOG_SPACE="${1#--sniff-log-space=}"
                export TE_SNIFF_LOG_SPACE ;;
            --sniff-log-name=*) TE_SNIFF_LOG_NAME="${1#--sniff-log-name=}"
                export TE_SNIFF_LOG_NAME ;;
            --sniff-log-fsize=*) TE_SNIFF_LOG_FSIZE="${1#--sniff-log-fsize=}"
                export TE_SNIFF_LOG_FSIZE ;;
            --sniff-log-ofill-drop*) TE_SNIFF_LOG_OFILL=1
                export TE_SNIFF_LOG_OFILL ;;
            --sniff-log-period=*) TE_SNIFF_LOG_PERIOD="${1#--sniff-period=}"
                export TE_SNIFF_LOG_PERIOD ;;

            --sniff-not-feed-conf*)
                TE_SNIFF_NOT_FEED_CONF=1;;
            --sniff=*) TE_SNIFF_IDX=${#TE_SNIFF_LOC[*]}
                TE_SNIFF_LOC[${TE_SNIFF_IDX}]="${1#--sniff=}";;
            --sniff-name=*) 
                TE_SNIFF_NAME[${TE_SNIFF_IDX}]="${1#--sniff-name=}";;
            --sniff-filter=*)
                TE_SNIFF_FILTER[${TE_SNIFF_IDX}]="${1#--sniff-filter=}" ;;
            --sniff-snaplen=*)
                TE_SNIFF_SNAPLEN[${TE_SNIFF_IDX}]="${1#--sniff-snaplen=}" ;;
            --sniff-space=*)
                TE_SNIFF_SPACE[${TE_SNIFF_IDX}]="${1#--sniff-space=}" ;;
            --sniff-fsize=*)
                TE_SNIFF_FSIZE[${TE_SNIFF_IDX}]="${1#--sniff-fsize=}" ;;
            --sniff-rotation=*)
                TE_SNIFF_ROTATION[${TE_SNIFF_IDX}]="${1#--sniff-rotation=}" ;;
            --sniff-ofill-drop*)
                TE_SNIFF_OFILL[${TE_SNIFF_IDX}]="1" ;;

            --no-ts-build) BUILD_TS= ; TESTER_OPTS="${TESTER_OPTS} --nobuild" ;;

            --tester-*) TESTER_OPTS="${TESTER_OPTS} --${1#--tester-}" ;;
            --test-sigusr2-verdict*) TE_TEST_SIGUSR2_VERDICT=1
                export TE_TEST_SIGUSR2_VERDICT ;;

            --trc-log=*) TRC_LOG="${1#--trc-log=}" ;;
            --trc-db=*) 
                TRC_DB="${1#--trc-db=}"
                if test "${TRC_DB:0:1}" != "/" ; then 
                    TRC_DB="${CONF_DIR}/${TRC_DB}"
                fi
                TRC_OPTS="${TRC_OPTS} --db=${TRC_DB}"
                TESTER_OPTS="${TESTER_OPTS} --trc-db=${TRC_DB}"
                ;;
            --trc-comparison=*) 
                TRC_OPTS="${TRC_OPTS} --comparison=${1#--trc-comparison=}"
                TESTER_OPTS="${TESTER_OPTS} $1"
                ;;
            --trc-key2html=*) 
                TRC_KEY2HTML="${1#--trc-key2html=}"
                if test "${TRC_KEY2HTML:0:1}" != "/" ; then 
                    TRC_KEY2HTML="${CONF_DIR}/${TRC_KEY2HTML}"
                fi
                TRC_OPTS="${TRC_OPTS} --key2html=${TRC_KEY2HTML}"
                ;;
            --trc-tag=*)
                TRC_TAGS="${TRC_TAGS} ${1#--trc-tag=}"
                TESTER_OPTS="${TESTER_OPTS} $1"
                ;;
            --trc-*) TRC_OPTS="${TRC_OPTS} --${1#--trc-}" ;;

            --rgt-xml2html-multi-*)
                RGT_X2HM_OPTS="${RGT_X2HM_OPTS} --${1#--rgt-xml2html-multi-}" ;;
    
            --cs-*) CS_OPTS="${CS_OPTS} --${1#--cs-}" ;;

            --build-only) RCF= ; CS=
                          TESTER_OPTS="${TESTER_OPTS} --no-run --no-cs" ;; 

            --build-parallel*)
                num=${1##--build-parallel=}
                [ "${num}" = "$1" ] && num=8
                BUILD_MAKEFLAGS="-j${num}"
                ;;

            --build-log=*) 
                BUILDER_OPTS="${BUILDER_OPTS} --pathlog=${1#--build-log=}"
                BUILDER=
                ;;

            --build-nolog=*) 
                BUILDER_OPTS="${BUILDER_OPTS} --pathnolog=${1#--build-nolog=}"
                BUILDER=
                ;;
                
            --build-*) 
                BUILDER_OPTS="${BUILDER_OPTS} --${1#--build-}" 
                BUILDER=
                ;;

            --build=*) 
                BUILDER_OPTS="${BUILDER_OPTS} --path=${1#--build=}"
                BUILDER=
                ;;

            *)  echo "Unknown option: $1" >&2;
                usage
                exit 1 ;;
        esac
        test -n "$opt" && cmd_line_opts_all="${cmd_line_opts_all} $opt"
        shift 1
    done
    export BUILD_MAKEFLAGS
}

#
# Check and fix arguments to avoid duplications of sniffers
#
sniffer_check_dup()
{
    for ((idx=1; idx <= TE_SNIFF_IDX ; idx++))
    do
        [ ${TE_SNIFF_LOC[${idx}]} ] ||
            continue
        : ${TE_SNIFF_NAME[${idx}]:=default_$(( $RANDOM % 1000 ))}
        for ((jdx=idx+1; jdx <= TE_SNIFF_IDX ; jdx++))
        do
            [ ${TE_SNIFF_LOC[${jdx}]} ] ||
                continue
            : ${TE_SNIFF_NAME[${jdx}]:=default_$(( $RANDOM % 1000 ))}
            [ ${TE_SNIFF_LOC[${idx}]} = ${TE_SNIFF_LOC[${jdx}]} ] &&   \
            [ ${TE_SNIFF_NAME[${idx}]} = ${TE_SNIFF_NAME[${jdx}]} ] && \
            {
                str="${TE_SNIFF_NAME[${jdx}]}_$(( $RANDOM % 1000 ))"
                TE_SNIFF_NAME[${jdx}]="${str}"
            }
        done
    done
}

#
# Make auxiliary configuration file for sniffers.
#
sniffer_make_conf()
{
    iface=
    agt=
    str=

    TE_SNIFF_CSCONF="${CONF_DIR}/cs.conf.sniffer"

    echo "<?xml version=\"1.0\"?>" > "${TE_SNIFF_CSCONF}"
    if test ! -w "${TE_SNIFF_CSCONF}" ; then
        echo "Couldn't create the sniffer conf file: ${TE_SNIFF_CSCONF}" >&2
        return 1
    fi 

    echo "<history>" >> "${TE_SNIFF_CSCONF}"

    sniffer_check_dup

    for ((idx=1; idx <= TE_SNIFF_IDX ; idx++))
    do
        if test -z "${TE_SNIFF_LOC[${idx}]}" ; then
          :
        elif [[ ${TE_SNIFF_LOC[${idx}]} == */* ]] ; then
            iface="${TE_SNIFF_LOC[${idx}]#*/}"
            agt="${TE_SNIFF_LOC[${idx}]%/*}"

            if [ ! ${iface} ] || [ ! ${agt} ] ; then
                continue
            fi

            echo "  <add>" >> "${TE_SNIFF_CSCONF}"
            str="    <instance oid=\"/agent:${agt}/interface:${iface}"
            str+="/sniffer:${TE_SNIFF_NAME[${idx}]}\"/>"
            echo "${str}" >> "${TE_SNIFF_CSCONF}"
            echo "  </add>" >> "${TE_SNIFF_CSCONF}"

            echo "  <set>" >> "${TE_SNIFF_CSCONF}"
            test ! "${TE_SNIFF_FILTER[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/filter_exp_str:\""
                str+=" value=\"${TE_SNIFF_FILTER[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            test ! "${TE_SNIFF_SNAPLEN[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/snaplen:\""
                str+=" value=\"${TE_SNIFF_SNAPLEN[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            test ! "${TE_SNIFF_SPACE[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/tmp_logs:"
                str+="/sniffer_space:\""
                str+=" value=\"${TE_SNIFF_SPACE[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            test ! "${TE_SNIFF_FSIZE[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/tmp_logs:"
                str+="/file_size:\""
                str+=" value=\"${TE_SNIFF_FSIZE[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            test ! "${TE_SNIFF_ROTATION[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/tmp_logs:"
                str+="/rotation:\""
                str+=" value=\"${TE_SNIFF_ROTATION[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            test ! "${TE_SNIFF_OFILL[${idx}]}" ||
            {
                str="    <instance oid=\"/agent:${agt}/interface:${iface}"
                str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/tmp_logs:"
                str+="/overfill_meth:\""
                str+=" value=\"${TE_SNIFF_OFILL[${idx}]}\"/>"
                echo "${str}" >> "${TE_SNIFF_CSCONF}"
            }
            str="    <instance oid=\"/agent:${agt}/interface:${iface}"
            str+="/sniffer:${TE_SNIFF_NAME[${idx}]}/enable:\" value=\"1\"/>"
            echo "${str}" >> "${TE_SNIFF_CSCONF}"
            echo "  </set>" >> "${TE_SNIFF_CSCONF}"
        fi
    done

    echo "</history>" >> "${TE_SNIFF_CSCONF}"

    if test -n "${TE_SNIFF_IDX}" && test -z ${TE_SNIFF_NOT_FEED_CONF}; then
        CS_OPTS="${CS_OPTS} --sniff-conf=${TE_SNIFF_CSCONF}"
    elif test -n "${TE_SNIFF_IDX}" ; then
        export TE_SNIFF_CSCONF
    fi
    return 0
}

# Export TE_BASE
if test -z "${TE_BASE}" ; then
    if test -e "${DISPATCHER_DIR}/configure.ac" ; then
        echo "TE source directory is ${DISPATCHER_DIR} - exporting TE_BASE."
        export TE_BASE="${DISPATCHER_DIR}"
    fi
fi

if test -z "$CONF_DIR" ; then
    if test -n "${TE_BASE}" ; then
        CONF_DIR="${TE_BASE}/conf"
    else
        CONF_DIR="${PWD}"
    fi
fi


# Process command-line options
cmd_line_opts="$@"
cmd_line_opts_all=
process_opts "$@"

sniffer_make_conf
retval=$?
if [ $retval -eq 1 ] ; then
    exit 1
fi


if test -z "$TE_BASE" -a -n "$BUILDER" ; then
    echo "Cannot find TE sources for building - exiting." >&2
    exit 1
fi

export TE_NO_AUTOTOOL
export TE_NO_PROMPT

for i in BUILDER LOGGER TESTER CS RCF RGT NUT ; do
    CONF_FILE_SET="$(eval echo '${CONF_'$i'_SET}')"
    CONF_FILE="$(eval echo '$CONF_'$i)"
    if test -n "${CONF_FILE}" -a "${CONF_FILE:0:1}" != "/" ; then
        eval CONF_$i=\"${CONF_DIR}/${CONF_FILE}\"
        CONF_FILE="$(eval echo '$CONF_'$i)"
        if test ! -f ${CONF_FILE} ; then
          # Conf file does not exist at specified path.
          # Check if this is the default value or user-specified
          if test -n "${CONF_FILE_SET}" ; then
              # User-specified, so rise an exception
              echo "Cannot find $i configuration file at ${CONF_FILE} path"
              exit 1;
          fi

          # Set Conf file path to empty string.
          eval CONF_$i=
        fi
    fi
done

# Create directory for temporary files
if test -z "$TE_TMP" ; then
    export TE_TMP="${TE_RUN_DIR}/te_tmp"
    mkdir -p "${TE_TMP}" || exit 1
fi
# May be it should be passed to TE Logger only...
export TE_LOGGER_PID_FILE="${TE_TMP}/te_logger.pid"

if test -z "$TE_BUILD" ; then
    if test -n "$BUILDER" ; then
        # Verifying build directory
        if test -e dispatcher.sh -a -e configure.ac ; then
            mkdir -p build
            TE_BUILD="${PWD}"/build
        else
            TE_BUILD="${PWD}"
        fi
    else
        if test -e build/builder.conf.processed ; then
            TE_BUILD="${PWD}"/build
        fi
        if test -z "${TE_BUILD}" ; then
            TE_BUILD="${PWD}"
            if test -z "${QUIET}" ; then
                echo "Guessing TE_BUILD=${TE_BUILD}"
            fi
        fi
    fi
elif test ! -e "${TE_BUILD}" ; then
    mkdir -p "${TE_BUILD}" || exit 1
elif test ! -d "${TE_BUILD}" ; then
    echo "${TE_BUILD} is not a directory" >&2
    exit 1
fi
export TE_BUILD
cd "${TE_BUILD}"

export TE_LOG_DIR="${TE_LOG_DIR}"
mkdir -p "${TE_LOG_DIR}"
export TE_LOG_RAW="${TE_LOG_RAW:-${TE_LOG_DIR}/tmp_raw_log}"

# Export TE_INSTALL
if test -z "$TE_INSTALL" ; then
    if test -e "$DISPATCHER_DIR/configure.ac" ; then
        if test -n "${TE_BUILD}" ; then
            TE_INSTALL="${TE_BUILD}/inst"
        else
            TE_INSTALL="$DISPATCHER_DIR/build/inst"
        fi
    else
        TE_PATH="$(dirname "${DISPATCHER_DIR}")"
        TE_INSTALL="$(dirname "${TE_PATH}")"
    fi
    if test -z "${QUIET}" ; then
        echo "Exporting TE installation directory as TE_INSTALL:"
        echo '    '"$TE_INSTALL"
    fi
    export TE_INSTALL
fi

if test -z "${TE_PATH}" ; then
    TE_PATH="${TE_INSTALL}/default"
fi

# Export PATH
if test -z "${QUIET}" ; then
    echo "Exporting path to host executables:"
    echo "    ${TE_PATH}/bin"
fi
export TE_PATH
export PATH="${TE_PATH}/bin:$PATH"
# Do not export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${TE_PATH}/lib" --
# it can add empty entry to LD_LIBRARY_PATH (if it was unset), so you'll
# get current dir as part of LD_LIBRARY_PATH, with misterious failures.
# We can use
# LD_LIBRARY_PATH=${LD_LIBRARY_PATH+$LD_LIBRARY_PATH:}${TE_PATH}/lib
# but it fails if LD_LIBRARY_PATH is set, but empty.
if [ -z "${LD_LIBRARY_PATH}" ]; then
    export LD_LIBRARY_PATH="${TE_PATH}/lib"
else
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${TE_PATH}/lib"
fi


if test -z "$TE_INSTALL_NUT" ; then
    export TE_INSTALL_NUT="${TE_INSTALL}/nuts"
fi

if test -z "$TE_INSTALL_SUITE" ; then
    export TE_INSTALL_SUITE="${TE_INSTALL}/suites"
fi

if test -z "$(which te_log_init 2>/dev/null)" ; then
    if test -z "$BUILDER" ; then
        echo "TE core executables are not installed - exiting." >&2
        exit_with_log
    fi
    #Export path to logging and building scripts, which are not installed yet
    export PATH="$PATH:${TE_BASE}/engine/logger:${TE_BASE}/engine/builder"
fi

. ${TE_BASE}/common_vars.sh

# Intitialize log
test -z "${DAEMON}" && te_log_init
te_log_message Dispatcher "Command-line options" "${cmd_line_opts}"
te_log_message Dispatcher "Expanded command-line options" \
    "${cmd_line_opts_all}"

# Log TRC tags
if test -n "${TRC_TAGS}" ; then
    te_log_message Dispatcher "TRC tags" "${TRC_TAGS}"
fi

# Build Test Environment

TE_BUILD_LOG="${TE_RUN_DIR}/build.log"
if test -n "$BUILDER" ; then
    pushd "${TE_BASE}" >/dev/null
    if test ! -e configure -o \
       0`stat --format=%Y configure 2>/dev/null` -lt \
       0`stat --format=%Y configure.ac 2>/dev/null` ; then
        if test -n "${QUIET}" ; then
            echo "Calling aclocal/autoconf/automake in ${PWD}" \
                >>"${TE_BUILD_LOG}"
        else
            echo "Calling aclocal/autoconf/automake in ${PWD}"
        fi
        aclocal -I "${TE_BASE}/auxdir" || exit_with_log
        autoconf || exit_with_log
        automake || exit_with_log
    fi
    popd >/dev/null
    # FINAL ${TE_BASE}/configure --prefix=${TE_INSTALL} --with-config=${CONF_BUILDER} 2>&1 | te_builder_log
    if test -n "${QUIET}" ; then
        "${TE_BASE}/configure" -q --prefix="${TE_INSTALL}" \
            --with-config="${CONF_BUILDER}" >"${TE_BUILD_LOG}" || \
            exit_with_log
        make install >>"${TE_BUILD_LOG}" || exit_with_log
    else
        "${TE_BASE}/configure" -q --prefix="${TE_INSTALL}" \
            --with-config="${CONF_BUILDER}" || exit_with_log
        make install || exit_with_log
    fi
fi

if test -n "${QUIET}" ; then
    te_builder_opts --quiet="${TE_BUILD_LOG}" $BUILDER_OPTS || exit_with_log
else
    te_builder_opts $BUILDER_OPTS || exit_with_log
fi

if test -z "${TE_SNIFF_LOG_DIR}" ; then
    TE_SNIFF_LOG_DIR="${TE_SNIFF_DEF_LOG_DIR}"
    # Get capture logs path from the Logger configuration file
    if test -f "${CONF_LOGGER}" ; then
        tmp=`te_log_get_path ${CONF_LOGGER}`
        if test -n "${tmp}" ; then
            TE_SNIFF_LOG_DIR="${tmp}"
        fi
    fi
fi
# Export directory path for sniffer capture
[[ ${TE_SNIFF_LOG_DIR} == /* ]] || [[ ${TE_SNIFF_LOG_DIR} == \~/* ]] ||
   TE_SNIFF_LOG_DIR="${TE_RUN_DIR}/${TE_SNIFF_LOG_DIR}"
export TE_SNIFF_LOG_DIR

# Goto the directory where the script was called ${TE_RUN_DIR}.
# It has to be the current dir for all TE Engine applications.
cd "${TE_RUN_DIR}"

if test -n "${SUITE_SOURCES}" -a -n "${BUILD_TS}" ; then
    te_build_suite `basename ${SUITE_SOURCES}` $SUITE_SOURCES || exit_with_log
fi

if test -n "${CONF_NUT_SET}" ; then
    if test ! -e "${CONF_NUT}" ; then
        echo "Specified NUTs configuration file does not exists:" >&2
        echo '    '"${CONF_NUT}" >&2
        exit_with_log
    fi
elif test -e "${CONF_NUT}" ; then
    # If NUT configuration file is not specified explicitly,
    # but default exists, then use it
    CONF_NUT_SET=1
fi

if test -n "${CONF_NUT_SET}" -a -n "${BUILD_NUTS}" ; then
    if test -z "${QUIET}" ; then
        te_build_nuts "${CONF_NUT}" || exit_with_log
    else
        TE_BUILD_NUTS_LOG="${TE_RUN_DIR}/build_nuts.log"
        te_build_nuts "${CONF_NUT}" >"${TE_BUILD_NUTS_LOG}" || exit_with_log
    fi
fi    


rm -f valgrind.* vg.*

# Ignore Ctrl-C when daemons are started
trap "" SIGINT

myecho() {
    if test -z "$LIVE_LOG" ; then
        echo $*
    fi
}

if test -n "${DAEMON}" ; then TE_ID=${DAEMON} ; else TE_ID=$$ ; fi
export TE_RCF="TE_RCF_${TE_ID}"
export TE_LOGGER="TE_LOGGER_${TE_ID}"
export TE_CS="TE_CS_${TE_ID}"

# Run RGT in live mode in background
if test -n "${LIVE_LOG}" ; then
    rgt-conv -m live -f "${TE_LOG_RAW}" &
    LIVE_LOG_PID=$!
fi

te_log_message Dispatcher Start "Starting TEN applications"
START_OK=0

LOGGER_NAME=Logger
LOGGER_EXEC=te_logger
LOGGER_SHUT=te_log_shutdown
RCF_NAME=RCF
RCF_EXEC=te_rcf
RCF_SHUT=te_rcf_shutdown
CS_NAME=Configurator
CS_EXEC=te_cs
CS_SHUT=te_cs_shutdown

start_daemon() {
    DAEMON=$1
    if test ${START_OK} -eq 0 -a -n "$(eval echo '$'$DAEMON)" ; then
        DAEMON_NAME="$(eval echo '${'$DAEMON'_NAME}')"
        DAEMON_EXEC="$(eval echo '${'$DAEMON'_EXEC}')"
        DAEMON_OPTS="$(eval echo '${'$DAEMON'_OPTS}')"
        DAEMON_CONF="$(eval echo '${CONF_'$DAEMON'}')"
        te_log_message Dispatcher Start \
            "Start ${DAEMON_NAME}: ${DAEMON_OPTS} ${DAEMON_CONF}"
        myecho -n "--->>> Starting ${DAEMON_NAME}... "
        if test -n "$(eval echo '${VG_'$DAEMON'}')" ; then
            # Run in foreground under valgrind
            valgrind ${VG_OPTIONS} ${DAEMON_EXEC} ${DAEMON_OPTS} \
                "${DAEMON_CONF}" 2>valgrind.${DAEMON_EXEC}
        else
            ${DAEMON_EXEC} ${DAEMON_OPTS} "${DAEMON_CONF}"
        fi
        START_OK=$?
        if test ${START_OK} -eq 0 ; then
            myecho "done"
            eval $(echo $DAEMON'_OK')=yes
        elif test ${START_OK} -eq 1 ; then
            myecho "failed"
        else
            myecho "unexpected"
            START_OK=1
        fi
    fi
}

if test -z "${DAEMON}" ; then
    start_daemon LOGGER

    start_daemon RCF

    if test -n "${RCF_OK}" ; then
        # Wakeup Logger when RCF is ready
        TE_LOGGER_PID="$(cat "${TE_LOGGER_PID_FILE}" 2>/dev/null)"
        if test -n "${TE_LOGGER_PID}" ; then
            kill -USR1 ${TE_LOGGER_PID}
        else
            echo "Failed to wake-up TE Logger" >&2
            START_OK=1
        fi
    fi

    start_daemon CS
else
    # It is assumed here that all TE engine daemons are running
    LOGGER_OK=yes
    RCF_OK=yes
    CS_OK=yes
fi

if test ${START_OK} -eq 0 -a -n "${TESTER}" ; then
    te_log_message Dispatcher Start \
        "Start Tester:${TESTER_OPTS} ${CONF_TESTER}"
    myecho "--->>> Start Tester"
    if test -n "$GDB_TESTER" ; then
        echo set args ${TESTER_OPTS} "${CONF_TESTER}" >gdb.te_tester.init
        gdb -x gdb.te_tester.init te_tester
        rm gdb.te_tester.init
    elif test -n "$VG_TESTER" ; then
        valgrind ${VG_OPTIONS} te_tester ${TESTER_OPTS} "${CONF_TESTER}" \
            2>valgrind.te_tester
    else
        te_tester ${TESTER_OPTS} "${CONF_TESTER}"
    fi
    START_OK=$?
fi

test "${START_OK}" -eq 1 && SHUTDOWN=yes


shutdown_daemon() {
    DAEMON=$1
    if test -n "${SHUTDOWN}" -a -n "$(eval echo '${'$DAEMON'_OK}')" ; then
        DAEMON_NAME="$(eval echo '${'$DAEMON'_NAME}')"
        DAEMON_SHUT="$(eval echo '${'$DAEMON'_SHUT}')"
        te_log_message Dispatcher Start "Shutdown ${DAEMON_NAME}"
        myecho -n "--->>> Shutdown ${DAEMON_NAME}... "
        ${DAEMON_SHUT}
        if test $? -eq 0 ; then
            myecho "done"
        else
            myecho "failed"
        fi
    fi
}

shutdown_daemon CS

if test -n "${LOGGER_OK}" -a -n "${RCF_OK}" ; then
    te_log_message Dispatcher Start "Flush log"
    myecho "--->>> Flush Logs"
    te_log_flush
fi

shutdown_daemon RCF 

shutdown_daemon LOGGER

if test -z "${SHUTDOWN}" ; then
    te_log_message Dispatcher Start "Leave TE daemon ${TE_ID}"
    myecho "--->>> Leave TE deamon ${TE_ID}"
fi

# Wait for RGT in live mode finish
if test -n "${LIVE_LOG}" ; then
    wait ${LIVE_LOG_PID}
fi

#
# Processing of sniffers capture logs
#
myecho -n "--->>> Logs conversion..."

merge_comm=""
if test ! -d ${TE_SNIFF_LOG_DIR} ; then
    TE_SNIFF_LOG_CONV_DISABLE=true;
fi
if test -z ${TE_SNIFF_LOG_CONV_DISABLE} ; then
    idx=0
    for plog in `ls ${TE_SNIFF_LOG_DIR}/ | grep \.pcap$`; do
        plog="${TE_SNIFF_LOG_DIR}/${plog}"
        xlogs[${idx}]=${plog/%.pcap/.xml}
        # Conversion from pcap to TE XML
        if type tshark >/dev/null 2> /dev/null ; then
            tshark -r ${plog} -T pdml 2> /dev/null | rgt-pdml2xml - ${xlogs[idx]}
        else
            myecho ""
            myecho "--->>> NOTE: tshark is missing, so sniffer logs won't be merged with the report"
            if test -z "$LIVE_LOG" ; then
                echo   "--->>>       find them in ${TE_SNIFF_LOG_DIR} directory"
            fi
            myecho -n "--->>> Continue logs conversion without sniffer logs..."
            break
        fi

        # Construct command to merge all capture logs 
        merge_comm="${merge_comm} ${xlogs[${idx}]}"
        let "idx += 1"
    done
    if test ${idx} -eq 0 ; then
        TE_SNIFF_LOG_CONV_DISABLE=true
    fi
fi

#
# RGT processing of the raw log
#
if test -n "${CONF_RGT}" ; then
    CONF_RGT="-c ${CONF_RGT}"
fi
if test -n "${RGT_LOG_TXT}" -o -n "${RGT_LOG_HTML_PLAIN}" ; then
    # Generate XML log do not taking into account control messages
    LOG_XML_PLAIN="log_plain.xml"
    LOG_XML_MERGED="log_plain_ext.xml"
    rgt-conv --no-cntrl-msg  -m postponed ${CONF_RGT} \
        -f "${TE_LOG_RAW}" -o "${LOG_XML_PLAIN}"
    if test $? -eq 0 -a -e "${LOG_XML_PLAIN}" ; then
        if test -z ${TE_SNIFF_LOG_CONV_DISABLE} ; then
            # Merge main TE log with capture logs
            rgt-xml-merge "${LOG_XML_MERGED}" "${LOG_XML_PLAIN}" \
                          ${merge_comm}
        else
            LOG_XML_MERGED="${LOG_XML_PLAIN}"
        fi

        if test -n "${RGT_LOG_TXT}" ; then
            if test -z ${RGT_LOG_TXT_DETAILED_PACKETS} ; then
                rgt-xml2text -f "${LOG_XML_MERGED}" -o "${RGT_LOG_TXT}"
            else
                rgt-xml2text -f "${LOG_XML_MERGED}" -o "${RGT_LOG_TXT}" \
                             --detailed-packets
            fi
        fi
        if test -n "${RGT_LOG_HTML_PLAIN}" ; then
            rgt-xml2html -f "${LOG_XML_MERGED}" -o "${RGT_LOG_HTML_PLAIN}"
        fi
    fi
fi
if test -n "${RGT_LOG_HTML}" ; then
    # Generate XML log taking into account control messages
    LOG_XML_STRUCT="log_struct.xml"
    LOG_XML_MERGED="log_struct_ext.xml"
    rgt-conv -m postponed ${CONF_RGT} \
        -f "${TE_LOG_RAW}" -o "${LOG_XML_STRUCT}"
    if test $? -eq 0 -a -e "${LOG_XML_STRUCT}" ; then
        if test -z ${TE_SNIFF_LOG_CONV_DISABLE} ; then
            # Merge main TE log with capture logs
            rgt-xml-merge "${LOG_XML_MERGED}" "${LOG_XML_STRUCT}" \
                          ${merge_comm}
        else
            LOG_XML_MERGED="${LOG_XML_STRUCT}"
        fi

        rgt-xml2html-multi ${RGT_X2HM_OPTS} "${LOG_XML_MERGED}" \
                           "${RGT_LOG_HTML}"
    fi
fi

if test -z ${TE_SNIFF_LOG_CONV_DISABLE} ; then
    for rmlog in ${xlogs[@]} ; do
        rm "${rmlog}"
    done
fi
myecho "done"

if test ${START_OK} -ne 1 -a -n "${DO_TCE}" ; then
    myecho "--->>> TCE processing"
    te_tce_process "${CONF_NUT}"
fi

# Run TRC, if any its option is provided
if test ${START_OK} -ne 1 ; then
    if test -n "${TRC_LOG}" ; then
        te-trc-log -j "${TE_LOG_RAW}" "${TRC_LOG}"
    fi
    if test -n "${TRC_OPTS}" ; then
        te_trc.sh ${TRC_OPTS} "${TE_LOG_RAW}"
    fi
fi

test -n "${SHUTDOWN}" && rm -rf "${TE_TMP}"

exit ${START_OK}
