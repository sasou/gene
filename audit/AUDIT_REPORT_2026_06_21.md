# Gene 扩展内存泄漏与并发性能分析报告

> 审计日期：2026-06-21
> 审计范围：FPM 模式 / Swoole 模式下的内存泄漏与极致并发性能优化点
> 审计版本：5.6.6（develop 分支）
> 审计方法：源码级静态审计（`src/gene.c`、`src/cache/memory.c`、`src/router/router.c`、`src/db/pool.c`、`src/cache/redis_pool.c`、`src/http/response.c`、`src/factory/factory.c` 等）

---

## 一、内存泄漏分析

### 1.1 FPM 模式 — 基本无泄漏 ✅

FPM 模式下每个请求结束后 PHP 引擎整体回收 arena 内存，扩展侧只需保证进程级资源在 MSHUTDOWN 释放、请求级资源在 RSHUTDOWN 释放。逐项核对结果：

| 资源 | 生命周期 | 释放路径 | 状态 |
|---|---|---|---|
| `default_ctx`（请求上下文） | 每请求 | `RSHUTDOWN` → `php_gene_close_request_globals()` → `gene_request_context_destroy()` | ✅ |
| `fn_cache`（闭包路由派发缓存） | 每请求（FPM） | `RSHUTDOWN` 中 `runtime_type < 2` 分支 destroy | ✅ |
| `cache` / `cache_easy`（持久缓存） | 进程级 | `MSHUTDOWN` → `gene_hash_destroy()` | ✅ |
| `gene_closure_src_cache`（P6 闭包源码缓存） | 进程级 | `MSHUTDOWN` → `gene_closure_src_cache_destroy()` | ✅ |
| `app_root` / `app_key` 等 INI 字符串 | 进程级 | `RSHUTDOWN` → `efree()` | ✅ |
| `cache_lock`（读写锁） | 进程级 | `MSHUTDOWN` → `gene_rwlock_destroy()` | ✅ |

**唯一隐患（低风险）**：`gene_closure_src_cache` 无 LRU/上限。

- 位置：`src/router/router.c:1585-1634`
- 若应用在 FPM 下动态注册大量不同闭包路由（跨请求 key 不同），该持久 HashTable 会无界增长
- 实际风险低——路由闭包通常是有限集合，且 `runtime_type < 2` 时才启用
- ZTS 构建下已通过 `#ifdef ZTS` 守卫关闭缓存，规避多线程竞争

### 1.2 Swoole 模式 — 两处需关注 ⚠️

Swoole worker 常驻内存，请求级资源必须显式回收。逐项核对：

| 资源 | 释放路径 | 状态 |
|---|---|---|
| `co_contexts`（协程上下文表） | `gene_co_context_dtor` + `gene_co_contexts_sweep()` | ⚠️ 见隐患 1 |
| `ctx_pool`（上下文结构池） | `RSHUTDOWN` → `gene_request_context_pool_drain()` | ✅ 有界 `ctx_pool_max=256` |
| `route_pc`（P3 预编译派发缓存） | `MSHUTDOWN` → `gene_router_pc_destroy()` | ✅ 路由数有限 |
| `cache_lru`（M1 LRU 跟踪集） | `MSHUTDOWN` / `clean()` → `gene_cache_lru_destroy()` | ✅ |
| `resident_ctx`（非协程驻留上下文） | `RSHUTDOWN` → destroy + pool_release | ✅ |
| `fn_cache`（Swoole 模式） | 进程级，`MSHUTDOWN` 释放 | ✅ |

#### 隐患 1：`co_contexts` 协程上下文泄漏 ⚠️

- 位置：`src/gene.c:671-729`（`gene_co_contexts_sweep`）、`src/gene.c:732-813`（`gene_request_ctx`）
- 协程结束时若用户**未调用** `Application::cleanup()`，其 `gene_request_context` 条目滞留在 `co_contexts` 中
- `gene_co_contexts_sweep()` 仅在条目数 ≥ `gene.co_contexts_max`（默认 1024）时触发
- 若 `co_contexts_max = 0`（可配置为不限），sweep **永不执行** → 无界增长
- 每个 stale 条目持有一个 `gene_request_context`（~320B + `path_params`/`request_attr` 数组内容）
- **影响**：长生命周期 worker 下 RSS 缓慢增长，最终可能 OOM
- **建议**：
  1. 生产环境务必设置 `gene.co_contexts_max` 为合理值（如 1024~8192）
  2. 在 `WorkerStop` 时主动调用 `Application::cleanup()`
  3. 文档强调协程结束必须 cleanup

#### 隐患 2：`Gene\Cache` 业务数据持久缓存无界增长 ⚠️

- 位置：`src/cache/memory.c:506-543`（`gene_memory_set`）、`src/cache/cache.c:1499-1501` 等
- `gene.cache_max_items = 0`（默认）时，`Gene\Cache::processCached` / `processCachedVersion` / `cachedBatch` 等业务写入的持久数据无上限
- Swoole worker 永不回收 → RSS 只增不降
- M1 已实现可选 LRU 上限（`gene.cache_max_items`），但**默认关闭**
- **影响**：若应用用 `Gene\Cache` 做进程内数据缓存，RSS 会随业务数据量增长
- **建议**：若使用 `Gene\Cache` 做进程内数据缓存，设置 `gene.cache_max_items=N`

### 1.3 跨请求安全 — 已修复 ✅

`gene_interned_str_persistent()`（`src/gene.c:184-198`）正确处理了 `opcache.file_cache_only=1` 场景：

- 仅在 `IS_STR_PERMANENT` 时缓存 slot
- 否则留 NULL 强制下个请求重新解析
- 消除了跨请求悬垂指针风险（曾导致 160GB+ 的 `ZSTR_LEN` 读取与 SIGSEGV）

`gene_lookup_class_str()`（`src/gene.c:212-242`）同样避免跨请求缓存 `zend_string*`，使用栈缓冲小写化 + `EG(class_table)` 直查。

---

## 二、并发性能优化分析

### 2.1 已落地优化项（源码确认）

| 项 | 优化点 | 位置 | 状态 |
|---|---|---|---|
| P1 | Swoole getcid C-API 直调（dlsym） | `src/gene.c:91-103` | ✅ 生产可用 |
| P3 | 路由叶子预编译派发（默认关） | `src/router/router.c:1337-1366` | 🧪 需 ASAN 验证 |
| P4 | factory 方法指针缓存 | `src/factory/factory.c:174` | ✅ action 已小写化 |
| P5 | pool/redis_pool 方法缓存 | `src/db/pool.c:97-107` / `src/cache/redis_pool.c` | ⚠️ 见发现 1 |
| P6 | FPM 闭包源码持久缓存 | `src/router/router.c:1603-1634` | ✅ ZTS 安全 |
| M1 | 持久缓存 LRU 上限（默认关） | `src/cache/memory.c:395-503` | ✅ |
| M3 | `ctx_pool_max` 上限 | `src/gene.c:512-527` | ✅ |
| M5 | `request_attr` 就地复用 | `src/gene.c:312-326` | ✅ 部分（char* 暂缓） |
| — | vm_stack 快路径跳过 getcid | `src/gene.c:746-748` | ✅ |
| — | `worker_ready` 后免锁读 | `src/cache/memory.h:25-26` | ✅ |
| — | `gene_memory_get_triple` 批量查 | `src/cache/memory.c:580-598` | ✅ |
| — | Swoole Response 方法指针缓存 | `src/http/response.c:126-149` | ✅ |
| — | `path_params` 内联 zval + 预分配 8 桶 | `src/gene.c:253-269` | ✅ |
| — | ctx 结构池预热（`ctx_pool_prewarm`） | `src/gene.c:554-579` | ✅ |
| — | `gene_memory_get_by_config` 锁外走 strtok | `src/cache/memory.c:612-657` | ✅ |

### 2.2 新发现问题

#### 发现 1：`pool.c` 的 `pool_in_coroutine()` 未复用 P1 C-API ⚠️ 高优先

- 位置：`src/db/pool.c:85-117`

```c
// pool.c:105 — 使用混合大小写 "getCid"
fn_getcid = zend_hash_str_find_ptr(&co_ce_cached->function_table, ZEND_STRL("getCid"));
```

对比 `redis_pool.c` 的正确实现：

```c
// redis_pool.c:690 — 正确复用 gene_get_coroutine_id()
return gene_get_coroutine_id() >= 0;
```

**两个问题**：

1. **潜在 bug**：PHP 函数表的方法键以**小写**存储，`zend_hash_str_find_ptr` 做精确字节比较。`"getCid"` 可能找不到 `"getcid"`（取决于 Swoole 是否注册了大小写别名）。若查找失败，`pool_in_coroutine()` 恒返回 0，DB 连接池无法识别协程上下文，可能导致连接池在协程模式下行为异常
2. **遗漏优化**：即使查找成功，仍走 `zend_call_known_function`（PHP 调用 ~300ns），未受益于 P1 的 dlsym C-API 直调（~5ns）

**建议修复**：将 `pool_in_coroutine()` 整体替换为 `return gene_get_coroutine_id() >= 0;`（与 `rpool_in_coroutine()` 一致）

#### 发现 2：`gene_co_contexts_sweep` 的 `exists()` 仍走 PHP 调用 — 中收益

- 位置：`src/gene.c:631-651`（`gene_swoole_co_exists`）

`gene_swoole_co_exists()` 每次探测一个 cid 是否存活都走 `zend_call_known_function`。当 `co_contexts` 达到上限触发 sweep 时，对 N 个条目做 N 次 PHP 调用。

**建议**：类似 P1，通过 dlsym 解析 Swoole C++ API 检查协程存活的内部接口，消除 sweep 路径的 PHP 调用开销。仅在 sweep 触发时生效（非常态路径），但高并发下 sweep 频率不可忽视。

#### 发现 3：`gene_memory_get_by_config` 每次 strtok — 低收益

- 位置：`src/cache/memory.c:612-657`

配置路径如 `"db/mysql/host"` 每次调用都做 `php_strtok_r` + 多层 `zend_symtable_str_find`。`worker_ready` 后配置冻结，可缓存解析后的指针。

**建议**：在 `worker_ready` 后对常用配置路径做一次性解析缓存。收益取决于应用读取配置的频率（通常 bootstrap 期为主，热路径较少）。

#### 发现 4：请求上下文 char* 字段 9 次 efree — 低中收益

- 位置：`src/gene.c:344-361`（`gene_request_context_free_fields`）

每请求 reset 时对 `method/path/router_path/module/controller/action/child_views/lang/log_file` 逐个 `efree`（最多 9 次）。M5 评估后暂缓了 char* stash 池化（因"指针非空=已设置"判据约束）。

**替代思路**：将这些字段批量分配在一个 arena buffer 中（单次 emalloc），reset 时单次 efree 整个 arena。改动面比 stash 方案小（只需改 set 路径的分配策略，不需改"指针非空=已设置"判据）。

#### 发现 5：Swoole Response header 每次分配 zend_string — 低收益

- 位置：`src/http/response.c:203-218`

每次 `header()` 调用做 2 次 `ZVAL_STRINGL`（分配 zend_string）+ 2 次 `zval_ptr_dtor`。常见 header（`Content-Type` 等）可缓存为 interned string。但每响应 header 数通常个位数，收益有限。

---

## 三、总结与优先级建议

### 3.1 内存泄漏总结

| 模式 | 结论 | 需关注项 |
|---|---|---|
| FPM | ✅ 无泄漏 | `gene_closure_src_cache` 无上限（低风险，路由闭包通常有限） |
| Swoole | ⚠️ 两处条件性泄漏 | 1. `co_contexts` 不调 cleanup 时 stale 条目堆积（设 `co_contexts_max`）<br>2. `Gene\Cache` 业务数据无上限（设 `cache_max_items`） |

### 3.2 并发优化优先级

| 优先级 | 项 | 预期收益 | 风险 | 改动量 |
|---|---|---|---|---|
| **高** | 修复 `pool_in_coroutine()` 复用 `gene_get_coroutine_id()` | 修复潜在 bug + 每次查询省 ~300ns | 低 | 一行 |
| **中** | P3 `route_precompile=1` 上线验证 | 每请求省 6-10 次哈希查找 | 中高（需 ASAN 验证 fn_cache 冻结不变量） | 已实现，待验证 |
| **中** | sweep 路径 `exists()` C-API 直调 | sweep 时 N 次 PHP 调用 → C 调用 | 中（dlsym 解析 + 回退） | 中 |
| **低** | config 路径解析缓存 | 配置读取减少 strtok + 哈希 | 低 | 中 |
| **低** | char* arena 批量分配 | 每请求省 ~8 次 efree | 中（改分配策略） | 中 |
| **低** | Response header interned string | 每响应省 2 次 zend_string 分配 | 低 | 低 |

### 3.3 最关键行动项

**修复 `pool.c` 的 `pool_in_coroutine()`** — 这既是潜在 bug 修复（大小写问题可能导致协程检测失效），又是性能优化（复用 P1 C-API 直调），且改动极小（一行）：

```c
// 修复前（src/db/pool.c:85-117）
static bool pool_in_coroutine(void) {
    if (GENE_G(runtime_type) < 2) return 0;
    static zend_function *fn_getcid = NULL;
    static zend_class_entry *co_ce_cached = NULL;
    if (UNEXPECTED(!fn_getcid)) {
        co_ce_cached = gene_lookup_class_str(ZEND_STRL("Swoole\\Coroutine"));
        if (!co_ce_cached) return 0;
        fn_getcid = zend_hash_str_find_ptr(&co_ce_cached->function_table, ZEND_STRL("getCid"));
        if (!fn_getcid) return 0;
    }
    zval cid_ret;
    ZVAL_UNDEF(&cid_ret);
    zend_call_known_function(fn_getcid, NULL, co_ce_cached, &cid_ret, 0, NULL, NULL);
    zend_long cid = (Z_TYPE(cid_ret) == IS_LONG) ? Z_LVAL(cid_ret) : -1;
    if (!Z_ISUNDEF(cid_ret)) zval_ptr_dtor(&cid_ret);
    return cid >= 0;
}

// 修复后（与 redis_pool.c:686-691 一致）
static bool pool_in_coroutine(void) {
    if (GENE_G(runtime_type) < 2) return 0;
    return gene_get_coroutine_id() >= 0;
}
```

---

## 四、验证建议

### 4.1 内存泄漏验证

- **Swoole worker 压测**：10 万请求前后对比 `memory_get_usage(true)` + 进程 RSS（`/proc/self/status`）
- **FPM 单请求**：valgrind/ASAN 检测泄漏
- **重点核对**：
  - M1 在 `cache_max_items > 0` 下淘汰、`clean()`、MSHUTDOWN 三处持久 key 释放无泄漏/双释放
  - `co_contexts` 在不调 cleanup 时的增长曲线（设/不设 `co_contexts_max` 对照）

### 4.2 并发性能验证

- **wrk/ab 压测**：对比 QPS 与 P99（demo 应用三类路由：MCA/字符串/闭包）
- **P3 对照**：`gene.route_precompile=0` vs `=1`，确认 digest 一致、QPS 提升
- **P1 对照**：`gene.swoole_getcid_capi=0` vs `=1`，确认协程上下文隔离正确
- **pool 修复对照**：修复前后 DB 连接池在协程模式下的行为与 QPS

### 4.3 回归测试

- 现有 demo + `clearAll` hook 路由、错误路由、连接池断连重连场景逐项回放
- ASAN 全回归（`--enable-debug` 构建）

---

## 五、参考文件索引

| 文件 | 关键内容 |
|---|---|
| `src/gene.c` | 请求上下文管理、协程上下文、ctx 结构池、getcid C-API |
| `src/gene.h` | 全局结构定义、INI 开关、宏定义 |
| `src/cache/memory.c` | 持久缓存、LRU 跟踪集、读写锁、批量查询 |
| `src/cache/memory.h` | 锁宏、`cache_layer_memory_write_depth` 宏 |
| `src/cache/cache.c` | `Gene\Cache` 业务层、`processCached` 等写入入口 |
| `src/router/router.c` | 路由派发、P3 预编译、P6 闭包源码缓存 |
| `src/db/pool.c` | DB 连接池、`pool_in_coroutine()`（发现 1） |
| `src/cache/redis_pool.c` | Redis 连接池、`rpool_in_coroutine()`（正确实现） |
| `src/http/response.c` | Swoole Response 方法指针缓存 |
| `src/factory/factory.c` | controller action 派发、方法指针缓存 |
| `src/app/application.c` | `workerReady()`、`cleanup()`、autoload |
| `audit/gene-memory-concurrency-audit.md` | 前序审计（M1/P1/P3/P6 等实施记录） |
