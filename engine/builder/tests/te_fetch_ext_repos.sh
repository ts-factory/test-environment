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

# Lock file the fetcher is run with; empty means the default one
# next to the builder configuration file.
LOCK=

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

    cat >"${conf}" <<CONF
TE_BS_EXT_REPOS=" ext"
TE_BS_EXT_REPO_ext_URL='$1'
TE_BS_EXT_REPO_ext_REF='$2'
CONF
}

#######################################
# Run the fetcher against the work directory.
# Globals:
#   FETCH
#   LOCK
#   WORK
# Arguments:
#   Options and the configuration file to pass to the fetcher.
# Outputs:
#   Writes what the fetcher reported to ${WORK}/out.
# Returns:
#   The exit status of the fetcher.
#######################################
function run_fetch() {
    ( cd "${WORK}" && TE_BUILD="${WORK}/build" "${FETCH}" "$@" \
        "${LOCK:-${WORK}/builder.conf.lock}" ) >"${WORK}/out" 2>&1
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
# Check that the lock file records the commit that is checked out.
# Globals:
#   GIT
#   LOCK
#   WORK
# Arguments:
#   URL the record must hold.
#   Reference the record must hold.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_lock() {
    local want_url="$1"; shift
    local want_ref="$1"; shift
    local file="${LOCK:-${WORK}/builder.conf.lock}"
    local head
    local want
    local got

    if [[ ! -r "${file}" ]] ; then
        fail "no lock file at ${file}"
        return
    fi
    head="$(${GIT} -C "${WORK}/build/ext-repos/ext" rev-parse HEAD)"
    want="ext ${want_url} ${want_ref} ${head}"
    got="$(grep "^ext " "${file}")"

    if [[ "${got}" == "${want}" ]] ; then
        ok "lock records ${want_ref} at the checked out commit"
    else
        fail "lock has [${got}], expected [${want}]"
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
#   LOCK
#   WORK
# Outputs:
#   Reports each step and each check, see step(), ok() and fail().
# Returns:
#   This function never returns, see finish().
#######################################
function main() {
    local recorded

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


    step "a branch ref is recorded on the first resolve"
    mk_conf "${WORK}/d.git" main "${WORK}/conf-d-main"
    run_fetch "${WORK}/conf-d-main" \
        || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker beta
    if [[ -f "${WORK}/builder.conf.lock" ]] ; then
        ok "lock file created"
    else
        fail "lock file was not created"
    fi
    expect_lock "${WORK}/d.git" main


    step "a recorded branch does not follow the tip"
    echo gamma >"${WORK}/d.git.work/marker.txt"
    ${GIT} -C "${WORK}/d.git.work" commit -q -am gamma
    ${GIT} -C "${WORK}/d.git.work" push -q "${WORK}/d.git" HEAD:refs/heads/main
    run_fetch "${WORK}/conf-d-main" \
        || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker beta
    if grep -q "fetching" "${WORK}/out" ; then
        fail "a recorded build went to the network: $(cat "${WORK}/out")"
    else
        ok "no fetch was made"
    fi


    step "--update-external moves the record to the current tip"
    if run_fetch --update "${WORK}/conf-d-main" ; then
        expect_marker gamma
        expect_out "updating"
    else
        fail "update failed: $(cat "${WORK}/out")"
    fi


    step "an update with nothing new reports that the ref is up to date"
    run_fetch --update "${WORK}/conf-d-main" \
        || fail "update failed: $(cat "${WORK}/out")"
    expect_marker gamma
    expect_out "already up to date"


    step "changing the ref in the configuration re-resolves it"
    mk_conf "${WORK}/d.git" v1 "${WORK}/conf-d-back"
    run_fetch "${WORK}/conf-d-back" \
        || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker alpha
    expect_out "declaration changed"
    expect_lock "${WORK}/d.git" v1


    step "ref equal to the default branch tip still materializes the tree"
    # A --no-checkout clone has HEAD at the tip, so this is the one case
    # where HEAD need not move and the script could skip the checkout
    mk_repo "${WORK}/e.git" only
    mk_conf "${WORK}/e.git" v1 "${WORK}/conf-e"
    rm -rf "${WORK}/build/ext-repos/ext"
    run_fetch "${WORK}/conf-e" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker only


    step "a changed URL costs no fetch while the record is available"
    # A repository moves while the commit already built sits in the
    # clone: there is nothing to resolve, so the script has no reason
    # to reach the new origin
    mk_repo "${WORK}/g.git" solo
    mk_conf "${WORK}/g.git" v1 "${WORK}/conf-g"
    LOCK="${WORK}/moved.lock"
    rm -rf "${WORK}/build/ext-repos/ext"
    run_fetch "${WORK}/conf-g" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker solo

    # The same repository at a second URL, which is then made unusable
    cp -R "${WORK}/g.git" "${WORK}/g2.git"
    mk_conf "${WORK}/g2.git" v1 "${WORK}/conf-g2"
    mv "${WORK}/g2.git" "${WORK}/g2.git.unreachable"
    python3 - "${WORK}/moved.lock" "${WORK}/g2.git" <<'PYEOF'
import sys
lock, url = sys.argv[1], sys.argv[2]
lines = open(lock).read().splitlines()
out = []
for l in lines:
    f = l.split()
    if len(f) == 4 and f[0] == 'ext':
        f[1] = url
        l = ' '.join(f)
    out.append(l)
open(lock, 'w').write('\n'.join(out) + '\n')
PYEOF
    if run_fetch "${WORK}/conf-g2" ; then
        ok "the build succeeded without touching the new origin"
        expect_marker solo
    else
        fail "the build went to the unreachable origin: $(cat "${WORK}/out")"
    fi
    expect_out "origin changed"
    LOCK=


    step "a clean build reproduces the recorded commit"
    # A fresh CI worker: no build tree, only the suite with its lock
    # file, while upstream has moved on since
    mk_repo "${WORK}/f.git" one
    mk_conf "${WORK}/f.git" main "${WORK}/conf-f"
    LOCK="${WORK}/clean.lock"
    rm -rf "${WORK}/build"
    run_fetch "${WORK}/conf-f" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker one
    recorded="$(grep '^ext ' "${LOCK}" | awk '{print $4}')"

    echo two >"${WORK}/f.git.work/marker.txt"
    ${GIT} -C "${WORK}/f.git.work" commit -q -am two
    ${GIT} -C "${WORK}/f.git.work" push -q "${WORK}/f.git" HEAD:refs/heads/main

    rm -rf "${WORK}/build"
    run_fetch "${WORK}/conf-f" || fail "fetcher failed: $(cat "${WORK}/out")"
    expect_marker one
    expect_eq "the recorded commit after a wiped build tree" \
        "$(grep '^ext ' "${LOCK}" | awk '{print $4}')" "${recorded}"
    LOCK=


    finish
}

main "$@"
