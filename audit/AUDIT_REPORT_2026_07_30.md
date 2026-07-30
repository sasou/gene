# Gene 扩展框架三向审计报告（内存泄漏 / 并发性能 / 功能完善）

> 审计版本：5.6.8（develop，HEAD `98241cd`）
> 审计日期：2026-07-30
> 审计方式：纯静态代码审查 + 历史审计报告（2026-07-03 / 07-12 / 07-13）交叉复核；**本轮仅出报告，未修改任何源码**
> **回写说明（2026-07-30 当日）**：报告出具后，P0/P1/P2 各项已于当日按方案落地实现，实现明细与台账状态更新见**第九节**及第五节台账表「状态」列；源码改动未经编译，运行时验证仍挂 O6
> 审计范围：`src/` 全模块，重点为历次审计未覆盖角度与遗留项闭环核查
> 结论约定：每条发现标注证据类型 ——「静态确认」或「需运行时验证」；本机（Windows）无法执行编译、ASAN/Valgrind、压测与长跑

---

## 一、总体结论

1. **未发现新的常规路径确定性内存泄漏**。FPM 请求级生命周期（RINIT/RSHUTDOWN 配对、`php_gene_close_request_globals`）、MSHUTDOWN 持久资源析构（cache/cache_easy/fn_cache/route_pc/closure_src_cache/LRU/连接池静态表）均已完整闭环；log.c、language.c、session.c 主体、memory.c LRU 写路径的分配/释放配对经逐路径核查无误。
2. **新发现 1 个跨请求 use-after-free 风险**（session 插件方法静态缓存，见 H1），威胁模型与 2026-05-29 view.c 已修复问题完全相同，属同类残留。复核中发现其命中条件比初判更宽松（method 指针跨请求恒定，仅需 CE 地址复用一项条件），应按确定性风险对待。
3. **Swoole 模式的主要内存风险仍是容量治理而非传统泄漏**：`cache_max_items=0` 无界（设计保留，仅有启动 NOTICE）与 cleanup 漏调两个 P1 项维持开放；对应的根治手段已列入功能建议 F1/F2。
4. **并发性能方面新确认 1 个中风险放大器**（sweep 超 cap 后重复全表扫描，见 P1），其余热点路径（免锁读、三键合并读、dlsym 协程 ID、CAS 计数）经复核实现正确。
5. **历史遗留台账 6 项已闭环、5 项仍开放**（见第五节），开放项均有明确的下一步归属。其中 07-12 §3.2 的「Swoole RINIT/RSHUTDOWN 触发频率」待验证项本轮按 SAPI 语义静态关闭（原 M2）。
6. **本轮修正说明**：初稿的 H1 修复方案一（`ZEND_ACC_IMMUTABLE` 门控）、M2 定级、F1 挂载点、L1/L4 修法经复核均已更正，详见对应条目。仓库既有验收基础设施（`tools/acceptance/*`、`test/TestRunner.php`、`config.m4`/`config.w32`）完备，运行时验证的阻塞点仅在于 Linux 环境供给，不在于工具缺失。

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
- 威胁模型与 2026-05-29 view.c `regex_strs` 修复（见 `AUDIT_REPORT_2026_05_29.md`）完全一致：非 opcache-SHM 环境下用户态 CE 及其 `function_table` 在请求结束时释放，函数内 static 槽位保留悬空指针；下一请求若 Zend MM 复用相同地址重建该类，缓存即命中已释放的 `zend_function*` → 派发时 UAF/崩溃。
- **命中条件修正（本轮复核上调风险）**：命中判据是 `ce` 指针 + `method` 指针双相等，其中 `method` 由 `gene_session_method_get/set/delete()` 经 `gene_interned_str_persistent` 返回（session.c:386-411），**指针跨请求恒定**。因此触发不需要「method 字符串恰好同地址」这一巧合，仅需 `ce` 地址被复用一项条件。实际可触发性显著高于「双巧合」估计，H1 应按确定性风险对待。
- ZTS 构建下该 static 数组跨线程共享，同时命中 2026-07-13 报告 H1 的同类问题（该轮修复了 common.c/validate.c/pdo.c 共 22 处，session.c 此结构体缓存不在其中）。
- 对比核查：`redis_pool.c`/`pool.c`/`application.c` 的同类 static 缓存目标均为**内部类**（Swoole\*、PDO、Gene\* 扩展自身类），进程生命周期恒定（ZTS 下线程各自复制的是符号表，内部 CE 指针共享，安全），不受影响；session 插件是唯一的用户态类缓存点。
- **同型正确实现已存在于仓库内**：`cache.c:466-518`（`gene_cache_get/set/incr/del`）面对同样是用户注入的 handler 对象，**每次调用直接 `zend_hash_find_ptr`，不做任何静态缓存**。session.c 是全仓唯一例外，删除缓存属于回归既有约定，而非性能退化。
- **修复建议**（本轮未实施）：
  1. **首选：直接删除该静态缓存**，改为与 `cache.c` 一致的每次 `zend_hash_find_ptr`。4 次查找仅发生在 session get/set/save/delete，非热路径，原收益本就微小，且此改法对 opcache 状态、SAPI、ZTS 全部免疫。
  2. 次选：缓存迁移到 `GENE_G`（请求级，天然随请求失效）；Swoole 常驻场景下仍需确认用户态 CE 在 worker 内不被卸载。
  3. **已排除的方案：`ZEND_ACC_IMMUTABLE` 门控**。IMMUTABLE 类位于 opcache SHM，但 opcache 重启（`restart_pending`、SHM OOM、`opcache_reset()`）会重置 SHM 并可能在同地址重建不同类，静态槽位依旧命中失效指针 —— 该方案只缩小窗口，不关闭问题，不应采纳。
- 回归验证：FPM + `opcache.enable_cli=0` / 无 opcache 下以真实插件循环 2 个以上请求；ASAN 下复现最佳。

### 中风险

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| M1 | `src/gene.c:844-847` + `721-770` | **sweep 超 cap 后逐协程重复全表扫描**：`gene_request_ctx()` 在新协程分配时，只要 `count >= eff_cap` 即调用 `gene_co_contexts_sweep()` 做 O(N) 全表存活探测 + `emalloc(victims)`。若活跃协程数持续 ≥ cap（`co_contexts_max` 配置偏小、或业务漏调 cleanup 且协程长命），sweep 后 count 仍 ≥ cap，**之后每个新协程都触发一次全扫描** → 协程风暴下 O(N²) 放大与 p99 尾延迟毛刺。2026-07-12 报告 SW-P3 提出的 batch/cursor 方案未实现 | 静态确认（放大倍数需压测量化） |

**M1 触发前置条件（本轮补充，直接影响压测搭法）**：

- O(N) 扫描**仅在 `gene_swoole_co_exists_resolve()` 成功时**发生（gene.c:746-761）。`have_exists == 0` 时 sweep 只累加遥测后返回，单次开销 O(1)，观测不到放大 —— 压测前必须确认 `Swoole\Coroutine::exists` 已解析，否则会得出「无问题」的假阴性结论。
- `total < 16` 提前返回（gene.c:735）：cap 配置低于 16 时 sweep 永不生效，冷却逻辑改造需保留并对齐该下限。

| # | 位置 | 结论 | 证据类型 |
|---|------|------|----------|
| M2 | `src/gene.c:974-977` 注释假设 | **本轮判定为「设计确认」，从待验证项撤销**。Swoole `Http\Server` 运行于 CLI SAPI：PHP 的 RINIT/RSHUTDOWN 按**脚本执行**触发一次，Swoole 的每个请求只是同一次脚本执行内的回调，不跨越 SAPI 请求边界。故「Swoole 模式 RSHUTDOWN 仅在 worker 退出/reload 时触发」的注释假设成立，`co_contexts` 与 ctx pool 不会每请求销毁重建，请求外存活协程（tick 定时器等）的 ctx 亦不会被提前销毁。2026-07-12 §3.2 的该待验证项可关闭；仅保留 reload / worker 退出路径的一次性冒烟，不再占用运行时验证预算 | 静态确认（原「需运行时验证」结论已推翻） |

### 低风险

| # | 位置 | 问题 |
|---|------|------|
| L1 | `src/cache/redis_pool.c:442-452`（`rpool_decrement_count`） | CAS 重试 64 轮后**静默放弃**：计数不再递减且无告警 → 极端争用下计数偏高不可观测。协作调度模型下 Atomic get/cmpset 不让出，实际几乎不可能触发，属防御缺口而非现实 bug。**修正建议**：不用裸 `E_WARNING`（Swoole worker 常驻，高频路径告警会淹没日志），改为「放弃计数器 + 纳入 F2 `Monitor::stats()` 出口」，并沿用 `co_contexts_cap_warned` 的 once 模式，仅在计数由 0 变 1 时告警一次 |
| L2 | `src/gene.c:748`（sweep victims 分配） | 每次 sweep `emalloc(sizeof(zend_ulong) * total)`；cap 调大（如 8192）时单次 64KB 瞬态分配。可改栈上分批（如 256/批）消除。与 M1 合并处理更优 |
| L3 | `src/cache/memory.c:424-434`（LRU touch） | 每次业务写在 WRLOCK 内做 remove + add + persistent key 拷贝/释放（约 2 次 hash 操作 + 1 次字符串持久化）。单进程协作模型下无争用，仅高写频场景的 WRLOCK 持有时间偏长；优先级低 |
| L4 | `src/tool/log.c:262-269` | `zend_read_property` 的 rv 槽（rv1/rv2/rv3）未初始化传入。message/file/line 是 Throwable 真实声明属性，不会走魔术方法产生临时值，当前无泄漏；与 2026-07-13 L2（pdo.c 已修）同型。**修正建议（本轮加强）**：问题不止「未初始化」——代码对读回的属性做 `ZVAL_COPY` 后**从不 dtor rv1/rv2/rv3**。一旦调用方传入带魔术 getter 的 Throwable 子类（`__get` 返回临时值写入 rv），即为确定性泄漏而非仅「脆弱惯例」。修法应为：三个 rv 先 `ZVAL_UNDEF`，拷贝完成后逐个 `zval_ptr_dtor` |

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

> **2026-07-30 回写更新**：O1 / O3 / O5 已于当日按方案落地实现（代码已改，运行时验证仍挂 O6）；O2 / O4 的治理手段（F2 / F6）亦已落地。各项实现明细见**第九节**。表中「状态」列为回写后最新状态。

| # | 事项 | 级别 | 状态 | 下一步归属 |
|---|------|------|------|-----------|
| O1 | **H1 session 插件 fn 静态缓存跨请求 UAF**（本轮新发现） | 高 | ✅ **已实现**（首选方案：删除静态缓存，对齐 `cache.c` 直查，`session.c`；§九.1） | 待 Linux ASAN 回归（§七.1） |
| O2 | `cache_max_items=0` 默认无界（设计保留兼容） | P1 | ✅ 治理手段已落地（F2 `Monitor::stats()` 可观测出口，§九.5）；默认语义维持不变 | 文档治理 + 生产压测数据回采 |
| O3 | Swoole cleanup 漏调驻留（机制性风险） | P1 | ✅ **已实现**（F1 协程 defer 自动 cleanup，`gene.swoole_auto_cleanup` 默认关；§九.4） | 灰度开启 + Swoole 环境验证 |
| O4 | `cache_easy` 独立 TTL/LRU | P2 | ✅ **已实现**（F6 `gene.cache_easy_ttl` 惰性过期，§九.8） | Swoole 环境验证 |
| O5 | M1 sweep 重复扫描 / L1 CAS 静默放弃 / L4 log.c rv | 中/低 | ✅ **已实现**（M1 冷却 + L1 计数器 + L4 rv 修正，§九.2/3/7）。**M2 已从本台账移除**：本轮静态确认 Swoole 请求不跨越 SAPI 请求边界，注释假设成立（第二节 M2） | M1 压测量化（§七.2） |
| O6 | 运行时验证全集：Linux 编译、ASAN/Valgrind、CAS 压测、dlsym 符号可见性、百万请求、24h RSS | 阻塞项 | ⏳ 维持开放 | 全部待人工 Linux 环境；环境未提供前不得宣称「无泄漏/UAF」。本轮新增改动全部经静态自查 + `php -l`，未经编译 |
| O7 | Linux 回归 9 项失败（07-12 §7.7） | 功能 | 🔶 部分推进：`Gene\Controller::init()` 已经 F3 落地（基类方法 + 直派调用，§九.6），Mvc 对应失败项预期转绿 | 其余：DB 驱动 `connect()` 二选一、Cache/Language/Http 夹具 triage 待 Linux 重跑 |

---

## 六、功能完善建议（按「使用效益 ÷ 实施成本」排序）

> 评估基线：07-12 报告 P0 缺口、Linux 回归暴露的 API 缺口、框架现有机制（Hook 生命周期、分区 stats、双模式运行时）。

### F1（P0，强烈推荐）：Swoole 请求自动 cleanup 兜底

- **问题**：O3 —— 业务漏调 `Application::cleanup(true)` 时协程上下文驻留，依赖 cap+sweep 兜底并引入 M1 放大。这是当前 Swoole 模式最大的内存风险点，且完全可由框架消除。
- **主方案（本轮修正，挂载点改为协程生命周期）**：原「在 `Application::run()` 派发后兜底」方案**覆盖不全** —— 产生 ctx 驻留压力的恰恰是不走 `run()` 的协程：`Swoole\Timer` tick 回调、task worker、用户自建协程中调用任意 Gene API 时，`gene_request_ctx()`（gene.c:834-856）一样会分配并登记 ctx，run() 钩子完全触及不到，M1 的压力源依旧存在。
  正确挂载点是**首次为某 cid 分配 ctx 时**（gene.c:834 的 `if (!ctx)` 分支）：若当前处于协程内，注册一次 `Swoole\Coroutine::defer()` 归还回调。这样 ctx 生命周期与协程生命周期严格绑定，覆盖全部入口，cap + sweep 退化为纯防御路径，**M1 也随之失去现实触发场景**。成本为每协程一次 defer 注册（非每请求、非每次 ctx 访问）。
- **降级方案**：`defer` 不可用（非协程上下文、Swoole 版本缺失该 API）时，退回 `Application::run()` 在 `runtime_type>=2` 下派发后检查并归还，业务已手动 cleanup 时为 O(1) no-op。
- **实现面**：`gene.c`（ctx 分配点注册 defer + co_contexts 删除）+ `application.c`（run/cleanup 幂等）+ `gene.swoole_auto_cleanup` INI（默认先关、灰度后开）。
- **风险**：需保证与手动 cleanup 严格幂等（defer 回调触发时 ctx 可能已被归还）；yield 中的嵌套 run 不可误清；defer 回调内不得再触发 `gene_request_ctx()` 重新登记。收益：**将 P1 机制性风险降为零，并抽掉 M1 的触发前提**。

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

### 6.1 实施顺序与依赖（本轮补充）

各项不可并行开工，建议严格按序：

1. **H1**（高危 UAF，按上文首选方案删除静态缓存）—— 唯一的正确性缺陷，先行；
2. **M1 + L2**（sweep 冷却 + victims 栈分批）—— 注意若 F1 主方案先落地，M1 的现实触发场景消失，可据此下调 M1 优先级，但冷却逻辑仍应保留作为防御；
3. **F2**（`Monitor` 聚合出口）—— 前置于 M1/F1 的效果验证：没有聚合数据，sweep 冷却是否生效、ctx 是否真正随协程回收都无法量化；同时承载 L1 的放弃计数器；
4. **F1**（协程 defer 自动 cleanup）；
5. **F3**（`Controller::init()`，顺带闭环一个回归失败项）+ **L4**（log.c rv 修正）；
6. **F4 / F5 / F6**。其中 **F4（路由中间件管道）不应在 O6 运行时验证全集打通前立项** —— 它是中等规模的派发链改造，在缺回归证据的前提下改派发路径，风险收益比不成立。

### 不建议本轮立项

- 视图解析状态机（28 趟 PCRE 合并）、C 层连接池原子计数、request arena、路由递归改迭代 —— 均为高成本结构重构，维持「profile 达标后独立 PR」的既有纪律。

---

## 七、需人工环境执行项（本机不可执行）

1. H1 修复后：FPM 无 opcache / `opcache.file_cache_only=1` 双配置 + ASAN 回归（session 插件注入，≥2 请求循环）；
2. M1 修复后：`tools/acceptance/swoole_context_soak.php` 协程风暴（10 万）验证单次 sweep CPU 受控。**前置校验**：必须先确认 `Swoole\Coroutine::exists` 已被 `gene_swoole_co_exists_resolve()` 解析成功（否则 sweep 为 O(1) 空转，会得到假阴性），且 `gene.co_contexts_max` ≥ 16；
3. ~~M2：实测 RINIT/RSHUTDOWN 触发频率~~ —— **本轮已按 SAPI 语义静态关闭，撤销该运行时项**；仅在 reload / worker 退出路径做一次性冒烟即可；
4. 07-12 既定全集：Linux 双构建（ZTS/NTS）、Valgrind `definitely lost: 0`、CAS 压测、dlsym 符号可见性矩阵、24h RSS 线性回归；
5. O7：补齐 `test/Language/Goodbye/Ko.php` 夹具、MySQL/PgSQL 测试服务后重跑全量回归，并 triage Cache 2 项与 Http 2 项失败。

---

## 九、落地实现回写（2026-07-30 当日，按 §6.1 顺序）

> 实施基线：5.6.8 develop（HEAD `98241cd`）。实施方式同审计约束：**本机 Windows 静态实施，未经编译**；全部 C 改动经逐区域回读 + `git diff` 复核，PHP 改动（demo/测试/ide-helper）经 `php -l`（PHP 8.1.34 NTS）通过。运行时验证（编译/ASAN/压测/长跑）仍挂 O6，待 Linux 环境。
> 未立项项维持原判：**F4（路由中间件管道）** 按 §6.1-6 不在 O6 打通前立项；**P3（LRU touch）/ P4（07-03 §10.3 八项）** 维持 profile gate 纪律。

### 9.1 H1（高危 UAF）→ 已实现 ✅

- **方案**：采纳首选方案——删除 `gene_session_call_method()` 内 4 槽静态 `(ce, method) -> zend_function*` 缓存，改为每次调用直接 `zend_hash_find_ptr(&called_scope->function_table, method)`，与 `cache.c:466-518`（`gene_cache_get/set/incr/del`）逐行为同型。次选（GENE_G 迁移）与已排除方案（IMMUTABLE 门控）均未采用。
- **改动**：`src/session/session.c`（净 -13 行）。
- **性质**：回归全仓既有约定而非性能退化——4 次查找仅发生在 session get/set/save/delete 非热路径；对 opcache 状态、SAPI、ZTS 全部免疫。

### 9.2 M1（sweep 重复扫描放大）+ L2（victims 分配）→ 已实现 ✅

- **M1 冷却**：`gene_request_ctx()` 超 cap 触发点新增冷却判定——仅当「距上次 sweep 新增 ctx 分配数 ≥ `cap/4`」或「表规模 ≥ 上次 sweep 水位 + `cap/4`」时才执行 sweep；被抑制触发计入新计数器 `co_contexts_sweep_skipped`，经 `Memory::stats()` 与 `Monitor::stats()` 可观测（满足 §6.1-3「冷却是否生效可量化」的前置要求）。`total < 16` 下限保留在 `gene_co_contexts_sweep()` 内未动，与报告要求对齐。新全局量：`co_ctx_allocs_since_sweep` / `co_contexts_sweep_mark` / `co_contexts_sweep_skipped`。
- **L2 栈分批**：`gene_co_contexts_sweep()` 的 victims 由 `emalloc(sizeof(zend_ulong) * total)` 改为 256/批栈上数组，批满即删。安全性论证：ZEND_HASH_FOREACH 沿 arData 单调推进、`zend_hash_index_del` 不重排，仅删除迭代器已越过的桶位，无迭代器失效（已在代码注释中固化）。
- **改动**：`src/gene.c`、`src/gene.h`、`src/cache/memory.c`（stats 新增 `co_contexts_sweep_skipped`）。
- **联动说明**：F1（§9.4）落地后 M1 的现实触发场景（漏调 cleanup）已被抽掉，冷却逻辑作为纯防御层保留，符合 §6.1-2 预期。

### 9.3 L1（RedisPool CAS 静默放弃）→ 已实现 ✅

- 按报告修正建议执行：`rpool_decrement_count()` 64 轮 CAS 耗尽后不再静默——计入 `GENE_G(redis_pool_cas_abandoned)` 并经 `Gene\Monitor::stats()` 出口（`redis_pool_cas_abandoned` 键）；告警沿用 `co_contexts_cap_warned` once 模式，仅计数 0→1 时 `E_WARNING` 一次，不使用裸高频告警。
- **改动**：`src/cache/redis_pool.c`、`src/gene.h`（+2 全局量）、`src/gene.c`（init_globals）。

### 9.4 F1（P0，Swoole 协程自动 cleanup 兜底）→ 已实现 ✅

- **主方案（defer 挂载点）**：`gene_request_ctx()` 的 `if (!ctx)` 分配分支内，`gene.swoole_auto_cleanup=1` 时注册一次性 `Swoole\Coroutine::defer('gene_auto_cleanup_defer')`。回调为新增内部函数（字符串 callable，进程生命周期恒定，无闭包分配、无跨请求指针风险），在协程结束时直接对 `co_contexts` 做幂等删除归还 ctx——**不经 `gene_request_ctx()`**，杜绝重新登记（报告风险条款 3 满足）；与手动 cleanup 幂等（重复删除为 no-op，风险条款 1 满足）；覆盖 run()/Timer tick/task worker/自建协程全部入口。
- **降级方案**：defer 解析失败（旧版 Swoole）时，`Application::run()` 在 `runtime_type>=2` 下派发后归还当前协程 ctx；新增 `GENE_G(run_depth)` 嵌套守卫（仅最外层 run 归还，风险条款 2 满足；bailout 残留经 RSHUTDOWN 归零兜底）；业务已手动 cleanup 时为 O(1) 哈希未命中 no-op。
- **INI**：`gene.swoole_auto_cleanup`（默认 `0`，先关、灰度后开，符合报告要求）。
- **遥测**：`swoole_auto_cleanup_defers` / `swoole_auto_cleanup_reclaimed`，经 `Monitor::stats()` 出口。
- **改动**：`src/gene.c`（resolver/register/回调函数/INI/init_globals）、`src/gene.h`、`src/app/application.c`。

### 9.5 F2（P0，`Gene\Monitor` 聚合出口）→ 已实现 ✅

- 新增 `Gene\Monitor`（final 类）静态方法 `stats(): array`，聚合：`memory`（与 `Memory::stats()` 同键全集 + `co_contexts_sweep_skipped` + `cache_easy_ttl/expired`）、`db_pools` / `redis_pools`（遍历两类 CE 静态 `instances` 注册表，逐池调用其既有 `stats()` 方法，键为池名，零计数器重复实现）、`requests`（`count`/`errors`，`Application::run()` 累计 + 派发后 pending exception 计错误）、以及 L1/F1 计数器。纯读、零副作用。
- **改动**：新增 `src/tool/monitor.c` / `src/tool/monitor.h`（约 190 行）；`src/gene.c`（include + `GENE_STARTUP(monitor)`）；`src/config.m4` / `src/config.w32`（新编译单元）；demo 新增 `GET /monitor` 端点（`demo/application/Controllers/Monitor.php` + `demo/config/router.ini.php`）；`test/CacheTest.php` 新增 `testMonitorStats()` 结构断言。

### 9.6 F3（P1，`Controller::init()` 钩子）→ 已实现 ✅

- `Gene\Controller` 基类新增空实现 `init()`（Yaf 同语义）；两处控制器直派路径——`gene_router_dispatch_direct()` 与 `PHP_METHOD(gene_router, dispatch)`——在实例化后、action 前经共用 helper `gene_router_call_controller_init()` 调用一次。仅作用于 `Gene\Controller` 子类（`instanceof_function` 门控），Hook/普通类直派目标不受影响；`init()` 抛异常时 unwinding 不再调用 action（dispatch_direct 返回 0 走既有错误路径）。
- **回归联动**：O7 中 Mvc 失败项（`test/MvcTest.php:38` 直接调用 `$this->controller->init()`）预期转绿。
- **改动**：`src/mvc/controller.c`、`src/router/router.c`（+include `mvc/controller.h`）。

### 9.7 L4（log.c rv 槽）→ 已实现 ✅

- 按报告加强版修法：`Log::exception()` 中 rv1/rv2/rv3 先 `ZVAL_UNDEF` 再传入 `zend_read_property`，三处 `ZVAL_COPY` 完成后逐个 `zval_ptr_dtor`。魔术 `__get` 的 Throwable 子类场景（临时值写入 rv）不再构成确定性泄漏。
- **改动**：`src/tool/log.c`。

### 9.8 F5 / F6（P2）→ 已实现 ✅

- **F5 `Validate::extend()`**：新增静态方法 `extend(string $rule, callable $fn)`，注册表存于 `GENE_G(validate_ext)`（生命周期镜像 fn_cache：FPM 请求级 RSHUTDOWN 销毁、Swoole worker 级 MSHUTDOWN 销毁）。`validCheck()` 分派顺序调整：实例闭包表 → **extend 用户表** → 内置 `rule_*` 表，与报告「先查用户表再落内置表」一致；回调签名 `fn($value, ...$args): bool`，失败消息回退链（规则 msg → 字段 msg → 通用模板）与内置规则完全复用。`test/HttpTest.php` 新增正/反两用例。
- **F6 cache_easy TTL**：新增 `gene.cache_easy_ttl` INI（秒，默认 `0` 关闭，完全向后兼容）；`load_file()` 读路径惰性过期——超 TTL 条目 WRLOCK 内删除并经未命中分支重建 + 重新 import；过期计数 `cache_easy_expired` 经 `Monitor::stats()` 可观测，闭环 O4。
- **改动**：`src/http/validate.c`、`src/gene.h`、`src/gene.c`（INI/生命周期）、`src/app/application.c`。

### 9.9 配套同步

- `CHANGELOG.md` [5.6.8]：安全/性能/新增/修复四类条目 + 修改文件一览全量登记。
- `gene-ide-helper`：新增 `Gene/Monitor.php`；`Controller.php` +`init()`；`Validate.php` +`extend()`。
- `gene-ai-helper`：`reference.md`（Controller/Validate 表 + 新增 Gene\Monitor 章节 + 两项 INI）；`swoole.md`（§7.1 自动 cleanup 兜底说明 + §8 常见坑表更新）。
- 测试：`test/CacheTest.php`（Monitor 结构断言）、`test/HttpTest.php`（extend 正反例）；`test/MvcTest.php:38` 既有 init 用例由红转绿（无需改动）。

### 9.10 回写后验证清单（并入 §七，全部待 Linux 环境）

1. **编译**：ZTS/NTS 双构建零告警（重点：gene.c 中段声明、monitor.c 新编译单元、config.m4/w32 同步）；
2. **H1 回归**：§七.1 既定项不变（FPM 无 opcache / `file_cache_only=1` + ASAN，≥2 请求循环）；
3. **M1 量化**：`swoole_context_soak.php` 协程风暴 10 万，确认 `co_contexts_sweep_skipped` 增长、`co_contexts_sweep_us` 受控（前置校验同 §七.2：`exists` 解析成功 + cap ≥ 16）；
4. **F1 验证**：`gene.swoole_auto_cleanup=1` 下漏调 cleanup 的 soak，`swoole_auto_cleanup_reclaimed` ≈ 协程数、RSS 平稳；defer 缺失环境（旧版 Swoole 或 kill 场景）验证 run() 降级路径；嵌套 run 不误清；
5. **F2 冒烟**：`/monitor` 端点 + `Monitor::stats()` 三分区结构断言（test/CacheTest）；
6. **F3 回归**：MvcTest init 用例转绿 + 子类重写 init 被直派调用一次的端到端用例；
7. **F5/F6 功能**：extend 正反例（HttpTest 已含）；`cache_easy_ttl` 小值下过期-重导入循环无泄漏（Valgrind）。

---

## 八、交叉索引

- `audit/AUDIT_REPORT_2026_07_12.md` §三/§十：P0-P2 问题分级与分阶段计划（本报告 O2/O3/O4/O6 来源）
- `audit/AUDIT_REPORT_2026_07_13.md`：H1（ZTS）修复范围（本报告 H1 为其同类残留）
- `audit/AUDIT_REPORT_2026_05_29.md`：view.c 跨请求 UAF 修复（本报告 H1 的判定先例）
- `audit/AUDIT_REPORT_2026_07_03.md` §10.2-10.4：静态缓存迁移残留与待验证项
