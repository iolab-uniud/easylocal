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
port=""
base_url=""
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

# Whether the server started is ready: false when it exited, as it does on a
# port taken by another program, which may have answered in its place.
ready() {
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        kill -0 "$server_pid" >/dev/null 2>&1 || return 1
        if "$curl_bin" --silent --max-time 1 --output /dev/null "${base_url}/"; then
            sleep 0.1
            kill -0 "$server_pid" >/dev/null 2>&1 || return 1
            return 0
        fi
        sleep 0.05
    done
    fail "server did not become ready"
}

# The server on a random port, another one when it is taken.
started=0
for ((attempt = 0; attempt < 5; ++attempt)); do
    port=$((40000 + RANDOM % 20000))
    base_url="http://127.0.0.1:${port}/tsp"
    "$server" "$port" >"${tmp_dir}/server.log" 2>&1 &
    server_pid=$!
    if ready; then
        started=1
        break
    fi
    wait "$server_pid" >/dev/null 2>&1 || true
done
[[ "$started" -eq 1 ]] || fail "the server could not start on a free port"

input='{"input":{"distance":[[0,2,9,10,7],[2,0,6,4,3],[9,6,0,8,5],[10,4,8,0,6],[7,3,5,6,0]]},"seed":7}'
submit="$("$curl_bin" --silent --show-error --request POST \
    --header 'Content-Type: application/json' --data "$input" \
    "${base_url}/runners/sa/runs")"
grep -Eq '"seed"[[:space:]]*:[[:space:]]*7' <<<"$submit" || fail "submission: $submit"
run_id="$(sed -E 's/.*"id"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/' <<<"$submit")"

succeeded=0
for ((attempt = 0; attempt < 200; ++attempt)); do
    status="$("$curl_bin" --silent "${base_url}/runs/${run_id}")"
    if grep -Eq '"status"[[:space:]]*:[[:space:]]*"succeeded"' <<<"$status"; then
        succeeded=1
        break
    fi
    grep -Eq '"status"[[:space:]]*:[[:space:]]*"failed"' <<<"$status" && fail "run failed: $status"
    sleep 0.05
done
[[ "$succeeded" -eq 1 ]] || fail "the run did not succeed in time: $status"

solution="$("$curl_bin" --silent "${base_url}/runs/${run_id}/solution")"
grep -Eq '"order"' <<<"$solution" || fail "no tour in: $solution"
grep -Eq '"length"[[:space:]]*:[[:space:]]*26(\.0+)?([^0-9.]|$)' <<<"$solution" || fail "not the optimal tour: $solution"

# The same app through its text hooks: the Input and the tour as text.
text_url="http://127.0.0.1:${port}/tsp-text"
text_input='{"input":"5\n0 2 9 10 7\n2 0 6 4 3\n9 6 0 8 5\n10 4 8 0 6\n7 3 5 6 0\n","initial_solution":"0 1 2 3 4","seed":7}'
submit="$("$curl_bin" --silent --show-error --request POST \
    --header 'Content-Type: application/json' --data "$text_input" \
    "${text_url}/runners/fi/runs")"
run_id="$(sed -E 's/.*"id"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/' <<<"$submit")"
[[ -n "$run_id" && "$run_id" != "$submit" ]] || fail "text submission: $submit"
for ((attempt = 0; attempt < 200; ++attempt)); do
    status="$("$curl_bin" --silent "${text_url}/runs/${run_id}")"
    grep -Eq '"status"[[:space:]]*:[[:space:]]*"succeeded"' <<<"$status" && break
    sleep 0.05
done
solution="$("$curl_bin" --silent "${text_url}/runs/${run_id}/solution")"
grep -Eq '"cost"[[:space:]]*:[[:space:]]*26(\.0+)?([^0-9.]|$)' <<<"$solution" || fail "text run: $solution"
grep -Eq '"solution"[[:space:]]*:[[:space:]]*"[0-9 ]+\\n"' <<<"$solution" \
    || fail "no tour as text in: $solution"
