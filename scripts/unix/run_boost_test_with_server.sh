#!/bin/bash
# Launch a headless OpenDungeons server, run the given boost test binary
# (which acts as a game client) against it, then shut the server down.
#
# Usage: run_boost_test_with_server.sh <od-binary> <level> <test-command...>
#
# This is wired as the LAUNCHER of the integration tests (the aa-*/ab-* ones)
# in source/tests/CMakeLists.txt, so that a plain `ctest` run works without
# having to start a server manually. run_unit_tests.sh remains the way to run
# the whole suite directly from a build directory.

set -u

if [ $# -lt 3 ]; then
    echo "Usage: $0 <od-binary> <level> <test-command...>" >&2
    exit 1
fi

OD_BINARY="$1"
LEVEL="$2"
shift 2

if [ ! -x "${OD_BINARY}" ]; then
    echo "Game binary not found or not executable: ${OD_BINARY}" >&2
    exit 1
fi

# The game must be started from the directory containing its binary so that it
# finds resources.cfg and the game data symlinked into the build tree.
OD_DIR="$(cd "$(dirname "${OD_BINARY}")" && pwd)"

SERVER_LOG="srvLog-$(basename "${LEVEL}" .level).txt"
(cd "${OD_DIR}" && exec "./$(basename "${OD_BINARY}")" --server "${LEVEL}" --port 32222 --log "${SERVER_LOG}") &
SERVER_PID=$!

stop_server() {
    kill "${SERVER_PID}" 2>/dev/null
    wait "${SERVER_PID}" 2>/dev/null
}
trap stop_server EXIT

# The test client retries the connection itself, but fail fast if the server
# died right away (e.g. level not found).
sleep 1
if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "Server failed to start (level ${LEVEL}). Check ${SERVER_LOG} in the user data folder." >&2
    exit 1
fi

"$@"
exit $?
