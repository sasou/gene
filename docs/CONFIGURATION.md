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
