# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
# shellcheck shell=bash
#
# Shared by the Builder script tests: a work directory that is
# removed when the test exits, a git that ignores the user's
# configuration, and the bookkeeping of checks.
#
# Source it after setting SCRIPT_DIR and end the test with finish:
#
#     . "${SCRIPT_DIR}/test_lib.sh"
#     ...
#     finish

set -u

# Work directory of the test, removed when the test exits.
WORK="$(mktemp -d)"
readonly WORK
trap 'rm -rf "${WORK}"' EXIT

# Git that ignores the configuration of the user who runs the test.
GIT="git -c user.email=te@example.com -c user.name=TE \
         -c commit.gpgsign=false -c init.defaultBranch=main"
readonly GIT

# Number of checks that failed so far.
failures=0

# Description of the step being checked, reported by a failure.
current=

#######################################
# Start a step and report it.
#
# A step groups the checks of one scenario; its description names
# the scenario in the output of a failed check.
# Globals:
#   current
# Arguments:
#   Step description.
# Outputs:
#   Writes the step description to stdout.
#######################################
function step() {
    current="$1"
    echo "=== ${current}"
}

#######################################
# Report a failed check and count it.
# Globals:
#   current
#   failures
# Arguments:
#   Arguments to the message.
# Outputs:
#   Writes the message to stderr.
#######################################
function fail() {
    echo "FAIL: ${current}: $*" >&2
    failures=$((failures + 1))
}

#######################################
# Report a successful check.
# Arguments:
#   Arguments to the message.
# Outputs:
#   Writes the message to stdout.
#######################################
function ok() {
    echo "  ok: $*"
}

#######################################
# Check that a value is the expected one.
# Arguments:
#   Description of the value being checked.
#   Value the test obtained.
#   Value the test expects.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_eq() {
    local what="$1"; shift
    local got="$1"; shift
    local want="$1"; shift

    if [[ "${got}" == "${want}" ]] ; then
        ok "${what} is '${want}'"
    else
        fail "${what} is '${got}', expected '${want}'"
    fi
}

#######################################
# Check that a whitespace-separated list holds a word.
# Arguments:
#   Description of the list being checked.
#   List the test obtained.
#   Word the list must hold.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_contains() {
    local what="$1"; shift
    local got="$1"; shift
    local want="$1"; shift

    case " ${got} " in
        *" ${want} "*) ok "${what} contains '${want}'" ;;
        *) fail "${what} is '${got}', expected it to contain '${want}'" ;;
    esac
}

#######################################
# Report the outcome of the test and exit with it. Call it last.
# Globals:
#   failures
# Outputs:
#   Writes the outcome to stdout, or to stderr if the test failed.
# Returns:
#   This function never returns; the test exits 0 if every check
#   succeeded, 1 otherwise.
#######################################
function finish() {
    echo
    if (( failures == 0 )) ; then
        echo "PASS: all checks succeeded"
        exit 0
    fi
    echo "FAILED: ${failures} check(s)" >&2
    exit 1
}
