#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# Tests for engine/builder/te_fetch_ext_repos.
#
# The test creates bare repositories in a temporary directory and
# runs against them, so it needs no network and touches nothing
# outside that directory.
#
# Usage: ./te_fetch_ext_repos.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SCRIPT_DIR
FETCH="${SCRIPT_DIR}/../te_fetch_ext_repos"
readonly FETCH

# shellcheck source=test_lib.sh
. "${SCRIPT_DIR}/test_lib.sh"

#######################################
# Create a bare repository with one commit per extra argument.
#
# Every commit is tagged v<n> and the content of the tracked file
# is the argument the commit was made for.
# Globals:
#   GIT
# Arguments:
#   Bare repository, a path.
#   Commit messages, one per commit to make.
#######################################
function mk_repo() {
    local bare="$1"; shift
    local work="${bare}.work"
    local n=0
    local msg

    ${GIT} init -q --bare "${bare}"
    ${GIT} init -q "${work}"
    for msg in "$@" ; do
        n=$((n + 1))
        echo "${msg}" >"${work}/marker.txt"
        ${GIT} -C "${work}" add marker.txt
        ${GIT} -C "${work}" commit -q -m "${msg}"
        ${GIT} -C "${work}" tag "v${n}"
    done
    ${GIT} -C "${work}" push -q "${bare}" HEAD:refs/heads/main --tags
}

#######################################
# Write a processed builder conf declaring one repository 'ext'.
# Arguments:
#   Repository URL.
#   Repository reference.
#   Configuration file to write, a path.
#######################################
function mk_conf() {
    local conf="$3"

    cat >"${conf}" <<EOF
TE_BS_EXT_REPOS=" ext"
TE_BS_EXT_REPO_ext_URL='$1'
TE_BS_EXT_REPO_ext_REF='$2'
EOF
}

#######################################
# Run the fetcher against the work directory.
# Globals:
#   FETCH
#   WORK
# Arguments:
#   Configuration file to pass to the fetcher.
# Outputs:
#   Writes what the fetcher reported to ${WORK}/out.
# Returns:
#   The exit status of the fetcher.
#######################################
function run_fetch() {
    ( cd "${WORK}" && TE_BUILD="${WORK}/build" "${FETCH}" "$@" ) \
        >"${WORK}/out" 2>&1
}

#######################################
# Report the content of the tracked file in the checkout.
# Globals:
#   WORK
# Outputs:
#   Writes the content to stdout, nothing if there is no checkout.
#######################################
function marker() {
    cat "${WORK}/build/ext-repos/ext/marker.txt" 2>/dev/null
}

#######################################
# Check that the checkout holds the expected commit.
# Arguments:
#   Content the tracked file of that commit has.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_marker() {
    local want="$1"; shift
    local got

    got="$(marker)"
    if [[ "${got}" == "${want}" ]] ; then
        ok "checkout contains '${want}'"
    else
        fail "checkout contains '${got}', expected '${want}'"
    fi
}

#######################################
# Check that the output of the fetcher mentions a text.
# Globals:
#   WORK
# Arguments:
#   Text the output must hold.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_out() {
    if grep -q -- "$1" "${WORK}/out" ; then
        ok "output mentions '$1'"
    else
        fail "output does not mention '$1'; got:$(sed 's/^/    /' \
             "${WORK}/out")"
    fi
}


#######################################
# Run every scenario and report the outcome.
# Globals:
#   FETCH
#   WORK
# Outputs:
#   Reports each step and each check, see step(), ok() and fail().
# Returns:
#   This function never returns, see finish().
#######################################
function main() {
    [[ -x "${FETCH}" ]] || {
        echo "ERROR: ${FETCH} is not executable" >&2
        exit 1
    }
    step "fresh clone checks out the requested tag"
    mk_repo "${WORK}/a.git" first second
    mk_conf "${WORK}/a.git" v1 "${WORK}/conf"
    run_fetch "${WORK}/conf" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker first


    step "a tag resolves without touching the origin"
    # Hiding the origin makes any network (here: filesystem) access fail
    mv "${WORK}/a.git" "${WORK}/a.git.hidden"
    run_fetch "${WORK}/conf" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker first
    mv "${WORK}/a.git.hidden" "${WORK}/a.git"


    step "changed URL under the same name is re-fetched, not ignored"
    mk_repo "${WORK}/b.git" other
    mk_conf "${WORK}/b.git" v1 "${WORK}/conf-b"
    run_fetch "${WORK}/conf-b" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker other
    expect_out "origin changed"


    step "tracked modification is kept while the commit does not change"
    mk_repo "${WORK}/c.git" third
    mk_conf "${WORK}/c.git" v1 "${WORK}/conf-c"
    rm -rf "${WORK}/build/ext-repos/ext"
    run_fetch "${WORK}/conf-c" || fail "fetcher failed: $(cat "${WORK}/out")"
    echo "edited by hand" >"${WORK}/build/ext-repos/ext/marker.txt"
    run_fetch "${WORK}/conf-c" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker "edited by hand"
    expect_out "local modifications"


    step "untracked file survives a re-run at the same commit"
    echo "scratch" >"${WORK}/build/ext-repos/ext/untracked.txt"
    run_fetch "${WORK}/conf-c" || fail "fetcher failed: $(cat "${WORK}/out")"
    if [[ -f "${WORK}/build/ext-repos/ext/untracked.txt" ]] ; then
        ok "untracked file kept"
    else
        fail "untracked file was removed"
    fi


    step "moving to another commit over a tracked modification is refused"
    mk_repo "${WORK}/d.git" alpha beta
    mk_conf "${WORK}/d.git" v1 "${WORK}/conf-d1"
    rm -rf "${WORK}/build/ext-repos/ext"
    run_fetch "${WORK}/conf-d1" || fail "fetcher failed: $(cat "${WORK}/out")"
    echo "edited by hand" >"${WORK}/build/ext-repos/ext/marker.txt"
    mk_conf "${WORK}/d.git" v2 "${WORK}/conf-d2"
    if run_fetch "${WORK}/conf-d2" ; then
        fail "checkout over a tracked modification succeeded"
    else
        ok "checkout over a tracked modification refused"
        expect_marker "edited by hand"
    fi


    step "discarding the modification with git lets the move through"
    ${GIT} -C "${WORK}/build/ext-repos/ext" reset -q --hard
    run_fetch "${WORK}/conf-d2" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker beta


    step "moving to another commit over an untracked file is refused too"
    mk_conf "${WORK}/d.git" v1 "${WORK}/conf-d1b"
    echo "scratch" >"${WORK}/build/ext-repos/ext/new_source.c"
    if run_fetch "${WORK}/conf-d1b" ; then
        fail "checkout over an untracked file succeeded"
    else
        ok "checkout over an untracked file refused"
        expect_marker beta
    fi
    rm -f "${WORK}/build/ext-repos/ext/new_source.c"


    step "ref equal to the default branch tip still materializes the tree"
    # A --no-checkout clone has HEAD at the tip, so this is the one case
    # where HEAD need not move and the script could skip the checkout
    mk_repo "${WORK}/e.git" only
    mk_conf "${WORK}/e.git" v1 "${WORK}/conf-e"
    rm -rf "${WORK}/build/ext-repos/ext"
    run_fetch "${WORK}/conf-e" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker only



    finish
}

main "$@"
