# Gene 扩展配置参考

本文整理 gene 扩展的全部 INI 配置项、含义与推荐最佳值。

所有配置项均为 `PHP_INI_SYSTEM` 级别（只能在 `php.ini` 中设置，运行期不可修改）。
源码定义见 `src/gene.c` 的 `PHP_INI_BEGIN()` 段。

## 配置项总览

| 配置项 | 默认 | 取值含义 | 作用 |
|--------|------|----------|------|
| `gene.run_environment` | `1` | `0`=dev / `1`=test / `2`=prod | 仅 dev(0) 记录 DB benchmark，prod 跳过开销 |
| `gene.runtime_type` | `1` | `<2`=FPM/CLI；`>=2`=Swoole(2)/协程(3) | 决定整个运行时分流（最关键开关） |
| `gene.use_namespace` | `1` | bool | 命名空间风格加载 / 回调解析 |
| `gene.view_compile` | `0` | bool | 模板编译缓存 |
| `gene.view_compile_check_mtime` | `0` | bool | 编译模板按 mtime 失效校验 |
| `gene.use_library` | `0` | bool | 启用 library 自动加载回退 |
| `gene.library_root` | `""` | path | library 根目录（配合 `use_library`） |
| `gene.co_contexts_max` | `1024` | long | 协程上下文软上限，超过触发 sweep |
| `gene.ctx_pool_max` | `256` | long | ctx 结构体池容量 |
| `gene.ctx_pool_prewarm` | `0` | long | RINIT 自动预热数量（仅 Swoole） |
| `gene.swoole_getcid_capi` | `1` | bool | 用 Swoole C-API 直接取协程 id（更快） |
| `gene.cache_max_items` | `0` | long | 业务缓存分区上限（0=不限，>0 启用 LRU 淘汰） |
| `gene.route_precompile` | `0` | bool | 路由预编译派发缓存（仅 Swoole，opt-in） |
| `gene.closure_src_cache_max` | `1024` | long | FPM 闭包源码缓存容量；`<=0` 关闭缓存 |
| `gene.swoole_auto_cleanup` | `0` | bool | 协程 ctx 随协程结束自动归还（仅 Swoole，opt-in） |
| `gene.cache_easy_ttl` | `0` | long | cache_easy 文件表 TTL 兜底秒数（0=禁用，惰性过期） |
| `gene.slow_query_ms` | `0` | long | 慢查询阈值（毫秒，0=禁用）；超限 SQL 计入 `Monitor::stats()` 的 `db_slow_query_count` |

## 推荐最佳配置

### 场景一：FPM / php-cgi（生产）

```ini
extension=gene.so
gene.run_environment=2           ; 生产环境，跳过 DB benchmark 开销
gene.runtime_type=1              ; FPM 模式
gene.use_namespace=1
gene.view_compile=1              ; 启用模板编译缓存
gene.view_compile_check_mtime=0  ; 生产关闭 mtime 校验；开发期改 1
```

> FPM 下连接池不生效（每请求新建 PDO），`co_contexts_*` / `ctx_pool_*` /
> `route_precompile` 等 Swoole 专属项无需设置。

### 场景二：Swoole / 协程（生产，推荐）

```ini
extension=gene.so
gene.run_environment=2           ; 生产环境
gene.runtime_type=2              ; Swoole 模式（纯协程用 3）
gene.use_namespace=1
gene.swoole_getcid_capi=1        ; C-API 直取协程 id（默认开，保持）

; —— 协程 / 上下文池（按 worker 并发上限调整） ——
gene.ctx_pool_max=512            ; 池容量，按单 worker 并发协程峰值选
gene.ctx_pool_prewarm=512        ; 冷启动预热满，首波流量零 emalloc
gene.co_contexts_max=16384       ; 协程上下文软上限

; —— 性能增强（opt-in，建议压测验证后开启） ——
gene.route_precompile=1          ; 路由预编译派发，消除每请求 hash 查找
gene.cache_max_items=10000       ; 业务缓存 LRU 上限，防长跑内存膨胀

; —— 模板 ——
gene.view_compile=1
gene.view_compile_check_mtime=0
```

配套启动脚本（`workerReady()` 会自动预热池并启用 lock-skip）：

```php
$server->on('WorkerStart', function () {
    \Gene\Application::getInstance()->workerReady();
});
```

## 关键调优建议

- **`runtime_type`** 是核心开关：`<2` 走 FPM 零开销静态 ctx 路径；`>=2` 才启用
  协程池 / 连接池。可用 `Application::setRuntimeType()` 在首个请求前覆盖。
- **`ctx_pool_max` / `ctx_pool_prewarm`**：按单 worker 实际并发协程峰值设置；
  prewarm 设成与 max 相等可消除冷启动 `emalloc` 抖动（`workerReady()` 已会自动
  prewarm，prewarm 项为可选增强）。
- **`co_contexts_max`**：高并发服务建议调到 `4096~16384`，避免频繁触发 sweep；
  不要设过小以免误淘汰长寿协程。
- **`route_precompile` / `cache_max_items`**：均为新引入的 opt-in 优化，仅 Swoole
  生效，建议先压测对比再上生产。
- **`run_environment=2`**：生产务必设为 prod，否则 DB 层每条 SQL 都会做 benchmark
  标记（仅 dev=0 时记录）。
- **`view_compile_check_mtime`**：开发期设 `1`（改模板即时生效），生产设 `0`
  （性能最优）。

## 新增 API（2026-08-07 审计补全批次）

以下方法在审计报告 `AUDIT_REPORT_2026_08_06.md` 中列为缺口，本批次已实现：

### P1 级

| 类 | 方法 | 说明 |
|----|------|------|
| `Gene\Di` | `alias($alias, $target)` | 服务别名，`instance($alias)` 解析到目标服务 |
| `Gene\Request` | `isSecure()` | 判断当前请求是否 HTTPS/TLS |
| `Gene\Memory` | `mget(array $keys)` | 批量获取缓存值 |
| `Gene\Memory` | `mset(array $items, int $ttl)` | 批量设置缓存值 |
| `Gene\Monitor` | `reset()` | 重置所有累计计数器 |
| `Gene\Monitor` | `exportPrometheus()` | Prometheus 文本格式导出 |

### P2 级

| 类 | 方法 | 说明 |
|----|------|------|
| `Gene\View` | `render($template, array $vars)` | 渲染模板返回字符串 |
| `Gene\View` | `clearAssign()` | 清除所有已赋值变量 |
| `Gene\Response` | `getStatusCode()` | 获取当前 HTTP 状态码 |
| `Gene\Response` | `isSent()` | 判断响应是否已发送 |
| `Gene\Response` | `sendFile($path, $filename, array $headers)` | 发送文件下载 |
| `Gene\Session` | `clear()` | 清除所有 session 数据 |
| `Gene\Session` | `all()` | 返回全部 session 数据 |
| `Gene\Validate` | `bail()` | 首错即停 |
| `Gene\Validate` | `sometimes($field, callable $callback)` | 条件验证（回调返回 false 跳过该字段规则） |
| `Gene\Log` | `critical($message, array $context)` | CRITICAL 级别日志（RFC-5424） |
| `Gene\Log` | `alert($message, array $context)` | ALERT 级别日志 |
| `Gene\Log` | `emergency($message, array $context)` | EMERGENCY 级别日志 |
| `Gene\Log` | 所有日志方法新增可选 `array $context` 参数 | 结构化上下文（JSON 编码追加到日志行） |
| `Gene\Db\Sqlite` | `attach($path, $schema)` | 附加外部 SQLite 数据库 |
| `Gene\Db\Sqlite` | `detach($schema)` | 分离已附加的 schema |
| `Gene\Benchmark` | `mark($name)` | 记录命名高精度时间戳 |
| `Gene\Benchmark` | `lap($name)` | 返回距上次 mark 的毫秒数并重置 |
| `Gene\Application` | `stop()` | 中止当前请求派发（跳过 action 和 after-hook） |
| `Gene\Application` | `isStopped()` | 查询 stop() 是否已调用 |

### Log 级别常量

新增 `LEVEL_CRITICAL`(6)、`LEVEL_ALERT`(7)、`LEVEL_EMERGENCY`(8) 三个类常量，
对应 RFC-5424 严重级别。`setLevel()` 接受范围扩展为 1~8。
