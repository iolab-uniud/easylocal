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
completed_run_capacity=3

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
    local headers="${6:-}"
    local code
    local args=(
        --silent
        --show-error
        --output "$output"
        --write-out "%{http_code}"
        --request "$method"
        --max-time 5
    )

    if [[ -n "$headers" ]]; then
        args+=(--dump-header "$headers")
    fi
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

assert_error_code() {
    local file="$1"
    local expected="$2"
    grep -Eq "\"code\"[[:space:]]*:[[:space:]]*\"${expected}\"" "$file" || {
        echo "--- error response ---" >&2
        cat "$file" >&2 || true
        fail "expected error code '${expected}'"
    }
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
                grep -Eq '"progress"[[:space:]]*:' "$body" &&
                grep -Eq '"evaluations"[[:space:]]*:[[:space:]]*[1-9][0-9]*' "$body"; then
            return
        fi
        sleep 0.05
    done
    echo "--- last run status ---" >&2
    cat "$body" >&2 || true
    fail "run ${run_id} never reported observable progress"
}

structured_input='{"input":{"demand":[4,4,2],"capacity":[5,5]}}'
text_input='{"input":"3 2 4 3 2 5 5"}'

"$server" "$port" "$completed_run_capacity" >"$server_log" 2>&1 &
server_pid=$!
wait_for_server

root_body="${tmp_dir}/root.json"
request GET "${base_url}/" 200 "$root_body"
grep -Eq '"application"[[:space:]]*:[[:space:]]*"assignment"' "$root_body" || fail "root response has wrong application"
grep -q '"fi"' "$root_body" || fail "root response does not advertise fi"
grep -q '"slow-fi"' "$root_body" || fail "root response does not advertise slow-fi"
grep -Eq '"completed_run_capacity"[[:space:]]*:[[:space:]]*3' "$root_body" || fail "root response has wrong retention capacity"

runners_body="${tmp_dir}/runners.json"
request GET "${base_url}/runners" 200 "$runners_body"
grep -q '"fi"' "$runners_body" || fail "runners response does not contain fi"
grep -q '"slow-fi"' "$runners_body" || fail "runners response does not contain slow-fi"

invalid_body="${tmp_dir}/invalid.json"
request POST "${base_url}/runners/fi/runs" 400 "$invalid_body" '{not-json'
assert_error_code "$invalid_body" invalid_json

missing_input_body="${tmp_dir}/missing-input.json"
request POST "${base_url}/runners/fi/runs" 422 "$missing_input_body" '{}'
assert_error_code "$missing_input_body" invalid_run_request

invalid_domain_body="${tmp_dir}/invalid-domain.json"
request POST "${base_url}/runners/fi/runs" 422 "$invalid_domain_body" \
    '{"input":{"demand":[1],"capacity":[]}}'
assert_error_code "$invalid_domain_body" invalid_run_request

initial_solution_body="${tmp_dir}/initial-solution.json"
request POST "${base_url}/runners/fi/runs" 422 "$initial_solution_body" \
    '{"input":{"demand":[4,4,2],"capacity":[5,5]},"initial_solution":[0,1,0]}'
assert_error_code "$initial_solution_body" invalid_run_request

unknown_body="${tmp_dir}/unknown.json"
request POST "${base_url}/runners/missing/runs" 404 "$unknown_body" "$structured_input"
assert_error_code "$unknown_body" unknown_runner

missing_run_body="${tmp_dir}/missing-run.json"
request GET "${base_url}/runs/does-not-exist" 404 "$missing_run_body"
assert_error_code "$missing_run_body" run_not_found

submit_body="${tmp_dir}/submit.json"
submit_headers="${tmp_dir}/submit.headers"
request POST "${base_url}/runners/fi/runs" 202 "$submit_body" "$structured_input" "$submit_headers"
run_id="$(json_string "$submit_body" id)"
[[ -n "$run_id" ]] || fail "successful submission has no id"
grep -Eiq "^Location:[[:space:]]*/assignment/runs/${run_id}[[:space:]]*$" "$submit_headers" || fail "submission has no correct Location header"

status_body="${tmp_dir}/status.json"
wait_for_status "$run_id" succeeded "$status_body"
grep -Eq '"progress"[[:space:]]*:' "$status_body" || fail "status response has no progress object"

solution_body="${tmp_dir}/solution.json"
request GET "${base_url}/runs/${run_id}/solution" 200 "$solution_body"
grep -q '"solution"' "$solution_body" || fail "solution response has no solution"
grep -q '"assignment"' "$solution_body" || fail "solution response has no assignment"
grep -q '"cost"' "$solution_body" || fail "solution response has no cost"
grep -Eq '"status"[[:space:]]*:[[:space:]]*"succeeded"' "$solution_body" || fail "solution response has no terminal status"

request POST "${base_url}/runs/${run_id}/cancel" 409 "${tmp_dir}/cancel-terminal.json"
assert_error_code "${tmp_dir}/cancel-terminal.json" run_not_active
request DELETE "${base_url}/runs/${run_id}" 204 "${tmp_dir}/removed.json"
[[ ! -s "${tmp_dir}/removed.json" ]] || fail "DELETE 204 returned a response body"
request GET "${base_url}/runs/${run_id}" 404 "${tmp_dir}/removed-status.json"

# The framework treats input as an opaque JSON value.  The Assignment codec
# deliberately accepts both a structured object and the existing text format.
text_submit_body="${tmp_dir}/text-submit.json"
request POST "${base_url}/runners/fi/runs" 202 "$text_submit_body" "$text_input"
text_run_id="$(json_string "$text_submit_body" id)"
[[ -n "$text_run_id" ]] || fail "text-input submission has no id"
wait_for_status "$text_run_id" succeeded "${tmp_dir}/text-status.json"
request DELETE "${base_url}/runs/${text_run_id}" 204 "${tmp_dir}/text-removed.json"

slow_demand="$(repeat_json_number 80 1)"
slow_capacity="$(repeat_json_number 16 10)"
slow_payload="{\"input\":{\"demand\":${slow_demand},\"capacity\":${slow_capacity}}}"
slow_submit_body="${tmp_dir}/slow-submit.json"
request POST "${base_url}/runners/slow-fi/runs" 202 "$slow_submit_body" "$slow_payload"
slow_run_id="$(json_string "$slow_submit_body" id)"
[[ -n "$slow_run_id" ]] || fail "slow submission has no id"

slow_status_body="${tmp_dir}/slow-status.json"
wait_for_progress "$slow_run_id" "$slow_status_body"

not_ready_body="${tmp_dir}/not-ready.json"
request GET "${base_url}/runs/${slow_run_id}/solution" 409 "$not_ready_body"
assert_error_code "$not_ready_body" result_not_ready

active_delete_body="${tmp_dir}/active-delete.json"
request DELETE "${base_url}/runs/${slow_run_id}" 409 "$active_delete_body"
assert_error_code "$active_delete_body" run_not_terminal

cancel_body="${tmp_dir}/cancel.json"
request POST "${base_url}/runs/${slow_run_id}/cancel" 202 "$cancel_body"
grep -Eq '"cancellation_requested"[[:space:]]*:[[:space:]]*true' "$cancel_body" || fail "cancellation was not acknowledged"

wait_for_status "$slow_run_id" cancelled "$slow_status_body"
slow_solution_body="${tmp_dir}/slow-solution.json"
request GET "${base_url}/runs/${slow_run_id}/solution" 200 "$slow_solution_body"
grep -q '"solution"' "$slow_solution_body" || fail "cancelled run has no partial solution"
grep -Eq '"status"[[:space:]]*:[[:space:]]*"cancelled"' "$slow_solution_body" || fail "partial solution has wrong status"
request DELETE "${base_url}/runs/${slow_run_id}" 204 "${tmp_dir}/slow-removed.json"

# Retention is bounded to completed_run_capacity.  With capacity 3, completing
# four undeleted runs must evict the oldest terminal record and retain the last
# three.  Active runs are never part of this eviction queue.
retained_ids=()
for index in 1 2 3 4; do
    body="${tmp_dir}/retention-submit-${index}.json"
    request POST "${base_url}/runners/fi/runs" 202 "$body" "$structured_input"
    retention_id="$(json_string "$body" id)"
    [[ -n "$retention_id" ]] || fail "retention submission ${index} has no id"
    retained_ids+=("$retention_id")
    wait_for_status "$retention_id" succeeded "${tmp_dir}/retention-status-${index}.json"
done

request GET "${base_url}/runs/${retained_ids[0]}" 404 "${tmp_dir}/evicted.json"
assert_error_code "${tmp_dir}/evicted.json" run_not_found
request GET "${base_url}/runs/${retained_ids[1]}" 200 "${tmp_dir}/retained.json"
request GET "${base_url}/runs/${retained_ids[3]}" 200 "${tmp_dir}/latest.json"
request GET "${base_url}/" 200 "$root_body"
grep -Eq '"completed_runs"[[:space:]]*:[[:space:]]*3' "$root_body" || fail "retention did not keep exactly three completed runs"

echo "REST HTTP integration: PASS"
