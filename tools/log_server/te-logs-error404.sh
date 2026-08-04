#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2007-2023 OKTET Labs Ltd. All rights reserved.
#
# CGI script to handle error 404 in log storage directories and
# generate requested file on fly.
#
# Uses the following CGI environment varialbes:
#   - REQUEST_URI
#
# Uses the following environment variables:
#   - TE_INSTALL (mandatory) to find TE log processing tools
#   - SHARED_URL (optional) shared static files of HTML logs to speed up
#   - TMPDIR (optoinal) location for temporarily unpacked log files
#
# Multi-page HTML logs have static part which is the same for instances.
# It could be stored on logs server in one place and referenced from
# all generated logs. It allows to use browser cache efficiently.
#
# Required size for TMPDIR depends on size of your logs. Typically 10 Gb
# is sufficient.

#############################
# Exit with "Not Found" message.
# Arguments:
#   None
# Outputs:
#   HTTP response
#############################
function not_found_exit() {
    printf "Status: 404 Not Found\r\n"
    printf "Content-Type: text/plain\r\n"
    printf "\r\n"
    printf "Not Found\r\n"
    [[ $# -eq 0 ]] || printf "%s\r\n" "$*"
    exit 0
}

#############################
# Exit with an internal server error without exposing generator output.
# Arguments:
#   None
# Outputs:
#   HTTP response
#############################
function internal_error_exit() {
    printf "Status: 500 Internal Server Error\r\n"
    printf "Content-Type: text/plain\r\n"
    printf "\r\n"
    printf "Log generation failed\r\n"
    exit 0
}

#############################
# Redirect to the now-generated request path.
# Arguments:
#   None
# Outputs:
#   HTTP response
#############################
function redirect_to_request() {
    printf "Status: 302 Found\r\n"
    printf "Location: %s\r\n" "${redirect_uri}"
    printf "\r\n"
    exit 0
}

[[ -n "${TE_INSTALL}" ]] || not_found_exit "TE_INSTALL is not set"

TE_PATH="${TE_INSTALL}/default/bin/"

case "${REQUEST_URI}" in
    # FIXME Put your logs URL to location mapping here
    /logs/* )
        root_dir_uri="/logs"
        root_dir="/srv/logs"
        # FIXME Set docs_url to test suite documentation html root to
        # have links to the test documentation in HTML and JSON logs.
        ;;
    * )
        not_found_exit
        ;;
esac

# Requested file. Ignore a query string when mapping the URI to the file
# system, but preserve it in redirects.
redirect_uri="${REQUEST_URI}"
request_uri_path="${REQUEST_URI%%\?*}"
request_uri_query=
if [[ "${REQUEST_URI}" == *\?* ]] ; then
    request_uri_query="?${REQUEST_URI#*\?}"
fi

# JSON node files belong to the json subdirectory. Normalize a root-level
# request before passing it to rgt-log-get-item, otherwise that tool creates
# the requested file literally in the run root.
request_uri_dir="${request_uri_path%/*}"
request_uri_name="${request_uri_path##*/}"
case "${request_uri_name}" in
    node_*.json | tree.json )
        if [[ "${request_uri_dir##*/}" != "json" ]] ; then
            request_uri_path="${request_uri_dir}/json/${request_uri_name}"
            redirect_uri="${request_uri_path}${request_uri_query}"
        fi
        ;;
esac

request_file="${root_dir}${request_uri_path#"${root_dir_uri}"}"
# Substitute %20->'space'
request_file="${request_file//%20/ }"
# Substitute %3A->':'
request_file="${request_file//%3A/:}"

# Reject paths which escape the configured logs root after normalization.
canonical_root=$(realpath -m -- "${root_dir}") || internal_error_exit
request_file=$(realpath -m -- "${request_file}") || internal_error_exit
case "${request_file}" in
    "${canonical_root}"/* ) ;;
    * ) not_found_exit ;;
esac

# Another request may have completed generation after Apache selected this
# error handler but before this process started.
[[ -r "${request_file}" ]] && redirect_to_request

# Serialize generation by final output path. The check after flock is
# essential: all waiters should reuse the first request's completed file.
lock_dir="${TMPDIR:-/tmp}/te-log-generation-locks"
mkdir -p -- "${lock_dir}" || internal_error_exit
lock_key=$(printf "%s" "${request_file}" | sha256sum) || internal_error_exit
lock_key="${lock_key%% *}"
# Use a bounded pool: collisions only serialize generation of unrelated files,
# while arbitrary request paths can create at most 256 persistent lock files.
lock_slot="${lock_key:0:2}"
exec {lock_fd}>"${lock_dir}/${lock_slot}.lock" || internal_error_exit
flock -x "${lock_fd}" || internal_error_exit

[[ -r "${request_file}" ]] && redirect_to_request

get_item_cmd=()
# It is recommended to run it under nice since really many requests
# could be generated via Web server
get_item_cmd+=(nice)
get_item_cmd+=("${TE_PATH}"/rgt-log-get-item)
[[ -n "${SHARED_URL}" ]] && get_item_cmd+=(--shared-url="${SHARED_URL}/")
[[ -n "${docs_url}" ]] && get_item_cmd+=(--docs-url="${docs_url}")
# Try to fix generated files permissions
get_item_cmd+=(--fix-permissions)
# Try to generate requested log file
status=$("${get_item_cmd[@]}" --req-path="${request_file}" 2>&1)
result=$?

if [[ -r "${request_file}" ]] ; then
    redirect_to_request
elif [[ "${result}" -eq 1 ]] ; then
    # The source log or requested item does not exist.
    not_found_exit
elif [[ "${result}" -ne 0 ]] ; then
    # Keep command output in the server log; it may contain host paths or
    # parser details which should not be returned to clients.
    printf "Failed to generate %s (status %d): %s\n" \
           "${REQUEST_URI}" "${result}" "${status}" >&2
    internal_error_exit
else
    not_found_exit
fi
