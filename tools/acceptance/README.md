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

## ORM（Gene\Orm）

`test/OrmTest.php` 已纳入 `TestRunner.php`（functional 默认命令会跑到）。
在 Linux 编译安装含 `src/orm/` 的扩展后，验收应看到 ORM class surface + SQLite CRUD 用例通过。

Swoole 长跑 / 池压测仍用既有 `swoole_context_soak.php`、`pool_concurrency.php`：
ORM 不额外持有连接，仅要求 `db.instance=true` + Pool，并在请求 `cleanup()`。

## Linux Swoole 一键验证

基础验证会自动构建 Gene，并执行隔离全测、四组 Swoole 开关矩阵、手动/自动 Context cleanup soak：

```bash
bash tools/acceptance/linux_swoole_verify.sh
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

脚本返回非零即表示至少一个启用阶段失败；输出目录同时生成 `status.tsv`、`summary.txt` 与同名 `.tar.gz` 归档。

`tx-hygiene` 之后若使用 `--all` / `--web`，会进入 **gene-web** 阶段（wrk 压测默认约 2.5 分钟，此前脚本无进度日志，易被误判为卡住）。若 `gene_web` 的 MySQL/Redis 不可达，`/healthz` 会在 `waitWorkerReady()` 上阻塞；请查看输出目录中的 `gene-web-swoole.log`，并视环境设置 `GENE_RUN_ENVIRONMENT=0|1`（默认已改为 `1` 即 test 配置）。
