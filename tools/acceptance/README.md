# Gene 验收工具

这些脚本只消费人工准备好的 PHP、Gene、FPM/Swoole、数据库与 Redis 环境；不会编译扩展、安装依赖或修改服务配置。

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
