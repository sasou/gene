# Gene 验收工具

常规 acceptance 脚本只消费人工准备好的 PHP、Gene、FPM/Swoole、数据库与 Redis 环境；`linux_swoole_verify.sh` 默认会编译 Gene，但不会安装依赖或修改服务配置。

```bash
php tools/acceptance/run_acceptance.php \
  --profile=fpm \
  --config=tools/acceptance/config/fpm.example.json \
  --output=audit/results/fpm-<run-id>
```

Swoole profile 会执行 `gene.swoole_getcid_capi` 与
`gene.route_precompile` 的四组开关矩阵，并要求 `RESULT-DIGEST` 一致。
配置内不应保存密码；连接凭据由已准备的服务环境注入。

路由预编译灰度流程：

1. 先在目标 Linux 环境执行 Swoole profile，并保存四组结果。
2. 仅将 `gene.route_precompile=1` 配置到约 5% worker 的独立实例组。
3. 连续观察 24 小时的错误率、p99、RSS、`Memory::stats()` 中的
   `route_pc_items` 与 `co_contexts_watermark`。
4. 出现 crash、digest 不一致、UAF/OOB，或 p99/CPU 每请求退化超过 3%
   时，关闭该 INI 开关并保留输出目录作为回归证据。

## 版本回归验证脚本

- `verify_5_6_6.php`（FPM/CLI，Windows 可跑）：5.6.6 内存与高并发审计落地项——`swoole_getcid_capi` / `cache_max_items` / `route_precompile` / `closure_src_cache_max` INI 注册、`Memory::stats()` 字段、业务缓存上限 + 近似 LRU 淘汰、`processCached` 多轮稳定性。
  `php tools/acceptance/verify_5_6_6.php`；`php -d gene.cache_max_items=10 tools/acceptance/verify_5_6_6.php` 复验淘汰行为。
- `verify_5_6_6_swoole.php`（仅 Linux + Swoole）：脚本自起 Swoole HTTP Server，注册闭包/MCA 直派/动态/带钩子/404 五类路由，`workerReady()` 后协程高并发自打流量逐条校验。四组 `capi × precompile` 开关矩阵要求输出 `ALL-PASS` 且 `RESULT-DIGEST` 完全一致。
  其中 404 用例注册 `->error(404, ...)`（整数事件名，注册为 `error:404`），期望响应体恰为 `R:404`；若返回 "Unknown Url" 警告即整数事件名未生效。
- 两者已被 `config/*.example.json` 的 `functional_commands` / `swoole_verify_script` 引用，随 `run_acceptance.php` 一起执行；`linux_swoole_verify.sh` 的 `swoole-matrix` 阶段也直接驱动后者。

## ORM（Gene\Orm）

`test/OrmTest.php` 已纳入 `TestRunner.php`（functional 默认命令会跑到）。
在 Linux 编译安装含 `src/orm/` 的扩展后，验收应看到 ORM class surface + SQLite CRUD 用例通过。

Swoole 长跑 / 池压测仍用既有 `swoole_context_soak.php`、`pool_concurrency.php`：
ORM 不额外持有连接，仅要求 `db.instance=true` + Pool，并在请求 `cleanup()`。

## MySQL / Redis 专项验证（gene_web 部署）

`mysql_redis_verify.sh` — 针对 `/data/webapp/www/gene_web/` 部署的 DB/Redis 快速验收，
头部「部署配置」段集中填写 DSN/账号/Redis 地址（同名环境变量可覆盖，密码留空则交互询问）：

```bash
# 前置：MySQL 密码必填（GENE_MYSQL_PASS），Redis 有密码时给 GENE_REDIS_PASS。
export GENE_MYSQL_PASS='***'
export GENE_REDIS_PASS='***'        # 无密码可省
bash tools/acceptance/mysql_redis_verify.sh          # preflight + 直连/Gene层探测 + 池并发 + tx-hygiene
bash tools/acceptance/mysql_redis_verify.sh --full   # 追加 exec linux_swoole_verify.sh --all
```

阶段：`mysql-raw`（PDO）→ `mysql-gene`（`Gene\Db\Mysql`）→ `redis-raw`（ext-redis）→
`redis-gene`（`Gene\Cache\Redis` 往返）→ `mysql-pool` / `redis-pool`（`pool_concurrency.php`
200 协程 × 1000 迭代，**每次借用执行一条真实命令**：redis `PING` / db `SELECT 1`，
失败计入 `commandFailures` 并走 `remove()` 弃连接而非放回池）→ `tx-hygiene`
（`audit/repro/tx_leak_pool.php`）。各阶段输出落 `$OUT/status.tsv` 与 `summary.txt`。

## Linux Swoole 一键验证

在 **Linux** 上构建并跑隔离全测、四组 Swoole 开关矩阵、手动/自动 Context cleanup soak、入口适配验证。发布验收以 Linux 为准。

`linux_swoole_verify.sh` 的入口验证阶段（`tools/acceptance/swoole_entry_verify.php`）：

- `entry-matrix`：`handleSwoole` 入口跑 `swoole_getcid_capi × route_precompile` 四格，digest 一致 + ALL-PASS×4；
- `entry-soak`：`--soak=100000` 次真实 HTTP 请求后经 `handleSwoole` 收口，断言 `co_contexts_items=0`、`ctx_pool` 不超限；
- `entry-bench-{manual,init,handle}`：九参手写 / `initSwoole`+手动 / `handleSwoole` 三入口各打空控制器、echo、JSON、DI 四类路径，输出吞吐/p50/p99/RSS；`entry-bench-equiv` 要求三入口 `RESULT-DIGEST` 完全一致（语义等价证明）。`RUN_ENTRY_BENCH=0` 可跳过。

单独运行：

```bash
php tools/acceptance/swoole_entry_verify.php --entry=handle
php tools/acceptance/swoole_entry_verify.php --entry=handle --soak=100000
for e in manual init handle; do php tools/acceptance/swoole_entry_verify.php --entry=$e --bench; done
```

`GENE_MYSQL_DSN`/`GENE_REDIS_HOST` 存在时脚本会经 `pools()+startPools()` 真实建池，`workerExit`/`workerStop` 对应 `stopPoolTimers()`/`closePools()`，顺带覆盖编排 API 的真实生命周期路径。

macOS 只需编出扩展：`tools/mac_build.sh`（见仓库根 `AGENTS.md`）。

```bash
bash tools/acceptance/linux_swoole_verify.sh
# `sh tools/acceptance/linux_swoole_verify.sh` 亦可：脚本顶部含 sh→bash 重引导
# （CentOS7 的 sh 是 bash4.2 POSIX 模式，禁用 <(...) 进程替换，会在 mapfile 行报语法错误）。
```

带 Redis、MySQL 和 gene_web HTTP 压测：

```bash
export GENE_REDIS_HOST=127.0.0.1 GENE_REDIS_PORT=6379
export GENE_MYSQL_DSN='mysql:dbname=gene_test;host=127.0.0.1;port=3306;charset=utf8mb4'
export GENE_MYSQL_USER=gene_test
read -rsp 'MySQL password: ' GENE_MYSQL_PASS; echo; export GENE_MYSQL_PASS

WRK_DURATION=10m bash tools/acceptance/linux_swoole_verify.sh \
  --all /path/to/gene_web \
  --output /tmp/gene-swoole-result
```

使用已有模块时传入 `GENE_SO`：

```bash
GENE_SO=/path/to/gene.so bash tools/acceptance/linux_swoole_verify.sh --no-build
```

已验证部署（gene_web @ `/data/webapp/www/gene_web/`，CentOS 7 + PHP 8.1 + Swoole 6）：

```bash
cd /data/src/gene
export GENE_REDIS_HOST=127.0.0.1 GENE_REDIS_PORT=6379 GENE_REDIS_PASS='***'
export GENE_MYSQL_DSN='mysql:dbname=gene_web;host=127.0.0.1;port=3306;charset=utf8mb4'
export GENE_MYSQL_USER='gene_web' GENE_MYSQL_PASS='***'
GENE_RUN_ENVIRONMENT=0 WRK_DURATION=2m \
  bash tools/acceptance/linux_swoole_verify.sh \
      --all /data/webapp/www/gene_web --output /tmp/gene-swoole-result-$(date +%Y%m%d)
# 前置：curl + wrk 在 PATH；gene_web 需暴露 /healthz 与 /metrics，
# 否则 wait_for_gene_web 阻塞 WEB_START_TIMEOUT(默认120s) 后判 FAIL。
```

结果判读：`status.tsv` 全 PASS；`RESULT-DIGEST` 在 swoole-matrix 四格、entry-matrix 四格、
entry-bench 三入口内各自一致即"优化开关不改语义"。`entry-bench` 各路径 req/s 是客户端短连接
微基准，偶发 10s 级超时是 ephemeral 端口/TIME_WAIT 耗尽噪声（soak 阶段为此用 keep-alive），
非判据。`process-rss.txt` 中 worker PID 高频轮换是应用 `max_request` 正常回收，看 RSS 是否
单调增长判断泄漏；`gene_requests_total` 为 per-worker 计数，随回收清零，与 wrk 总量不可直接比对。

脚本返回非零即表示至少一个启用阶段失败；输出目录同时生成 `status.tsv`、`summary.txt` 与同名 `.tar.gz` 归档。

`tx-hygiene` 之后若使用 `--all` / `--web`，会进入 **gene-web** 阶段（wrk 压测默认约 2.5 分钟；脚本会打 `START gene-web` 与 wrk 进度日志）。若 `gene_web` 的 MySQL/Redis 不可达，`/healthz` 会在 `waitWorkerReady()` 上阻塞；请查看输出目录中的 `gene-web-swoole.log`，并视环境设置 `GENE_RUN_ENVIRONMENT=0|1`（默认 `1` 即 test 配置）。

## 验收记录

| 日期 | 环境 | 结果 | 证据 |
|------|------|------|------|
| 2026-08-25 | Linux 192.168.27.101，PHP 8.1.34，MySQL + Redis + gene_web | **12/12 PASS** | `gene-swoole-verify-20260825-195941`；`RESULT-DIGEST=b887e533c417447e`；`tx-hygiene` → `POOL TX HYGIENE OK`；gene-web wrk 5816 req/s、0 错误。计划文档回写见 `plan/orm-v2.closed.md` §十六。 |
| 2026-09-13 | 同上，PHP 8.1.34 NTS DEBUG，gene 6.2.3 + swoole 6.1.9，MariaDB 10.5.9 | **17/17 PASS** | `gene-swoole-result-20260913`；`swoole-matrix` digest `856ba31839fa8675`（9/12 起 404 修复改响应，与旧值不同属预期）；entry 三入口 digest `fd1425a4658c643a` 一致；entry-soak 10 万请求 24895 req/s、`co_contexts_items=0`；gene-web wrk **19849 req/s**（238 万请求，0 错误，p50 23ms/p99 121ms），RSS 平稳无泄漏。 |
