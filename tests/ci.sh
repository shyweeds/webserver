#!/usr/bin/env bash
set -euo pipefail

PORT=8080
SERVER_BIN=./bin/wserver
CLIENT_BIN=./bin/wclient
BASE_DIR=./bin/basedir
TESTOUT_DIR=./tests/test-out
EXPECTED_DIR=./tests/expected
THREADS=10
BUFFERS=10
SCHEDALG=FIFO

# Utils

BLUE="\e[1;34m"
RED="\e[1;31m"
RESET="\e[0m"

log() {
  echo -e "${BLUE}[CI]${RESET} $1"
}

fail() {
  echo -e "${RED}[FAIL]${RESET} $1"
  exit 1
}

# Build

build() {
  log "Building project..."
  make clean >/dev/null 2>&1 || true # clean failed is alright
  make >/dev/null 2>/dev/null

  [[ -x "$SERVER_BIN" ]] || fail "wserver not built"
  [[ -x "$CLIENT_BIN" ]] || fail "wclient not built"
}

# Tests

safe_name() {
  echo "$1" | sed 's#^/##'
}

run_case() {
  local name="$1"
  local path="$2"
  local expected_file="${3:-}"
  local expected_status="${4:-0}"

  local filename=$(safe_name "$path")
  local out="${TESTOUT_DIR}/$filename.out"
  local err="${TESTOUT_DIR}/$filename.err"

  log "Testing ${name}..."

  set +e
  "$CLIENT_BIN" localhost "$PORT" "$path" >"$out" 2>"$err"
  status=$?
  set -e

  if [[ "$status" -ne "$expected_status" ]]; then
    fail "${name}: unexpected exit code ${status}, expected ${expected_status}"
  fi

  if [[ -n "$expected_file" ]]; then
    if ! cmp -s "$expected_file" "$out"; then
      diff -u "$expected_file" "$out" || true
      fail "${name}: output mismatch"
    fi
  else
    if [[ ! -s "$out" ]]; then
      fail "${name}: empty response"
    fi
  fi
}

run_parallel_case() {
  local name="$1"
  local path="$2"
  local expected_file="${3:-}"
  local expected_status="${4:-0}"
  local n="${5:-8}"

  local filename=$(safe_name "$path")
  local out="${TESTOUT_DIR}/$filename.out"
  local err="${TESTOUT_DIR}/$filename.err"
  pids=()

  log "Testing ${name} with ${n} concurrent clients..."

  for ((i=1; i<=n; i++)); do
    (
    set +e
    "$CLIENT_BIN" localhost "$PORT" "$path" \
      >"${out}.${i}" \
      2>"${err}.${i}"
    echo $? >"${TESTOUT_DIR}/status.${i}"
    set -e
    ) &
    pids+=($!)
  done

  for pid in "${pids[@]}"; do
    wait "$pid"
  done

  # check the status
  for ((i=1; i<=n; i++)); do
    status=$(cat "${TESTOUT_DIR}/status.${i}")

    if [[ "$status" != "$expected_status" ]]; then
      fail "${name}: client ${i} exit code ${status}, expected ${expected_status}"
    fi

    if [[ -n "$expected_file" ]]; then
      if ! cmp -s "$expected_file" "${out}.${i}"; then
        diff -u "$expected_file" "${out}.${i}" || true
        fail "${name}: client ${i} output mismatch"
      fi
    else
        fail "${name}: no expected files!!!"
    fi
  done
}

test_multithread_static() {
  run_parallel_case "multithread static file test" "/a.html" "$EXPECTED_DIR/a.html.out" 0 20
}

test_multithread_cgi() {
  run_parallel_case "multithread static file test" "/spin.cgi" "$EXPECTED_DIR/spin.cgi.out" 0 20
}

test_static() {
  run_case "a simple static file test" "/a.html" "$EXPECTED_DIR/a.html.out" 0
}

test_cgi() {
  run_case "a simple cgi file test" "/spin.cgi" "$EXPECTED_DIR/spin.cgi.out" 0
}

test_404() {
  run_case "404 not find test" "/notfind.html" "$EXPECTED_DIR/notfind.html.out" 0
}

# Server lifecycle
start_server() {
  log "Starting server..."

  # start server
  "$SERVER_BIN" -d "$BASE_DIR" -p "$PORT" -t "$THREADS" -b "$BUFFERS" -s "$SCHEDALG" \
    >"${TESTOUT_DIR}/server.out"\
    2>"${TESTOUT_DIR}/server.err" &
  SERVER_PID=$!

  for _ in {1..50}; do
#    if ! kill -0 "SERVER_PID" 2>/dev/null; then
#      fail "server exist during startup (see ${TESTOUT_DIR}/server.err)"
#    fi
    
    set +e
    "$CLIENT_BIN" localhost "$PORT" /a.html >/dev/null 2>&1
    response=$?
    set -e
    if [[ $response -eq 0 ]]; then
      return 0
    fi

    sleep 0.5
  done

  fail "server did not become ready in time"
}

stop_server() {
  log "Stopping server..."
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  log "Stopping server[OK]!!!!"
}

# Main
main() {
  trap stop_server EXIT

  mkdir -p "$TESTOUT_DIR"
  build
  start_server

  test_static
  test_cgi
  test_404
  test_multithread_static
  test_multithread_cgi

  log "All tests passed!!"
}

main

