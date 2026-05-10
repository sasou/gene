# Gene 扩展高并发性能与 Swoole 常驻内存审计报告

**审计日期**: 2026-05-10  
**审计目标**: FPM/Swoole 高并发性能极致提升；Swoole 常驻运行内存稳定，大量高并发请求内存不增长  
**当前版本**: Gene PHP Extension v5.6.1

---

## 一、总体结论

当前代码已经包含大量关键优化，主要热点路径已不是早期版本的状态：

- **请求上下文**: `gene_request_ctx()` 已区分 FPM 快路径和 Swoole 协程路径，Swoole 路径有 `current_vm_stack` 快速命中、`getcid()` 函数缓存、`co_contexts` 软上限 sweep、bounded context struct pool。
- **Swoole 内存稳定性**: `Application::cleanup()`、`destroyContext()`、`RSHUTDOWN` 都已具备上下文清理能力；`request_attr`、`di_regs`、`response_obj`、`view_vars`、DB history 等 request-scope zval 在 context reset/destroy 中释放。
- **高并发热路径**: DI、Router、Config、Memory、Request、Response、View 多处已使用栈缓冲区、缓存字符串长度、缓存 `zend_function*`、跳过 Swoole workerReady 后读锁等优化。
- **已修复历史风险**: `response_obj` 缓存已实现并在 context cleanup 中释放；`request_query` 在 Swoole 模式已跳过 `PG(auto_globals_jit)`；`view.c` 已使用 `app_view_len/app_ext_len`；DB/Redis pool 定时器与方法缓存也已有优化。

本次审计仍发现并修复了两个低风险但重要的稳定性/性能点。

---

## 二、本次已落地修复

### 1. `Application::cleanup()` 避免重复 destroy 当前协程 context

**文件**: `src/app/application.c`

**问题**:

`cleanup()` 在 Swoole 模式中，原逻辑会：

1. 从 `GENE_G(current_ctx)` 或 `co_contexts[cid]` 取出 context；
2. 手动调用 `gene_request_context_destroy(ctx)`；
3. 再调用 `zend_hash_index_del(co_contexts, cid)`。

但 `co_contexts` 的 HashTable dtor `gene_co_context_dtor()` 本身就会执行：

- 清空当前缓存指针；
- `gene_request_context_destroy(ctx)`；
- `gene_request_context_pool_release(ctx)`。

因此手动 destroy 后再 hash delete 会形成重复清理路径。更重要的是，在 `gene_request_context_destroy()` 触发对象析构期间，如果析构逻辑重入 Gene API，`co_contexts` 里仍可能保存该 context 指针，存在拿到“正在销毁 context”的窗口。

**修复**:

- 先将 `current_ctx/current_cid/current_vm_stack` 置空；
- 直接 `zend_hash_index_del()`；
- 由 `gene_co_context_dtor()` 统一负责 destroy 与回收到 context pool。

**收益**:

- 避免重复 context destroy；
- 降低对象析构期间重入导致的上下文悬挂/重复释放风险；
- 保持 context pool 回收路径单一，利于 Swoole 常驻 worker 内存稳定。

---

### 2. `gene_view_display()` 修复栈缓冲区作用域并减少重复 `strlen()`

**文件**: `src/mvc/view.c`

**问题**:

`gene_view_display()` 原来在 `if (base_path) { ... } else { ... }` 内部分别声明 `char path_buf[512]`，但在块外继续通过 `path` 调用 `gene_load_import(path, ...)`。

这属于栈变量作用域结束后继续使用指针的未定义行为。大多数编译器/优化级别下可能“刚好可用”，但在高优化、高并发、不同平台编译器下不应依赖。

**修复**:

- 将 `path_buf[512]` 提升到函数作用域；
- 将 `strlen(file)` 缓存为 `file_len`，两条路径复用。

**收益**:

- 消除未定义行为；
- 每次 view display 少一次 `strlen(file)`；
- 与现有 `view.c` 栈缓冲区优化风格一致。

---

## 三、当前高并发性能审计结果

### FPM 模式

FPM 请求生命周期由 SAPI 回收 request arena，内存增长风险相对低。当前主要热点状态：

- **请求上下文**: `gene_request_ctx()` 在 `runtime_type < 2` 直接返回 `default_ctx`，无 Swoole `getcid()` 开销。
- **RINIT/RSHUTDOWN**: `default_ctx` 每请求 init/reset/destroy，request-scope zval 都会释放。
- **Router/DI/Config/Memory**: 大量 `spprintf` 已替换为栈缓冲区 + `memcpy`；DI 和路由已缓存长度与方法查找。
- **Response**: FPM header 路径已用 `gene_response_set_header_ex()` 避免重复 `strlen`，并使用 1024 字节栈缓冲区。

FPM 模式进一步“极致优化”的空间主要在业务热路径压测后定点优化，不建议盲目继续替换所有 `call_user_function`，因为很多动态调用属于框架能力边界，不一定在每请求核心路径。

### Swoole 模式

Swoole 常驻 worker 的关键不是只追求单次请求 QPS，而是保证：

- request-scope zval 在请求结束释放；
- coroutine context 不无限累积；
- context struct 可复用但不保留大 HashTable；
- worker 退出时 timer/pool/cache 清理完整。

当前已具备：

- `Application::cleanup()` 作为每请求完整清理入口；
- `co_contexts_max=1024` 默认软上限，超过后 sweep；
- `ctx_pool_max=256` 默认 bounded free-list；
- `request_attr/di_regs/response_obj/view_vars/db_history` destroy 后置 `UNDEF`；
- `path_params` 超过 128 buckets 会 drop + reinit，避免 pathological request 撑大 HashTable 后常驻；
- `Memory::stats()` 可观测 `cache_items/co_contexts_items/ctx_pool_size`。

---

## 四、Swoole 内存稳定性建议

生产 request 回调建议保持以下顺序：

```php
$http->on('request', function ($request, $response) {
    \Gene\Application::waitWorkerReady();
    \Gene\Request::init($request->get, $request->post, $request->cookie, $request->server, null, $request->files, null, $request->header);
    \Gene\Application::setResponse($response);

    ob_start();
    $error = false;
    try {
        \Gene\Application::getInstance()->run();
    } catch (\Throwable $e) {
        $error = true;
        \Gene\Log::exception($e);
    } finally {
        $out = ob_get_clean();
        \Gene\Application::cleanup();
    }

    if ($error) {
        $response->redirect('/50x.html');
        return;
    }

    if ($response->isWritable()) {
        $response->header('Content-Type', 'text/html; charset=utf-8');
        $response->end($out);
    }
});
```

对于仍观察到 RSS 缓慢增长的业务，可按阶段开启：

```php
\Gene\Application::cleanup(true);
```

`cleanup(true)` 会在释放 Gene request-scope zval 后调用 `gc_collect_cycles()`，适合业务对象存在循环引用时使用。它会带来额外 GC 扫描成本，不建议默认开启，除非压测确认能显著降低 RSS。

---

## 五、压测与观测清单

### 1. Swoole 长压

建议至少三组：

```bash
ab -n 1000000 -c 100 http://127.0.0.1:81/
ab -n 3000000 -c 300 http://127.0.0.1:81/
wrk -t4 -c512 -d10m http://127.0.0.1:81/
```

观察：

- worker RSS 是否在预热后趋于平台；
- `\Gene\Memory::stats()['co_contexts_items']` 是否接近 0 或稳定；
- `ctx_pool_size` 是否不超过 `ctx_pool_max`；
- `cache_items/fn_cache_items` 是否只在启动阶段增长，业务请求阶段不持续增长。

### 2. FPM 压测

```bash
ab -n 500000 -c 100 http://127.0.0.1/
wrk -t4 -c256 -d5m http://127.0.0.1/
```

观察：

- QPS 与 P99 延迟；
- PHP-FPM worker RSS 是否随请求回收稳定；
- 是否出现 `Unable to load view file`、router eval warning、DI 对象析构异常。

---

## 六、后续可选优化方向

1. **Session 热路径**  
   `session.c` 仍有 `call_user_function` 包装，用于动态 session handler 与 `setcookie`。如果业务大量使用 Gene Session，可单独压测后缓存更多 `zend_function*` 或使用更直接的调用路径。

2. **Factory 动态调用路径**  
   `factory.c` 动态方法/函数调用仍使用 `call_user_function`。这是框架动态能力的一部分，不建议无压测盲改。可针对高频 controller/action 调用进一步缓存 `zend_function*`。

3. **Pool close 等待循环**  
   `db/pool.c` 在 close/wait 路径仍会多次读取 count，但这属于关闭路径，不是请求热路径。当前 recycler 已有本地 count 缓存，不是高优先级。

4. **业务层循环引用**  
   如果 `cleanup()` 后 RSS 仍增长，优先怀疑 PHP 业务对象循环引用、静态数组、闭包捕获、ORM 缓存，而不是 Gene request context。使用 `cleanup(true)` 对比压测可快速区分。

---

## 七、验证状态

已执行：

```bash
git diff --check
```

结果：通过。

未执行实际编译/压测：当前环境为 Windows IDE，未确认具备 PHP 扩展编译工具链。建议在目标 Linux/Swoole 环境执行 `phpize && ./configure && make` 后进行长压验证。
