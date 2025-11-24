#!/usr/bin/env bash
# Run putall workload sweeps and capture metrics into CSV.

set -euo pipefail

THREADS_LIST=${THREADS_LIST:-"2 4 8 16 32 64 128"}
DURATION=${DURATION:-300}
VALUE_BYTES=${VALUE_BYTES:-512}
OUTPUT=${OUTPUT:-results_putall.csv}
URL=${URL:-http://localhost:8080}
SERVER_PID=${SERVER_PID:-$(pgrep -f proxy_server | head -n1 || true)}
SAMPLE_INTERVAL=${SAMPLE_INTERVAL:-5}

if [[ -z "$SERVER_PID" ]]; then
  echo "proxy_server is not running (port 8080). Start it first." >&2
  exit 1
fi

echo "threads,throughput_req_per_s,lat_avg_ms,lat_p50_ms,lat_p95_ms,lat_p99_ms,cpu_avg_pct" > "$OUTPUT"

for t in $THREADS_LIST; do
  echo "[RUN] threads=$t duration=${DURATION}s"
  cpu_log=$(mktemp /tmp/putall_cpu.XXXXXX)

  # Sample server CPU while the loadgen is running.
  (
    while ps -p "$SERVER_PID" > /dev/null 2>&1; do
      ps -p "$SERVER_PID" -o %cpu= | awk '{printf "%s\n", $1}' >> "$cpu_log"
      sleep "$SAMPLE_INTERVAL"
    done
  ) &
  sampler_pid=$!

  run_out=$(./loadgen putall --url "$URL" --threads "$t" --duration "$DURATION" --value-bytes "$VALUE_BYTES")

  kill "$sampler_pid" >/dev/null 2>&1 || true
  wait "$sampler_pid" >/dev/null 2>&1 || true

  throughput=$(echo "$run_out" | sed -nE 's/.*throughput=([0-9.]+) .*/\1/p')
  lat_line=$(echo "$run_out" | grep latency_avg= || true)
  lat_avg=$(echo "$lat_line" | sed -nE 's/.*latency_avg=([0-9.]+) ms.*/\1/p')
  lat_p50=$(echo "$lat_line" | sed -nE 's/.*p50=([0-9.]+) ms.*/\1/p')
  lat_p95=$(echo "$lat_line" | sed -nE 's/.*p95=([0-9.]+) ms.*/\1/p')
  lat_p99=$(echo "$lat_line" | sed -nE 's/.*p99=([0-9.]+) ms.*/\1/p')

  cpu_avg=$(awk '{s+=$1; n++} END{if(n>0) printf "%.2f", s/n; else print "0.00"}' "$cpu_log")
  rm -f "$cpu_log"

  echo "$t,$throughput,$lat_avg,$lat_p50,$lat_p95,$lat_p99,$cpu_avg" >> "$OUTPUT"

  echo "$run_out"
  echo "[DONE] threads=$t throughput=${throughput} req/s cpu_avg=${cpu_avg}%"
done

echo "[OK] Results written to $OUTPUT"
