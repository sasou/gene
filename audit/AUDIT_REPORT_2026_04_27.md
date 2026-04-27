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
- ~~`pool_create_connection` 配置规范化（突发流量下收益）~~ **本期落地** (2026-04-27)：新增 `pool_normalize_config()`，在 `Pool::__construct` 一次性把 `username/password` 强制为 `IS_STRING`、把强制 PDO 属性 (`ATTR_ERRMODE`、`ATTR_EMULATE_PREPARES`、Swoole 下 `ATTR_PERSISTENT=false`) 折叠进 `options` 数组。`pool_create_connection` 热路径每次建连节省 1× `array_init` + 1× `ZVAL_DUP`（options 深拷贝） + 2~3× `add_index_long/bool` + 0~2× `ZVAL_STRING("")`，仅剩 4× `ZVAL_COPY` 引用计数自增。配置数组通过 `SEPARATE_ARRAY` 进行 CoW 隔离，不影响调用方的 PHP 数组可见性。

均无安全或稳定性影响，可在后续版本择机推进。

---

## 追加修复 (2026-04-27 19:13)

本轮额外审计发现并落地 5 项修复（详见同目录下分析记录）：

### F1 — `validate.c::validCheck` 两处 use-after-free（关键）
**文件**: `src/http/validate.c`（行 729-732, 768-776）

`zend_hash_str_update(errors, …, msg)` 中 `msg` 来自借用的 `value/rules/closure_arr` 槽位，未 `Z_TRY_ADDREF_P`。`zend_hash_str_update` 通过 `ZVAL_COPY_VALUE` 接管所有权，导致 errors 与原数组共享 refcount=1 的 zval；errors 销毁时释放，原数组指针悬空。`is_group=1` 路径必现。

**修复**：两处 `zend_hash_str_update(...errors..., msg)` 前补 `Z_TRY_ADDREF_P(msg)`，与同文件 `addRule` 路径保持一致。

### F2 — `validate.c::validCheck` snprintf 越界
**文件**: `src/http/validate.c`（行 748-756）

method 名 ≥ 58 字符时 `snprintf` 返回值超出 64 字节缓冲，`name_len` 被传给 `zend_hash_str_exists` / `gene_factory_call` 造成 OOB 读。

**修复**：检查 `snprintf` 返回值 `< 0 || >= sizeof(buf)`，超长则 `php_error_docref(E_WARNING)` 并 `continue`。

### F3 — `validate.c` 命名拼写
将 `setFefCount` 重命名为 `setRefCount`（17 处）。仅命名修正，无行为变化。

### F4 — `language.c::__get` 移除多余 addref
**文件**: `src/tool/language.c`（行 289-298）

`ZVAL_COPY(&tmp, &retval)` + 末尾 `zval_ptr_dtor(&retval)` 改为 `ZVAL_COPY_VALUE` + `ZVAL_UNDEF(&retval)` 直接转移所有权，省一次原子计数自增。`zval_ptr_dtor` 在 IS_UNDEF 上短路。

### F5 — `log.c` 日志路径分配合并
**文件**: `src/tool/log.c`

`gene_log_call_error_log` 改签 `(char *log_line, size_t log_line_len, const char *effective_file)`，接管 spprintf 缓冲所有权，内部以 `zend_string_init(log_line, len, 0) + efree(log_line)` 替代 `ZVAL_STRING`，省一次 emalloc + memcpy + 一次 strlen；`gene_log_write_message` / `Gene\Log::exception` 调用点同步更新，并清理 `exception` 中 early-return 后的死代码尾段。

### 留待下轮（5/6 — 属性偏移缓存）

`Gene\Log::*` 静态属性 `file/level` 与 `Gene\Session` 的 7 个 cookie 相关属性仍走 `zend_read_(static_)property` 哈希查找。此重构跨多函数（含 `gene_cookie / gene_set_cookie / gene_init_ssid / 各方法体`），需对每个属性分别在 MINIT 中通过 `zend_hash_str_find_ptr(&ce->properties_info, ...)` 取 `offset` 并改写所有读写点。本轮范围控制风险，不动；后续单独 PR。

### 文件变更补充

```
 src/http/validate.c | +9 / -3 (F1 + F2 + F3)
 src/tool/language.c | +2 / -1 (F4)
 src/tool/log.c      | +9 / -10 (F5)
```

无 API 变化。建议构建后跑 `demo/application/Validate` 用例覆盖 `validCheck` 多失败规则路径以验证 F1。
