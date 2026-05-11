# Gene 扩展 FPM/Swoole 性能与常驻内存审计报告

**审计日期**: 2026-05-11  
**目标**: 框架功能代码审计与优化：FPM/Swoole 性能、Swoole 常驻模式内存泄漏  
**范围**: `src/gene.c`、`src/app`、`src/http`、`src/router`、`src/db/pool.c`、`src/cache/redis_pool.c`、`src/session`、`src/mvc/view.c`、`demo/public/swoole.php`

---

## 一、结论

本轮审计在已有多轮性能与内存治理基础上继续复核，整体请求热路径已经较成熟：

- FPM 模式下 `gene_request_ctx()` 直接返回 `default_ctx`，无协程上下文开销。
- Swoole 模式下请求上下文有 `current_vm_stack` 快路径、`getCid()` 缓存、`co_contexts_max` 软上限和 bounded ctx pool。
- 路由、DI、工厂、请求读取、连接池借还等核心路径大多已使用栈缓冲区、缓存长度、`zend_call_known_function()` 和 C 层 named pool cache。
- Swoole 示例入口已在请求 `finally` 中调用 `Application::cleanup(true)`，并在 `workerStop` 中关闭 DB/Redis 池和触发 GC。

本轮发现并修复 1 处 Swoole 常驻关闭路径的一致性问题：`RedisPool::closeAll()` 在第二阶段关闭池时直接遍历静态实例表；若 `close()` 内部发生 Channel pop 等 yield，存在遍历表期间被改动导致漏关闭或迭代失效的风险。DB `Pool::closeAll()` 已经使用快照规避，Redis 池应保持一致。

---

## 二、本次修复

### F1 - `RedisPool::closeAll()` 关闭阶段改为快照遍历

**文件**: `src/cache/redis_pool.c`

**问题**:

`Gene\Cache\RedisPool::closeAll()` Phase 2 原先直接对 `instances` 做 `ZEND_HASH_FOREACH_VAL` 并调用每个 pool 的 `close()`。在 Swoole 常驻环境中，`close()` 可能执行 Channel drain / pop / close 等操作；如果期间发生协程切换或业务触发新的 pool 注册/删除，直接遍历原 HashTable 存在风险。

**修复**:

与 `Gene\Pool::closeAll()` 对齐：

1. 先统计 `instances` 数量；
2. 使用 `safe_emalloc` 创建 `zval` 快照数组；
3. 对每个 pool `ZVAL_COPY` 保持引用；
4. 遍历快照调用 `close()`；
5. 逐个 `zval_ptr_dtor` 快照引用并 `efree(snapshot)`。

**收益**:

- 避免 `close()` 内部 yield 窗口导致静态实例表迭代失效；
- 防止部分 Redis pool 漏关闭，使 channel / Redis 对象能在 worker 生命周期内释放；
- 与 DB Pool 关闭逻辑保持一致，降低后续维护成本。

---

## 三、复核结果

### FPM 性能路径

- `PHP_RINIT_FUNCTION(gene)` 只 reset `default_ctx`，FPM 无 `co_contexts` / getCid 负担。
- `request_query()` 对 php-cgi / FPM 保留 `auto_globals_jit` 激活 `_COOKIE/_ENV/_SERVER/_REQUEST` 的兼容路径，未破坏 session cookie 等场景。
- `router.c` 热路径已使用栈缓冲区和直接 C 调度，未发现新增堆分配热点。
- `session.c` 的 session handler 调用已具备 `(class_entry, method) -> zend_function*` 小缓存；剩余 `call_user_function` 是动态 fallback，不建议无压测强改。

### Swoole 性能路径

- `gene_request_ctx()` 使用 `current_vm_stack` 快路径，常规非 yield C 调用链中无需每次调用 `Swoole\Coroutine::getCid()`。
- `Application::cleanup(true)` 能在请求结束删除当前 coroutine context，并可选触发 cycle GC。
- DB/Redis pool 借还路径已有 named pool C cache，避免每次命令调用 PHP 层 `getInstance()`。
- DB/Redis idle recycler 已修复 count drift 与 push 失败计数问题，并在关闭时 null 掉 channel property，使内部连接对象在 worker 生命周期内释放。

### Swoole 常驻内存

- `gene_request_context_free_fields()` 会释放 `request_attr`、`di_regs`、`response_obj`、`view_vars`、DB history 等 request-scope zval，并对异常膨胀的 path params 做重建。
- `co_contexts` 有软上限和 sweep；ctx struct pool 有 `ctx_pool_max` 上限。
- `demo/public/swoole.php` 已正确使用 `try/finally` 和 `cleanup(true)`，`workerStop` 已调用 `Pool::closeAll()`、`RedisPool::closeAll()`、`gc_collect_cycles()`。
- 本次修复后，DB Pool 与 RedisPool 的 `closeAll()` 都使用快照关闭，常驻关闭阶段更稳健。

---

## 四、刻意未改项

- `session.c` 动态 fallback：保留 `call_user_function`，避免破坏动态 handler / 函数替换能力。
- `factory.c` 动态调用 fallback：属于框架扩展点边界，已有直接函数表查找 fast path。
- `app/application.c` 的 `webscan` callback 调用：属于可配置用户回调，非正常请求热路径。
- `request.c::Request::init()` 合并 GET/POST 到 REQUEST：当前已按总元素数预分配；如果应用显式传入 `$request`，则直接共享，不再复制。

---

## 五、验证

已执行：

```bash
git diff --check
git diff --stat
```

结果：

- `git diff --check` 通过；
- 本次代码改动仅 `src/cache/redis_pool.c`。

未在当前 Windows IDE 环境执行扩展编译和 Swoole 压测。建议在目标 Linux + PHP + Swoole 环境执行：

```bash
phpize && ./configure && make -j && make install
php -m | grep gene
```

随后进行长压并观察 worker RSS、`Gene\Memory::stats()`、`co_contexts_items`、`ctx_pool_size`、DB/Redis pool stats 是否稳定。

---

## 六、修改文件清单

- `src/cache/redis_pool.c` — `RedisPool::closeAll()` Phase 2 改为快照遍历关闭。
- `audit/AUDIT_REPORT_2026_05_11.md` — 本审计报告。
