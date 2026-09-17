#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# Tests for the external repository support of
# engine/builder/te_meson_build.
#
# The test sources the script rather than run it, and calls its
# function process_builder_conf(), which turns the builder
# configuration into what a build reads. The test creates the
# repository in the work directory, so it needs no network.
#
# The test compiles nothing. Its checks stop at the inputs of the
# build: the variables a configuration turns into and where the
# sources are checked out. It does not check that a library from
# such a repository links.
#
# Usage: ./te_meson_build.sh

# Not readonly: te_meson_build owns this name too, and the test
# points it back at the Builder after sourcing the script.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILDER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly BUILDER_DIR
TE_BASE_DIR="$(cd "${BUILDER_DIR}/../.." && pwd)"
readonly TE_BASE_DIR
MESON_BUILD="${BUILDER_DIR}/te_meson_build"
readonly MESON_BUILD
FETCH="${BUILDER_DIR}/te_fetch_ext_repos"
readonly FETCH

# shellcheck source=test_lib.sh
. "${SCRIPT_DIR}/test_lib.sh"

# The platform the configurations below declare first. It becomes
# TE_HOST, and that is what TE_EXT_REPO binds libraries to when no
# platform is named.
readonly TEST_PLATFORM=default

# The catalog handed to the build, if any (dispatcher.sh --external).
CATALOG=

#######################################
# Create a directory with one source file and its meson.build.
#
# That is the least a library directory can hold. The test does
# not compile it.
# Arguments:
#   Directory to create, a path.
#   Source file name.
#######################################
function mk_dir() {
    local dir="$1"; shift
    local src="$1"; shift

    mkdir -p "${dir}"
    echo "sources += files('${src}')" >"${dir}/meson.build"
    echo "/* Nothing builds this, see the test header. */" >"${dir}/${src}"
}

#######################################
# Create a bare repository with one library.
#
# The single commit is tagged v1.
# Globals:
#   GIT
# Arguments:
#   Bare repository, a path.
#######################################
function mk_ext_repo() {
    local bare="$1"; shift
    local work="${bare}.work"

    ${GIT} init -q --bare "${bare}"
    ${GIT} init -q "${work}"
    mk_dir "${work}/tapi_ext_selftest" tapi_ext_selftest.c
    ${GIT} -C "${work}" add .
    ${GIT} -C "${work}" commit -q -m "minimal external repository"
    ${GIT} -C "${work}" tag v1
    ${GIT} -C "${work}" push -q "${bare}" HEAD:refs/heads/main --tags
}

#######################################
# Run a command with the functions of te_meson_build.
#
# The script builds only when executed, so the test sources it in a
# subshell with the environment of a build: without 'set -u', which
# the script does not use and its configuration macros would trip
# over, and with the positional parameters cleared, since the script
# takes its configuration from $1. The test then drops the 'set -e'
# and the error trap the script arms, so that a check can inspect a
# failure instead of dying of it.
# Globals:
#   BUILDER_DIR
#   CATALOG
#   MESON_BUILD
#   TE_BASE_DIR
#   WORK
# Arguments:
#   Command to run and its arguments.
# Returns:
#   The exit status of the command.
#######################################
function with_builder() {
    local cmd=("$@")

    (
        set --
        set +u
        TE_BASE="${TE_BASE_DIR}"
        TE_BUILD="${WORK}/build"
        TE_INSTALL="${WORK}/inst"
        TE_INSTALL_NUT="${WORK}/inst/nut"
        TE_INSTALL_SUITE="${WORK}/inst/suites"
        TE_EXTERNAL_YML="${CATALOG}"
        # shellcheck source=../te_meson_build
        . "${MESON_BUILD}"
        set +e
        trap - ERR
        # The script looks for its helpers next to the script that
        # was run, here the test, so point it back at the Builder.
        SCRIPT_DIR="${BUILDER_DIR}"
        "${cmd[@]}"
    )
}

#######################################
# Write a builder configuration from the lines given on stdin.
#
# The platform declaration a real configuration opens with is put in
# front of them.
# Globals:
#   WORK
#######################################
function mk_conf() {
    {
        echo "TE_PLATFORM([], [], [], [], [], [])"
        cat
    } >"${WORK}/builder.conf"
}

#######################################
# Process the builder configuration the way a build does.
#
# The catalog, when there is one, is read first.
# Globals:
#   WORK
# Outputs:
#   Writes the processed configuration to ${WORK}/processed and
#   reports a failure, see fail().
# Returns:
#   0 if the configuration was processed, non-zero otherwise.
#######################################
function process_conf() {
    if with_builder process_builder_conf "${WORK}/builder.conf" \
            >"${WORK}/processed" 2>"${WORK}/err" ; then
        return 0
    fi
    fail "processing the configuration failed: $(cat "${WORK}/err")"
    return 1
}

#######################################
# Report a variable of the processed configuration.
#
# It is read the way read_processed_builder_conf() reads it: inside
# a loop, since the macros report an error by breaking out of one,
# and without 'set -u', since they test variables that may be unset.
# Globals:
#   WORK
# Arguments:
#   Variable name.
# Outputs:
#   Writes the value of the variable to stdout.
#######################################
function conf_get() {
    local var="$1"

    (
        set +u
        TE_BS_CONF_ERR=
        while true ; do
            # shellcheck source=/dev/null
            . "${WORK}/processed"
            break
        done
        printf '%s\n' "${!var-}"
    )
}

#######################################
# Check that the processed configuration was refused.
# Arguments:
#   Text the reason for the refusal must mention.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_refused() {
    local want="$1"
    local err

    err="$(conf_get TE_BS_CONF_ERR)"
    case "${err}" in
        "") fail "the configuration was accepted" ;;
        *"${want}"*) ok "refused: ${err}" ;;
        *) fail "refused for another reason: ${err}" ;;
    esac
}

#######################################
# Check that a file was checked out.
# Arguments:
#   Description of the file being checked.
#   File, a path.
# Outputs:
#   Reports the check, see ok() and fail().
#######################################
function expect_file() {
    local what="$1"; shift
    local file="$1"; shift

    if [[ -f "${file}" ]] ; then
        ok "${what} is checked out"
    else
        fail "${what}: no ${file}"
    fi
}

#######################################
# Fetch the repositories of the processed configuration.
#
# The sources land in ${WORK}/build.
# Globals:
#   FETCH
#   WORK
# Outputs:
#   Writes what the fetcher reported to ${WORK}/out and reports a
#   failure, see fail().
# Returns:
#   0 if every repository was obtained, non-zero otherwise.
#######################################
function run_fetch() {
    if ( cd "${WORK}" && TE_BUILD="${WORK}/build" \
            "${FETCH}" "${WORK}/processed" ) \
            >"${WORK}/out" 2>&1 ; then
        return 0
    fi
    fail "fetching failed: $(cat "${WORK}/out")"
    return 1
}

#######################################
# Run every scenario and report the outcome.
#
# The configurations are heredocs, so their bodies and terminators
# start at column zero.
# Globals:
#   FETCH
#   MESON_BUILD
#   TEST_PLATFORM
#   WORK
# Outputs:
#   Reports each step and each check, see step(), ok() and fail().
# Returns:
#   This function never returns, see finish().
#######################################
function main() {
    local bare="${WORK}/ext.git"
    local lib_src="${WORK}/build/ext-repos/extselftest/tapi_ext_selftest"
    local f

    for f in "${MESON_BUILD}" "${FETCH}" ; do
        [[ -x "${f}" ]] || {
            echo "ERROR: ${f} is not executable" >&2
            exit 1
        }
    done
    mk_ext_repo "${bare}"

    step "TE_EXT_REPO declares the repository and its library"
    mk_conf <<EOF
TE_EXT_REPO([extselftest], [], [${bare}], [v1], [tapi_ext_selftest])
EOF
    if process_conf ; then
        expect_eq "the configuration error" "$(conf_get TE_BS_CONF_ERR)" ""
        expect_contains "the repository list" "$(conf_get TE_BS_EXT_REPOS)" \
                        extselftest
        expect_eq "the recorded URL" \
                  "$(conf_get TE_BS_EXT_REPO_extselftest_URL)" "${bare}"
        expect_eq "the recorded ref" \
                  "$(conf_get TE_BS_EXT_REPO_extselftest_REF)" v1
        expect_contains "the platform library list" \
                        "$(conf_get "${TEST_PLATFORM}_LIBS")" \
                        tapi_ext_selftest
        expect_eq "the library sources" \
            "$(conf_get \
               "TE_BS_LIB_${TEST_PLATFORM}_tapi_ext_selftest_SOURCES")" \
            "${lib_src}"
    fi

    step "A repository name that is not an identifier is refused"
    mk_conf <<EOF
TE_EXT_REPO([1extselftest], [], [${bare}], [v1], [tapi_ext_selftest])
EOF
    if process_conf ; then
        expect_refused "does not start with a letter"
    fi

    step "A repository without a ref is refused"
    mk_conf <<EOF
TE_EXT_REPO([extselftest], [], [${bare}], [], [tapi_ext_selftest])
EOF
    if process_conf ; then
        expect_refused "URL and ref are mandatory"
    fi

    step "TE_EXT_REPO_USE takes the URL and the ref from the catalog"
    CATALOG="${WORK}/external.yml"
    cat >"${CATALOG}" <<EOF
repositories:
  - name: extselftest
    url: ${bare}
    ref: v1
    libs:
      - tapi_ext_selftest
EOF
    mk_conf <<'EOF'
TE_EXT_REPO_USE([extselftest], [], [tapi_ext_selftest])
EOF
    if process_conf ; then
        expect_eq "the configuration error" "$(conf_get TE_BS_CONF_ERR)" ""
        expect_eq "the catalog URL" \
                  "$(conf_get TE_BS_EXT_REPO_extselftest_URL)" "${bare}"
        expect_eq "the catalog ref" \
                  "$(conf_get TE_BS_EXT_REPO_extselftest_REF)" v1
        expect_eq "the library sources" \
            "$(conf_get \
               "TE_BS_LIB_${TEST_PLATFORM}_tapi_ext_selftest_SOURCES")" \
            "${lib_src}"
    fi

    step "TE_EXT_REPO_USE refuses a library the catalog does not provide"
    mk_conf <<'EOF'
TE_EXT_REPO_USE([extselftest], [], [tapi_not_there])
EOF
    if process_conf ; then
        expect_refused "does not provide library tapi_not_there"
    fi

    step "TE_EXT_REPO_USE refuses a repository that was never declared"
    CATALOG=
    mk_conf <<'EOF'
TE_EXT_REPO_USE([nosuchrepo], [], [tapi_ext_selftest])
EOF
    if process_conf ; then
        expect_refused "nosuchrepo is not declared"
    fi

    step "The sources land where the configuration says they will"
    mk_conf <<EOF
TE_EXT_REPO([extselftest], [], [${bare}], [v1], [tapi_ext_selftest])
EOF
    if process_conf && run_fetch ; then
        expect_file "the library sources" "${lib_src}/tapi_ext_selftest.c"
        expect_file "the library build file" "${lib_src}/meson.build"
    fi

    finish
}

main "$@"
