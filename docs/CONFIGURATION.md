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
| `gene.view_compile_check_mtime` | `1` | bool | 编译模板按 mtime 失效校验（默认开启，生产建议显式设为 `0`） |
| `gene.view_stat_ttl` | `0` | 秒，`0`=关闭 | mtime 检查结果缓存时间；开启后源模板更新最多延迟该时长生效 |
| `gene.view_fresh_max` | `512` | long，`<=0`=不限 | view stat 缓存条目上限；新 key 达上限时整表清空，防 worker 常驻增长 |
| `gene.use_library` | `0` | bool | 启用 library 自动加载回退 |
| `gene.library_root` | `""` | path | library 根目录（配合 `use_library`） |
| `gene.co_contexts_max` | `1024` | long | 协程上下文软上限，超过触发 sweep |
| `gene.ctx_pool_max` | `256` | long | ctx 结构体池容量 |
| `gene.ctx_pool_prewarm` | `0` | long | RINIT 自动预热数量（仅 Swoole） |
| `gene.swoole_getcid_capi` | `1` | bool | 用 Swoole C-API 直接取协程 id（更快） |
| `gene.cache_max_items` | `0` | long | 业务缓存分区上限（0=不限，>0 启用 LRU 淘汰） |
| `gene.cache_reserve` | `4096` | long | 进程缓存哈希表桶预留容量；Swoole `workerReady()` 冻结前按此值预扩展。必须大于 `cache_max_items`；`<=` 时自动矫正为 `max_items + max(64, max_items/4)` 并记 warning |
| `gene.route_precompile` | `0` | bool | 路由预编译派发缓存（仅 Swoole，opt-in） |
| `gene.closure_src_cache_max` | `1024` | long | FPM 闭包源码缓存容量；`<=0` 关闭缓存 |
| `gene.swoole_auto_cleanup` | `0` | bool | 协程 ctx 随协程结束自动归还（仅 Swoole，opt-in） |
| `gene.cache_easy_ttl` | `0` | long | cache_easy 文件表 TTL 兜底秒数（0=禁用，惰性过期） |
| `gene.slow_query_ms` | `0` | long | 慢查询阈值（毫秒，0=禁用）；超限 SQL 计入 `Monitor::stats()` 的 `db_slow_query_count` |
| `gene.log_keep_open` | `0` | bool | Swoole 下复用 worker 自持的无缓冲日志流；FPM 保持逐条 `error_log(type=3)` |
| `gene.log_reopen_interval` | `5` | 秒 | 常驻日志流检查 rename/copytruncate 并重开的间隔 |

> **`Gene\Memory` TTL 语义说明**：`Memory::set($k, $v, $ttl)` 的 `$ttl` 以秒计，`0` 表示永久。**FPM**：过期键由读路径惰性删除 + 每 32 次 TTL 写入抽样主动清扫，内存可回收。**Swoole**：`workerReady()` 冻结的只是进程级框架缓存（路由/配置表）；`Gene\Memory` 的用户态 set/del/incr/decr/mset/rateLimit/lock 仍写独立的业务分区，worker 内请求期可写。过期键在读路径返回 miss，实体由后续业务写入的清扫或 `del()` 回收。仍不建议在 Swoole 下用带 TTL 的 Memory 键做高频轮转写入（如 `rate:$ip`）——需要跨 worker 共享或确定性过期回收时请用 `Gene\Cache`（Redis/Memcached）层。

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
- **`co_contexts_max`**：默认 `1024` 对高并发 Swoole 偏小。按「单 worker 峰值协程 + 余量」
  显式设到 `4096~16384`，避免频繁 sweep；不要设过小以免误淘汰长寿协程。
- **`cache_max_items=0`**：兼容保留的**无界**默认，长跑可能抬高 RSS。**代码不改默认**；
  生产先用 `Gene\Monitor::stats()` 回采业务缓存分区水位，再显式设 LRU 上限（如 `10000`）。
  `route_precompile` 为 Swoole opt-in，建议压测后再开。
- **`run_environment`**：仅 `0`（dev）记录 SQL history/benchmark；默认 `1`（test）已关闭。
  生产设 `2`。
- **`view_compile_check_mtime`**：默认 `1`（改模板即时生效，对开发友好），
  生产环境建议显式设 `0`（性能最优）。
- **`cache_reserve`**：Swoole 下与 `cache_max_items` 联动。启用 LRU（`cache_max_items>0`）
  时务必让 `cache_reserve > cache_max_items`（如 `cache_max_items=10000` 配
  `cache_reserve=12500`），否则 `workerReady()` 会自动向上矫正并告警；
  `Memory` 与 `Gene\Cache` 在冻结后共享这部分余量，高频新增 key 的业务可适当调大。

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
| `Gene\Monitor` | `prometheus()` | Prometheus 文本格式导出 |

### P2 级

| 类 | 方法 | 说明 |
|----|------|------|
| `Gene\View` | `render($template, array $vars)` | 渲染模板返回字符串 |
| `Gene\View` | `clearAssign()` | 清除所有已赋值变量 |
| `Gene\Response` | `getStatusCode()` | 获取当前 HTTP 状态码 |
| `Gene\Response` | `isSent()` | 判断响应是否已发送 |
| `Gene\Response` | `sendFile(string $file, int $offset = 0, int $length = 0)` | 发送文件下载；仅限本地普通文件（不接受 `http://`、`php://` 等流包装器），`$file` 不要直接拼接用户输入；响应头需先经 `Response::header()` 设置 |
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

## Gene\Orm（ActiveRecord v1）

数据访问 Model 请继承 `Gene\Orm\Model`（而非裸 `Gene\Model`）。`Gene\Model` 仍只负责 DI + `success/error/data`。

| 类 | 方法 | 说明 |
|----|------|------|
| `Gene\Orm\Model` | `find / findAll / paginate / query / where` | 查询；默认返回 `array` |
| `Gene\Orm\Model` | `create / updateBy / destroy / destroyAll` | 写入 |
| `Gene\Orm\Model` | `fill / save / delete / toArray` | 实例 ActiveRecord |
| `Gene\Orm\Model` | `flip($id, $field, $values = [0, 1])` | 单条 UPDATE 翻转状态；整型内联、布尔按驱动输出、字符串走绑定 |
| `Gene\Orm\Model` | `page($where, $page, $perPage, $order = null)` | 按页码换算 offset 后派发到被调类的 `paginate()`，子类覆盖生效 |
| `Gene\Orm\Query` | `where / in / order / limit / all / row / cell / count` | 链式；终端后 reset Db |

子类声明：`protected static $table`、`$primaryKey`、`$fields`、可选 `$timestamps`、`$connection`、`$versionKeys`、`$versionScanLimit`。

- `$versionKeys`：`版本键 => 行内列名` 映射。写入成功后按行级失效——写前按主键或同一 where 预读受影响行，映射列发生变更时同时失效新旧值，payload 未包含的映射列按其当前行值失效；事务内 bump 按 PDO 连接分桶，仅在本连接 commit 后冲刷、rollback 丢弃。
- `$versionScanLimit`：非主键 `updateBy`/批量删除的预读行数上限，默认 1000；超限发出 `E_WARNING` 并跳过失效而非部分失效。预读 SELECT 与 UPDATE 在事务外非原子，需要严格失效时应放进 `transaction()` 内执行。
- 上述静态属性在 C 层父类中均为**无类型**声明，子类不得加 PHP 类型（如 `protected static string $table` 会触发致命错误），类型意图写进 PHPDoc。

FPM：`db.instance => false`。Swoole：`instance => true` + 连接池；请求 `cleanup(true)` 即可释放 ORM 请求级 meta 缓存。

### Log 级别常量

新增 `LEVEL_CRITICAL`(6)、`LEVEL_ALERT`(7)、`LEVEL_EMERGENCY`(8) 三个类常量，
对应 RFC-5424 严重级别。`setLevel()` 接受范围扩展为 1~8。
