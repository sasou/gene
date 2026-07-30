# Gene 扩展框架三向审计报告（内存泄漏 / 并发性能 / 功能完善）

> 审计版本：5.6.8（develop，HEAD `98241cd`）
> 审计日期：2026-07-30
> 审计方式：纯静态代码审查 + 历史审计报告（2026-07-03 / 07-12 / 07-13）交叉复核；**本轮仅出报告，未修改任何源码**
> 审计范围：`src/` 全模块，重点为历次审计未覆盖角度与遗留项闭环核查
> 结论约定：每条发现标注证据类型 ——「静态确认」或「需运行时验证」；本机（Windows）无法执行编译、ASAN/Valgrind、压测与长跑

---

## 一、总体结论

1. **未发现新的常规路径确定性内存泄漏**。FPM 请求级生命周期（RINIT/RSHUTDOWN 配对、`php_gene_close_request_globals`）、MSHUTDOWN 持久资源析构（cache/cache_easy/fn_cache/route_pc/closure_src_cache/LRU/连接池静态表）均已完整闭环；log.c、language.c、session.c 主体、memory.c LRU 写路径的分配/释放配对经逐路径核查无误。
2. **新发现 1 个跨请求 use-after-free 风险**（session 插件方法静态缓存，见 H1），威胁模型与 2026-05-29 view.c 已修复问题完全相同，属同类残留。
3. **Swoole 模式的主要内存风险仍是容量治理而非传统泄漏**：`cache_max_items=0` 无界（设计保留，仅有启动 NOTICE）与 cleanup 漏调两个 P1 项维持开放；对应的根治手段已列入功能建议 F1/F2。
4. **并发性能方面新确认 1 个中风险放大器**（sweep 超 cap 后重复全表扫描，见 P1），其余热点路径（免锁读、三键合并读、dlsym 协程 ID、CAS 计数）经复核实现正确。
5. **历史遗留台账 6 项已闭环、5 项仍开放**（见第五节），开放项均有明确的下一步归属。

---

## 二、确认的问题

### 高风险

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| H1 | `src/session/session.c:82-102`（`gene_session_call_method` 内 4 槽静态缓存） | **用户态类指针跨请求缓存，FPM 无 opcache / `opcache.file_cache_only=1` / CLI 下构成 UAF；ZTS 下跨线程共享** | 静态确认（利用条件需运行时复现） |

**H1 详情**：

```c
static struct { zend_class_entry *ce; zend_string *method; zend_function *fn; } gene_session_fn_cache[4] = {0};
```

- 缓存对象：`gene_session_get_handler()` 返回的**用户注入 session 存储插件**（`GENE_SESSION_HANDLER` 属性，IS_OBJECT，用户态类）。
- 威胁模型与 2026-05-29 view.c `regex_strs` 修复（见 `AUDIT_REPORT_2026_05_29.md`）完全一致：非 opcache-SHM 环境下用户态 CE 及其 `function_table` 在请求结束时释放，函数内 static 槽位保留悬空指针；下一请求若 Zend MM 复用相同地址重建该类、且 interned method 字符串同地址（确定性分配模式下概率不低），缓存命中已释放的 `zend_function*` → 派发时 UAF/崩溃。
- ZTS 构建下该 static 数组跨线程共享，同时命中 2026-07-13 报告 H1 的同类问题（该轮修复了 common.c/validate.c/pdo.c 共 22 处，session.c 此结构体缓存不在其中）。
- 对比核查：`redis_pool.c`/`pool.c`/`application.c` 的同类 static 缓存目标均为**内部类**（Swoole\*、PDO、Gene\* 扩展自身类），进程生命周期恒定，不受影响；session 插件是唯一的用户态类缓存点。
- **修复建议**（三选一，本轮未实施）：
  1. 仅当 `called_scope->ce_flags & ZEND_ACC_IMMUTABLE`（opcache SHM 持久类）时写入/命中缓存，否则每次直接 `zend_hash_find_ptr`；
  2. 缓存迁移到 `GENE_G`（请求级，天然随请求失效）；
  3. 直接删除缓存——4 次 `zend_hash_find_ptr` 仅在 session get/set/save/delete 时发生，非热路径，收益本就微小。
- 回归验证：FPM + `opcache.enable_cli=0` / 无 opcache 下以真实插件循环 2 个以上请求；ASAN 下复现最佳。

### 中风险

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| M1 | `src/gene.c:844-847` + `721-770` | **sweep 超 cap 后逐协程重复全表扫描**：`gene_request_ctx()` 在新协程分配时，只要 `count >= eff_cap` 即调用 `gene_co_contexts_sweep()` 做 O(N) 全表存活探测 + `emalloc(victims)`。若活跃协程数持续 ≥ cap（`co_contexts_max` 配置偏小、或业务漏调 cleanup 且协程长命），sweep 后 count 仍 ≥ cap，**之后每个新协程都触发一次全扫描** → 协程风暴下 O(N²) 放大与 p99 尾延迟毛刺。2026-07-12 报告 SW-P3 提出的 batch/cursor 方案未实现 | 静态确认（放大倍数需压测量化） |
| M2 | `src/gene.c:974-977` 注释假设 vs 实际 SAPI 行为 | **Swoole 下 RINIT/RSHUTDOWN 真实触发频率未闭环**：注释假设「Swoole 模式 RSHUTDOWN 仅在 worker 退出时触发」。若目标 Swoole 版本实际为每请求触发，则 `co_contexts` 与 ctx pool 每请求销毁重建 —— 功能仍正确（ctx pool 预热仅首请求），但跨请求协程上下文复用语义失效，且请求外存活协程（tick 定时器等）的 ctx 会被提前销毁。2026-07-12 §3.2 已列为待验证项，本轮复核源码假设仍未变 | 需运行时验证 |

### 低风险

| # | 位置 | 问题 |
|---|------|------|
| L1 | `src/cache/redis_pool.c:442-452`（`rpool_decrement_count`） | CAS 重试 64 轮后**静默放弃**：计数不再递减且无告警 → 极端争用下计数偏高不可观测。协作调度模型下 Atomic get/cmpset 不让出，实际几乎不可能触发，属防御缺口而非现实 bug。建议放弃路径输出一次 E_WARNING |
| L2 | `src/gene.c:748`（sweep victims 分配） | 每次 sweep `emalloc(sizeof(zend_ulong) * total)`；cap 调大（如 8192）时单次 64KB 瞬态分配。可改栈上分批（如 256/批）消除。与 M1 合并处理更优 |
| L3 | `src/cache/memory.c:424-434`（LRU touch） | 每次业务写在 WRLOCK 内做 remove + add + persistent key 拷贝/释放（约 2 次 hash 操作 + 1 次字符串持久化）。单进程协作模型下无争用，仅高写频场景的 WRLOCK 持有时间偏长；优先级低 |
| L4 | `src/tool/log.c:262-269` | `zend_read_property` 的 rv 槽（rv1/rv2/rv3）未初始化传入。message/file/line 是 Throwable 真实声明属性，不会走魔术方法产生临时值，当前无泄漏；与 2026-07-13 L2（pdo.c 已修）同型，属脆弱惯例，建议同样初始化 rv |

---

## 三、复核后排除的疑似问题（避免误报）

- **`application.c:353-355` webscan 静态 ce/ctor/check 缓存** —— 缓存目标是内部类 `Gene\Webscan`（MINIT 注册，进程恒定），ZTS 下同样安全，不属 H1 同类。
- **`redis_pool.c`/`pool.c` 全部 static fn 缓存** —— 目标均为 Swoole\Channel/Swoole\Atomic/Swoole\Timer/Redis/PDO 等内部类方法，进程生命周期恒定；Swoole 不支持 ZTS，无跨线程问题。维持 2026-07-03 §10.2#3「逐步迁移、实际风险低」的结论。
- **sweep 删除与遍历并发** —— 先收集 victims 再统一删除（gene.c:747-760），`gene_co_context_dtor` 正确清理 current_ctx 绑定，无迭代器失效。
- **RSHUTDOWN 对 resident_ctx / co_contexts / ctx pool 的销毁顺序** —— 先解绑 current_*，dtor 经 pool 回收，最后 drain，无双重释放。
- **`gene_closure_src_cache_put` 满容清表** —— 同 key 更新不计基数，`zend_hash_str_update_ptr` 先 dtor 旧项，无泄漏；清表语义（全清而非 LRU）为设计决策并有 flushes 计数观测。

---

## 四、并发性能评估

### 4.1 已验证正确的既有机制（本轮复核）

- workerReady 后持久缓存免锁读（`memory.h` RDLOCK 跳过）与 `gene_memory_write_allowed` 写冻结守卫完备；
- 路由派发三键合并读（`gene_memory_get_triple`）将锁周期 3×→1×；
- 协程 ID 获取 dlsym C-API 快路径 + kill-switch（`gene.swoole_getcid_capi`）+ PHP 回退三级正确；
- RedisPool 计数 CAS 递减、DB Pool 计数增减配对（push 成功后计数、push 失败回滚）正确；
- ctx pool 有 `ctx_pool_max` 硬上限，hit/miss 可观测。

### 4.2 性能发现与建议（按预计收益排序）

| # | 级别 | 事项 | 建议 |
|---|------|------|------|
| P1 | 中 | **M1 sweep 重复扫描放大** | 实施 sweep 冷却：记录上次 sweep 后新增的 ctx 分配数，仅当新增超过阈值（如 cap/4）或 count 创新高时再扫；或实现 07-12 SW-P3 的 cursor 分批。**收益**：cleanup 漏调/cap 偏小场景消除 O(N²) 放大与 p99 毛刺。**风险**：低，纯触发条件变更 |
| P2 | 低 | sweep victims 栈分批（L2） | 随 P1 一并处理 |
| P3 | 低 | LRU touch 降低 WRLOCK 持有（L3） | 仅新 key 做持久化拷贝；复用已跟踪 key 的 Bucket 移动。需 profile 证明写路径占比后实施 |
| P4 | 观察 | 07-03 §10.3 八项候选（路由注册 memcpy、route_pc 预热、配置路径缓存、arena、SQL 构造、视图状态机、C 原子、递归改迭代） | **维持 profile gate 纪律，不建议本轮启动**。`tools/acceptance/profile_gate.php` 当前对全部候选输出 DEFER，无证据不重构 |

### 4.3 容量默认值观察（非代码缺陷）

- `gene.co_contexts_max=1024` 默认对高并发 Swoole 服务偏小：活跃协程超过 1024 时即触发 M1 的重复扫描路径。生产建议按「单 worker 峰值协程 + 余量」显式配置（07-12 §十一已给原则，本报告 M1 修复前该默认值的影响被放大）。

---

## 五、历史遗留台账复核（本轮逐项对照源码）

### 已闭环 ✅

| 事项 | 来源 | 闭环证据 |
|------|------|----------|
| DB 池协程识别 `getCid` 大小写 bug | 07-03 P1 | `pool.c:85-96` 已改用 `gene_get_coroutine_id()` |
| FPM 闭包源码缓存无上限 | 07-03 P2 / 07-12 FPM-P2 | INI `gene.closure_src_cache_max=1024`（`gene.c:154`），cap 执行于 `router.c:1627-1654`，含 flushes 计数 |
| `cache_max_items=0` 启动 NOTICE | 07-03 P3 / 07-12 阶段1 | `application.c:1318-1325`，workerReady 后每 worker 一次 |
| `Memory::stats()` 分区观测 | 07-12 WP-02 | `memory.c:1128-1156`：closure/route_pc/业务缓存/ctx pool hit-miss/sweep 遥测/高水位全量落地 |
| 视图编译 mtime 检查 INI | 05-29 建议 | `gene.view_compile_check_mtime`（`gene.c:145`，默认 0=信任编译产物） |
| sweep 误删活协程 | 07-12 WP-04 | `gene.c:721-770` 仅删 `exists()==0` 条目，超 cap 保留并告警 |

### 仍开放 ❌（含本轮新增）

| # | 事项 | 级别 | 下一步归属 |
|---|------|------|-----------|
| O1 | **H1 session 插件 fn 静态缓存跨请求 UAF**（本轮新发现） | 高 | 建议下一修复窗口按二节方案 1 处理 + ASAN 回归 |
| O2 | `cache_max_items=0` 默认无界（设计保留兼容） | P1 | 通过 F2 可观测性 + 文档治理；不建议改默认语义 |
| O3 | Swoole cleanup 漏调驻留（机制性风险） | P1 | **功能建议 F1（自动 cleanup 兜底）根治** |
| O4 | `cache_easy` 独立 TTL/LRU | P2 | 07-12 阶段 4 候选，可随 F6 提前 |
| O5 | M1 sweep 重复扫描 / M2 RSHUTDOWN 语义假设 / L1 CAS 静默放弃 | 中/低 | 见第二、四节 |
| O6 | 运行时验证全集：Linux 编译、ASAN/Valgrind、CAS 压测、dlsym 符号可见性、百万请求、24h RSS | 阻塞项 | 全部待人工 Linux 环境；环境未提供前不得宣称「无泄漏/UAF」 |
| O7 | Linux 回归 9 项失败（07-12 §7.7） | 功能 | 本轮核实：`Gene\Controller::init()` 确不存在（`controller.c` 方法表无此项）；所有 DB 驱动均无 `connect()` 方法（测试与 API 不一致，需二选一）；Cache/Language/Http 失败含夹具缺失，需逐项 triage |

---

## 六、功能完善建议（按「使用效益 ÷ 实施成本」排序）

> 评估基线：07-12 报告 P0 缺口、Linux 回归暴露的 API 缺口、框架现有机制（Hook 生命周期、分区 stats、双模式运行时）。

### F1（P0，强烈推荐）：Swoole 请求自动 cleanup 兜底

- **问题**：O3 —— 业务漏调 `Application::cleanup(true)` 时协程上下文驻留，依赖 cap+sweep 兜底并引入 M1 放大。这是当前 Swoole 模式最大的内存风险点，且完全可由框架消除。
- **方案**：`Application::run()` 在 `runtime_type>=2` 时，于派发完成后检查当前协程 ctx 是否仍在 `co_contexts` 中，若是则自动执行与 `cleanup(false)` 等价的上下文归还；业务已手动 cleanup 时为 O(1) no-op。提供 `gene.swoole_auto_cleanup` INI（默认建议先关、灰度后开）。
- **实现面**：`application.c`（run/cleanup）+ `gene.c`（co_contexts 查询/删除），预计 <100 行。
- **风险**：需保证与手动 cleanup 严格幂等；yield 中的嵌套 run 不可误清。收益：**将 P1 机制性风险降为零**。

### F2（P0，强烈推荐）：`Gene\Monitor` 聚合可观测出口

- **问题**：07-12 WP-02 已铺好全部分区 stats（Memory/Pool/RedisPool/sweep/ctx pool），但缺少单一聚合出口，容量调参（`cache_max_items`、`co_contexts_max`、`ctx_pool_max`）无数据抓手。
- **方案**：新增 `Gene\Monitor::stats(): array` 聚合上述三类 stats + 请求计数/错误计数；demo 增加 `/monitor` 端点示例。纯读、零副作用。
- **实现面**：新增 `src/tool/monitor.c`（或并入 memory.c），调用既有只读接口，<150 行。
- **收益**：让 O2/O4 容量治理、M1 验证、长跑 RSS 归因（Gene vs 业务）全部具备数据基础，直接补齐 07-12 P0 缺口的最后一环。

### F3（P1）：Controller 生命周期 `init()` 钩子

- **问题**：Linux 回归 Mvc 失败项证实 `Gene\Controller::init()` 被测试/用户期望但不存在（本轮已核实方法表）。子类当前只能重写 `__construct` 且须维护签名。
- **方案**：路由直派实例化控制器后、调用 action 前，若子类定义了 `init()` 则调用一次（Yaf 同语义）。
- **实现面**：`factory.c`/`router.c` 直派路径，<30 行。**同时闭环一个回归失败项**。

### F4（P1）：路由级中间件管道

- **问题**：Hook 已有 before/after/handle 生命周期与命名钩子雏形，但无法对单路由声明「鉴权→限流→日志」式的有序横切链。
- **方案**：路由配置支持 `route.middleware = "Auth,RateLimit"`（或 `$router->middleware()`），派发前按序执行各中间件（Gene\Hook 子类）`handle()`，任一返回 false 中断并走 error 路径。复用既有 `gene_router_exec_hook_direct` 直派机制，零 eval 开销。
- **实现面**：`router.c`（配置解析 + 派发链）+ 文档/demo，中等规模。
- **收益**：鉴权/限流/审计等横切关注点的标准挂载点，是框架级使用效益最高的扩展点。

### F5（P2）：Validate 自定义规则扩展点

- **方案**：`Gene\Validate::extend(string $rule, callable $fn)` 注册表，规则分派先查用户表再落内置表。实现面小（`validate.c` 分派前加一次 hash 查找），解除 46KB 内置规则集的封闭性。

### F6（P2）：`cache_easy` TTL 治理（提前 07-12 阶段 4 候选）

- **方案**：`cache_easy` 条目增加写入时间戳与 `gene.cache_easy_ttl` INI，读时惰性过期。配合 F2 观测闭环 O4。

### 不建议本轮立项

- 视图解析状态机（28 趟 PCRE 合并）、C 层连接池原子计数、request arena、路由递归改迭代 —— 均为高成本结构重构，维持「profile 达标后独立 PR」的既有纪律。

---

## 七、需人工环境执行项（本机不可执行）

1. H1 修复后：FPM 无 opcache / `opcache.file_cache_only=1` 双配置 + ASAN 回归（session 插件注入，≥2 请求循环）；
2. M1 修复后：`tools/acceptance/swoole_context_soak.php` 协程风暴（10 万）验证单次 sweep CPU 受控；
3. M2：目标 Swoole 版本实测 RINIT/RSHUTDOWN 触发频率（可在 RINIT/RSHUTDOWN 加临时计数桩或 strace 验证）；
4. 07-12 既定全集：Linux 双构建（ZTS/NTS）、Valgrind `definitely lost: 0`、CAS 压测、dlsym 符号可见性矩阵、24h RSS 线性回归；
5. O7：补齐 `test/Language/Goodbye/Ko.php` 夹具、MySQL/PgSQL 测试服务后重跑全量回归，并 triage Cache 2 项与 Http 2 项失败。

---

## 八、交叉索引

- `audit/AUDIT_REPORT_2026_07_12.md` §三/§十：P0-P2 问题分级与分阶段计划（本报告 O2/O3/O4/O6 来源）
- `audit/AUDIT_REPORT_2026_07_13.md`：H1（ZTS）修复范围（本报告 H1 为其同类残留）
- `audit/AUDIT_REPORT_2026_05_29.md`：view.c 跨请求 UAF 修复（本报告 H1 的判定先例）
- `audit/AUDIT_REPORT_2026_07_03.md` §10.2-10.4：静态缓存迁移残留与待验证项
