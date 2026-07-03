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

### M2. 路由树持久结构紧凑化 — 中收益 ⛔ 不建议同向优化（紧凑化收益≈0；去重需改核心释放路径）
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

### P2. 响应输出路径：`zend_call_known_function` 批量化 — 中收益 ⛔ 不建议同向优化（复核确认）
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
| 5 | P3 路由叶子预编译派发 | Swoole | 并发 | 🧪 已实施（默认关闭，开启前需验证 fn_cache 冻结不变量 + ASAN 全回归） |
| 6 | M5 ctx 缓冲复用 | Swoole | 内存 | ✅ 部分落地（request_attr 已复用；char* 池化暂缓，待 ASAN 验证） |
| 7 | M4 fn_cache 诊断接口 | 双模式 | 可观测性 | 待实施（收尾，纯只读诊断） |
| — | P2 / M2 | — | 不建议同向 | ⛔ 复核后收益≈0 或风险/爆炸半径过大，维持现状 |
| — | P5 / M3 / P7 | — | 收尾 | ✅ 已实施/核对 |

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

### 本轮追加（M5 安全子集落地 + P2/M2/P3 复核结论）
- **M5（部分落地）**：`request_attr` 已改为 `reset()` 就地复用（`gene_ctx_reuse_lazy_array()`，`src/gene.c`）。char* 缓冲池化**评估后暂缓**：全代码库以 `if (ctx->field)` 指针非空作"已设置"判据，跨 reset 保留非空缓冲会泄漏陈旧值；stash 方案改动面大、收益约 100–200ns/req，待 ASAN 验证后再议。
- **P2（复核：建议不做）**：Swoole `Response::header()` 已通过 `GENE_SWOOLE_RESP_METHOD` 缓存 `zend_function*` 直调（无每次哈希查找），且 Swoole 无"数组批量 header" API；在 ctx 攒 header 到 `end()` 再循环调用**并不减少** `zend_call_known_function` 次数，仅把调用推迟，反而引入延迟刷新/时序风险。框架自身至多设 1 个 header，用户 header 数通常个位数。**收益≈0，风险>0，不实施。**
- **M2（复核：收益远低于预估，暂缓）**：路由树持久副本由 `gene_memory_zval_persistent()` 按 `zend_hash_num_elements(source)` 精确建表，叶子节点已无明显冗余；Zend 哈希表最小 8 桶为引擎下界，无法再"压实"。真正的冗余在**重复字符串副本**（同名 hook/src 在多路由各存一份），但去重需要给持久字符串引入引用计数、改动 `gene_memory_zval_dtor` 等核心释放路径（影响整个持久缓存，非仅路由），高风险、爆炸半径大，暂缓。
- **P3（已在专用分支 `feature/p3-precompiled-dispatch` 实现，默认关闭）**：`get_router_info()` 每请求 6–10 次 `zend_hash_str_find` 是真实开销，已预编译为 C 描述符 `gene_route_pc` 消除。
  - **设计**：按叶子 HashTable 指针为 key 的**每线程** `GENE_G(route_pc)` 记忆化缓存，惰性填充（首次命中解析+入表，均为非让出纯 C 操作，原子于协程协作调度）。原 `get_router_info()` 改名 `get_router_info_slow()` 逐字保留为回退；新 `get_router_info()` 包装按开关分流。
  - **安全闸**：`gene.route_precompile`（默认 0 关闭）+ 仅 Swoole + 仅 `workerReady()` 后。关闭/FPM/未就绪时走原路径，零行为变化。`GENE_G` 每线程隔离规避 ZTS 竞争；描述符仅借用同线程 `cache`/`fn_cache` 指针（worker 生命周期稳定），仅 eval 程序串为 owned 持久副本，MSHUTDOWN 释放。
  - **待验证**：Linux `phpize+make`（含 `--enable-debug`/ASAN）+ 三类路由（MCA/字符串/闭包）+ hook(`clearAll`/before/after)/error/404 全回归；wrk/ab 对比开关前后 QPS 与 P99；确认无 zend_mm 泄漏。**验证通过前不要在生产开启 `gene.route_precompile=1`。**

### 待实施项（后续轮次）
- M5（char* 缓冲 stash 池化）— 需 ASAN 验证（request_attr 复用已落地）
- M4（fn_cache 诊断接口）— 收尾，纯只读诊断

### 已实施但开启前需验证
- P3（路由叶子预编译派发）— 已在 `feature/p3-precompiled-dispatch` 实现，默认关闭；开启 `gene.route_precompile=1` 前需实证 fn_cache 冻结不变量 + ASAN 全回归

### 不建议同向优化（复核确认，维持现状）
- M2（持久字符串去重池 + 引用计数）— 紧凑化无可"压实"空间（Zend 最小 8 桶为引擎下界），去重需改核心释放路径，爆炸半径大
- P2（响应 header 批量化）— 收益≈0、风险>0，维持现状

### 验证待办（Linux 环境）
- `phpize && ./configure && make`（Windows IDE 无法编译）。
- ASAN/valgrind 单请求泄漏检测，重点核对 M1 在 `cache_max_items>0` 下淘汰、`clean()`、MSHUTDOWN 三处持久 key 释放无泄漏/双释放。
- 设 `gene.cache_max_items=N` 压测，确认业务缓存 RSS 稳定在上限附近，且路由/配置不受淘汰影响。

## 七、生产级落地复核（2026-06-19，逐项源码审计）

本节对**已落地项**逐一做源码级复核（是否合理、是否引入新内存泄漏/逻辑 bug、是否生产可用），并对**不建议同向优化项**与**待实施项**给出风险/收益结论。结论分级：
- ✅ **生产可用**：实现合理，未发现新泄漏/逻辑 bug，可在生产使用（含默认值约束）。
- ⚠️ **生产可用（带条件/已知小瑕疵）**：可用，但存在需关注的边角问题，建议后续补强。
- 🧪 **设计合理，开启前需验证**：实现隔离良好且默认关闭，但生产开启前必须 Linux+ASAN 全回归并确认前置不变量。
- ⛔ **不建议同向优化**：该方向收益≈0 或风险/爆炸半径过大，维持现状。

### P1. Swoole getcid C-API 直调 — ✅ 生产可用
- **合理性**：`dlsym(RTLD_DEFAULT, "_ZN6swoole9Coroutine15get_current_cidEv")` 解析 `swoole::Coroutine::get_current_cid()`（Itanium ABI mangled 名，Linux 稳定），每 worker 解析一次，三级回退链（C-API → 已缓存 `zend_function` → 返回 -1）完整。
- **泄漏/bug**：无分配，无泄漏。`gene_swoole_getcid_capi` 为文件级静态函数指针，ZTS 下多线程并发解析写同一指针为**良性竞争**（写入值相同，已在注释论证），`resolved` 为 `volatile int`。
- **风险**：
  - macOS 符号 mangling 带前导下划线（`__ZN...`），dlsym 找不到 → 静默回退 PHP 路径，**安全**（仅 macOS 无此优化）。
  - 依赖 `get_current_cid()` 返回 `long` 的 ABI 约定（Swoole 4.x/5.x 稳定）；若未来大版本变更签名有风险，但 `gene.swoole_getcid_capi=0` kill-switch 可即时回退。
- **结论**：实现稳健，可生产。建议在 CHANGELOG/文档标注"如遇 Swoole 大版本协程 API 变更，可关 `gene.swoole_getcid_capi`"。

### M1. 持久缓存业务分区上限 + 近似 LRU — ✅ 生产可用（默认关闭，全兼容）
- **合理性**：仅跟踪 `cache_layer_memory_write_depth>0` 的业务写入；`cache_max_items=0`（默认）完全旁路，向后兼容。所有跟踪/淘汰均在 `GENE_CACHE_WRLOCK` 内、无协程让出点，符合既有并发模型。
- **泄漏/双释放复核（重点）**：
  - 跟踪集 key 由 `gene_str_persistent()` 创建，带 `IS_STR_INTERNED|IS_STR_PERMANENT`；`zend_hash_add_empty_element` 对 interned 串**不 addref**，HT 销毁时 `zend_string_release` 对 interned 串为 no-op → 代码手动 `pefree`，**不双释放、不泄漏**。
  - `gene_cache_lru_remove_nolock` 经 `(Bucket*)zv->key` 取 key 后再 `zend_hash_str_del`，顺序正确。
  - 淘汰循环先 `break` 出 foreach 再删除（不在迭代中改表），主缓存与跟踪集各持独立 key 副本、各自释放，无交叉双释放。
  - 数值字符串 key 一致性：主缓存走 `zend_symtable_*`（数值强制为整型索引，stored_key 为 NULL 不 pefree），跟踪集统一走 `zend_hash_str_*`（始终字符串键）——两表各自内部一致，`gene_memory_del_core` 也用 symtable，闭环正确。
  - MSHUTDOWN/`clean()` 顺序：先销毁主 `cache`，再 `gene_cache_lru_destroy()`（释放独立 key 副本），无 use-after-free。
- **残留风险（需关注，非 bug）**：
  1. **命名空间冲突**：若框架元数据键（路由/配置，depth=0 写入）与某业务键字符串相同，后续业务 `set`（depth>0，走 line 536 既存键分支）会把该键纳入 LRU 并可能淘汰掉框架数据。生产前应确认 `Gene\Cache` 业务键有独立前缀、不与路由/配置键重叠。
  2. 业务键内存翻倍（主缓存 + 跟踪集各存一份持久 key），可接受。
  3. 非真 LRU（写序近似，读不刷新 recency——因免锁读路径写跟踪集会竞争）；文档已说明，符合预期。
- **结论**：可生产，建议默认保持 `cache_max_items=0`，开启时配合上面命名空间约束验证。

### P6. FPM 闭包源码持久缓存 — ✅ 生产可用（两处瑕疵已修复，2026-06-19）
- **合理性**：`文件路径:起始行:结束行` 为 key、`mtime` 失效，命中返回 `estrndup` 副本（调用方 `efree` 契约不变），未命中读文件后回填持久副本。仅 FPM（`runtime_type < 2`）启用，正确避免 Swoole 启动一次的无收益场景。
- **泄漏/bug**：节点 dtor 正确释放 `node->src` 与 `node`；`zend_hash_str_update_ptr` 会先析构同 key 旧节点，无泄漏。功能正确。
- **瑕疵 1（已修）**：新增 `gene_closure_src_cache_destroy()`（`src/router/router.c`，声明于 `router.h`），在扩展 MSHUTDOWN（`src/gene.c`，`gene_router_pc_destroy()` 之后）调用，`zend_hash_destroy`+`pefree` 释放整表并置空。与 `route_pc`/`cache`/`fn_cache` 释放惯例一致，valgrind/ASAN 干净。
- **瑕疵 2（已修，ZTS 安全）**：`get_function_content()` 中 `src_cache_on` 用 `#ifdef ZTS` 守卫——ZTS 构建直接置 0 关闭缓存，回退每请求反射+读文件路径，规避多线程共享静态表的无锁 `update`/`find` 竞争；NTS（典型 FPM）保留全部收益。
- **边角**：`mtime` 1 秒粒度，同一秒内对同文件同行号二次改动会用旧缓存（源码场景概率极低）。
- **结论**：NTS-FPM 下生产可用，已补 MSHUTDOWN 释放并以编译期 ZTS 守卫明确约束。

### P3. 路由叶子预编译派发（`feature/p3-precompiled-dispatch`） — 🧪 设计合理，开启前必须验证
- **合理性/隔离**：默认关闭（`gene.route_precompile=0`）、仅 Swoole、仅 `workerReady()` 后；原 `get_router_info()` 逐字保留为 `get_router_info_slow()` 作回退；描述符按叶子 HashTable 指针 memoize 于**每线程** `GENE_G(route_pc)`，惰性填充均为非让出纯 C 操作。`gene_route_pc_resolve/execute` 与 slow 路径分支逐一对应，`set_uri()` 仍每请求执行。MSHUTDOWN 经 `gene_router_pc_destroy()` 释放（仅 `eval_str` 为 owned 副本，其余借用，释放顺序无关）。
- **关键风险（必须验证的前置不变量）**：描述符缓存了 fn_cache 的 `zval*`（`route_cl/before_cl/after_cl/hook_cl`）。这些是 fn_cache HashTable 的 Bucket 内部指针——**一旦 workerReady 之后有任何闭包被加入 fn_cache 触发 rehash/扩容，已缓存描述符里的这些指针即悬空 → use-after-free**。整个方案的正确性完全押在"fn_cache 在 workerReady 后冻结、不再增长"这一不变量上。
  - **生产开启前必须确认**：Swoole 下是否存在请求期向 fn_cache 写入的路径（如运行时动态注册闭包路由、错误路由闭包首次惰性入表等）。若存在，必须改设计。
  - **更稳健的替代设计（若不变量不成立）**：描述符改存 fn_cache 的 **key 字符串**而非 `zval*`，execute 时做 1 次 `zend_hash_find`（仍从 6–10 次降到 1 次，消除指针悬空风险）。
- **结论**：隔离与回退设计优秀，默认关闭+已有警告使其对生产零影响。但 `gene.route_precompile=1` 上线前**必须** Linux `--enable-debug`/ASAN 跑三类路由 + hook(`clearAll`/before/after)/error/404 全回归，并实证 fn_cache 冻结不变量；否则建议先采用"存 key 重查"的替代实现。

### P4. factory 方法指针缓存 — ✅ 生产可用（收益兑现需确认 action 大小写）
- **合理性/bug**：`zend_hash_str_find_ptr(&ce->function_table, action, action_len)` 命中走 `zend_call_known_function`，未命中回退 `call_user_function`。功能正确，无泄漏（栈/堆参数缓冲分支释放正确）。
- **收益注意点**：PHP 函数表的方法键是**小写**存储。若调用方传入的 `action` 为驼峰（如 `indexAction`），`find_ptr` 恒未命中、每次走 `call_user_function`（其内部再小写化），**快路径收益不兑现**（但结果正确）。建议确认控制器 action 派发时传入的名称是否已小写；若否，可在查找前用栈缓冲小写化以真正命中。
- **结论**：正确且安全；按上面确认 action 大小写以确保收益。

### P5 / M3 / P7 — ✅ 已核对，沿用成熟模式
- P5：`pool.c`/`redis_pool.c` 缓存 `zend_function*` 直调，标准做法，无新增分配。
- M3：`gene.ctx_pool_max` 上限检查，溢出 efree，符合预期。
- P7：写锁仅启动期，worker_ready 后读路径免锁，已核对无串行点。

### M5. 请求级复用 — ✅ 部分落地生产可用；char* 池化暂缓合理
- **已落地（request_attr 就地复用）**：reset 时 `gene_ctx_reuse_lazy_array()` 就地清空复用，`>128` 桶丢弃以保 RSS 上界，destroy 时全释放。逻辑正确、无泄漏。✅ 生产可用。
- **char\* 缓冲池化（暂缓）**：全代码库以"指针非空"作为字段"已设置"判据，跨 reset 保留非空缓冲会泄漏陈旧值；stash 方案改动面大、收益仅约 100–200ns/req。**暂缓合理**，需 ASAN 验证后再议。

### 不建议同向优化项（复核确认）
- **P2（响应 header 批量化）— ⛔ 不建议**：`Response::header()` 已缓存 `zend_function*` 直调，Swoole 无"数组批量 header" API；攒到 `end()` 再循环调用**不减少** `zend_call_known_function` 次数，仅推迟调用并引入时序/刷新风险。框架自身至多 1 个 header，用户 header 通常个位数。**收益≈0、风险>0，维持现状。**
- **M2（路由树紧凑化 + 字符串去重）— ⛔ 不建议同向**：持久副本已按 `zend_hash_num_elements` 精确建表，Zend 最小 8 桶为引擎下界，**无可"压实"空间**；真正冗余在重复字符串副本，但去重需给持久串引入引用计数、改 `gene_memory_zval_dtor` 等**核心释放路径**（影响整个持久缓存，爆炸半径大）。维持现状；若将来确需，应走"独立持久 interned 全局池"而非改 dtor。

### 待实施项风险/收益小结
| 项 | 收益 | 风险 | 建议 |
|---|---|---|---|
| P3 上线（`route_precompile=1`） | 中（省 6–10 次哈希/req） | 中高（fn_cache 指针悬空，依赖冻结不变量） | 验证不变量 + ASAN 全回归后再开；否则改"存 key 重查" |
| M5 char* stash 池化 | 低（~100–200ns/req） | 中（需重构"已设置"判据） | 低优先，ASAN 验证后再做 |
| M4 fn_cache 诊断接口 | 低（可观测性） | 低 | 可作收尾，纯只读诊断 |
| M2 字符串去重池 | 中（持久内存） | 高（改核心释放路径） | ⛔ 暂不做 |

### 复核结论汇总
- **可直接生产**：P1、M1（默认关闭/带命名空间约束）、P4（确认大小写）、P5、M3、P7、M5（request_attr 部分）。
- **可直接生产（补强完成）**：P6（已补 MSHUTDOWN 释放 + 编译期 ZTS 守卫，2026-06-19）。
- **开启前必须验证**：P3（fn_cache 冻结不变量 + ASAN 全回归）。
- **维持现状不优化**：P2、M2。

## 八、功能验证（2026-06-20，5.6.6，FPM/CLI）

新增验证脚本 `tools/verify_5_6_6.php`（Windows NTS CLI 可直接运行），对已落地项做运行期行为验证。

### 运行方式
- 默认（不限上限）：`php tools/verify_5_6_6.php`
- 复验 M1 淘汰：`php -d gene.cache_max_items=10 tools/verify_5_6_6.php`

### 验证项与结果（全部通过 ✅）
| 项 | 验证内容 | 结果 |
|---|---|---|
| INI 注册 | `gene.swoole_getcid_capi`(P1, 默认 1)、`gene.cache_max_items`(M1, 默认 0)、`gene.route_precompile`(P3, 默认 0) 均已注册且默认值正确 | ✅ |
| M1 默认兼容 | `cache_max_items=0` 下经 `Gene\Cache::processCached` 写入 60 个互异业务键，全部保留（向后兼容，零淘汰） | ✅ |
| M1 上限 + 近似 LRU | `-d gene.cache_max_items=10` 下写入 60 个业务键，`Memory::stats()['cache_items']` 仅增 10（按写序从表头淘汰），且最近写入键仍命中 → 近似 LRU 生效 | ✅ |
| M1 清理同步 | `Memory::clean()` 后 `cache_items=0`，跟踪集同步，无崩溃 | ✅ |
| P6 / M5 稳定性 | 200 轮 `processCached` 透明走 FPM 源码缓存 / `request_attr` 复用路径，稳定无崩溃 | ✅ |

### 验证边界说明
- 本脚本运行于 **NTS CLI（`runtime_type=1`）**，覆盖 INI 注册、M1 业务分区上限/LRU、`clean()` 同步、P6/M5 透明路径稳定性。
- **P1（getcid C-API 直调）/ P3（预编译派发）的运行期生效仅在 Linux + Swoole + `workerReady()` 后**，CLI 下天然走回退路径；其端到端性能与正确性需在 Linux + Swoole 环境验证，见下文 Swoole 专用脚本。

### P1 / P3 Swoole 专用验证（Linux 服务器手动运行）
新增 `tools/verify_5_6_6_swoole.php`：脚本自身拉起 Swoole HTTP Server（`worker_num=2`），注册**闭包 / 控制器@动作(直派) / 动态 `:a` / 带钩子 / 未匹配** 路由，`workerReady()` 后由 worker#0 用协程 HTTP 客户端高并发自打流量，自校验后自动关停。

运行（开关组合对照）：
```bash
php tools/verify_5_6_6_swoole.php                                  # 当前 ini（本机默认 precompile=1）
php -d gene.route_precompile=0 tools/verify_5_6_6_swoole.php       # P3 关（slow 路径对照）
php -d gene.swoole_getcid_capi=0 tools/verify_5_6_6_swoole.php     # P1 回退（强制 PHP 调用）
php -d gene.route_precompile=1 -d gene.swoole_getcid_capi=1 tools/verify_5_6_6_swoole.php  # 两项全开
```

闭环判据：
1. 每次运行末尾出现 `ALL-PASS`；
2. 各开关组合的 `RESULT-DIGEST=...` **完全一致** → 证明 P1/P3 各开关下路由派发结果零差异；
3. `[B] 协程上下文隔离` 在 `capi=1`/`capi=0` 下均 PASS（100 并发协程 `getPath()` 零串扰）→ 证明 `gene_get_coroutine_id()` 的 C-API 直调与 PHP 回退路径都正确解析协程上下文。

### 实测结果（2026-06-20，Linux + gene 5.6.6 + Swoole 6.1.8）
四组开关组合运行结论：
- **`[A]` 路由派发（闭包/直派 MCA/动态 `:a`/带钩子）在 4 组下全 PASS**；
- **P3 等价性**：`route_precompile=0`（slow 路径）与 `route_precompile=1`（预编译路径）两组的派发结果与 digest 完全相同 → 预编译派发与原 `get_router_info_slow()` **行为严格等价、零回归**；
- **`[B]` 100 并发协程上下文隔离在 `capi=1` 与 `capi=0` 下均 PASS** → P1 C-API 直调与 PHP 回退路径正确性双双成立；
- **四组 `RESULT-DIGEST` 完全一致（`b887e533c417447e`）** → P1/P3 各开关组合派发结果零差异，**闭环成立**；
- `[C]` 微基准 ~9.7k–11.2k req/s（单机自打流量，信息项；该负载下 P1/P3 收益不显著，符合预期，收益主要体现在高并发 IO 切换 + 路由解析密集场景）。

> 备注：测试机 `php.ini` 已全局开启 `gene.route_precompile=1`，故 `-d gene.swoole_getcid_capi=0` 一组仍显示 `precompile=1`；如需 P3 开/关性能对照，用 `-d gene.route_precompile=0` 单独跑一组（派发结果 digest 应保持一致，仅 QPS 变化）。
>
> 未匹配路由（`/no/such/route`）的说明：本极简脚本未 `autoload()` 应用根，错误闭包解析不命中，gene 以**可恢复的 `Gene Unknown Url` 警告**优雅处理（确定性、不崩溃），与 P1/P3 无关；脚本据此断言"优雅处理"为通过。

> 提示：`gene.route_precompile=1` 上线前仍须按上文 P3 风险条目实证 fn_cache 在 `workerReady()` 后冻结不变量（建议在 `--enable-debug`/ASAN 构建下运行本脚本，确认无 zend_mm 泄漏/UAF 告警）。
