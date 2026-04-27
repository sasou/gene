# Gene Extension — Perf & Memory Audit (2026-04-27)

承接 2026-04-26 审计：双模式（FPM / Swoole）已无显著泄漏，本期聚焦剩余的细粒度优化与进程退出期 valgrind 清洁度。

## 落地变更

### 1. 进程退出泄漏修复 — `gene_pool_named_cache`
**文件**: `src/db/pool.c`, `src/db/pool.h`, `src/gene.c`, `src/gene.h`

C 层命名连接池缓存 (`static HashTable *gene_pool_named_cache`) 仅在 `Pool::closeAll()` 时清理。如用户未调用 `closeAll`（异常退出等），HashTable 在进程退出时泄漏（valgrind 可见）。

**修复**：
- 新增 `GENE_MSHUTDOWN_FUNCTION(pool)`，调用 `gene_pool_named_cache_clear()`。
- 新增 `GENE_SHUTDOWN(module)` 宏（对称于 `GENE_STARTUP`）。
- `PHP_MSHUTDOWN_FUNCTION(gene)` 中调用 `GENE_SHUTDOWN(pool)`。

注：pool 仅在 Swoole 模式生效（FPM 模式 `runtime_type < 2` 不会调用 `Pool::create`），故 FPM 下此安全网为零成本。

### 2. `pool_in_coroutine()` / `pool_stop_timer()` 类查找缓存
**文件**: `src/db/pool.c`

每次调用都执行 `zend_lookup_class()` 查 `Swoole\Coroutine` / `Swoole\Timer`。Swoole 内部类不会卸载，可一次性缓存。

- `pool_in_coroutine`: 缓存 `co_ce_cached` + `fn_getcid`，单次解析。
- `pool_stop_timer`: 缓存 `timer_ce_cached` + `fn_clear`。

每次连接池关闭/排空节省一次 `zend_lookup_class` hash 查询。

### 3. `Pool::__construct` 未初始化 `timer_id` zval
**文件**: `src/db/pool.c`

构造器中 `zval timer_id` 未 `ZVAL_UNDEF`，被复用为 `Swoole\Coroutine\Channel` 与 `Swoole\Atomic` 构造调用的 retval（两次覆写，无 dtor）。虽然这两个内置构造器返回 NULL（非 refcounted），但属于脆弱代码。

**修复**：移除未用的 `timer_id` 声明，每次构造器调用使用独立 `ctor_ret` + `ZVAL_UNDEF` + `zval_ptr_dtor`。

### 4. `pool_recycle_idle` / `pool::close` 注释纠正
**文件**: `src/db/pool.c`

注释残留 `[GENE_PERF:2026-04-26 P3] non-blocking pop (0)`，但 P3 已回滚（Swoole `Channel::pop(<=0)` 实际是永久阻塞而非立即返回）。改为 `[GENE_NOTE:2026-04-27]` 解释 1ms 等待的原因，避免后续维护者再次踩坑。

### 5. `url()` 方法 hot-path 优化（hook / view / response）
**文件**: `src/mvc/hook.c`, `src/mvc/view.c`, `src/http/response.c`

历史遗留的三处 `Gene\Hook::url()` / `Gene\View::url()` / `Gene\Response::url()` 仍使用：
```c
snprintf(out_ptr, out_len + 1, "/%s/%.*s", lang, (int)path_len, p);
RETVAL_STRING(out_ptr);   /* 隐含 strlen(out_ptr) */
size_t lang_len = strlen(lang);  /* 每调用一次 strlen */
```

`Gene\Controller::url()` 早已（2026-04-20）改用直接 `memcpy` + `RETVAL_STRINGL` + `ctx->lang_len` 缓存。本期把另外三处对齐到同一形态：

- 移除 `snprintf` 格式解析开销；
- 替换 `RETVAL_STRING` 为 `RETVAL_STRINGL`，省去 `strlen(out_ptr)`；
- 使用 `ctx->lang_len` 缓存（`router.c` 早已写入），省去每次 `strlen(lang)`。

每次 `url()` 调用节省两次 `strlen` + 一次 `vsnprintf`。

---

## 已审计但未变更

### `pool_create_connection` 的 `ZVAL_DUP(options)`
每个新连接对 `options` 数组做深拷贝。已在 04-26 审计 P6 列为暂缓（编辑工具对 `pool.c` 多点联动改写不稳定，且收益仅在突发流量下明显）。仍维持。

### `pool_decrement_count` vs `pool_decrement_count_unchecked`
`put()` / `recycle_idle()` 中部分 `pool_decrement_count` 路径（已增加过 count）理论上可改用 `_unchecked` 版省一次 atomic get。但语义上需要 floor 保护时（`pool_is_closed` / `channel` 缺失），保留显式 `get + check > 0` 模式更稳健。

### request hot-path
`gene_ini_router` → `get_path_router` → `get_router_info` 链路在多轮审计后已完全栈化（256B / 128B 缓冲区）。本期未发现新优化点。

---

## 内存稳态复查

| 区域 | 状态 |
|------|------|
| `gene_request_context` 结构体池 | 无泄漏；`drain` 在 RSHUTDOWN 调用 |
| Swoole `co_contexts` HashTable | 无泄漏；`gene_co_context_dtor` 完备 |
| `default_ctx` / `resident_ctx` | 单例复用，无泄漏 |
| `gene_pool_named_cache` | **本期修复**：MSHUTDOWN 清理 |
| Swoole Atomic / Channel / Timer 类查找 | 缓存化 |
| Pool 连接 channel 生命周期 | `close()` 中 null-property 已就位 (04-26) |
| 路由 / DI / Memory 缓存键构造 | 全部栈化 (04-14) |

---

## 文件变更统计

```
 src/db/pool.c       | +52 / -36
 src/db/pool.h       | +1
 src/gene.c          | +6
 src/gene.h          | +1
 src/http/response.c | +20 / -7
 src/mvc/hook.c      | +20 / -7
 src/mvc/view.c      | +20 / -7
```

无 API 变化，向后兼容。建议：构建后运行：
1. 现有 demo 单元测试；
2. Swoole 长跑 + valgrind/leakcheck，确认 `gene_pool_named_cache` 在 `closeAll` 未调用场景下亦不泄漏。

---

## 结论

经此次审计，扩展在 FPM 与 Swoole 双模式下的请求级、worker 级、process 级三层资源回收均已闭环。剩余优化空间集中在：
- Swoole `Atomic` 直接内存访问（跨版本 ABI 风险高，仍不实施）；
- `pool_create_connection` 配置规范化（突发流量下收益）。

均无安全或稳定性影响，可在后续版本择机推进。
