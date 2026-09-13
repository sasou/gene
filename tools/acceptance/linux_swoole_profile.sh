#!/usr/bin/env bash
# Re-exec under full bash when invoked via `sh`: POSIX-mode bash (<5.1) and
# non-bash shells (dash/ash) lack features this script relies on.
if [ -z "${BASH_VERSION:-}" ] || set -o 2>/dev/null | grep -qE '^posix[[:space:]]+on'; then
    exec bash "$0" "$@"
fi
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GENE_REPO="${GENE_REPO:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
PHP_BIN="${PHP_BIN:-php}"
WORKER_PID="${WORKER_PID:-}"
ROUTE_URL="${ROUTE_URL:-}"
DB_URL="${DB_URL:-}"
PERF_FREQUENCY="${PERF_FREQUENCY:-999}"
PROFILE_DURATION="${PROFILE_DURATION:-60}"
WARMUP_DURATION="${WARMUP_DURATION:-15s}"
WRK_THREADS="${WRK_THREADS:-4}"
WRK_CONNECTIONS="${WRK_CONNECTIONS:-64}"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-}"
OUT="${OUT:-/tmp/gene-swoole-profile-$(date +%Y%m%d-%H%M%S)}"

usage() {
    cat <<'EOF'
Usage: tools/acceptance/linux_swoole_profile.sh [options]

Required:
  --worker-pid PID      One Swoole worker PID, not the manager PID
  --route-url URL       Representative route/view request without DB
  --db-url URL          Representative DB + ORM + view request

Options:
  --output PATH         Result directory
  --duration SECONDS    perf sampling duration (default: 60)
  --connections N       wrk connections (default: 64)
  --threads N           wrk threads (default: 4)
  --flamegraph PATH     FlameGraph checkout containing stackcollapse-perf.pl
  --help                Show this help

The target worker must already be serving both URLs with production settings:
run_environment>=2, view_compile=1, view_compile_check_mtime=1, OPcache CLI
and realpath cache fixed according to plan/Performance-tuning-V1.closed.md §7.2.
EOF
}

while (($#)); do
    case "$1" in
        --worker-pid) WORKER_PID="${2:?--worker-pid requires a PID}"; shift 2 ;;
        --route-url) ROUTE_URL="${2:?--route-url requires a URL}"; shift 2 ;;
        --db-url) DB_URL="${2:?--db-url requires a URL}"; shift 2 ;;
        --output) OUT="${2:?--output requires a path}"; shift 2 ;;
        --duration) PROFILE_DURATION="${2:?--duration requires seconds}"; shift 2 ;;
        --connections) WRK_CONNECTIONS="${2:?--connections requires a number}"; shift 2 ;;
        --threads) WRK_THREADS="${2:?--threads requires a number}"; shift 2 ;;
        --flamegraph) FLAMEGRAPH_DIR="${2:?--flamegraph requires a path}"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 64 ;;
    esac
done

for command in perf wrk curl tar awk sed sort head ps grep tr readlink; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Missing required command: $command" >&2
        exit 2
    fi
done
if [[ "$(uname -s)" != Linux ]]; then
    echo "This profiling script requires Linux perf." >&2
    exit 2
fi
if [[ -z "$WORKER_PID" || -z "$ROUTE_URL" || -z "$DB_URL" ]]; then
    echo "Missing required options: --worker-pid, --route-url and --db-url." >&2
    usage >&2
    exit 64
fi
if [[ ! "$WORKER_PID" =~ ^[1-9][0-9]*$ ]] || ! kill -0 "$WORKER_PID" 2>/dev/null; then
    echo "Invalid or inaccessible WORKER_PID: $WORKER_PID" >&2
    exit 2
fi
if [[ -z "$ROUTE_URL" || -z "$DB_URL" || "$ROUTE_URL" == "$DB_URL" ]]; then
    echo "ROUTE_URL and DB_URL are required and must be different." >&2
    exit 2
fi
if [[ ! "$PROFILE_DURATION" =~ ^[1-9][0-9]*$ ]]; then
    echo "PROFILE_DURATION must be a positive integer." >&2
    exit 2
fi
# Validate the FlameGraph checkout before any sampling: the previous lazy check
# inside profile_case() ran only after a full warmup+perf cycle, wasting a whole
# scenario when the path was wrong.
if [[ -n "$FLAMEGRAPH_DIR" ]] && { [[ ! -x "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]] || [[ ! -x "$FLAMEGRAPH_DIR/flamegraph.pl" ]]; }; then
    echo "Invalid FLAMEGRAPH_DIR: $FLAMEGRAPH_DIR (need executable stackcollapse-perf.pl and flamegraph.pl)" >&2
    exit 2
fi

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

{
    date --iso-8601=seconds
    uname -a
    "$PHP_BIN" -v
    "$PHP_BIN" --ri gene
    "$PHP_BIN" --ri swoole
    "$PHP_BIN" -i | grep -E '^(opcache\.(enable|enable_cli|memory_consumption|interned_strings_buffer|max_accelerated_files|validate_timestamps|save_comments|jit|jit_buffer_size)|realpath_cache_(size|ttl)|gene\.(runtime_type|run_environment|view_compile|view_compile_check_mtime|route_precompile|swoole_getcid_capi|swoole_auto_cleanup)) =>'
    perf --version
    ps -p "$WORKER_PID" -o pid,ppid,lstart,etime,%cpu,%mem,rss,vsz,cmd
    printf 'worker_exe=%s\n' "$(readlink -f "/proc/$WORKER_PID/exe")"
    printf 'worker_cmdline='; tr '\0' ' ' <"/proc/$WORKER_PID/cmdline"; echo
    printf 'git_commit='; git -C "$GENE_REPO" rev-parse HEAD 2>/dev/null || echo unavailable
    printf 'route_url=%s\ndb_url=%s\n' "$ROUTE_URL" "$DB_URL"
} >"$OUT/environment.txt" 2>&1

profile_case() {
    local name="$1" url="$2" dir="$OUT/$1"
    mkdir -p "$dir"
    curl -fsS --connect-timeout 5 --max-time 30 "$url" >"$dir/probe-response.txt"
    wrk -t"$WRK_THREADS" -c"$WRK_CONNECTIONS" -d"$WARMUP_DURATION" --latency "$url" >"$dir/wrk-warmup.txt" 2>&1
    if ! grep -qE '[1-9][0-9]* requests in' "$dir/wrk-warmup.txt"; then
        echo "profile_case($name): wrk warmup got 0 responses for $url (see $dir/wrk-warmup.txt)" >&2
        exit 2
    fi
    perf record -F "$PERF_FREQUENCY" -g -p "$WORKER_PID" -o "$dir/perf.data" -- sleep "$PROFILE_DURATION" >"$dir/perf-record.txt" 2>&1 &
    local perf_pid=$!
    sleep 1
    wrk -t"$WRK_THREADS" -c"$WRK_CONNECTIONS" -d"${PROFILE_DURATION}s" --latency "$url" >"$dir/wrk-profile.txt" 2>&1
    wait "$perf_pid"
    perf report -i "$dir/perf.data" --stdio --no-children --sort=dso --percent-limit 0 >"$dir/perf-dso.txt" 2>&1
    perf report -i "$dir/perf.data" --stdio --no-children --sort=symbol --percent-limit 0 >"$dir/perf-symbols.txt" 2>&1
    awk '/^[[:space:]]*[0-9]+\.[0-9]+%/ { pct=$1; gsub(/%/, "", pct); symbol=$0; sub(/^[[:space:]]*[0-9]+\.[0-9]+%[[:space:]]+/, "", symbol); print pct "\t" symbol }' "$dir/perf-symbols.txt" \
        | sort -nr >"$dir/symbols-sorted.tsv"
    head -20 "$dir/symbols-sorted.tsv" >"$dir/top-20.tsv"
    awk '/^[[:space:]]*[0-9]+\.[0-9]+%/ && /gene\.so/ { pct=$1; gsub(/%/, "", pct); sum += pct } END { printf "%.2f\n", sum + 0 }' "$dir/perf-dso.txt" >"$dir/gene-so-self-percent.txt"
    perf script -i "$dir/perf.data" >"$dir/perf.script"
    # Guard against sampling an idle or wrong worker (e.g. a stale server still
    # owning the pid file, or a PID that never serves $url): without samples the
    # FlameGraph stage would fail later with cryptic "Stack count is low" errors.
    local samples
    samples=$(grep -cE '^[[:space:]]*[^#[:space:]].*[0-9]+\.[0-9]+:[[:space:]]+[0-9]+' "$dir/perf.script" || true)
    if ((samples < 50)); then
        {
            echo "profile_case($name): only $samples perf samples captured on worker $WORKER_PID."
            echo "The worker was probably idle or this is the wrong PID — confirm it is a leaf"
            echo "worker of the server actually listening on $url (check $dir/wrk-profile.txt;"
            echo "stale servers: pgrep -fa 'swoole.php')."
        } >&2
        exit 2
    fi
    if [[ -n "$FLAMEGRAPH_DIR" ]]; then
        "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" "$dir/perf.script" >"$dir/perf.folded"
        "$FLAMEGRAPH_DIR/flamegraph.pl" "$dir/perf.folded" >"$dir/flamegraph.svg"
    fi
}

profile_case route-view "$ROUTE_URL"
profile_case db-orm-view "$DB_URL"

{
    echo -e 'scenario\tgene_so_self_percent'
    printf 'route-view\t%s\n' "$(cat "$OUT/route-view/gene-so-self-percent.txt")"
    printf 'db-orm-view\t%s\n' "$(cat "$OUT/db-orm-view/gene-so-self-percent.txt")"
    echo
    echo 'Top-20 files: route-view/top-20.tsv, db-orm-view/top-20.tsv'
    if [[ -n "$FLAMEGRAPH_DIR" ]]; then
        echo 'Flamegraphs: route-view/flamegraph.svg, db-orm-view/flamegraph.svg'
    else
        echo 'Flamegraphs not rendered; perf.script is included. Re-run with --flamegraph PATH or render offline.'
    fi
} | tee "$OUT/summary.txt"

tar -C "$(dirname "$OUT")" -czf "$OUT.tar.gz" "$(basename "$OUT")"
echo "Result archive: $OUT.tar.gz"
