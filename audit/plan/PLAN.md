# Gene 未实现功能与待验证项计划

> 本文件整理自 audit 历史报告，用于存放当前代码中尚未落地或已 revert 的功能，以及仍需 Linux/压测/回归验证的待办项。
> 与之对应，`AUDIT_REPORT_2026_07_30.md` 已在 5.6.9 中回退 F3，并移除相关实现；当前代码中 F3/F4 等功能不存在。

---

## 一、已 revert / 未实现的功能

### F3 `Gene\Controller::init()` 生命周期钩子

- **来源**：`AUDIT_REPORT_2026_07_30.md` §6 / §9.6
- **现状**：在 v5.6.9 中已回退（commit `fc10274`）。`Gene\Controller` 不再提供 `init()` 方法，控制器继续使用 `__construct` / `beforeAction`。
- **原因**：该钩子带来的复杂度超过收益。
- **待决策**：若后续确有 Yaf 语义需求，可重新设计为不破坏 `__construct` 约定的可选钩子。

### F4 路由级中间件管道

- **来源**：`AUDIT_REPORT_2026_07_30.md` §6
- **现状**：仅停留在建议阶段，未立项实现。
- **方案**：路由配置支持 `route.middleware = "Auth,RateLimit"`（或 `$router->middleware()`），派发前按序执行各 `Gene\Hook` 子类 `handle()`，任一返回 false 即中断并走 error 路径。复用现有 `gene_router_exec_hook_direct` 直派机制。
- **约束**：应在 O6 运行时验证全集打通后再立项，避免在缺乏回归证据时改动派发路径。

---

## 二、性能/压测量化后待立项的优化

### route_pc 全树预热

- **来源**：`AUDIT_REPORT_2026_07_12.md` WP-04
- **现状**：`gene.route_precompile=1` 当前为**惰性填充**（按叶子 HashTable 指针 memoize），首次请求仍需解析。
- **待实现**：在 `workerReady()` 后遍历路由树，预先生成所有叶子对应的 `gene_route_pc` 描述符。
- **前提**：先由 profile 证明首批请求收益，并验证 `workerReady` 后 `fn_cache` / 路由树冻结不变量。

### sweep batch/cursor 方案

- **来源**：`AUDIT_REPORT_2026_07_12.md` SW-P3；`AUDIT_REPORT_2026_07_30.md` M1
- **现状**：sweep 在 `count >= cap` 时做 O(N) 全表扫描；cap 偏小时每个新协程均触发扫描，存在 O(N²) 放大。
- **建议**：实现 cursor / batch 分批清理，避免重复全表扫描。需压测量化 `co_contexts_sweep_skipped`、`co_contexts_sweep_us`。

### route_precompile ASAN 验证与稳健替代设计

- **来源**：`AUDIT_REPORT_2026_06_20.md` P3
- **现状**：`route_pc` 已合入主线，默认关闭。
- **待验证**：Linux `phpize+make`（含 `--enable-debug`/ASAN）+ 三类路由（MCA/字符串/闭包）+ hook（`clearAll`/before/after）/error/404 全回归；wrk/ab 对比 QPS/P99；确认无 zend_mm 泄漏。
- **替代设计**：若 `fn_cache` 在 `workerReady` 后无法保证冻结，将描述符中缓存的 `zval*` 改为 key 字符串，`execute` 时做 1 次 `zend_hash_find`。

### FPM 热路径优化（需 profile 达标）

- **来源**：`AUDIT_REPORT_2026_07_12.md` WP-05
- **待实施**：
  - 路由注册 key builder 重构，减少 `strtok` / `snprintf`。
  - SQL 构造链 property 直取优化。
  - `char*` 缓冲 stash 池化（06-20 M5）。
  - `fn_cache` 只读诊断接口（06-20 M4）。
- **约束**：必须通过 `tools/acceptance` 的 profile 准入脚本，否则不得进入主线。

### fn_cache / 连接池可选演进

- **来源**：`AUDIT_REPORT_2026_05_25.md` §9.1
- **待评估**：
  - `fn_cache` LRU 容量治理。
  - 连接池 overflow 硬熔断。
  - `named_cache` 改用 `pemalloc`。

---

## 三、待 Linux 环境验证的阻塞项

### O6 运行时验证全集

- **来源**：`AUDIT_REPORT_2026_07_30.md` §五
- **内容**：
  - ZTS/NTS 双构建零告警。
  - ASAN/Valgrind 无内存错误。
  - 百万请求/24h RSS 长跑。
  - CAS/pool 压测。
  - dlsym 符号可见性验证。
- **状态**：代码已静态实施，`php -l` 通过；**所有运行时验证均因缺少 Linux 环境而悬置**。

### O7 Linux 回归 9 项失败

- **来源**：`AUDIT_REPORT_2026_07_30.md` §五 / `AUDIT_REPORT_2026_07_12.md` §7.7
- **待处理**：
  - DB 驱动 `connect()` 二选一逻辑。
  - Cache / Language / Http 夹具 triage。
  - Mvc 失败项：F3 已回退，相关用例需重新评估或移除对 `init()` 的依赖。

### 07-03 遗留待验证

- **来源**：`AUDIT_REPORT_2026_07_03.md` §8.10
- **待办**：
  - Linux + phpize + make 编译验证。
  - 1.3 identifier quote 全量 SQL 回归。
  - 1.4 CAS 递减 Swoole 高并发压测。
  - dlsym `_ZN6swoole9Coroutine11get_by_cidEl` 符号可见性验证。
  - 预引号包裹注入向量回归用例。

### C-API 兼容矩阵

- **来源**：`AUDIT_REPORT_2026_07_12.md` WP-04
- **待办**：整理 Swoole C-API 调用点（`get_by_cid`、`exists` 等）的版本兼容矩阵，并补充回退路径。

---

## 四、代码迁移/清理

### 剩余 function-local static 函数指针缓存迁移

- **来源**：`AUDIT_REPORT_2026_07_03.md` §10.2 / 状态表
- **现状**：`GENE_CG_FN_LOOKUP` 基础设施已建，`benchmark.c`、`log.c`、`common.c`、`pdo.c`、`validate.c` 已迁移。
- **待迁移**：`redis_pool.c` 等文件中剩余的 `static zend_function *` 局部缓存。Swoole 不支持 ZTS，实际风险低，但应逐步对齐仓库约定。

---

## 五、可选演进（05-25 记录）

- **Swoole RSHUTDOWN 守卫**：明确 RSHUTDOWN 仅在 worker 退出/reload 触发，防止误配置导致协程上下文被提前销毁。
- **file_cache_only 压测回归**：opcache `file_cache_only=1` 场景下的跨请求 UAF 回归。
- **persistent PDO / 外部代理部署建议**：生产部署文档与灰度验收模板。
- **FPM worker RSS 与 `pm.max_requests` 验收**：依赖人工提供生产等价环境和基线数据。

---

## 六、如何维护本计划

- 每轮审计完成后，将**仍未实现/未验证**的项从审计报告迁移到本文件。
- 已落地或验证通过的项应及时删除或标注 `✅ 已完成`。
- 新立项前必须在 `tools/acceptance` 拿到 profile/ASAN 证据，避免无依据的大范围改动。
