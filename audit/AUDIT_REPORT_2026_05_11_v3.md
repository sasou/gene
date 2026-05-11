# Gene 扩展 FPM/Swoole 性能与常驻内存审计报告（v3）

**审计日期**: 2026-05-11 (v3)
**目标**: 框架功能代码审计与优化：FPM/Swoole 性能、Swoole 常驻模式内存泄漏
**范围**: `src/http/webscan.c` 及已审计过的热路径复核（`src/gene.c`、`src/router/router.c`、`src/di/di.c`、`src/mvc/view.c`、`src/http/request.c`、`src/session/session.c`、`src/db/pool.c`、`src/cache/redis_pool.c`）

---

## 一、总体结论

经过前 16 轮审计（详见同目录下 `AUDIT_REPORT_2026_03_24.md` 至 `AUDIT_REPORT_2026_05_11_v2.md`），框架请求主干已高度优化：

- **FPM 模式**：`gene_request_ctx()` 直接返回 `default_ctx`，零协程开销；路由、DI、工厂、请求读取等热路径已全面使用栈缓冲区 + 缓存长度 + `zend_call_known_function()`。
- **Swoole 模式**：`gene_request_ctx()` 具备 `current_vm_stack` 超快路径、`getCid()` 缓存、`co_contexts_max` 软上限与 bounded ctx struct pool；请求结束 `Application::cleanup(true)` + `workerStop` 的 `Pool::closeAll()`/`RedisPool::closeAll()`/`gc_collect_cycles()` 三重清理已落地。
- **常驻内存**：`gene_request_context_free_fields()` 完整释放 request-scope zval；`co_contexts` sweep、ctx struct pool、DB/Redis Pool closeAll 均使用快照遍历避免 yield 期间的迭代失效。

本轮新增发现并修复 1 处 webscan 热路径的非必要堆分配：**`Gene\Webscan::check()` 在启用时每个被扫描值都重建一次 PCRE 正则 zend_string**，在高并发 Swoole 常驻模式下对每请求产生可观的分配压力（N×2 次 `strpprintf` + 对应 `zend_string_release`）。本轮修复为进程级一次性缓存。

---

## 二、本次修复

### F1 - Webscan 正则 zend_string 改为进程级缓存

**文件**：`src/http/webscan.c`

**问题**：

`Gene\Webscan::check()` 启用后，框架会遍历 `$_GET`/`$_POST`/`$_COOKIE`/`HTTP_REFERER`，对每个扁平化后的值 `V` 和其键 `K` 各调用一次 `preg_match`。原实现：

```c
static int gene_webscan_match_pattern(const char *pattern, zval *value) {
    ...
    regex_str = strpprintf(0, "/%s/is", pattern);   /* 每次调用分配一个 zend_string */
    ZVAL_STR(&params[0], regex_str);
    ...
    zval_ptr_dtor(&params[0]);                      /* 释放 */
}
```

三种过滤规则（`get`/`post`/`cookie`）都是**进程生命期常量**，但这段代码在每个被扫描 item 上都会 `strpprintf` 构造一次完整的 `/pattern/is` zend_string 并立即释放，浪费了堆分配、`vsnprintf` 扫描与 refcount 操作。

例如一个有 20 个 GET + 20 个 POST + 20 个 COOKIE 参数的请求，在开启全部扫描时会触发 **120+ 次 `strpprintf` 分配和释放**。FPM 模式下由 ZMM 承担，Swoole 常驻模式下长期累积还会加剧 `emalloc`/`efree` 的内部碎片。

同时 `gene_webscan_stop_attack()` 对键做了一次多余的 `ZVAL_COPY + convert_to_string`：所有调用点都已保证 `key` 是 `IS_STRING`（`ZVAL_STR_COPY` 传入的 HashTable key、`ZVAL_LONG + convert_to_string` 处理的数字键、`ZVAL_STRING` 的字面量 `HTTP_REFERER`），这段 copy+convert 是纯粹的空转。

**修复**：

1. 新增 `gene_webscan_build_regex()`（一次性构建 `/pattern/is` 并用 `zend_string_init_interned(..., 1)` 转为 persistent interned）。
2. 新增 `gene_webscan_regex_get_cached()`、`gene_webscan_regex_post_cached()`、`gene_webscan_regex_cookie_cached()` 三个懒初始化 getter，每个内部使用函数级 `static zend_string *cached` 缓存。
3. 重构 `gene_webscan_match_pattern(const char *, zval *)` 为 `gene_webscan_match_pattern(zend_string *, zval *)`，直接复用缓存 regex，`ZVAL_STR(&params[0], regex)` 对 interned string 是零 refcount 操作，`params[0]` 无需 dtor。
4. 重构 `gene_webscan_stop_attack(zval *, zval *, const char *)` 为 `gene_webscan_stop_attack(zval *, zval *, zend_string *)`，并删除 `ZVAL_COPY(&key_str, key); convert_to_string(&key_str);` 与配套的 `zval_ptr_dtor(&key_str);`。
5. `PHP_METHOD(gene_webscan, check)` 的 4 处调用点相应改为传入对应的 `gene_webscan_regex_*_cached()`。

**收益**：

- Webscan 启用时，每请求消除 `2 × (GET条目 + POST条目 + COOKIE条目 + 1)` 次 `strpprintf` 堆分配 + `zend_string_release`。对典型 30+ 参数请求 ≈ 消除 60+ 次分配/释放。
- 消除每个被扫描值一次 `ZVAL_COPY + convert_to_string` 的 refcount/类型转换开销。
- Interned string 在 `preg_match` 内部作为 pattern cache key 更稳定（同一 `zend_string*` 指针），避免 PCRE 内部重复编译相同模式的风险（以 PCRE 内部 cache 的 key 策略为准，保守地消除了每次新 zend_string 带来的 cache miss 可能）。
- 行为完全保留：仍然是 `/pattern/is`，大小写不敏感、dotall，语义与 PHP 层 `preg_match` 一致。

**风险控制**：

- `zend_string_init_interned(..., 1)` 生成的是 persistent interned 字符串，进程关闭时由 Zend 接管；静态函数内 `static zend_string *cached` 只在首次调用时赋值一次，无并发写入（FPM 单进程串行、Swoole 为 worker 进程级 static）。
- `ZVAL_STR(&params[0], regex)` 对 interned 字符串不增加 refcount；`zval_ptr_dtor` 对 interned 字符串为 no-op，即便未来其他调用路径误 dtor 也是安全的。
- 模式字面量 `gene_webscan_get_filter`/`post_filter`/`cookie_filter` 未改动，语义等价。

---

## 三、复核结果（未发现需修改项）

### FPM 性能路径

- `PHP_RINIT_FUNCTION(gene)` 未涉及协程上下文开销。
- `request_query()` 在 FPM/CGI 保留 `auto_globals_jit` 激活 `_COOKIE/_ENV/_SERVER/_REQUEST` 的兼容路径。
- `router.c` 的 `get_router_content_run`/`get_router_error_run*`/`get_router_info` 均为栈缓冲 + 长键 heap fallback，已在前一版审计加固。
- `di.c` 单例注册路径正确地 `ZVAL_COPY` 一份给 class-key、原 zval 转交 name-key。
- `view.c` 模板编译的 28 对 `regex/replace` 已经是 process-lifetime interned；视图变量表为浅拷贝，`ZVAL_COPY`/COW 隔离正确。

### Swoole 性能路径

- `gene_request_ctx()` 具备 `current_vm_stack` 快路径和 `current_cid` 二次校验，cid 复用 / ctx 回收都有防护。
- `gene_get_coroutine_id()` / `gene_swoole_co_exists()` 使用 `zend_call_known_function` + 缓存 `zend_function*`。
- DB / Redis Pool 借还均走 C 层 named pool cache。

### Swoole 常驻内存

- `gene_request_context_free_fields()` 覆盖所有 request-scope zval（含 `request_attr`、`di_regs`、`response_obj`、`view_vars`、DB history）。
- `path_params` 内嵌在 ctx，`nTableSize > 128` 时 drop+re-init 防止单次暴涨后常驻膨胀。
- `co_contexts_sweep()` 双阶段（precise via `Swoole\Coroutine::exists()` + FIFO 回退）。
- `gene_request_context_pool_*` 上下界明确，`ctx_pool_max` 默认 256。
- `Pool::closeAll()` / `RedisPool::closeAll()` 使用快照遍历（前一版 v2 已加固）。

### 刻意未改项

- `validate.c::gene_preg_match_str()` 目前对 `GENE_VALIDATE_MOBILE` / `GENE_VALIDATE_DATE` 仍 `ZVAL_STRING` 重建。只在显式调用 `rule_mobile` / `rule_date` 时触发，非请求主干热点，保留现状以避免改动 API。
- `http/request.c` 合并 GET/POST 到 `$_REQUEST` 已按总元素数预分配；Swoole `init()` 传入显式 `$request` 时直接共享，已是最优。
- `session.c` 动态 handler 的 `call_user_function` fallback 路径保留，避免破坏用户可插拔 handler / 函数替换能力。

---

## 四、验证

```bash
git diff --check
git diff --stat
```

- `git diff --check` 通过；
- 本次代码改动仅 `src/http/webscan.c`。

未在当前 Windows IDE 环境执行扩展编译与 Swoole 压测，建议在目标 Linux + PHP + Swoole 环境下：

```bash
phpize && ./configure && make -j && make install
php -m | grep gene
```

同时建议：

1. 用开启 `Webscan::check()` 的场景跑 ab / wrk 压测，观察 `Gene\Memory::stats()` 与 worker RSS 的稳定性。
2. 验证恶意 payload（`' or 1=1--`、`<script>alert(1)</script>`、`UNION SELECT...`）仍能被三类过滤器捕获。
3. 确认空输入、嵌套数组、超大字符串、包含 NUL 字节的键不会触发新路径的异常（本次未改动输入解析与 flatten 逻辑）。

---

## 五、修改文件清单

- `src/http/webscan.c`
  - 新增 `gene_webscan_build_regex()` 以及三个 `gene_webscan_regex_*_cached()` 懒初始化函数。
  - `gene_webscan_match_pattern()` 签名改为 `(zend_string *regex, zval *value)`，取消每次调用的 `strpprintf` 分配。
  - `gene_webscan_stop_attack()` 签名改为 `(zval *key, zval *value, zend_string *regex)`，删除对键的 `ZVAL_COPY + convert_to_string` 冗余转换。
  - `PHP_METHOD(gene_webscan, check)` 的 4 个调用点改用新签名。
- `audit/AUDIT_REPORT_2026_05_11_v3.md` — 本审计报告。

---

## 六、后续建议

- 对 `validate.c::gene_preg_match_str()` 引入 `pattern 指针 → zend_string*` 的小型 LRU 缓存（命中率近 100%，但需评估是否值得引入框架复杂度）。
- Swoole 长期压测后，根据 RSS 观测结果决定是否需要对 `co_contexts`、`ctx_pool` 再做更细粒度的自适应 sweep。
- 在扩展测试集中新增一条 `webscan` 的端到端用例，固化本次优化的行为兼容性。
