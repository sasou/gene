# Gene 扩展内存与高并发极致优化审计方案

对 Gene 扩展在 FPM / Swoole 两种运行模式下做新一轮审计，聚焦常驻内存（RSS）极致压缩与高并发吞吐极致提升，输出逐项方案（含收益/风险评估），本轮不改代码。

## 背景与现状

已完成 20+ 轮审计（`audit/` 下 22 份报告），热路径主要优化（栈缓冲、interned string、worker_ready 免锁读、ctx 结构池、直派/闭包派发替代 eval、PDO/Redis 连接池）均已落地。本轮针对**剩余未实施项 + 新发现项**整理终极方案。

## 一、内存极致优化项

### M1. 持久缓存（GENE_G(cache)）膨胀治理 — 高收益
- **问题**：路由树/配置/事件全部存于 pemalloc 持久 HashTable，`Gene\Memory::set` 无 LRU/容量上限；业务若用 Memory 做数据缓存，RSS 只增不降（Swoole worker 永不回收）。
- **方案**：
  - 新增 `gene.cache_max_bytes`（或条目数上限）INI + 近似 LRU（带访问时间戳的惰性淘汰，写入时触发）。
  - 区分"框架元数据区"（路由/配置，启动后只读，不淘汰）与"业务数据区"（可淘汰），按 key 前缀或 set 入口分区。
- **收益**：杜绝业务误用导致的无界 RSS；**风险**：中（需保证 worker_ready 后框架区只读不受影响）。

### M2. 路由树持久结构紧凑化 — 中收益
- **问题**：路由树每个 path 段一个嵌套 HashTable（最小 8 桶 ≈ 几百字节），叶子带 `run/src/frun/hook` 多个 zend_string 副本；大量路由（千级）时持久内存数 MB。
- **方案**：启动注册完毕（workerReady）后做一次"压实"：`zend_hash_rehash` + 缩容到实际元素数；叶子内重复字符串（同一 hook 名等）做持久 interned 去重池。
- **收益**：路由密集应用持久内存降 30–50%；**风险**：低（只在 workerReady 一次性执行）。

### M3. ctx 结构池与 co_contexts 上限联动 — 低收益、低风险 ✅ 已实施
- **现状**：`ctx_pool` 无上限收缩策略（`ctx_pool_prewarm` 仅预热）；突发并发后池可能滞留大量空闲 struct。
- **方案**：sweep 时（`gene_co_contexts_sweep`）顺带把 `ctx_pool_size` 收缩到 prewarm 水位；或加 `gene.ctx_pool_max`。
- **实施状态**：`gene.ctx_pool_max` INI 已存在，`gene_request_context_pool_release()` 已实现上限检查（见 `src/gene.c`），池溢出时直接 efree。

### M4. fn_cache 闭包持有的隐性引用 — 低收益
- **现状**：`gene_fn_cache_store` 按对象 handle 去重，已有界；但闭包捕获的 use 变量（可能含大对象）在 worker 生命周期常驻。
- **方案**：文档化约束（路由闭包勿捕获大对象）+ 可选 `Router::freezeClosures()` 诊断接口输出 fn_cache 占用。

### M5. 请求级 zend_string/数组复用 — 中收益（Swoole）
- **问题**：每请求 `ctx->method/path/module/controller/action` 均 emalloc；`request_attr` 数组每请求重建。
- **方案**：ctx 结构池回收时保留这些缓冲（按容量记录，下次复用，类似 smart_str 复用），`path_params`/`request_attr` 用 `zend_hash_clean` 复用而非销毁重建（已在 reset 部分做，确认覆盖 request_attr）。
- **收益**：每请求省 5–8 次 emalloc/efree；**风险**：中（需小心 reset 语义与引用计数）。

### M6. FPM 模式 RSHUTDOWN 成本
- **现状**：FPM 下大部分由 arena 释放，`php_gene_close_request_globals` 已精简。仅核对 `fn_cache` FPM 每请求 destroy（闭包路由在 FPM 下重复 ReflectionFunction+读源文件，见 P6）。

## 二、高并发极致提升项

### P1. `Swoole getcid()` 跨 PHP 调用消除 — 高收益
- **问题**：`gene_get_coroutine_id()` 走 `zend_call_known_function`（PHP 函数调用 ≈ 数百 ns）。vm_stack 快路径已覆盖多数场景，但 yield 后（每次 IO 之后第一次 GENE_REQ）必走慢路径。
- **方案**：编译期检测 swoole 头文件可用时直接调用 `swoole_coroutine_get_current_id()` C API（dlsym 或弱符号运行时解析，失败回退 PHP 调用）。
- **收益**：每次协程切换后的首次上下文解析从 ~300ns 降至 ~5ns；**风险**：中（需处理 Swoole 未加载/版本差异，保留回退）。

### P2. 响应输出路径：`zend_call_known_function` 批量化 — 中收益
- **现状**：Swoole 下每个 header/cookie 一次 PHP 方法调用。
- **方案**：在 ctx 内攒 header 数组，`end()` 时一次性循环调用（或调用 Swoole `Response::header` 的数组形式若可用）；redirect/end 保持现状。
- **风险**：中（需保持 header 时序语义）。

### P3. 路由派发 eval 残余路径压缩 — 中收益
- **现状**：direct/closure 路径已覆盖常见情形，但 `get_router_info` 每请求仍做多次 `zend_hash_str_find`（hook:before/after、hsrc、fcl 共 6–10 次）。
- **方案**：路由注册完成后（workerReady 压实阶段，与 M2 合并）把叶子解析为预编译 C 结构体（指针直引 before_src/after_src/fn 槽位），dispatch 时零哈希查找。
- **收益**：每请求省 6–10 次哈希查找 + snprintf；**风险**：中高（结构改动大，需充分回归）。

### P4. `factory.c` 残余 `call_user_function` 热路径 — 低中收益 ✅ 已实施
- **现状**：`gene_factory_call` 仍有 `ZVAL_STRINGL(&function_name)` + `call_user_function` 分支（每次分配/释放一个 zend_string）。
- **方案**：统一改为 `zend_hash_str_find_ptr(&ce->function_table)` + `zend_call_known_function`（与 response.c 的方法缓存模式一致，按 ce 缓存 fn 指针）。controller action 派发每请求至少 1 次，收益确定。
- **实施状态**：`gene_factory_call()` 已使用 `zend_hash_str_find_ptr(&ce->function_table)` + `zend_call_known_function` 快路径（见 `src/factory/factory.c` 行 174-197），仅未找到方法时回退 `call_user_function`。

### P5. db/pool.c `pool_get_count` 原子读开销 — 低收益 ✅ 已实施
- **现状**：每次 get/put 经 `zend_read_property` + Swoole\Atomic 方法调用。
- **方案**：缓存 Atomic 对象的 `zend_function*`（同 P4 模式）；recycler 循环内已缓存 count，剩余调用点同样处理。`redis_pool.c` 的 `call_user_function` 同理。
- **实施状态**：`pool.c` 已缓存 Swoole\Timer、Swoole\Coroutine 的 `zend_function*`（见 `src/db/pool.c` 行 97-107、126-135、165-174），`redis_pool.c` 同理。

### P6. FPM 闭包路由源码读取（ReflectionFunction+SplFileObject）— FPM 高收益 ✅ 已实施
- **问题**：FPM 下路由注册每请求执行，闭包路由每请求做反射+读文件（IO！）。
- **方案**：以 `文件路径+起止行+mtime` 为 key 把 `get_function_content` 结果写入持久缓存（FPM 下跨请求复用，opcache 同源失效逻辑）；Swoole 不受影响（仅启动一次）。
- **收益**：FPM 闭包路由应用每请求省 2 次文件 IO；**风险**：低中。
- **实施状态**：`src/router/router.c` 新增 `gene_closure_src_cache` 持久 HashTable（仅 `runtime_type < 2` 启用），`get_function_content()` 先查缓存，未命中时读文件后回填。按 `mtime` 失效，每次命中返回 `estrndup` 副本（调用方 `efree` 契约不变）。

### P7. 锁层面确认项（核对，无需改动预期） ✅ 已核对
- worker_ready 后读路径已免锁；写锁仅启动期。核对 `session.c`/`application.c` 中残余 `GENE_CACHE_WRLOCK` 是否可能在 worker_ready 后触发（若有则是潜在串行点）。
- **核对结果**：`session.c` 无 `GENE_CACHE_WRLOCK`。`application.c` 的 `load_file()` 全部写入点均经 `gene_memory_write_allowed()` 守卫（见 `src/cache/memory.c` 行 167-178），Swoole `workerReady()` 后写入被拒绝。不存在 worker_ready 后的串行点，无需改动。

## 三、实施优先级建议

| 优先级 | 项 | 模式 | 类型 | 状态 |
|---|---|---|---|---|
| 1 | P1 getcid C API 直调 | Swoole | 并发 | ✅ 已实施 |
| 2 | M1 持久缓存上限/LRU | 双模式 | 内存 | ✅ 已实施 |
| 3 | P4 factory 方法指针缓存 | 双模式 | 并发 | ✅ 已实施 |
| 4 | P6 闭包源码持久缓存 | FPM | 并发 | ✅ 已实施 |
| 5 | M2+P3 路由树压实+预编译派发 | 双模式 | 双收益 | 待实施（高风险，需单独分支） |
| 6 | M5 ctx 缓冲复用 | Swoole | 内存 | 待实施 |
| 7 | P2/P5/M3/M4/P7 | — | 收尾 | ✅ P5/M3/P7 已实施/核对 |

## 四、验证方案

- **内存**：Swoole worker 压测 10 万请求前后对比 `memory_get_usage(true)` + 进程 RSS（`/proc/self/status`）；FPM 用 valgrind/ASAN 单请求泄漏检测。
- **并发**：wrk/ab 压测对比 QPS 与 P99（demo 应用三类路由：MCA、字符串路由、闭包路由）；Swoole 模式开 `--enable-debug` 校验无 zend_mm 泄漏告警。
- **回归**：现有 demo + `clearAll` hook 路由、错误路由、连接池断连重连场景逐项回放。

## 备注

- 全部实施前逐项经你确认；高风险项（M1 分区、P3 预编译派发）建议单独分支验证。
- Windows IDE 环境无法编译，需在 Linux 环境 phpize+make 验证。

## 五、本轮实施记录（2026-06-18）

### 已落地项
- **P6**：FPM 闭包路由源码持久缓存已实施（`src/router/router.c`）。
- **P7**：锁层面核对完成，确认无 post-worker_ready 串行点。
- **P4/P5/M3**：核对确认已在前期审计中实施。

### 修改文件
- `src/router/router.c` — 新增 `gene_closure_src_cache` 持久 HashTable 及相关辅助函数
- `CHANGELOG.md` — 新增 Unreleased 版本记录

## 六、本轮实施记录（2026-06-19）

### 已落地项
- **P1（Swoole getcid C-API 直调）**：`src/gene.c` 新增 `gene_resolve_getcid_capi()`，经 `dlsym(RTLD_DEFAULT, "_ZN6swoole9Coroutine15get_current_cidEv")` 解析 `swoole::Coroutine::get_current_cid()`，每 worker 解析一次。`gene_get_coroutine_id()` 优先直调 C-API，失败回退已缓存的 `zend_function` PHP 路径。新增 `gene.swoole_getcid_capi` INI 开关（默认 1）。Windows 下 `#ifndef PHP_WIN32` 跳过 dlsym，自然回退。
- **M1（持久缓存业务分区上限 + 近似 LRU）**：`src/cache/memory.c` 新增 `gene_cache_lru_*` 系列函数与 `GENE_G(cache_lru)` 跟踪集，`gene.cache_max_items` INI（默认 0 不限）。仅跟踪 `cache_layer_memory_write_depth>0` 的业务写入；框架元数据/普通 `Memory::set` 不跟踪不淘汰。写时近似 LRU、超限从表头淘汰；`gene_memory_del`/`clean()`/MSHUTDOWN 同步跟踪集。删除/淘汰改用 `zend_symtable_str_find` + Bucket 直取 key 的 O(1) 实现。
  - **并发安全论证**：业务写入与淘汰均在 `GENE_CACHE_WRLOCK` 内、且不让出协程；Swoole NTS 单线程协程模型下与免锁读不会真正并发（与既有 `Gene\Cache` 请求期写入语义一致），ZTS 下的免锁读约束沿用既有"业务键不得跨协程并发读写"不变量，本项未引入新的竞争面。

### 修改文件
- `src/gene.c` / `src/gene.h` — P1 解析器 + 开关；M1 全局量 / INI / 析构
- `src/cache/memory.c` / `src/cache/memory.h` — M1 LRU 跟踪与淘汰
- `CHANGELOG.md` — Unreleased 记录补充 P1 / M1

### 待实施项（后续轮次，高风险需单独分支 + 充分回归）
- M2+P3（路由树压实 + 预编译派发）— 中高收益，结构改动大
- M5（ctx 缓冲复用）— 中收益，需小心 reset 语义与引用计数
- M2/M4/P2 — 收尾项

### 验证待办（Linux 环境）
- `phpize && ./configure && make`（Windows IDE 无法编译）。
- ASAN/valgrind 单请求泄漏检测，重点核对 M1 在 `cache_max_items>0` 下淘汰、`clean()`、MSHUTDOWN 三处持久 key 释放无泄漏/双释放。
- 设 `gene.cache_max_items=N` 压测，确认业务缓存 RSS 稳定在上限附近，且路由/配置不受淘汰影响。
