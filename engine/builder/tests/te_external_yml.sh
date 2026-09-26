#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# Tests for engine/builder/te_external_yml.
#
# The catalog parser covers a fixed subset of YAML and must refuse
# the rest, so that a catalog reads the same way here and in a full
# YAML reader. These tests pin both halves: what the subset accepts
# and what the parser rejects.
#
# Usage: ./te_external_yml.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SCRIPT_DIR
YML="${SCRIPT_DIR}/../te_external_yml"
readonly YML

# shellcheck source=test_lib.sh
. "${SCRIPT_DIR}/test_lib.sh"

#######################################
# Write the catalog given on stdin to the work directory.
# Globals:
#   WORK
# Arguments:
#   Catalog file name, relative to the work directory.
#######################################
function catalog() {
    cat >"${WORK}/$1"
}

#######################################
# Report the shell variables a set of catalogs generates.
#
# The generated code reports a configuration error with 'break', as
# the builder.m4 macros do, so it is sourced inside a dummy loop.
# Globals:
#   WORK
#   YML
# Arguments:
#   Catalog file names, relative to the work directory.
# Outputs:
#   Writes one '<name>=<value>' line per reported variable to stdout.
# Returns:
#   0 if every catalog was accepted, non-zero otherwise.
#######################################
function generated() {
    local c

    : >"${WORK}/gen.sh"
    for c in "$@" ; do
        "${YML}" "${WORK}/${c}" >>"${WORK}/gen.sh" || return 1
    done
    bash -c '
        while true ; do
            . '"${WORK}"'/gen.sh
            break
        done
        echo "ERR=${TE_BS_CONF_ERR:-}"
        echo "URL=${TE_BS_EXT_REPO_r_URL:-}"
        echo "REF=${TE_BS_EXT_REPO_r_REF:-}"
        echo "LIBS=${TE_BS_EXT_REPO_r_LIBS:-}"
        echo "AGENTS=${TE_BS_EXT_REPO_r_AGENTS:-}"
    '
}

#######################################
# Check the value of a variable the last generated() call reported.
# Globals:
#   WORK
# Arguments:
#   Value the variable must hold.
#   Variable name.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_var() {
    local want="$1"; shift
    local got

    got="$(grep "^$1=" "${WORK}/out" | head -n 1)"
    if [[ "${got}" == "$1=${want}" ]] ; then
        ok "$1 is '${want}'"
    else
        fail "got '${got}', expected '$1=${want}'"
    fi
}

#######################################
# Check that a catalog is refused with the expected diagnostics.
# Globals:
#   WORK
#   YML
# Arguments:
#   Catalog file name, relative to the work directory.
#   Text the refusal must mention.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_rejected() {
    local out

    if out="$("${YML}" "${WORK}/$1" 2>&1 >/dev/null)" ; then
        fail "catalog was accepted, expected a refusal"
        return
    fi
    if echo "${out}" | grep -q -- "$2" ; then
        ok "refused: $2"
    else
        fail "refused with '${out}', expected a mention of '$2'"
    fi
}

#######################################
# Run every scenario and report the outcome.
#
# The catalogs are heredocs, so their bodies and terminators start
# at column zero.
# Globals:
#   WORK
#   YML
# Outputs:
#   Reports each step and each check, see step(), ok() and fail().
# Returns:
#   This function never returns, see finish().
#######################################
function main() {
    local conflict

    [[ -x "${YML}" ]] || {
        echo "ERROR: ${YML} is not executable" >&2
        exit 1
    }

    step "a catalog is turned into repository declarations"
    catalog good.yml <<'EOF'
repositories:
  - name: r
    url: https://example.com/r.git
    ref: v1.0.0
    libs:
      - tapi_r
      - ta_r
EOF
    generated good.yml >"${WORK}/out" || fail "parser failed"
    expect_var '' ERR
    expect_var 'https://example.com/r.git' URL
    expect_var 'v1.0.0' REF
    expect_var 'tapi_r ta_r' LIBS


    step "a comment is cut but a fragment in a URL is not"
    catalog hash.yml <<'EOF'
repositories:
  # a repository
  - name: r
    url: https://example.com/r.git#frag   # the URL above keeps its fragment
    ref: v1
EOF
    generated hash.yml >"${WORK}/out" || fail "parser failed"
    expect_var 'https://example.com/r.git#frag' URL
    expect_var 'v1' REF


    step "the same declaration twice is not a conflict"
    generated good.yml good.yml >"${WORK}/out" || fail "parser failed"
    expect_var '' ERR
    expect_var 'v1.0.0' REF


    step "two catalogs disagreeing about the ref is a conflict"
    catalog other-ref.yml <<'EOF'
repositories:
  - name: r
    url: https://example.com/r.git
    ref: v2.0.0
    libs:
      - tapi_r
      - ta_r
EOF
    generated good.yml other-ref.yml >"${WORK}/out" || fail "parser failed"
    conflict="^ERR=external repo r is already declared with a different ref$"
    if grep -q "${conflict}" "${WORK}/out" ; then
        ok "the conflict is reported"
    else
        fail "no conflict reported: $(grep '^ERR=' "${WORK}/out")"
    fi


    step "two catalogs disagreeing about the libraries is a conflict"
    catalog other-libs.yml <<'EOF'
repositories:
  - name: r
    url: https://example.com/r.git
    ref: v1.0.0
    libs:
      - tapi_r
EOF
    generated good.yml other-libs.yml >"${WORK}/out" || fail "parser failed"
    conflict="^ERR=external repo r is already declared with a different list"
    if grep -q "${conflict}" "${WORK}/out" ; then
        ok "the conflict is reported"
    else
        fail "no conflict reported: $(grep '^ERR=' "${WORK}/out")"
    fi


    step "agent types are declared alongside the libraries"
    catalog agents.yml <<'EOF'
repositories:
  - name: r
    url: https://example.com/r.git
    ref: v1.0.0
    agents:
      - riscv_agent
EOF
    generated agents.yml >"${WORK}/out" || fail "parser failed"
    expect_var 'riscv_agent' AGENTS
    expect_var '' LIBS


    step "a bad agent type name is refused"
    catalog bad-agent.yml <<'EOF'
repositories:
  - name: r
    url: u
    ref: v1
    agents:
      - 9ag
EOF
    expect_rejected bad-agent.yml "bad agent type name"


    step "a bad repository name is refused"
    catalog bad-name.yml <<'EOF'
repositories:
  - name: 9r
    url: u
    ref: v1
EOF
    expect_rejected bad-name.yml "bad repository name"


    step "an unknown key is refused"
    catalog unknown.yml <<'EOF'
repositories:
  - name: r
    url: u
    ref: v1
    branch: main
EOF
    expect_rejected unknown.yml "unknown repository keys"


    step "a missing mandatory key is refused"
    catalog no-ref.yml <<'EOF'
repositories:
  - name: r
    url: u
EOF
    expect_rejected no-ref.yml '"ref" is mandatory'


    step "a flow collection is refused, not misread"
    catalog flow.yml <<'EOF'
repositories:
  - name: r
    url: u
    ref: v1
    libs: [tapi_r, ta_r]
EOF
    expect_rejected flow.yml "does not support flow collections"


    step "a tab is refused"
    printf 'repositories:\n  - name: r\n\turl: u\n    ref: v1\n' \
        >"${WORK}/tab.yml"
    expect_rejected tab.yml "a tab"


    step "an anchor is refused"
    catalog anchor.yml <<'EOF'
repositories:
  - name: &base r
    url: u
    ref: v1
EOF
    expect_rejected anchor.yml "does not support an anchor"


    step "several documents are refused"
    catalog docs.yml <<'EOF'
---
repositories:
  - name: r
    url: u
    ref: v1
EOF
    expect_rejected docs.yml "one document only"


    step "a block scalar is refused"
    catalog block.yml <<'EOF'
repositories:
  - name: r
    url: |
      https://example.com/r.git
    ref: v1
EOF
    expect_rejected block.yml "does not support block scalars"


    step "an unexpected top-level key is refused"
    catalog toplevel.yml <<'EOF'
defaults:
  ref: v1
repositories:
  - name: r
    url: u
    ref: v1
EOF
    expect_rejected toplevel.yml "unexpected top-level entry"


    finish
}

main "$@"
