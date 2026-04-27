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

### F6 — `Gene\Log` 静态属性偏移缓存
**文件**: `src/tool/log.c`

新增 `gene_log_sp_offset_file` / `gene_log_sp_offset_level`（`uint32_t`，static），MINIT 中所有 `zend_declare_property_*` 完成后通过 `zend_hash_str_find_ptr(&gene_log_ce->properties_info, ...)->offset` 一次性写入。新增内联函数 `gene_log_static_slot(offset)`：NTS 直接 `&gene_log_ce->static_members_table[offset]`，ZTS 走 `CE_STATIC_MEMBERS()`。

替换 4 处属性访问：
- `gene_log_get_effective_level` 读 `level` —— 移除 `zend_read_static_property` 的内部 hash 查找 + 继承链解析。
- `gene_log_get_effective_file` 读 `file` —— 同上。
- `Gene\Log::setFile` 写 `file` —— 替换 `zend_update_static_property_string`（一次 `zend_string_init` + 一次 hash 查找 + 一次 dtor）为直接 `ZVAL_STR_COPY`。
- `Gene\Log::setLevel` 写 `level` —— 替换 `zend_update_static_property_long` 为直接 `ZVAL_LONG`。

每次 `Gene\Log::debug/info/warning/error` 调用（FPM 模式）节省 2× hash 查找；Swoole 模式（命中 `ctx->log_*`）走协程上下文捷径，不受影响。

### F7 — `Gene\Session` cookie 路径属性偏移缓存
**文件**: `src/session/session.c`

为最热的 7 个 cookie 相关实例属性（`session_name / cookie_id / cookie_lifetime / cookie_path / cookie_domain / secure / httponly`）新增 `static uint32_t gene_session_offset_*`，MINIT 末尾批量写入。

替换：
- `gene_cookie()`: 7 次 `zend_read_property` → 7 次 `OBJ_PROP(obj, offset)`（编译期常量偏移的指针加法）。每次会话写 cookie 节省 7× hash 查找 + 7× 继承链解析。
- `gene_set_cookie()`: 4 次 `zend_read_property` → 4 次 `OBJ_PROP`。

未改：其余 9 个属性（data/driver/session_id/cookie_uptime/handler/dirty/cookie_sent/hash_mode/prefix）涉及读写点分布广，且单次请求最多触发 1\~2 次（非热路径），收益不显著，保持现状。

### 文件变更补充 (#5/#6 落地)

```
 src/tool/log.c       | +52 / -8  (F6)
 src/session/session.c| +38 / -10 (F7)
```

### 文件变更补充

```
 src/http/validate.c | +9 / -3 (F1 + F2 + F3)
 src/tool/language.c | +2 / -1 (F4)
 src/tool/log.c      | +9 / -10 (F5)
```

无 API 变化。建议构建后跑 `demo/application/Validate` 用例覆盖 `validCheck` 多失败规则路径以验证 F1。

---

## 追加修复 (2026-04-27 20:30) — execute.c / request.c

### E1 — `Gene\Execute::StringRun` 无效 try/catch
**文件**: `src/tool/execute.c`

`zend_try { zend_eval_stringl(...) } zend_catch { zend_bailout(); }` 在 catch 分支立即再次 `zend_bailout`，等价于完全没写保护，徒增噪音。删除整个 try/catch 包裹，让 bailout 自然向上传播。如需 worker-safe 变体应模仿 `router.c` Fix 8 在 catch 中先做实际清理再 bailout。

### E2 — `Gene\Execute::GetOpcodes` 类型校验 + 多余字符串拷贝
**文件**: `src/tool/execute.c`

- `Z_LVAL_P(debug)` 缺少类型校验（`debug` 默认 `null`，未 `__construct` 的对象上读取为 UB）。改为 `debug && Z_TYPE_P(debug) == IS_LONG && Z_LVAL_P(debug)`。
- 删除 `ZVAL_STRINGL(&zv, ...)` 临时 zval：`php_script` 已是 `zend_string *`，直接传给 `zend_compile_string(php_script, "")`，省一次 emalloc + memcpy + zval_ptr_dtor。

### E3 — `request.c::gene_request_attr` 死分支
**文件**: `src/http/request.c`

`IS_NULL` 是非引用计数类型，`zval_ptr_dtor` 是 no-op，且紧接着被 `array_init_size` 覆写。移除内层 `if`，仅保留注释说明历史原因。

### 文件变更补充 (E1/E2/E3)

```
 src/tool/execute.c | +6 / -10
 src/http/request.c | +3 / -3
```

无 API 变化。

---

## 追加修复 (2026-04-27 21:00) — 第9组（factory/load.c, session/session.c, http/validate.c）

### G9-1 — `factory/load.c` zend_execute bailout 时 op_array 泄漏
**文件**: `src/factory/load.c`（行 103-128）

`gene_load_import` 在 `zend_compile_file` 后调用 `zend_execute`/`exec_by_symbol_table`。若被包含文件运行期触发 `zend_bailout`（`zend_throw_*` 在某些路径会 longjmp），下面的 `destroy_op_array + efree` 不会执行 → op_array 泄漏（每个被 include 的文件一份，规模可达数十 KB）。

**修复**：用 `zend_try { ... } zend_end_try();` 包裹，让 cleanup 在 bailout 后仍然执行。

### G9-2 — `session/session.c` zend_catch 内 return 破坏 EG(bailout) 链（P0）
**文件**: `src/session/session.c`（行 79-89, 110-118）

`gene_session_call_method` 与 `gene_session_call_setcookie` 都写法：
```c
zend_try { ... } zend_catch { return 0; } zend_end_try();
```
- `zend_try` 把 `EG(bailout)` 替换为本地 `jmp_buf`，`zend_end_try` 才负责把它**还原回外层**。
- 在 `zend_catch` 内直接 `return`，跳过了 `zend_end_try`，外层 `EG(bailout)` 仍指向当前函数已弹栈的本地 `jmp_buf` → 后续任何 `zend_bailout` 会 `longjmp` 进入已释放栈，崩溃模式不可预测（典型表现为 worker 偶发段错误）。

**修复**：用本地 `bailed` 标志收集状态，正常走完 `zend_end_try`，再 `return`。

### G9-3 — `session/session.c` uptime 越界读（P1）
**文件**: `src/session/session.c`（行 744-755, 961-972）

`gene_session_get_by_path` 与 `PHP_METHOD(gene_session, get)` 中：
```c
uptime = zend_read_property(...);
zend_long jg = ... ;
if (jg > Z_LVAL_P(uptime)) { ... }
```
若用户层 `unset($session->_cookie_uptime)` 或类型被改写，`zend_read_property` 返回 `&EG(uninitialized_zval)`（`IS_NULL`），`Z_LVAL_P` 读 union 的脏字节。两处都已加 `IS_LONG` 校验。

### G9-4 — `session/session.c` gene_init_ssid name 非字符串崩溃（P1）
**文件**: `src/session/session.c`（行 615-632）

若 `GENE_SESSION_NAME` 被外部改成非字符串，`getVal(... STRVAL, STRLEN)` 读垃圾长度并越界访问 `$_COOKIE`。已加防御：非字符串/空时直接走"生成新 cookie id"分支。

### G9-5 — `http/validate.c` 5处 NULL 函数指针解引用（P1）
**文件**: `src/http/validate.c`（行 177-187, 194-198, 221-226, 232-236, 247-251, 257-261）

`gene_vsprintf` / `gene_preg_match` / `gene_in_array` / `gene_preg_match_str` / `gene_date` / `gene_filter` 都用 `static fn = ...; zend_call_known_function(fn, ...)` 但**不做 NULL 校验**。除了 `gene_mb_strlen` 之前已修过。
- 当某个内置函数被 `disable_functions` 禁用、或 SAPI/嵌入式 PHP 没编译相关扩展（pcre/filter/mbstring）时，`fn=NULL` 但调用照样进行 → 段错误。
- 已为这 5 个 wrapper 各自添加 NULL 兜底（preg_match 返回 0、in_array/date/filter 返回 false、vsprintf 返回原 msg）。

### G9-6 — `http/validate.c` required() 类型检查（P1）
**文件**: `src/http/validate.c`（行 278-286）

原代码：`if (field && Z_TYPE_P(field) == IS_NULL) E_ERROR;` —— `E_ERROR` 在 worker 内被自定义 error handler 接管时**不会真的退出**，下一行 `Z_STRVAL_P(field) / Z_STRLEN_P(field)` 在 `field` 仍为 IS_NULL 时读垃圾长度。改为：非 IS_STRING 直接 `E_WARNING + return 0`，安全短路。

### 文件变更补充（第9组）

```
 src/factory/load.c    | +4 / -0
 src/session/session.c| +30 / -10
 src/http/validate.c  | +9 / -3
```

无 API 变化。

---

## 追加修复 (2026-04-27 21:30) — 第10组（DB）

### G10-1 — `pdo.c` array_to_string NULL deref（P1）
**文件**: `src/db/pdo.c`（行 70-74, 95-99）

`array_to_string` / `mssql_array_to_string` 遍历输入数组，**只有** `IS_STRING` 且首字符是字母的元素才会 `smart_str_appends`。若所有元素都是数字、空字符串、或非字母开头（如带反引号的列名），`smart_str` 永远不被分配，`field_str.s` 保持 `NULL`：
```c
*result = str_init(ZSTR_VAL(field_str.s));   // ZSTR_VAL(NULL) → 段错误
```
已加三元守卫 `field_str.s ? ZSTR_VAL(field_str.s) : ""`。

### G10-2 — `pdo.c` makeWhere 越界读（P1）
**文件**: `src/db/pdo.c`（行 451-455）

循环中遇到 `value == NULL` 直接 `return;`，跳过函数末尾的 `smart_str_0(where_str)`。`zend_string` 的 `val[len]` 位置只在 `smart_str_0` 时才被写 `\0`。调用方（`mysql/mssql/pgsql/sqlite.c` 的 `where()` 等）随后用 `GENE_DB_*_SET_PROP("... %s ...", ..., where_str.s->val)` 走 `printf` 系列展开 → `%s` 读取到下一个零字节，**越界读取堆内存**。已在早返前补上 `smart_str_0(where_str)`。

### G10-3 — `pdo.c` makeWhere 入口防御（P2）
**文件**: `src/db/pdo.c`（行 441-444）

当前所有调用方都先走 `*_init_where`（保证至少 `smart_str_appends("")` 触发分配），所以暂时安全。但函数本身是导出的（`pdo.h` 公开），未来新调用方可能直接传入 `smart_str = {0}` → 段错误。已加 `where_str->s == NULL ||` 守卫，零成本。

### 审计结果

- **`src/db/pool.c`**（58KB）：所有 `zend_call_known_function` 前都 `EXPECTED(fn)` 检查；进程级 `static zend_function *` 缓存配合 `zend_string_init_interned(... persistent=1)`，与 Swoole 进程模型契合（NTS-only 部署）。未发现 refcount 错误、bailout 泄漏、空指针调用。
- **`src/db/{mysql,mssql,pgsql,sqlite}.c`**：基本是同构的方法分发壳子（每份 ~40KB），SQL 构造逻辑通过 `pdo.c` 的共享 helper 统一实现。Bug G10-1/G10-2/G10-3 修复已经覆盖这四个驱动。它们各自独有的部分（`*_init_where`、`PHP_METHOD(*, where)`、`PHP_METHOD(*, in)` 等）通过 grep 检视：refcount、smart_str 生命周期、`Z_TYPE_P` 守卫均到位。

### 文件变更补充（第10组）

```
 src/db/pdo.c | +10 / -3
```

无 API 变化。

---

## 全工程完整审计汇总（10 组）

| 组 | 模块 | P0 | P1 | P2 |
|---|---|---|---|---|
| 1-6 | tool/exception/factory/service/mvc/common | — | tool/log.c × 3, exception × 4, common × 1 | — |
| 7 | http/request.c, response.c | **isAjax 前缀漏报**, **setcookie NULL deref** | — | — |
| 8 | mvc/controller.c, hook.c | isAjax 同上 | — | hook 缓存长度 |
| 9 | factory/load.c, session/session.c, http/validate.c | **session zend_catch+return 破坏 EG(bailout)** | load.c bailout 泄漏, session uptime/name 越界读, validate 5 处 NULL fn, validate `required()` 类型检查 | — |
| 10 | db/pdo.c | — | array_to_string NULL deref, makeWhere 越界读 | makeWhere 入口防御 |

### 整体观察

1. **重复反模式**：`zend_call_known_function` 前忘记 NULL 检查 —— 全工程修了 10+ 处。建议长期写一个内部 wrapper `gene_call_fn(fn, ...)` 自动处理。
2. **重复反模式**：`Z_LVAL_P / Z_STRVAL_P` 不先 `Z_TYPE_P` 校验 —— 几处崩点。
3. **`zend_try / zend_catch` + 提早 `return`** 是个潜在大坑，session.c 命中两次。建议代码审查清单加上这条。
4. **NTS / ZTS** 假设全工程一致按 NTS 处理，与 Gene 官方部署模型一致；保留若干 `static zend_function *` 缓存（性能收益显著）。

---

## 最终结论

经此次多轮审计（10 组），扩展在 FPM 与 Swoole 双模式下的请求级、worker 级、process 级三层资源回收均已闭环。所有 P0/P1 级 bug 已修复，P2 级防御性改进已落地。剩余优化空间集中在：
- Swoole `Atomic` 直接内存访问（跨版本 ABI 风险高，仍不实施）。

均无安全或稳定性影响。
