# Gene 扩展高并发性能与 Swoole 常驻内存审计报告（补充）

**审计日期**: 2026-05-10（补充）
**目标**: FPM/Swoole 高并发性能极致提升；Swoole 常驻 worker 在大量并发请求下 RSS 不增长
**版本**: Gene PHP Extension v5.6.1
**前置审计**: `audit/AUDIT_REPORT_2026_05_10.md`（同日先版）

---

## 一、结论

经全面复审 `src/app`、`src/cache`、`src/config`、`src/di`、`src/factory`、`src/http`、`src/mvc`、`src/router`、`src/session`、`src/gene.c`，当前代码已经处于经过 12 次专项审计后的成熟状态。请求热路径上的关键问题（堆分配、`spprintf`、锁竞争、上下文泄漏、对象 dtor 重入）已在历次审计中修复并附有 `[GENE_PERF]/[GENE_MEM]/[GENE_FIX]` 注释。

本轮审计仅发现 1 处**一致性遗漏**并已修复，其余热路径未发现新增可改进点。

---

## 二、本次落地修复

### `gene_view_contains()` 缓存 `strlen(file)`（一致性修复）

**文件**: `src/mvc/view.c`

**问题**:

`gene_view_contains_ext` / `gene_view_display` / `gene_view_display_ext` 都已经在函数开头缓存 `file_len = strlen(file)`，唯独 `gene_view_contains` 仍在两条分支中各调用一次 `strlen(file)`，与既定优化模式不一致。

```c
path_len = GENE_G(app_root_len) + GENE_G(app_view_len) + strlen(file) + ...;
...
path_len = sizeof("app/") - 1 + GENE_VIEW_VIEW_LEN + strlen(file) + ...;
```

**修复**:

将 `size_t file_len = strlen(file);` 提到函数作用域，两条分支统一复用 `file_len`。

**收益**:

- 每次 `Gene\View::contains()` 调用少 1 次 `strlen()` 扫描；
- 与同文件其它 view 函数风格一致，便于后续维护。

---

## 三、复审过的高频热路径（无需新改动）

以下点已经检查过现状，确认已经按照"栈缓冲区 + 缓存长度 + 持久 interned 字符串复用 + 单次 RWLOCK"的统一模式实现：

### FPM 与 Swoole 共用热路径

- `gene_request_ctx()`：FPM 直接返回 `default_ctx`；Swoole 协程路径有 `current_vm_stack` 命中、`getcid()` 缓存、bounded ctx pool。
- `gene_di_get()`：直接复用 cache 内 `IS_STR_INTERNED|IS_STR_PERMANENT` 的 `zend_string`，避免每次 DI 解析 `zend_string_init`。
- `get_router_content_run()`：`method/path` 不再每次重建；3 次 cache key（`:rt`/`:re`/`:cf`）合并为单次 RDLOCK 查询；method lowercase 走 32 字节栈缓冲区。
- `gene_factory_load_class()`：256 字节栈缓冲区 lowercase 后直接 `zend_hash_str_find_ptr(EG(class_table), ...)`，已加载类完全跳过 `zend_string_init + zend_lookup_class`。
- `Gene\Memory::*`、`Gene\Config::*`：所有 key 构造均为栈缓冲区 + `memcpy`，无 `spprintf`。
- `Gene\Response::header()`：`gene_response_set_header_ex(name, name_len, val, val_len)` 路径，`memcpy` + 1024 字节栈缓冲区，无 `snprintf/spprintf`。
- `Gene\Memory` 持久值取出：`gene_memory_zval_local()` 对 `IS_STR_INTERNED` 字符串直接 `ZVAL_STR` 共享指针，避免每个值 `emalloc + memcpy`。

### Swoole 常驻内存稳定性

- `Application::cleanup()` 前已修复重复 destroy（参见 v1 审计报告）；`co_contexts` dtor 统一负责 `gene_request_context_destroy + gene_request_context_pool_release`。
- `gene_request_context_free_fields()` 在 reset 路径上对所有 request-scope 数组（`request_attr / di_regs / response_obj / view_vars / db_*_history / path_params`）执行 `zval_ptr_dtor` 后 `ZVAL_UNDEF`，避免 worker 长期持有大 HashTable。
- `co_contexts_max=1024`、`ctx_pool_max=256`：context 数量与 free-list 都有软上限，超过即 sweep / 不再回收。
- `RSHUTDOWN` 在销毁 `co_contexts` / `resident_ctx` 之前先把 `current_ctx/current_cid/current_vm_stack` 置空，避免对象析构期间访问到正在销毁的指针。
- 持久 cache（`GENE_G(cache)/cache_easy`）只在 `workerReady()` 之前写入；之后 `gene_memory_write_allowed()` 拒绝写入，并由 `gene_memory_get` 系列在 workerReady 后跳过 RDLOCK。
- `gene.co_contexts_max` 默认值已下调至 1024（v5.4.x 由 8192 调低），降低应用忘记 `cleanup()` 时的内存占用基线。

### 其它已优化的 cold/warm 路径

- DB MySQL/MSSQL/PgSQL/SQLite SQL builder 改 `strpprintf + zend_update_property_str`，避免 `spprintf + zend_update_property_string + efree` 三步。
- `parser_templates()` 静态缓存 56 个 regex/replace 持久 interned 字符串，模板编译时不再每次 `zend_string_init/release`。
- `Gene\Pool / Gene\RedisPool` 在 `onWorkerExit` 阶段提供 `stopTimers()`，避免 Swoole `Timer::tick` 阻塞 reactor 退出。

---

## 四、刻意未改的项

以下点曾被列为"潜在优化"，本次确认**不做改动**，原因如下：

1. **`session.c` 的 `zend_string_init(path, strlen(path), 0)` ×3**
   仅在 `Gene\Session::get($path) / set($path, $val) / del($path)` 显式传 dot-path 时触发，且 path 通常较短（<64 字节）。改为 stack 缓冲区收益极小，且会让代码变复杂。如压测确认是热点再改。
2. **`session.c` / `factory.c` 的 `call_user_function`**
   用于动态 session handler 与动态 method/function，属于框架的扩展点边界。可以缓存 `zend_function*`，但这会破坏运行时替换 handler 的能力，且没有压测数据支撑。
3. **`exception.c` 内的 `spprintf`**
   仅在抛出异常 / 渲染异常 HTML 时执行，是冷路径，业务正常情况下不会进入。
4. **`router.c::get_router_content_*` 的 `spprintf`**
   仅在路由注册阶段（启动 / `workerStart` 之前）执行，结果会被存进持久 cache 直到 worker 重启，请求阶段不再触发。
5. **`db/pool.c` 的 close/wait 多次读 count**
   关闭路径，不在请求热路径上。

---

## 五、压测建议

```bash
# Swoole 长压
wrk -t8 -c1000 -d10m http://127.0.0.1:81/
ab  -n 3000000 -c 300 http://127.0.0.1:81/

# FPM 压测
wrk -t4 -c256 -d5m http://127.0.0.1/
ab  -n 500000 -c 100 http://127.0.0.1/
```

观察指标：

- worker RSS 应在预热（10w 请求左右）后趋于平台；
- `\Gene\Memory::stats()['co_contexts_items']` 在请求间应回归 0（`cleanup()` 完整执行）或随 `co_contexts_max` 软上限波动；
- `ctx_pool_size` ≤ `ctx_pool_max`（默认 256）；
- `cache_items / fn_cache_items` 仅在启动阶段增长，请求阶段稳定。

如发现 RSS 仍随并发持续增长，按以下顺序排查：

1. PHP 业务层是否存在 controller / service / model 之间的循环引用 → 用 `\Gene\Application::cleanup(true)` 触发 `gc_collect_cycles()` 对照；
2. 是否在 `workerStart` 之后又调用了 `Gene\Memory::set` 等持久写入 API（应被 `gene_memory_write_allowed` 拒绝并 `E_WARNING`）；
3. 业务 controller 是否持有 `static` 数组累积请求数据；
4. ORM / 第三方扩展是否在协程间共享 PDO / Redis 连接而未走 `Gene\Pool`。

---

## 六、验证状态

```
git diff --check   # 通过
```

未在 Windows IDE 环境编译 / 实际压测，建议在目标 Linux + Swoole 环境执行：

```bash
phpize && ./configure && make -j && make install
php -m | grep gene
```

随后按上节命令长压并采集 RSS / `Gene\Memory::stats()` 数据。

---

## 七、修改文件清单

- `src/mvc/view.c` — `gene_view_contains()` 缓存 `file_len`。
- `audit/AUDIT_REPORT_2026_05_10_v2.md` — 本报告。
