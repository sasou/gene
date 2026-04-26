# Gene Extension — Perf & Memory Audit (2026-04-26)

聚焦：FPM / Swoole 高并发热路径与 Swoole 常驻内存稳态。

## 背景

近 6 期审计（2026-03-24 起）已系统性消除：
- 路由分派 / DI / 配置 / Memory 缓存键的 `spprintf` 堆分配（栈缓冲 256B 模式）。
- 锁竞争（`worker_ready` 后跳过持久缓存读锁）。
- `gene_request_context` 结构体池（pre-warm + 自由链表，避免每协程 ecalloc/efree）。
- 连接池 count drift / TOCTOU / 过期连接 / `clearState` 资源 UAF。
- `Gene\Pool::stopTimers()` + `onWorkerExit` 解决 reactor 退出死锁。

本期复查发现仍可优化的点（按优先级排序）。

---

## P1 — `pool_in_coroutine()` 多次 PHP 调用（`db/pool.c:41-90`）

### 现状
每次 `Pool::close()` 调用 `pool_in_coroutine()` 时执行：
1. `Swoole\Coroutine::getCid()` → `zend_call_known_function`
2. `Swoole\Coroutine::exists($cid)` → 第二次 `zend_call_known_function`

### 问题
`getCid()` 已经表明协程身份：返回 ≥0 即处于协程。`exists($cid)` 是冗余调用，每次 close 多一次 PHP 函数调用 + zval 构造/释放。

### 建议
```c
return cid >= 0;   /* 删除 exists() 调用块 */
```
节省每个 close ≈ 1 次 zend_call + 1 zval 构造。close 不是热路径，但 closeAll 在 worker shutdown 时遍历多个 pool。

---

## P2 — `pool_decrement_count` 双 atomic 调用（`db/pool.c:375-387`）

### 现状
```c
POOL_ATOMIC_CALL(atomic, "get", 0, &ret);     /* atomic op #1 */
if (val > 0) POOL_ATOMIC_CALL(atomic, "sub", 1, NULL); /* atomic op #2 */
```

`Swoole\Atomic::sub()` 内部本身是 CAS / lock-prefixed，已是原子。`get`+`sub` 两步存在 TOCTOU（其他协程可能在 get 后 sub），且 cost 翻倍。

### 建议
对于"已知刚 increment 过"的回滚路径（`pool::get` 第 615、650 行），新增 `pool_decrement_count_unchecked` 直接 `sub(1)`，节省 1 次 atomic op。仅 `put()` 失败路径需要 floor 保护时使用旧逻辑——或干脆都改 unchecked，因为 `count` 不会负到无意义（最坏 -1，下次 increment 恢复）。

每查询节省 1 次原子操作 + 1 次 `zend_call_known_function`。

---

## P3 — `pool_recycle_idle` 内 0.001s pop 引起协程让出（`db/pool.c:509`）

### 现状
```c
if (!pool_channel_pop(channel, 0.001, &item)) break;   /* 1ms timeout */
```

Swoole `Channel::pop(0)` 是**非阻塞 immediate**；`pop(0.001)` 会触发 timer 注册 + 协程让出 1ms，即使 channel 非空。每个空闲连接 recycle 一次会让出协程，timer tick 期间挤占其他请求。

### 建议
```c
if (!pool_channel_pop(channel, 0, &item)) break;
```
同样适用于 `pool::get` 第 581 行（首次非阻塞尝试）和 close 第 741 行 phase 1 drain。

收益：recycle timer 不再让出，减少与正常请求的协程切换。

---

## P4 — DB 查询每次 `Pool::getInstance()` 查找（`db/pool.c:1119-1167`）

### 现状
每个 SQL 查询都走：
```
gene_pool_get_pdo
  → Pool::getInstance($name)   /* 静态属性 instances 数组 hash 查找 */
  → $pool->get()
```

`getInstance` 每次访问静态属性 `instances`，做 zend_hash_find。Pool 实例数量极少（通常 1-3 个 named pool），可以用 C 层 hashtable 缓存。

### 建议
新增模块级 `static HashTable *gene_pool_named_cache`，首次解析时把 `(name → pool zval*)` 缓存。在 `Pool::create` 时插入、`Pool::closeAll` 时清空。`getInstance` 走快速路径直接命中。

收益：每个 DB 查询节省 1 次 PHP 静态属性读 + 1 次 hash 查找 + 1 次 `zend_call_known_function`（getInstance 本身）。Swoole 高 QPS 下显著。

---

## P5 — `Pool::create` 时 `Pool::__construct` 通过 `zend_call_known_function` 走 PHP 调用栈

### 现状
`db/pool.c:1065` 的 `zend_call_known_function(gene_pool_ce->constructor, ...)` 会进入 PHP frame。但 `__construct` 全是 C 实现 (`PHP_METHOD(gene_pool, __construct)`)。

### 建议
低优先级：直接调用内部 C 函数即可（提取 `__construct` 主体到 `pool_ctor_internal(zval *self, zval *config)`，PHP 方法和 `create` 都调用它）。仅启动开销，省一次 frame。

---

## P6 — `pool_create_connection` 每次 `ZVAL_DUP(options)`（`db/pool.c:195`）

### 现状
每创建一个池连接，都对 config 中的 `options` 数组深拷贝。即使 options 不变，每个连接都拷一份。

### 建议
config 在 worker 生命周期内 immutable。可在 `__construct` 时一次性把 options 规范化（添加 ATTR_ERRMODE / EMULATE_PREPARES / 非 PERSISTENT）后保存到 protected 属性 `_pdoOptions`，`pool_create_connection` 只 `Z_TRY_ADDREF` 引用，不深拷。

收益：每次新连接节省一次数组深拷贝。pool 扩容/重建时显著（突发流量场景）。

---

## P7 — `pool_atomic_call_fn` 仍然为简单原子操作通过 `zend_call_known_function`

### 现状
每次 `pool_increment_count` / `pool_decrement_count` / `pool_get_count`：
```
zend_call_known_function(Swoole\Atomic::add/sub/get, ...)
```
进入 PHP frame、ZE 调度、return zval。

### 建议（前瞻性）
检查是否能直接访问 Swoole\Atomic 的内部 long 值（Swoole 源码暴露 `swAtomic*` 或对象 internal value）。通过 `zend_object` 内嵌偏移直接 `__sync_fetch_and_add` 可彻底消除 PHP 调用开销。**注意**：跨 Swoole 版本 ABI 不稳定，需用宏隔离。

收益：每查询节省 ~3 次 zend_call_known_function（add/get/sub），中高 QPS 下可观。

---

## 内存稳态复查

### default_ctx / co_contexts / resident_ctx
均通过 `gene_request_context_pool_*` 复用。FPM 单请求净增 0；Swoole 协程峰值后稳态净增 0。**无变更**。

### Pool channel 内 PDO 残留
`Pool::close()` 第 786 行已添加 `zend_update_property_null(...CHANNEL...)`，channel refcount→0，PDO 在 worker 生命期内释放。修复已就位。

### `db/mysql.c` 中 `mysqlSaveHistory`
仅当 `db_history` 配置开启时才填充 `db_mysql_history`（per-context 数组）。已被 `clearState` 清。无累积。

### 推荐运维约束（文档）
1. Swoole 路由 / hook 必须在 `workerStart` 注册一次（`fn_cache_id` 单调递增，运行期注册会增长）。
2. 每个请求结束 **必须**调用 `Application::clearState()` 或 `cleanup()`。
3. Worker 退出顺序：`onWorkerExit` → `Pool::stopTimers()`；`onWorkerStop` → `Pool::closeAll()`。

---

## 落地状态（2026-04-26）

| 项 | 状态 | 备注 |
|----|------|------|
| P1 | ✅ 已落地 | `pool_in_coroutine` 移除 `Coroutine::exists` 冗余调用，缓存 `getCid` 函数指针 |
| P2 | ✅ 已落地 | 新增 `pool_decrement_count_unchecked`，`pool::get` 两处回滚使用 unchecked 版 |
| P3 | ✅ 已落地 | 三处 `pool_channel_pop(channel, 0.001, ...)` → `pool_channel_pop(channel, 0, ...)` |
| P4 | ✅ 已落地 | 新增 `gene_pool_named_cache` 模块级 HashTable；`Pool::create` 写入、`closeAll` 清空、`Pool::getInstance` 与 `gene_pool_get_pdo` 走快速路径 |
| P5 | ⏸ 跳过 | 启动期成本，价值低 |
| P6 | ⏸ 暂缓 | 编辑工具对 `pool.c` 的多点联动改写不稳定；原 `ZVAL_DUP(options)` 仍保留 |
| P7 | ⏸ 不实施 | 跨 Swoole 版本 ABI 风险高 |

修改文件：`src/db/pool.c`（+97 / -45 行）。

## 推荐落地顺序

| 优先级 | 项 | 风险 | 预估 QPS 收益 |
|------|-----|------|--------------|
| 高   | P3 (pop 0ms) | 极低 | recycle 期间不再阻塞 |
| 高   | P4 (named pool 缓存) | 低 | 高负载下 +3-5% |
| 中   | P2 (decrement_count_unchecked) | 低 | +1-2% |
| 中   | P6 (options 规范化一次) | 低 | 突发场景 |
| 低   | P1 (去 exists 冗余) | 低 | shutdown only |
| 低   | P5 (ctor 内联) | 低 | 启动 only |
| 前瞻 | P7 (Atomic 直访) | 高（ABI） | 高负载下 +5-8% |

---

## 结论

扩展的请求级和连接池路径在历经多轮审计后**已无显著泄漏**，核心优化空间集中在 **Pool / Atomic 的 PHP→C 调用频次**。建议按 P3、P4、P2 顺序落地，可在不改 API、不引入风险的前提下进一步压缩 Swoole 模式下每查询的开销。
