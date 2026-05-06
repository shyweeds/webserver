#!/usr/bin/env bash
set -euo pipefail

PORT=8080
SERVER_BIN=../bin/wserver
CLIENT_BIN=../bin/wclient
BASE_DIR=../bin/basedir
TESTDIR=./tests
TESTOUT_DIR=./test-out
THREADS=4
BUFFERS=4
SCHEDALG=FIFO

# Utils

BLUE="\e[1;34m"
RED="\e[1:31m"
RESET="\e[0m"

log() {
  echo -e "${BLUE}[CI]${RESET} $1"
}

fail() {
  echo -e "${RED}[FAIL]${RESET} $1"
}

# Build

build() {
  log "Building project..."
  cd ..
  make clean || true # clean failed is alright
  make >/dev/null 2>/dev/null
  cd "$TESTDIR"

  [[ -f "$SERVER_BIN" ]] || fail "wserver not built"
}

# Server lifecycle
start_server() {
  log "Starting server..."
  "$SERVER_BIN" -d "$BASE_DIR" -p "$PORT" -t "$THREADS" -b "$BUFFERS" -s "$SCHEDALG" 2>/dev/null &
  SERVER_PID=$!
  sleep 1
}

stop_server() {
  log "Stopping server..."
  kill "$SERVER_PID" || true
}

# Tests
test_static() {
  log "Testing static file..."
  mkdir -p "$TESTOUT_DIR"
  body=$("$CLIENT_BIN" localhost "$PORT" /a.html)
  echo "$body" 2> "${TESTOUT_DIR}/a.html.err" > "${TESTOUT_DIR}/a.html.out" || fail "static file test failed!"
}

test_cgi() {
  log "Testing CGI..."
  mkdir -p "$TESTOUT_DIR"
  body=$("$CLIENT_BIN" localhost "$PORT" /spin.cgi )
  echo "$body" 2> "${TESTOUT_DIR}/spin.cgi.err" > "${TESTOUT_DIR}/spin.cgi.out" || fail "cgi file test failed!"
}

test_404() {
  log "Testing 404..."
  mkdir -p "$TESTOUT_DIR"
  body=$("$CLIENT_BIN" localhost "$PORT" /notfind.html)
  echo "$body" 2> "${TESTOUT_DIR}/notfind.html.err" > "${TESTOUT_DIR}/notfind.html.out" || fail "404 notfind test failed!"
}

# Main
main() {
  build

  start_server
  trap stop_server EXIT

  test_static
  test_cgi
  test_404

  log "All tests passed!!"
}

main

