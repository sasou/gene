#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GENE_REPO="${GENE_REPO:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
GENE_WEB="${GENE_WEB:-}"
PHP_BIN="${PHP_BIN:-php}"
PHPIZE_BIN="${PHPIZE_BIN:-phpize}"
PHP_CONFIG_BIN="${PHP_CONFIG_BIN:-php-config}"
MAKE_BIN="${MAKE_BIN:-make}"
BUILD_GENE="${BUILD_GENE:-1}"
RUN_REDIS_POOL="${RUN_REDIS_POOL:-0}"
RUN_MYSQL_POOL="${RUN_MYSQL_POOL:-0}"
RUN_WEB="${RUN_WEB:-0}"
CONTEXT_COROUTINES="${CONTEXT_COROUTINES:-100000}"
CONTEXT_CONCURRENCY="${CONTEXT_CONCURRENCY:-500}"
POOL_MAX="${POOL_MAX:-32}"
POOL_COROUTINES="${POOL_COROUTINES:-200}"
POOL_ITERATIONS="${POOL_ITERATIONS:-1000}"
POOL_TIMEOUT="${POOL_TIMEOUT:-600}"
WRK_THREADS="${WRK_THREADS:-8}"
WRK_CONNECTIONS="${WRK_CONNECTIONS:-500}"
WRK_WARMUP_DURATION="${WRK_WARMUP_DURATION:-30s}"
WRK_DURATION="${WRK_DURATION:-2m}"
MATRIX_TIMEOUT="${MATRIX_TIMEOUT:-180}"
RSS_INTERVAL="${RSS_INTERVAL:-10}"
GENE_SWOOLE_HOST="${GENE_SWOOLE_HOST:-127.0.0.1}"
GENE_SWOOLE_PORT="${GENE_SWOOLE_PORT:-9501}"
GENE_SWOOLE_WORKERS="${GENE_SWOOLE_WORKERS:-4}"
GENE_SWOOLE_PID_FILE="${GENE_SWOOLE_PID_FILE:-/tmp/gene-web-swoole.pid}"
GENE_RUN_ENVIRONMENT="${GENE_RUN_ENVIRONMENT:-0}"
WEB_START_TIMEOUT="${WEB_START_TIMEOUT:-120}"
CURL_CONNECT_TIMEOUT="${CURL_CONNECT_TIMEOUT:-5}"
CURL_MAX_TIME="${CURL_MAX_TIME:-30}"
OUT="${OUT:-/tmp/gene-swoole-verify-$(date +%Y%m%d-%H%M%S)}"
GENE_SO="${GENE_SO:-}"
SERVER_PID=""
RSS_PID=""
FAILURES=0
STATUS_FILE=""

usage() {
    cat <<'EOF'
Usage: tools/acceptance/linux_swoole_verify.sh [options]

Options:
  --no-build          Use GENE_SO instead of rebuilding Gene
  --redis             Run Redis pool concurrency verification
  --mysql             Run MySQL pool concurrency verification
  --web PATH          Run gene_web Swoole HTTP and wrk verification
  --all PATH          Run Redis, MySQL, and gene_web verification
  --output PATH       Result directory
  --help              Show this help

Required environment:
  PHP_BIN              PHP CLI binary (default: php)
  PHPIZE_BIN           Matching phpize binary (default: phpize)
  PHP_CONFIG_BIN       Matching php-config binary (default: php-config)
  GENE_SO              Required with --no-build

Redis environment:
  GENE_REDIS_HOST, GENE_REDIS_PORT, GENE_REDIS_PASS, GENE_REDIS_DB

MySQL environment:
  GENE_MYSQL_DSN, GENE_MYSQL_USER, GENE_MYSQL_PASS

Useful tuning:
  CONTEXT_COROUTINES=100000 CONTEXT_CONCURRENCY=500
  POOL_MAX=32 POOL_COROUTINES=200 POOL_ITERATIONS=1000 POOL_TIMEOUT=600
  WRK_DURATION=10m WRK_CONNECTIONS=500 GENE_SWOOLE_WORKERS=4
  MATRIX_TIMEOUT=180
  GENE_RUN_ENVIRONMENT=1 WEB_START_TIMEOUT=120
  CURL_CONNECT_TIMEOUT=5 CURL_MAX_TIME=30
EOF
}

while (($#)); do
    case "$1" in
        --no-build)
            BUILD_GENE=0
            shift
            ;;
        --redis)
            RUN_REDIS_POOL=1
            shift
            ;;
        --mysql)
            RUN_MYSQL_POOL=1
            shift
            ;;
        --web)
            GENE_WEB="${2:?--web requires a path}"
            RUN_WEB=1
            shift 2
            ;;
        --all)
            GENE_WEB="${2:?--all requires a gene_web path}"
            RUN_REDIS_POOL=1
            RUN_MYSQL_POOL=1
            RUN_WEB=1
            shift 2
            ;;
        --output)
            OUT="${2:?--output requires a path}"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 64
            ;;
    esac
done

mkdir -p "$OUT"
STATUS_FILE="$OUT/status.tsv"
printf 'stage\tstatus\texit_code\n' >"$STATUS_FILE"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*"
}

record() {
    local stage="$1" status="$2" code="$3"
    printf '%s\t%s\t%s\n' "$stage" "$status" "$code" >>"$STATUS_FILE"
    if [[ "$status" == FAIL ]]; then
        FAILURES=$((FAILURES + 1))
    fi
}

run_logged() {
    local stage="$1" logfile="$2"
    shift 2
    log "START $stage"
    set +e
    "$@" 2>&1 | tee "$logfile"
    local code=${PIPESTATUS[0]}
    set -e
    if ((code == 0)); then
        record "$stage" PASS "$code"
        log "PASS  $stage"
    else
        record "$stage" FAIL "$code"
        log "FAIL  $stage (exit=$code)"
    fi
    return 0
}

stop_server() {
    if [[ -n "$RSS_PID" ]] && kill -0 "$RSS_PID" 2>/dev/null; then
        kill "$RSS_PID" 2>/dev/null || true
        wait "$RSS_PID" 2>/dev/null || true
    fi
    RSS_PID=""
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    SERVER_PID=""
}

curl_probe() {
    curl -fsS \
        --connect-timeout "$CURL_CONNECT_TIMEOUT" \
        --max-time "$CURL_MAX_TIME" \
        "$@"
}

wait_for_gene_web() {
    local url="$1" deadline=$((SECONDS + WEB_START_TIMEOUT))
    while ((SECONDS < deadline)); do
        if curl_probe "$url" >/dev/null 2>&1; then
            return 0
        fi
        if [[ -n "$SERVER_PID" ]] && ! kill -0 "$SERVER_PID" 2>/dev/null; then
            log "gene-web server exited before ready (see $OUT/gene-web-swoole.log)"
            return 1
        fi
        sleep 1
    done
    log "gene-web not ready after ${WEB_START_TIMEOUT}s (see $OUT/gene-web-swoole.log)"
    return 1
}

finish() {
    local code=$?
    stop_server
    if [[ -d "$OUT" ]]; then
        log "Archiving results to $OUT.tar.gz"
        tar -C "$(dirname "$OUT")" -czf "$OUT.tar.gz" "$(basename "$OUT")" 2>/dev/null || true
    fi
    if ((code != 0)); then
        exit "$code"
    fi
}
trap finish EXIT
trap 'stop_server; exit 130' INT TERM

REQUIRED_COMMANDS=("$PHP_BIN" "$PHP_CONFIG_BIN" timeout)
if ((BUILD_GENE)); then
    REQUIRED_COMMANDS+=("$PHPIZE_BIN" "$MAKE_BIN")
fi
for required_command in "${REQUIRED_COMMANDS[@]}"; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "Missing required command: $required_command" >&2
        exit 2
    fi
done

if [[ ! -d "$GENE_REPO/src" || ! -f "$GENE_REPO/src/config.m4" ]]; then
    echo "Invalid GENE_REPO: $GENE_REPO" >&2
    exit 2
fi

if ((BUILD_GENE)); then
    log "Building Gene release module"
    (
        cd "$GENE_REPO/src"
        "$PHPIZE_BIN" --clean >/dev/null 2>&1 || true
        "$PHPIZE_BIN"
        CFLAGS="${CFLAGS:--O2 -g -fno-omit-frame-pointer}" ./configure --enable-gene=shared
        "$MAKE_BIN" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
    ) 2>&1 | tee "$OUT/build.log"
    GENE_SO="$GENE_REPO/src/modules/gene.so"
fi

if [[ -z "$GENE_SO" || ! -f "$GENE_SO" ]]; then
    echo "Gene module not found. Set GENE_SO or allow the build stage." >&2
    exit 2
fi
GENE_SO="$(cd "$(dirname "$GENE_SO")" && pwd)/$(basename "$GENE_SO")"

EXT_DIR="$("$PHP_CONFIG_BIN" --extension-dir)"
PHP_ARGS=(-n)
for ext in pdo pdo_sqlite pdo_mysql pdo_pgsql curl openssl igbinary msgpack redis swoole; do
    if [[ -f "$EXT_DIR/$ext.so" ]]; then
        PHP_ARGS+=(-d "extension=$EXT_DIR/$ext.so")
    fi
done
PHP_ARGS+=(-d "extension=$GENE_SO")
PHP_CMD=("$PHP_BIN" "${PHP_ARGS[@]}")
printf -v GENE_TEST_PHP_ARGS '%q ' "${PHP_ARGS[@]}"
export GENE_TEST_PHP_ARGS

{
    uname -a
    "$PHP_BIN" -v
    "${PHP_CMD[@]}" -v
    "${PHP_CMD[@]}" -m
    "${PHP_CMD[@]}" --ri gene
    "${PHP_CMD[@]}" --ri swoole
    ldd "$GENE_SO" || true
} >"$OUT/environment.txt" 2>&1

set +e
"${PHP_CMD[@]}" -r '
$required = ["gene", "swoole", "pdo_sqlite", "curl", "openssl"];
$missing = [];
foreach ($required as $extension) {
    $loaded = extension_loaded($extension);
    printf("%-12s %s %s\n", $extension, $loaded ? "OK" : "MISSING", phpversion($extension) ?: "");
    if (!$loaded) $missing[] = $extension;
}
exit($missing ? 2 : 0);
' | tee "$OUT/preflight.txt"
PREFLIGHT_CODE=${PIPESTATUS[0]}
set -e
if ((PREFLIGHT_CODE != 0)); then
    record preflight FAIL "$PREFLIGHT_CODE"
    exit "$PREFLIGHT_CODE"
fi
record preflight PASS 0

if ((RUN_REDIS_POOL || RUN_WEB)); then
    if ! "${PHP_CMD[@]}" -r 'exit(extension_loaded("redis") ? 0 : 2);'; then
        echo "Redis verification requires ext-redis and its dependencies (for example igbinary)." >&2
        record redis-extension FAIL 2
        exit 2
    fi
    record redis-extension PASS 0
fi
if ((RUN_MYSQL_POOL || RUN_WEB)); then
    if ! "${PHP_CMD[@]}" -r 'exit(extension_loaded("pdo_mysql") ? 0 : 2);'; then
        echo "MySQL verification requires pdo_mysql." >&2
        record pdo-mysql-extension FAIL 2
        exit 2
    fi
    record pdo-mysql-extension PASS 0
fi

run_logged full-tests "$OUT/test-runner.log" \
    "${PHP_CMD[@]}" -d gene.runtime_type=1 "$GENE_REPO/test/TestRunner.php"

MATRIX_FAILED=0
for capi in 0 1; do
    for precompile in 0 1; do
        name="capi-${capi}-precompile-${precompile}"
        set +e
        timeout "$MATRIX_TIMEOUT" "${PHP_CMD[@]}" \
            -d gene.runtime_type=2 \
            -d gene.swoole_getcid_capi="$capi" \
            -d gene.route_precompile="$precompile" \
            "$GENE_REPO/tools/verify_5_6_6_swoole.php" \
            2>&1 | tee "$OUT/$name.log"
        code=${PIPESTATUS[0]}
        set -e
        if ((code != 0)); then
            MATRIX_FAILED=1
        fi
    done
done

grep -hE 'ALL-PASS|RESULT-DIGEST|RUN-CONFIG|req/s|FAILED' \
    "$OUT"/capi-*-precompile-*.log >"$OUT/swoole-matrix-summary.txt" || true
mapfile -t DIGESTS < <(grep -hEo 'RESULT-DIGEST=[a-f0-9]+' "$OUT"/capi-*-precompile-*.log | cut -d= -f2)
UNIQUE_DIGESTS="$(printf '%s\n' "${DIGESTS[@]:-}" | sed '/^$/d' | sort -u | wc -l)"
ALL_PASS_COUNT="$( { grep -h -c 'ALL-PASS' "$OUT"/capi-*-precompile-*.log || true; } | awk '{s+=$1} END {print s+0}')"
if ((MATRIX_FAILED == 0)) && ((${#DIGESTS[@]} == 4)) && ((UNIQUE_DIGESTS == 1)) && ((ALL_PASS_COUNT == 4)); then
    record swoole-matrix PASS 0
else
    record swoole-matrix FAIL 1
fi

run_logged context-manual "$OUT/context-manual.json" \
    "${PHP_CMD[@]}" \
    -d gene.runtime_type=2 \
    -d gene.co_contexts_max=4096 \
    -d gene.ctx_pool_max=512 \
    -d gene.swoole_auto_cleanup=0 \
    "$GENE_REPO/tools/acceptance/swoole_context_soak.php" \
    --coroutines="$CONTEXT_COROUTINES" \
    --concurrency="$CONTEXT_CONCURRENCY" \
    --omit-cleanup-rate=0

run_logged context-auto "$OUT/context-auto.json" \
    "${PHP_CMD[@]}" \
    -d gene.runtime_type=2 \
    -d gene.co_contexts_max=4096 \
    -d gene.ctx_pool_max=512 \
    -d gene.swoole_auto_cleanup=1 \
    "$GENE_REPO/tools/acceptance/swoole_context_soak.php" \
    --coroutines="$CONTEXT_COROUTINES" \
    --concurrency="$CONTEXT_CONCURRENCY" \
    --omit-cleanup-rate=1

if ((RUN_REDIS_POOL)); then
    run_logged redis-pool "$OUT/redis-pool.json" \
        timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
        "$GENE_REPO/tools/acceptance/pool_concurrency.php" \
        --pool=redis --pool-max="$POOL_MAX" \
        --coroutines="$POOL_COROUTINES" --iterations="$POOL_ITERATIONS"
else
    record redis-pool SKIP 0
fi

if ((RUN_MYSQL_POOL)); then
    run_logged mysql-pool "$OUT/mysql-pool.json" \
        timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
        "$GENE_REPO/tools/acceptance/pool_concurrency.php" \
        --pool=db --pool-max="$POOL_MAX" \
        --coroutines="$POOL_COROUTINES" --iterations="$POOL_ITERATIONS"
    run_logged tx-hygiene "$OUT/tx-leak-pool.log" \
        timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
        "$GENE_REPO/audit/repro/tx_leak_pool.php"
else
    record mysql-pool SKIP 0
    record tx-hygiene SKIP 0
fi

if ((RUN_WEB)); then
    log "START gene-web"
    if [[ ! -d "$GENE_WEB/public" || ! -f "$GENE_WEB/public/swoole.php" ]]; then
        record gene-web FAIL 2
        log "FAIL  gene-web (invalid GENE_WEB path)"
    elif ! command -v curl >/dev/null 2>&1 || ! command -v wrk >/dev/null 2>&1; then
        echo "gene_web verification requires curl and wrk" >&2
        record gene-web FAIL 2
        log "FAIL  gene-web (missing curl or wrk)"
    else
        GENE_WEB="$(cd "$GENE_WEB" && pwd)"
        export GENE_SWOOLE_HOST GENE_SWOOLE_PORT GENE_SWOOLE_WORKERS GENE_SWOOLE_PID_FILE
        export GENE_MONITOR_TOKEN="${GENE_MONITOR_TOKEN:-$(openssl rand -hex 24 2>/dev/null || date +%s%N)}"
        log "gene-web launching on 127.0.0.1:$GENE_SWOOLE_PORT (run_environment=$GENE_RUN_ENVIRONMENT workers=$GENE_SWOOLE_WORKERS)"
        (
            cd "$GENE_WEB"
            exec "${PHP_CMD[@]}" \
                -d gene.runtime_type=2 \
                -d gene.run_environment="$GENE_RUN_ENVIRONMENT" \
                -d gene.co_contexts_max=16384 \
                -d gene.ctx_pool_max=512 \
                -d gene.ctx_pool_prewarm=512 \
                -d gene.cache_max_items=10000 \
                -d gene.cache_reserve=16384 \
                -d gene.swoole_auto_cleanup=1 \
                public/swoole.php
        ) >"$OUT/gene-web-swoole.log" 2>&1 &
        SERVER_PID=$!
        echo "$SERVER_PID" >"$OUT/gene-web-server.pid"

        WEB_FAILED=0
        HEALTH_URL="http://127.0.0.1:$GENE_SWOOLE_PORT/healthz"
        METRICS_URL="http://127.0.0.1:$GENE_SWOOLE_PORT/metrics"
        if ! wait_for_gene_web "$HEALTH_URL"; then
            WEB_FAILED=1
        fi

        if ((WEB_FAILED == 0)); then
            curl_probe "$HEALTH_URL" >"$OUT/health-before.json" || WEB_FAILED=1
            curl_probe "$METRICS_URL" >"$OUT/metrics-before.txt" || WEB_FAILED=1
        fi

        if ((WEB_FAILED == 0)); then
            log "gene-web wrk warmup ($WRK_WARMUP_DURATION)"
            wrk -t"$WRK_THREADS" -c"$WRK_CONNECTIONS" -d"$WRK_WARMUP_DURATION" --latency \
                "$HEALTH_URL" >"$OUT/wrk-warmup.txt" 2>&1 || WEB_FAILED=1
        fi

        if ((WEB_FAILED == 0)); then
            (
                while kill -0 "$SERVER_PID" 2>/dev/null; do
                    date '+%F %T'
                    ps -C "$(basename "$PHP_BIN")" -o pid,ppid,rss,vsz,%cpu,%mem,etime,cmd --sort=pid || true
                    sleep "$RSS_INTERVAL"
                done
            ) >"$OUT/process-rss.txt" 2>&1 &
            RSS_PID=$!
            log "gene-web wrk load test ($WRK_DURATION)"
            wrk -t"$WRK_THREADS" -c"$WRK_CONNECTIONS" -d"$WRK_DURATION" --latency \
                "$HEALTH_URL" >"$OUT/wrk-health.txt" 2>&1 || WEB_FAILED=1
            kill "$RSS_PID" 2>/dev/null || true
            wait "$RSS_PID" 2>/dev/null || true
            RSS_PID=""
            curl_probe "$HEALTH_URL" >"$OUT/health-after.json" || WEB_FAILED=1
            curl_probe "$METRICS_URL" >"$OUT/metrics-after.txt" || WEB_FAILED=1
        fi

        stop_server
        if ((WEB_FAILED == 0)); then
            record gene-web PASS 0
            log "PASS  gene-web"
        else
            record gene-web FAIL 1
            log "FAIL  gene-web (see $OUT/gene-web-swoole.log and health/metrics artifacts)"
        fi
    fi
else
    record gene-web SKIP 0
    log "SKIP  gene-web"
fi

{
    echo "Gene Swoole verification summary"
    echo "Output: $OUT"
    echo "Gene module: $GENE_SO"
    echo
    column -t -s $'\t' "$STATUS_FILE" 2>/dev/null || cat "$STATUS_FILE"
    echo
    cat "$OUT/swoole-matrix-summary.txt" 2>/dev/null || true
} | tee "$OUT/summary.txt"

if ((FAILURES > 0)); then
    log "Verification failed in $FAILURES stage(s). Results: $OUT"
    exit 1
fi
log "All enabled verification stages passed. Results: $OUT"
exit 0
