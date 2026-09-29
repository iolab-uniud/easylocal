#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <assignment-rest-server> <curl>" >&2
    exit 2
fi

server="$1"
curl_bin="$2"
port=$((20000 + ($$ % 20000)))
base_url="http://127.0.0.1:${port}/assignment"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/easylocal-rest.XXXXXX")"
server_log="${tmp_dir}/server.log"
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
    echo "REST integration test failed: $*" >&2
    if [[ -f "$server_log" ]]; then
        echo "--- server log ---" >&2
        cat "$server_log" >&2
    fi
    exit 1
}

request() {
    local method="$1"
    local url="$2"
    local expected="$3"
    local output="$4"
    local data="${5:-}"
    local code
    local args=(
        --silent
        --show-error
        --output "$output"
        --write-out "%{http_code}"
        --request "$method"
        --max-time 5
    )

    if [[ -n "$data" ]]; then
        args+=(
            --header "Content-Type: application/json"
            --data "$data"
        )
    fi

    code="$("$curl_bin" "${args[@]}" "$url")" || fail "$method $url: curl failed"
    if [[ "$code" != "$expected" ]]; then
        echo "--- response body ---" >&2
        cat "$output" >&2 || true
        fail "$method $url: expected HTTP $expected, got $code"
    fi
}

json_string() {
    local file="$1"
    local key="$2"
    sed -n "s/.*\"${key}\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$file" | head -n 1
}

repeat_json_number() {
    local count="$1"
    local value="$2"
    local result="["
    local i
    for ((i = 0; i < count; ++i)); do
        if ((i != 0)); then
            result+=","
        fi
        result+="$value"
    done
    result+="]"
    printf '%s' "$result"
}

wait_for_server() {
    local body="${tmp_dir}/ready.json"
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        if ! kill -0 "$server_pid" >/dev/null 2>&1; then
            fail "server exited before becoming ready"
        fi
        if "$curl_bin" --silent --show-error --max-time 1 \
                --output "$body" "${base_url}/" >/dev/null 2>&1; then
            return
        fi
        sleep 0.05
    done
    fail "server did not become ready"
}

wait_for_status() {
    local run_id="$1"
    local expected="$2"
    local body="$3"
    local attempt
    for ((attempt = 0; attempt < 200; ++attempt)); do
        request GET "${base_url}/runs/${run_id}" 200 "$body"
        local status
        status="$(json_string "$body" status)"
        if [[ "$status" == "$expected" ]]; then
            return
        fi
        if [[ "$status" == "failed" ]]; then
            echo "--- failed run ---" >&2
            cat "$body" >&2
            fail "run ${run_id} failed"
        fi
        sleep 0.05
    done
    echo "--- last run status ---" >&2
    cat "$body" >&2 || true
    fail "run ${run_id} did not reach status '${expected}'"
}

wait_for_progress() {
    local run_id="$1"
    local body="$2"
    local attempt
    for ((attempt = 0; attempt < 200; ++attempt)); do
        request GET "${base_url}/runs/${run_id}" 200 "$body"
        if grep -Eq '"status"[[:space:]]*:[[:space:]]*"running"' "$body" &&
                grep -Eq '"evaluations"[[:space:]]*:[[:space:]]*[1-9][0-9]*' "$body"; then
            return
        fi
        sleep 0.05
    done
    echo "--- last run status ---" >&2
    cat "$body" >&2 || true
    fail "run ${run_id} never reported observable progress"
}

"$server" "$port" >"$server_log" 2>&1 &
server_pid=$!
wait_for_server

root_body="${tmp_dir}/root.json"
request GET "${base_url}/" 200 "$root_body"
grep -Eq '"application"[[:space:]]*:[[:space:]]*"assignment"' "$root_body" || fail "root response has wrong application"
grep -q '"fi"' "$root_body" || fail "root response does not advertise fi"
grep -q '"slow-fi"' "$root_body" || fail "root response does not advertise slow-fi"

runners_body="${tmp_dir}/runners.json"
request GET "${base_url}/runners" 200 "$runners_body"
grep -q '"fi"' "$runners_body" || fail "runners response does not contain fi"
grep -q '"slow-fi"' "$runners_body" || fail "runners response does not contain slow-fi"

invalid_body="${tmp_dir}/invalid.json"
request POST "${base_url}/runners/fi/runs" 400 "$invalid_body" '{not-json'
grep -q '"error"' "$invalid_body" || fail "invalid JSON response has no error"

unknown_body="${tmp_dir}/unknown.json"
request POST "${base_url}/runners/missing/runs" 404 "$unknown_body" \
    '{"demand":[4,4,2],"capacity":[5,5]}'

submit_body="${tmp_dir}/submit.json"
request POST "${base_url}/runners/fi/runs" 202 "$submit_body" \
    '{"demand":[4,4,2],"capacity":[5,5]}'
run_id="$(json_string "$submit_body" run_id)"
[[ -n "$run_id" ]] || fail "successful submission has no run_id"

status_body="${tmp_dir}/status.json"
wait_for_status "$run_id" succeeded "$status_body"
grep -Eq '"stoppable"[[:space:]]*:[[:space:]]*true' "$status_body" || fail "fi should advertise cooperative stop"

solution_body="${tmp_dir}/solution.json"
request GET "${base_url}/runs/${run_id}/solution" 200 "$solution_body"
grep -q '"solution"' "$solution_body" || fail "solution response has no solution"
grep -q '"assignment"' "$solution_body" || fail "solution response has no assignment"
grep -q '"cost"' "$solution_body" || fail "solution response has no cost"

removed_body="${tmp_dir}/removed.json"
request DELETE "${base_url}/runs/${run_id}" 200 "$removed_body"
grep -Eq '"removed"[[:space:]]*:[[:space:]]*true' "$removed_body" || fail "completed run was not removed"
request GET "${base_url}/runs/${run_id}" 404 "${tmp_dir}/removed-status.json"

slow_demand="$(repeat_json_number 80 1)"
slow_capacity="$(repeat_json_number 16 10)"
slow_payload="{\"demand\":${slow_demand},\"capacity\":${slow_capacity}}"
slow_submit_body="${tmp_dir}/slow-submit.json"
request POST "${base_url}/runners/slow-fi/runs" 202 "$slow_submit_body" "$slow_payload"
slow_run_id="$(json_string "$slow_submit_body" run_id)"
[[ -n "$slow_run_id" ]] || fail "slow submission has no run_id"

slow_status_body="${tmp_dir}/slow-status.json"
wait_for_progress "$slow_run_id" "$slow_status_body"
grep -Eq '"stoppable"[[:space:]]*:[[:space:]]*true' "$slow_status_body" || fail "slow-fi should advertise cooperative stop"

cancel_body="${tmp_dir}/cancel.json"
request DELETE "${base_url}/runs/${slow_run_id}" 202 "$cancel_body"
grep -Eq '"status"[[:space:]]*:[[:space:]]*"cancellation_requested"' "$cancel_body" || fail "cancellation was not acknowledged"

wait_for_status "$slow_run_id" cancelled "$slow_status_body"
slow_solution_body="${tmp_dir}/slow-solution.json"
request GET "${base_url}/runs/${slow_run_id}/solution" 200 "$slow_solution_body"
grep -q '"solution"' "$slow_solution_body" || fail "cancelled run has no partial solution"

request DELETE "${base_url}/runs/${slow_run_id}" 200 "${tmp_dir}/slow-removed.json"

echo "REST HTTP integration: PASS"
