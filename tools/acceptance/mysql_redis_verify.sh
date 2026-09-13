#!/usr/bin/env bash
# Re-exec under full bash when invoked via `sh`: bash < 5.1 in POSIX mode
# (e.g. CentOS 7 `sh` -> bash 4.2) disables process substitution used below.
if [ -z "${BASH_VERSION:-}" ] || set -o 2>/dev/null | grep -qE '^posix[[:space:]]+on'; then
    exec bash "$0" "$@"
fi
set -Eeuo pipefail

# ============================================================================
# mysql_redis_verify.sh — gene_web 部署的 MySQL / Redis 专项验收
#
# 针对部署：gene_web 应用根目录 /data/webapp/www/gene_web/
#
# 覆盖 8 个阶段（全部输出 PASS/FAIL 到 status.tsv）：
#   preflight    PHP/扩展/路径检查（gene swoole pdo_mysql redis 必装）
#   mysql-raw    原生 PDO 连接 + SELECT VERSION() / SELECT 1
#   mysql-gene   Gene\Db\Mysql 驱动层 SELECT 1（直连模式，不走池）
#   redis-raw    ext-redis connect/auth/select/ping
#   redis-gene   Gene\Cache\Redis set/get/del/ping 往返
#   mysql-pool   Swoole 协程池压测（200 协程 × 1000 次 get/put）
#   redis-pool   同上，RedisPool
#   tx-hygiene   池归还事务卫生（release 未提交事务 → 回滚+告警）
#
# 用法：
#   1) 编辑下方「部署配置」中的密码/DSN（或同名环境变量注入）
#   2) bash tools/acceptance/mysql_redis_verify.sh            # 仅 DB/Redis 阶段
#      bash tools/acceptance/mysql_redis_verify.sh --full     # 再追加完整门禁
#                                                             # （含 TestRunner、四格矩阵、
#                                                             #  ctx soak、gene_web wrk 压测）
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GENE_REPO="${GENE_REPO:-$(cd "$SCRIPT_DIR/../.." && pwd)}"

# ------------------------- 部署配置（按实际修改） -------------------------
GENE_WEB="${GENE_WEB:-/data/webapp/www/gene_web}"   # gene_web 应用根目录
PHP_BIN="${PHP_BIN:-php}"                            # PHP CLI（须与 gene.so 同版本）
GENE_SO="${GENE_SO:-}"                               # 留空=自动找/编译 src/modules/gene.so
BUILD_GENE="${BUILD_GENE:-1}"

export GENE_MYSQL_DSN="${GENE_MYSQL_DSN:-mysql:dbname=gene_web;host=127.0.0.1;port=3306;charset=utf8mb4}"
export GENE_MYSQL_USER="${GENE_MYSQL_USER:-gene_web}"
export GENE_MYSQL_PASS="${GENE_MYSQL_PASS:-}"        # 必填；留空则运行时交互询问

export GENE_REDIS_HOST="${GENE_REDIS_HOST:-127.0.0.1}"
export GENE_REDIS_PORT="${GENE_REDIS_PORT:-6379}"
export GENE_REDIS_PASS="${GENE_REDIS_PASS:-}"        # 无密码留空
export GENE_REDIS_DB="${GENE_REDIS_DB:-0}"

export GENE_RUN_ENVIRONMENT="${GENE_RUN_ENVIRONMENT:-0}"  # gene_web 配置环境：0=dev（2026-08-25 验收同值）1=test
# ------------------------------------------------------------------------

POOL_MAX="${POOL_MAX:-32}"
POOL_COROUTINES="${POOL_COROUTINES:-200}"
POOL_ITERATIONS="${POOL_ITERATIONS:-1000}"
POOL_TIMEOUT="${POOL_TIMEOUT:-600}"
OUT="${OUT:-/tmp/gene-mysql-redis-verify-$(date +%Y%m%d-%H%M%S)}"
RUN_FULL=0

usage() {
    cat <<'EOF'
Usage: tools/acceptance/mysql_redis_verify.sh [--full] [--output PATH]

  (default)   preflight + mysql/redis 直连与 Gene 层探测 + 池并发 + tx-hygiene
  --full      上述全部跑完后，再 exec linux_swoole_verify.sh --all $GENE_WEB
              （含隔离全测、四组 Swoole 开关矩阵、10 万协程 soak、wrk 压测）

Env overrides: GENE_WEB GENE_REPO GENE_SO PHP_BIN BUILD_GENE
               GENE_MYSQL_DSN/USER/PASS  GENE_REDIS_HOST/PORT/PASS/DB
               GENE_RUN_ENVIRONMENT  POOL_MAX POOL_COROUTINES POOL_ITERATIONS
EOF
}

while (($#)); do
    case "$1" in
        --full)   RUN_FULL=1; shift ;;
        --output) OUT="${2:?--output requires a path}"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 64 ;;
    esac
done

mkdir -p "$OUT"
STATUS_FILE="$OUT/status.tsv"
printf 'stage\tstatus\texit_code\n' >"$STATUS_FILE"
FAILURES=0

log()    { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }
record() { printf '%s\t%s\t%s\n' "$1" "$2" "$3" >>"$STATUS_FILE"; [[ "$2" == FAIL ]] && FAILURES=$((FAILURES+1)); return 0; }

run_logged() {
    local stage="$1" logfile="$2"; shift 2
    log "START $stage"
    set +e
    "$@" 2>&1 | tee "$logfile"
    local code=${PIPESTATUS[0]}
    set -e
    if ((code == 0)); then record "$stage" PASS "$code"; log "PASS  $stage"
    else record "$stage" FAIL "$code"; log "FAIL  $stage (exit=$code, see $logfile)"; fi
    return 0
}

run_timeout() {
    local duration="$1"; shift
    if command -v timeout >/dev/null 2>&1; then timeout "$duration" "$@"
    elif command -v perl >/dev/null 2>&1; then perl -e 'alarm shift; exec @ARGV' "$duration" "$@"
    else echo "Missing timeout/perl" >&2; return 127; fi
}

cpu_count() { getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2; }

# ------------------------- MySQL 密码（必填） -------------------------
if [[ -z "$GENE_MYSQL_PASS" ]]; then
    if [[ -t 0 ]]; then
        read -rsp 'MySQL password: ' GENE_MYSQL_PASS; echo
        export GENE_MYSQL_PASS
    else
        echo "GENE_MYSQL_PASS is required (export it or edit the config block)." >&2
        exit 2
    fi
fi

# ------------------------- gene.so 定位/编译 -------------------------
if [[ -z "$GENE_SO" && -f "$GENE_REPO/src/modules/gene.so" ]]; then
    GENE_SO="$GENE_REPO/src/modules/gene.so"
fi
if [[ -z "$GENE_SO" && "$BUILD_GENE" == "1" && -f "$GENE_REPO/src/config.m4" ]]; then
    log "Building Gene module (phpize)"
    ( cd "$GENE_REPO/src" && phpize && \
      CFLAGS="${CFLAGS:--O2 -g -fno-omit-frame-pointer}" ./configure --enable-gene=shared && \
      make -j"$(cpu_count)" ) 2>&1 | tee "$OUT/build.log"
    GENE_SO="$GENE_REPO/src/modules/gene.so"
fi
if [[ -z "$GENE_SO" || ! -f "$GENE_SO" ]]; then
    echo "gene.so not found. Set GENE_SO or keep BUILD_GENE=1." >&2
    exit 2
fi
GENE_SO="$(cd "$(dirname "$GENE_SO")" && pwd)/$(basename "$GENE_SO")"

# PHP_CMD：-n 干净环境，显式加载依赖扩展 + gene.so（与 linux_swoole_verify.sh 同策略）
EXT_DIR="$(php-config --extension-dir 2>/dev/null || true)"
if [[ -z "$EXT_DIR" || ! -d "$EXT_DIR" ]]; then
    EXT_DIR="$("$PHP_BIN" -i 2>/dev/null | awk -F'=> ' '/^extension_dir/ {print $2; exit}')"
fi
PHP_ARGS=(-n)
for ext in pdo pdo_sqlite pdo_mysql pdo_pgsql curl openssl igbinary msgpack redis swoole; do
    if [[ -n "$EXT_DIR" && -f "$EXT_DIR/$ext.so" ]]; then
        PHP_ARGS+=(-d "extension=$EXT_DIR/$ext.so")
    fi
done
PHP_ARGS+=(-d "extension=$GENE_SO")
PHP_CMD=("$PHP_BIN" "${PHP_ARGS[@]}")

# ------------------------- preflight -------------------------
log "START preflight"
set +e
{
    uname -a; "${PHP_CMD[@]}" -v
    "${PHP_CMD[@]}" -r '
$req = ["gene","swoole","pdo_mysql","redis"];
$bad = 0;
foreach ($req as $e) {
    $ok = extension_loaded($e);
    printf("%-10s %s %s\n", $e, $ok ? "OK" : "MISSING", phpversion($e) ?: "");
    if (!$ok) $bad = 1;
}
exit($bad);'
} >"$OUT/preflight.txt" 2>&1
PF_CODE=$?
set -e
cat "$OUT/preflight.txt"
if ((PF_CODE == 0)); then record preflight PASS 0; log "PASS  preflight"
else record preflight FAIL "$PF_CODE"; log "FAIL  preflight"; exit 2; fi

# ------------------------- mysql-raw：原生 PDO -------------------------
run_logged mysql-raw "$OUT/mysql-raw.log" run_timeout 60 "${PHP_CMD[@]}" -r '
$pdo = new PDO(getenv("GENE_MYSQL_DSN"), getenv("GENE_MYSQL_USER"), getenv("GENE_MYSQL_PASS"),
    [PDO::ATTR_TIMEOUT => 5, PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
printf("server_version=%s\n", $pdo->query("SELECT VERSION()")->fetchColumn());
printf("select_1=%s\n", $pdo->query("SELECT 1")->fetchColumn());
echo "MYSQL RAW CONNECT OK\n";'

# ------------------------- mysql-gene：Gene\Db\Mysql 层 -------------------------
run_logged mysql-gene "$OUT/mysql-gene.log" run_timeout 60 "${PHP_CMD[@]}" -r '
$db = new Gene\Db\Mysql([
    "dsn" => getenv("GENE_MYSQL_DSN"),
    "username" => getenv("GENE_MYSQL_USER"),
    "password" => getenv("GENE_MYSQL_PASS"),
]);
$one = $db->sql("SELECT 1")->cell();
printf("gene_db_select_1=%s\n", var_export($one, true));
exit(((int)$one === 1) ? 0 : 1);'

# ------------------------- redis-raw：ext-redis -------------------------
run_logged redis-raw "$OUT/redis-raw.log" run_timeout 60 "${PHP_CMD[@]}" -r '
$r = new Redis();
if (!$r->connect(getenv("GENE_REDIS_HOST"), (int)getenv("GENE_REDIS_PORT"), 3.0)) {
    fwrite(STDERR, "connect failed\n"); exit(1);
}
$pass = getenv("GENE_REDIS_PASS");
if (is_string($pass) && $pass !== "") { if (!$r->auth($pass)) { fwrite(STDERR,"auth failed\n"); exit(1); } }
$db = (int)getenv("GENE_REDIS_DB");
if ($db > 0) $r->select($db);
printf("ping=%s\n", var_export($r->ping(), true));
echo "REDIS RAW CONNECT OK\n";'

# ------------------------- redis-gene：Gene\Cache\Redis 层 -------------------------
run_logged redis-gene "$OUT/redis-gene.log" run_timeout 60 "${PHP_CMD[@]}" -r '
$params = ["host" => getenv("GENE_REDIS_HOST"), "port" => (int)getenv("GENE_REDIS_PORT"),
           "timeout" => 3.0];
$pass = getenv("GENE_REDIS_PASS");
if (is_string($pass) && $pass !== "") $params["password"] = $pass;
$db = (int)getenv("GENE_REDIS_DB");
if ($db > 0) $params["database"] = $db;
$redis = new Gene\Cache\Redis($params);
$redis->set("gene_probe_key", "gene-ok", 30);
$got = $redis->get("gene_probe_key");
$redis->del("gene_probe_key");
printf("set_get_roundtrip=%s ping=%s\n", var_export($got, true), var_export($redis->ping(), true));
exit(($got === "gene-ok") ? 0 : 1);'

# ------------------------- mysql-pool：协程池并发 -------------------------
run_logged mysql-pool "$OUT/mysql-pool.log" \
    run_timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
    "$GENE_REPO/tools/acceptance/pool_concurrency.php" \
    --pool=db --pool-max="$POOL_MAX" \
    --coroutines="$POOL_COROUTINES" --iterations="$POOL_ITERATIONS"

# ------------------------- redis-pool：协程池并发 -------------------------
run_logged redis-pool "$OUT/redis-pool.log" \
    run_timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
    "$GENE_REPO/tools/acceptance/pool_concurrency.php" \
    --pool=redis --pool-max="$POOL_MAX" \
    --coroutines="$POOL_COROUTINES" --iterations="$POOL_ITERATIONS"

# ------------------------- tx-hygiene：池归还事务卫生 -------------------------
run_logged tx-hygiene "$OUT/tx-leak-pool.log" \
    run_timeout "$POOL_TIMEOUT" "${PHP_CMD[@]}" -d gene.runtime_type=2 \
    "$GENE_REPO/audit/repro/tx_leak_pool.php"

# ------------------------- 汇总 -------------------------
{
    echo "Gene MySQL/Redis verification summary"
    echo "Output: $OUT"
    echo "gene.so: $GENE_SO"
    echo
    column -t -s $'\t' "$STATUS_FILE" 2>/dev/null || cat "$STATUS_FILE"
} | tee "$OUT/summary.txt"

if ((FAILURES > 0)); then
    log "FAILED in $FAILURES stage(s). Results: $OUT"
    exit 1
fi
log "All stages passed. Results: $OUT"

if ((RUN_FULL)); then
    log "Delegating to full gate: linux_swoole_verify.sh --all $GENE_WEB"
    exec bash "$GENE_REPO/tools/acceptance/linux_swoole_verify.sh" --all "$GENE_WEB" --output "$OUT-full"
fi

echo
echo "如需完整门禁（四格矩阵 + 10万协程 soak + gene_web wrk 压测）："
echo "  GENE_MYSQL_PASS='***' bash tools/acceptance/linux_swoole_verify.sh \\"
echo "      --all $GENE_WEB --output /tmp/gene-swoole-result"
exit 0
