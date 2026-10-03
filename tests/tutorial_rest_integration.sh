#!/usr/bin/env bash
# Smoke test of the tutorial's REST service (examples/tutorial/rest_main.cpp):
# submit a seeded Simulated Annealing run and read its solution.

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <tutorial-rest-server> <curl>" >&2
    exit 2
fi

server="$1"
curl_bin="$2"
port=$((40000 + ($$ % 20000)))
base_url="http://127.0.0.1:${port}/tsp"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/easylocal-tutorial-rest.XXXXXX")"
server_pid=""

cleanup() {
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" >/dev/null 2>&1; then
        kill "$server_pid" >/dev/null 2>&1 || true
        wait "$server_pid" >/dev/null 2>&1 || true
    fi
    rm -rf "$tmp_dir"
}
trap cleanup EXIT INT TERM

fail() {
    echo "tutorial REST test failed: $*" >&2
    cat "${tmp_dir}/server.log" >&2 2>/dev/null || true
    exit 1
}

"$server" "$port" >"${tmp_dir}/server.log" 2>&1 &
server_pid=$!

for ((attempt = 0; attempt < 100; ++attempt)); do
    kill -0 "$server_pid" >/dev/null 2>&1 || fail "server exited before becoming ready"
    if "$curl_bin" --silent --max-time 1 --output /dev/null "${base_url}/"; then
        break
    fi
    sleep 0.05
done

input='{"input":{"distance":[[0,2,9,10,7],[2,0,6,4,3],[9,6,0,8,5],[10,4,8,0,6],[7,3,5,6,0]]},"seed":7}'
submit="$("$curl_bin" --silent --show-error --request POST \
    --header 'Content-Type: application/json' --data "$input" \
    "${base_url}/runners/sa/runs")"
grep -Eq '"seed"[[:space:]]*:[[:space:]]*7' <<<"$submit" || fail "submission: $submit"
run_id="$(sed -E 's/.*"id"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/' <<<"$submit")"

for ((attempt = 0; attempt < 200; ++attempt)); do
    status="$("$curl_bin" --silent "${base_url}/runs/${run_id}")"
    grep -Eq '"status"[[:space:]]*:[[:space:]]*"succeeded"' <<<"$status" && break
    grep -Eq '"status"[[:space:]]*:[[:space:]]*"failed"' <<<"$status" && fail "run failed: $status"
    sleep 0.05
done

solution="$("$curl_bin" --silent "${base_url}/runs/${run_id}/solution")"
grep -Eq '"order"' <<<"$solution" || fail "no tour in: $solution"
grep -Eq '"length"[[:space:]]*:[[:space:]]*26' <<<"$solution" || fail "not the optimal tour: $solution"
