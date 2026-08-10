# Gene 扩展框架三向审计报告（内存泄漏 / 并发性能 / 功能完善）

> 审计版本：5.6.9（HEAD `f198821`）
> 审计日期：2026-08-06
> 审计方式：纯静态代码审查 + 与 `AUDIT_REPORT_2026_07_30.md`、`audit/plan/PLAN.md` 交叉复核
> 审计范围：`src/` 全模块（约 1.1MB C 代码，24 个子模块），另抽查 `test/`、`docs/`、`gene-ide-helper/`
> 环境约束：本机为 Windows，**无法执行编译、ASAN/Valgrind、压测与长跑**；所有结论标注「静态确认」或「需运行时验证」
> 落地情况见第九节，仅反映当前代码实际状态；未实现/待验证项汇总至 `audit/plan/PLAN.md`。
>
> **状态：已关闭（2026-08-10 复核）**。逐条对照源码复核结论：
> C1 / PF1、F0-1 / F0-2 / F0-3、F1-1~F1-8 及 P1 小面批 / P2 批（`Di::alias`、`Request::isSecure`、
> `Memory::mget/mset`、`Monitor::reset/prometheus`、`View::render/clearAssign`、
> `Response::getStatusCode/isSent/sendFile`、`Session::clear/all`、`Validate::bail/sometimes`、
> `Log::critical/alert/emergency`+context、`SQLite::attach/detach`、`Benchmark::mark/lap`、
> `Application::stop/isStopped`）**全部已实现**，证据与 §9.1 一致。
> §9.2 的未实现清单（F4、F3、Pool 连接泄漏检测、Hook 优先级/`stopPropagation`、
> Redis/Memcached pipeline、`Application::getResponse()`、`gene.pool_max_overflow`、
> `gene.fn_cache_max`、C2/C3/C4/ML1/ML2/PF2~PF4）**逐项复核属实**，且均已在
> `audit/plan/PLAN.md` §一/§二/§三 立项跟踪。
> 本次复核**未发现新问题**；运行时验证需求（§9.4 十二项）由 PLAN.md §四 承接。

---

## 一、总体结论

1. **未发现任何新的确定性内存泄漏或 use-after-free**。上一轮的 H1（`session.c` 4 槽静态方法缓存 UAF）已确认正确修复 —— 静态缓存被完整删除，改为每次调用 `zend_hash_find_ptr`（<ref_snippet file="F:\github_code\gene\src\session\session.c" lines="78-89" />），修法与 `cache.c` 的既有约定一致。全仓再次扫描 `static zend_function *` / `static zend_class_entry *` / `static zend_string *`，**剩余的缓存目标全部是内部类**（`Swoole\Timer`、`Swoole\Atomic`、`Gene\*`、PDO、内置函数），进程生命周期恒定，不构成同类风险。
2. **本轮最有价值的新发现是一处「修复未对称落地」**：2026-07-03 为 `redis_pool.c` 实现的 CAS 原子递减（P2 项），**从未同步到 `db/pool.c`** —— `pool_decrement_count()` 至今仍是 get→sub 的 TOCTOU 序列（见 C1）。这不是新引入的缺陷，而是一次已知缺陷的**半程修复**，两个连接池的并发正确性等级因此不一致。
3. **上一轮的 M1（sweep O(N²) 放大）已闭环**：`gene.c:907-935` 已实现 cooldown（`cap/4` 分配数 + 表增长双触发），并导出 `co_contexts_sweep_skipped` 遥测，PLAN.md 中「sweep batch/cursor 方案」一项可据此收敛为「已缓解，batch 化降级为可选优化」。
4. **功能完善方面，最突出的短板集中在三处**：DB 构建器不完整（MySQL/PgSQL 无 `join`/`union`，MSSQL 构建器缺失过半）、安全相关 API 缺位（`Session::regenerateId()` 缺失，会话固定攻击无标准防护手段）、以及 `Gene\Request` 与 `Gene\Controller` 之间的 HTTP 方法判定 API 不对称（`Request` 缺 `isDelete()`）。
5. **本轮明确驳回 2 项候选发现**（见第六节）。审计过程中对每一条候选结论都做了原文复核，未通过复核的不进入正式发现，避免向台账注入噪声。
6. 总体评价：**这是一个内存管理质量相当高的 C 扩展**。历史审计的闭环执行到位，注释中的 `[GENE_AUDIT:...]` / `[GENE_PERF:...]` 溯源标记体系有效。当前的主要风险已不在内存安全，而在**并发修复的一致性**与**功能面覆盖**。

---

## 二、内存泄漏 / 内存安全

### 2.1 结论：无高危、无中危新发现

本轮逐路径核查了下列内容，**未发现新的泄漏、重复释放或悬垂指针**：

| 核查项 | 位置 | 结论 |
|--------|------|------|
| `session.c` H1 修复复核 | `src/session/session.c:68-111` | ✅ 静态缓存已删除，每次调用直接查找；`call_user_function` 回退路径的 `function_name` 正确 `zval_ptr_dtor` |
| 全仓静态用户态指针缓存残留 | `common.c` / `validate.c` / `pdo.c` / `view.c` / `di.c` / `factory.c` / `hook.c` | ✅ 无残留，历史修复完整 |
| persistent 资源 MSHUTDOWN 析构 | `gene.c:1212-1225`、`pool.c:1462-1466`、`redis_pool.c:1723-1727` | ✅ cache / cache_easy / fn_cache / route_pc / closure_src_cache / LRU / 两个 named_cache 均已挂载清理 |
| `co_contexts` 条目生命周期 | `gene.c:620-636`（dtor）、`711-782`（sweep）、`1096-1101`（RSHUTDOWN 重置） | ✅ dtor 配对正确，D2 的 cooldown 状态重置已补齐 |
| RINIT / RSHUTDOWN 配对 | `gene.c:1251-1296` | ✅ |
| `router.c` smart_str / goto cleanup | `router.c:992/1176/1282/1320/1323/1791` | ✅ 所有错误分支均经 cleanup 标签释放 |
| `memory.c` persistent key 管理 | `memory.c:62-150`、`396-477` | ✅ `gene_hash_destroy` 正确 `pefree` persistent key；LRU tracking set 配对正确 |
| `spprintf` / 堆缓冲释放 | `exception.c:364-404/466-494`、`log.c:176-177/312-320`、`controller.c:434-437`、`view.c:858-861`、`response.c:618-621` | ✅ |

### 2.2 低风险 / 观察项

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| ML1 | `src/db/pool.c:55-76`、`src/cache/redis_pool.c:42-63` | 两个 `named_cache` 用 `emalloc` 而非 `pemalloc` 分配进程级 HashTable。**已由 MSHUTDOWN 兜底，无实际泄漏**；残留问题是语义错配：一个跨请求存活的表挂在请求级堆上，Swoole worker 被信号硬杀时表交由 PHP MM 收尾（valgrind 可见但无害）。**建议维持现状**，`pool.c:55-68` 的注释已完整记录该权衡，改为 pemalloc 的收益不抵改动风险 | 静态确认 |
| ML2 | `src/gene.c:748` | sweep 的 `emalloc(sizeof(zend_ulong) * total)` 瞬态分配（上轮 L2）。M1 cooldown 落地后触发频率已大幅下降，**优先级可从 L2 进一步下调**，除非压测显示 cap≥8192 时该 64KB 分配进入火焰图 | 需运行时验证 |

---

## 三、并发正确性与性能

### 3.1 中风险

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| **C1** | `src/db/pool.c:476-488`（`pool_decrement_count`） | **CAS 修复未对称落地**：`redis_pool.c` 于 2026-07-03 已改为 CAS 循环，`db/pool.c` 的同名逻辑至今仍是 get→sub 的 TOCTOU 序列 | 静态确认（下溢需运行时复现） |

**C1 详情**

`db/pool.c` 现状：

```c
476: static void pool_decrement_count(zval *self) {
477:     zval *atomic = zend_read_property(..., GENE_POOL_PROPERTY_COUNT, ...);
478:     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
481:         POOL_ATOMIC_CALL(atomic, "get", 0, &ret);          // 读
482:         zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
484:         if (val > 0) {
485:             POOL_ATOMIC_CALL(atomic, "sub", 1, NULL);      // 改（非原子）
486:         }
```

`redis_pool.c:427-473` 对**完全相同的语义**已实现 CAS 循环 + 64 轮上限 + `redis_pool_cas_abandoned` 遥测 + once 告警。

- **风险模型（务必准确理解）**：`Swoole\Atomic` 建立在共享内存上，其设计意图就是**跨 worker 进程**共享。同一 worker 内，Swoole 是协作式调度，`Atomic::get`/`sub` 是内部方法不产生让出点，故**单 worker 内的协程之间不会撕裂这个序列**。真正的窗口在于同一个 `Atomic` 被多个 worker 进程共享的部署形态（在 `Server::start()` 之前创建 pool、随 fork 复制的场景）。这也正是 07-03 报告为 redis_pool 立项修复时给出的理由。
- **后果**：`val` 读到 1 → 两个进程都执行 `sub(1)` → 计数下溢至 -1 → `put()` 的 `count > max` 自动收缩判据（`pool.c:814`）与 `get()` 的容量判据长期偏移，池要么无法回收溢出连接，要么虚假认为仍有余量而持续新建连接。
- **修复建议**：将 `redis_pool.c:414-473` 的 `rpool_atomic_cmpset` + CAS 循环**原样移植**到 `pool.c`，并对齐遥测：在 `gene.h` 的 module globals 中增补 `db_pool_cas_abandoned` / `db_pool_cas_warned`，在 `monitor.c:128` 附近与 `redis_pool_cas_abandoned` 并列导出。这是一次「把已验证的修法复制到孪生代码」的低风险改动，无需新设计。
- **附带收益**：现状 `put()` 路径为 `pool_get_count`（1 次 PHP 调用）+ `pool_get_max` + `pool_decrement_count`（2 次 PHP 调用）= **每次归还最多 3 次 `zend_call_known_function` 跨界**。CAS 化后可与 `pool_get_count` 的结果复用，压缩到 1~2 次。
- **验证方法**：多 worker（≥4）+ 高并发借还压测，观察 `Monitor::stats` 中 pool count 是否出现负值或与 channel length 长期不一致；TSAN 对 Swoole 共享内存无效，需靠计数一致性断言。

### 3.2 低风险 / 观察项

| # | 位置 | 问题 | 证据类型 |
|---|------|------|----------|
| C2 | `src/gene.c:89-90、102-103` | dlsym 解析结果 `gene_swoole_getcid_capi` + `..._resolved` 为进程级 static，ZTS 下两个线程可能并发解析。**代码注释已声明这是良性竞态**（两线程写入同一 dlsym 结果），复核确认该论断成立。唯一的理论瑕疵是 `resolved` 标志与指针写入之间无内存屏障，弱内存序架构（ARM）上可能出现「读到 resolved=1 但指针尚未可见」。**建议**：把 `resolved` 改为在指针写入**之后**赋值并配 release 屏障，或 ZTS 下走 `pthread_once`。考虑到 Swoole 不支持 ZTS，实际风险极低 | 静态确认 |
| C3 | `src/db/pool.c:55`、`src/cache/redis_pool.c:42` | 进程级 static `HashTable *named_cache` 在 ZTS 下跨线程共享且无锁。同 C2，Swoole 不支持 ZTS，属约定对齐问题而非现实 bug。与 PLAN.md「剩余 function-local static 迁移」合并处理 | 静态确认 |
| C4 | `src/cache/memory.c:565-570` | `GENE_CACHE_RDLOCK()` 依据 `GENE_G(worker_ready)` 决定是否跳过加锁。该标志单调 0→1 且在 worker 单线程阶段翻转，**复核认为设计成立**；仅在未来引入多线程 worker 时需重新评估。记录为观察项，不建议现在改动 | 静态确认 |

### 3.3 性能建议（均需 profile 达标后方可立项）

| # | 位置 | 建议 | 预估收益 | 验证方法 |
|---|------|------|----------|----------|
| PF1 | `src/db/pool.c:790-822`（`put()`） | 合并 `pool_get_count` / `pool_get_max` / `pool_decrement_count` 的重复原子读；`pool_get_max` 是普通属性读（`pool.c:513-516`），可提到循环外。每次归还省 1~2 次 PHP 跨界调用 | 高并发借还路径 5-10% | pool get/put 微基准 + wrk P99 |
| PF2 | `src/router/router.c:2233-2544` 一带的 `snprintf("%s%s", ...)` | 简单拼接改 `memcpy`。**注意：这些几乎全在路由注册/编译阶段**（应用启动时执行一次），不在每请求热路径上。收益体现在冷启动/reload 时间，**不应按热路径优化对待** | 路由编译 5-10%，QPS 无感 | 启动耗时对比 |
| PF3 | `src/router/router.c:245-278` | `str_init` + `php_strtok_r` 的字符串复制。**仅在配置了 `langs` 的多语言路由中执行**；核心匹配函数 `get_path_router_inner`（`router.c:289+`）早已是指针扫描实现。改造收益局限于多语言站点 | 多语言路由 3-5% | 多语言路由基准 |
| PF4 | `src/router/router.c:656` | 闭包 fn_cache 的 key 由 `snprintf(fid, "fn_%u", handle)` 生成后做字符串 hash；`handle` 本身是 uint32，可直接 `zend_hash_index_find(GENE_G(fn_cache), handle)` | 闭包路由 1-2% | 闭包路由基准 |

> **准入约束**（沿用 PLAN.md）：PF1~PF4 必须先通过 `tools/acceptance` 的 profile 准入脚本拿到证据，否则不得进入主线。其中仅 PF1 兼具正确性收益（与 C1 同批实施最经济）。

### 3.4 复核确认无问题的并发点

- `gene.c:848-881`：vm_stack 身份 fast path **后接 second-chance cid 校验**，协程复用 cid 的串话场景已被覆盖，实现正确。
- `gene.c:907-935`：sweep cooldown（上轮 M1）已落地，`cap/4` 分配数与表增长双触发，`co_contexts_sweep_skipped` 遥测已导出至 `Memory::stats` 与 `Monitor::stats`。
- `cache/redis_pool.c:414-473`：CAS 实现正确，64 轮上限 + 计数 + once 告警的组合符合上轮 L1 的修正建议。
- `cache/memory.c:595-607`：`gene_memory_get_triple` 单锁批量读实现正确。
- `db/pool.c:337-347`：PDO `getAttribute` 按 class entry 缓存，正确。
- `gene.h:71-85`：`GENE_CG_FN_LOOKUP` 的 ZTS / 非 ZTS 分支划分正确。

---

## 四、功能完善度

### 4.1 模块完备度总览

| 模块 | 现状 | 主要缺口 | 优先级 |
|------|------|----------|--------|
| Application | 单例 / load / autoload / run / cleanup / workerReady / prewarmCtxPool | 无优雅停机 `stop()`；`setResponse()` 无对应 getter | P2 |
| Router | 二叉树路由、group/prefix/lang、事件与路由树管理 | 无 `match()` 纯匹配（路由无法单测）；无中间件管道（F4，未实现） | P1 |
| Controller | 参数访问、redirect/alert/display、JSON、assign | 无 `forward()` 内部转发；生命周期钩子缺位（F3 已 revert） | P1 |
| View | display/assign/contains、模板编译、scope 隔离 | 无 `render()` 返回字符串（API 场景）；无 `clearAssign()` | P2 |
| Model | DI 集成 + success/error/data 响应封装 | 无 ORM/ActiveRecord 能力 | P2（见下方说明） |
| Hook | before/after/handle | 无优先级、无 `stopPropagation()` | P2 |
| Di | get/set/has/del/getInstance | 无 `instance($class,$params)` 显式实例化；无 `alias()` | P1 |
| Request | get/post/server/cookie/header/files/env、isAjax、方法判定、rawContent、init/clear | **缺 `isDelete()`**（与 Controller 不对称）；无 `isSecure()` | P1 |
| Response | header/cookie/redirect/end/JSON | 无 `getStatusCode()` / `isSent()` / `sendFile()` | P2 |
| Session | get/set/del/has、load/save/destroy、sessionId、lifetime、cookie | **无 `regenerateId()`**（会话固定攻击无标准防护）；无 `clear()` / `all()` | **P0** |
| Cache(Memory) | set/get/getTime/exists/del/clean/stats | **无 `incr()` / `decr()`**（限流、计数场景刚需）；无 `mget()/mset()` | P1 |
| Cache(Redis/Memcached) | 代理透传 | 无 pipeline/multi（可经 `call()` 透传，非硬缺口） | P2 |
| Db(PDO) | 基础包装、参数绑定 | 无 `lastInsertId()` / `rowCount()` / `quote()` | P1 |
| Db(MySQL/PgSQL) | select/insert/update/delete + where/in/group/order/limit | **无 `join()` / `union()` / 子查询** | **P0** |
| Db(MSSQL) | 仅基础 CRUD | **构建器缺失过半**（where/group/order 等），与其他驱动能力严重不齐 | **P0** |
| Db(SQLite) | 构建器 + 事务，相对完整 | 无 attach | P2 |
| Db(Pool) | borrow/return/stats/recycleIdle | 无 `healthCheck()`；无 overflow 硬熔断；无连接泄漏检测 | P1 |
| Validate | 规则丰富、链式、自定义、分组 | 无 `bail()` 首错即停；无 `sometimes()` | P2 |
| Log | debug~exception、级别控制 | 无 `critical()/emergency()`；无 context 结构化字段 | P2 |
| Monitor | 仅 `stats()` | 无 `reset()`；无 Prometheus 导出；缺池等待队列/慢查询指标 | P1 |
| Benchmark / Language / Exception / Factory / Service | 基本可用 | 增强项，见下 | P2 |

> **关于 Model 的定级说明**：子代理将「Model 缺 ORM」定为 P0，**本报告下调为 P2**。Gene 的 Model 定位是 DI 容器 + 响应封装，DB 能力由独立的 `Gene\Db\*` 构建器承担，这是一个**明确的架构选择**而非缺陷。引入 ActiveRecord 会带来表映射、关联、事件、脏检查等大量新表面，属于新架构立项而非「补功能」，不应与 MSSQL 构建器缺失这类真实短板混为一谈。

### 4.2 优先级建议清单

#### P0

**F0-1　`Session::regenerateId()`**
- 动机：登录 / 提权后无法更换 session id，**会话固定（Session Fixation）攻击缺乏标准防护手段**。这是全表中唯一带安全属性的缺口。
- 涉及文件：`src/session/session.c`（`setSessionId` 附近，方法表 `session.c` 尾部）
- 草案：`PHP_METHOD(gene_session, regenerateId)`，签名 `regenerateId(bool $deleteOld = true): string`。复用现有 SSID 生成逻辑，写入 `GENE_SESSION_ID` 属性、标脏、调 `gene_session_auto_cookie()`；`$deleteOld` 为真时对旧 id 调用存储插件的 delete。
- 风险：低。需确认自定义存储插件均实现 delete（`gene_session_method_delete()` 已存在）。

**F0-2　MySQL / PgSQL `join()` / `union()`**
- 动机：无连接查询能力，任何多表业务都必须绕过构建器手写 SQL，等于放弃了构建器的注入防护。
- 涉及文件：`src/db/mysql.c`、`src/db/pgsql.c`
- 草案：`join(string $table, string $on, string $type = 'INNER')`、`leftJoin()`、`rightJoin()`、`union(string|object $query)`。表名/别名必须走现有 identifier quote 路径（参见 07-03 §1.3 的引号处理），`$on` 条件建议只接受结构化数组而非裸字符串，避免开出新的注入面。
- 风险：中。SQL 构建复杂度上升，**必须补齐注入回归用例**后再合入。

**F0-3　MSSQL 构建器补全**
- 动机：`mssql.c` 与 `mysql.c` 体量相近（40KB vs 41KB）但对外能力差距明显，用户在驱动间迁移会遇到不一致的 API 面。
- 涉及文件：`src/db/mssql.c`
- 草案：对齐 MySQL 的 where/in/group/order/limit 集合，`limit` 需适配 T-SQL 的 `OFFSET ... FETCH NEXT`。
- 风险：中，需 SQL Server 环境回归；本机与 CI 均缺该环境，**建议先补测试夹具再动代码**。

#### P1

| # | 建议 | 涉及文件 | 草案 |
|---|------|----------|------|
| F1-1 | `Request::isDelete()` | `src/http/request.c` | 与 `isGet/isPost/isPut/isHead/isOptions` 同型；`Controller::isDelete` 已存在，纯粹补齐不对称（详见 4.3） |
| F1-2 | `Memory::incr()` / `decr()` | `src/cache/memory.c` | `incr(string $key, int $step = 1): int\|false`。必须在 `GENE_CACHE_WRLOCK` 内完成读-改-写，不可拆成 get+set |
| F1-3 | `Router::match()` | `src/router/router.c` | `match(string $method, string $uri): array\|false`，复用 dispatch 匹配逻辑但不执行 handler。**这是解锁路由单元测试的前置条件** |
| F1-4 | `Pool::healthCheck()` | `src/db/pool.c` | 对 idle 连接执行轻量探活，返回存活数；与 `recycleIdle` 复用遍历 |
| F1-5 | `Di::instance()` | `src/di/di.c` | `instance(string $class, array $params = []): object`，走 `gene_class_instance` 但不入容器 |
| F1-6 | `Controller::forward()` | `src/mvc/controller.c` | `forward(string $controller, string $action, array $params = [])`；**必须带转发深度上限**（建议 ≤5）防止无限递归 |
| F1-7 | Monitor 指标补全 | `src/tool/monitor.c` | 增补：pool 等待队列长度、获取超时次数、memory cache 命中率、慢查询计数、`db_pool_cas_abandoned`（配合 C1） |
| F1-8 | PDO `lastInsertId()` / `rowCount()` | `src/db/pdo.c` | 直接透传底层 PDO |

#### P2

`View::render()`（返回字符串）、`Log::critical()/emergency()` + context 字段、`Benchmark::mark()/lap()`、`Monitor::reset()` 与 Prometheus 文本导出、`Validate::bail()`、`Hook` 优先级与 `stopPropagation()`、`Application::stop()`。

### 4.3 API 一致性问题

| 位置 | 现状 | 建议 | 破坏兼容 |
|------|------|------|----------|
| `src/http/request.c` 方法表 vs `src/mvc/controller.c` 方法表 | `Controller` 提供 `isGet/isPost/isPut/isHead/isOptions/isDelete/isCli`；`Request` 提供同一组**但独缺 `isDelete`** | 为 `Request` 补 `isDelete()`。两者语义本应完全镜像，缺失属遗漏而非设计 | 否（纯新增） |
| `src/cache/memory.c`（`set`）、`src/config/configs.c`（`set`） | TTL `0` 表示永久 | 语义本身合理，但需在 `docs/CONFIGURATION.md` 中显式写明；**不建议改为 `-1`**，收益不抵破坏性 | —（仅文档） |
| `src/mvc/model.c` `success($text, $code=2000)` / `error($text, $code=4000)` | 默认码 2000 / 4000 | **不建议改**。这是框架自有的业务码空间，与 HTTP 状态码本就不同源，改成 200/400 反而制造歧义。仅需文档说明 | —（仅文档） |
| `src/db/mysql.c:120` `mysql_reset_sql_params` | 内部函数，无对外重置入口 | 暴露公开 `reset()`，便于复用同一实例构建多条 SQL | 否 |
| Router 方法命名 | `getEvent/getTree/getConf`（驼峰） | 全仓已统一为驼峰，**无需改动**（子代理提出的 snake_case 建议已驳回） | — |

### 4.4 配置项审计

`src/gene.c:141-158` 共 16 项 `gene.*` INI 指令（2026-08-07 二批新增 `gene.slow_query_ms` 后为 17 项）。

| 关注项 | 当前默认 | 评估 |
|--------|----------|------|
| `gene.cache_max_items` | `0`（无界） | ⚠️ 危险默认。已有启动 NOTICE，但生产建议在 `docs/CONFIGURATION.md` 中给出明确推荐值（如 10000）并解释内存换算方式 |
| `gene.closure_src_cache_max` | `1024` | ⚠️ **缺文档**。应补入 `docs/CONFIGURATION.md` |
| `gene.co_contexts_max` | `1024` | 高并发下偏小；M1 cooldown 落地后已不再引发 O(N²)，但仍建议文档注明生产参考值 |
| `gene.ctx_pool_max` | `256` | 同上 |

建议新增：`gene.slow_query_ms`（慢查询阈值，配合 F1-7 —— **已于 2026-08-07 二批落地**）、`gene.pool_max_overflow`（连接池硬熔断，PLAN.md 既有项）、`gene.fn_cache_max`（fn_cache LRU 容量治理，PLAN.md 既有项）。

### 4.5 文档与测试缺口

**文档**
- `docs/` 目前仅 `CONFIGURATION.md`。建议补：`API_REFERENCE.md`、`ARCHITECTURE.md`（尤其是 Swoole 协程上下文模型与 `co_contexts` 语义，这是本扩展最难理解的部分）、`PERFORMANCE_TUNING.md`（INI 调参 + Monitor 指标解读）。
- `gene-ide-helper/Gene/Application.php` 版本注解为 5.4.3，实际 5.6.9，需更新。
- `Router::getRouterUri()` 在 C 层有实现但 ide-helper 中无声明。

**测试**（`test/` 现有 12 个用例文件）

完全无独立测试的模块：**Di、Hook、Monitor、Exception、Factory、Memcached、RedisPool、MSSQL**。其中 **Di 与 Hook 是核心派发路径**，缺测试的风险最高，建议优先补 `test/DiTest.php`、`test/HookTest.php`。`Router::match()`（F1-3）落地后可显著提升 RouterTest 的可测性。

---

## 五、待运行时验证清单（承接 PLAN.md O6/O7）

本轮**未新增任何需要 Linux 才能推进的阻塞项**，原有清单不变：ZTS/NTS 双构建零告警、ASAN/Valgrind、百万请求/24h RSS 长跑、CAS/pool 压测、dlsym 符号可见性。

本轮新增的验证需求仅一条：**C1 修复后，多 worker 并发借还压测下 pool count 的一致性断言**。

---

## 六、本轮驳回的候选发现

记录于此以避免后续审计重复投入。

| 候选 | 驳回理由 |
|------|----------|
| `db/pool.c:814` `pool_get_count(self) > pool_get_max(self)` 存在竞态 | **不成立**。`pool_get_max` 是普通对象属性读（`pool.c:513-516`），不是原子对象；`max` 为创建时确定的配置值，运行期不变。两次读之间也不存在协程让出点。该行的真实问题只是**多余的 PHP 跨界调用开销**，已归入 PF1 |
| `Request::getBody()` 缺失 | **不成立**。`Gene\Request::rawContent()` 已提供原始 body 读取（`request.c` 方法表），功能已覆盖 |
| Router 方法名应改为 snake_case | **不采纳**。全仓已一致使用驼峰，改名是纯破坏性变更且无收益 |
| `Model` 缺 ORM 属 P0 | **下调为 P2**。属架构选择，见 4.1 说明 |
| sweep O(N²) 放大（上轮 M1）仍开放 | **已闭环**。`gene.c:907-935` cooldown 已实现并带遥测 |

---

## 七、建议的实施顺序

1. **C1**（`db/pool.c` CAS 对称化 + `db_pool_cas_abandoned` 遥测）—— 唯一的正确性缺口，且修法已在 `redis_pool.c` 中验证过，风险最低、价值最高。可与 **PF1** 同批实施。
2. **F0-1**（`Session::regenerateId()`）—— 唯一带安全属性的功能缺口。
3. **F1-3**（`Router::match()`）+ `test/DiTest.php` / `test/HookTest.php` —— 先补测试基础设施，为后续 DB 构建器改动提供回归网。
4. **F0-2 / F0-3**（DB 构建器补全）—— 改动面最大，必须在第 3 步的回归网就位后进行。
5. **C2 / C3 / ML1 / ML2 / PF2~PF4** —— 观察项，不建议主动改动，等 profile 或 ZTS 需求出现后再评估。

---

## 八、维护约定

按 `audit/plan/PLAN.md` §六：本报告中**仍未实现/未验证**的条目（C1 之外的观察项、F0/F1/F2 清单、文档与测试缺口）应在处理完成后回写状态；新立项前须在 `tools/acceptance` 取得 profile/ASAN 证据。

---

## 九、落地情况（反映当前代码实际状态）

> 以下为审计发现各项及建议的当前落地状态，经源码核实。运行时验证（编译/ASAN/压测/长跑）仍因 Windows 环境约束而悬置，待 Linux 环境补齐后承接 PLAN.md O6/O7。

### 9.1 已落地项

#### 并发修复

| 项 | 状态 | 落地说明 |
|---|------|----------|
| **C1** db/pool.c CAS 对称化 | ✅ 已实现 | `pool.c:476-547`：将 get→sub 的 TOCTOU 序列改为 CAS 循环（`pool_atomic_cmpset` + 64 轮上限），对齐 `redis_pool.c`；新增 `db_pool_cas_abandoned` / `db_pool_cas_warned` 全局计数 + once 告警，经 `Monitor::stats()` 出口。保留无 `cmpset` 时的 get→sub 回退路径 |
| **PF1** pool put() 跨界调用合并 | ✅ 已实现 | `pool.c:899-924`：合并 `pool_get_count` / `pool_get_max` / `pool_decrement_count` 的重复跨界读，`pool_get_max` 提到循环外 |

#### 功能补全（P0）

| 项 | 状态 | 落地说明 |
|---|------|----------|
| **F0-1** `Session::regenerateId()` | ✅ 已实现 | `session.c:1175-1225`：生成新 ID、按需删除旧会话、刷新内部 ID 与 cookie、标记 dirty |
| **F0-2** MySQL/PgSQL `join()`/`union()` | ✅ 已实现 | `mysql.c` / `pgsql.c`：新增 `join()` / `leftJoin()` / `rightJoin()` / `union()` / `reset()`，含标识符引用、ON 条件拼接、UNION 子查询参数合并。错误路径 `smart_str` 释放已补齐 |
| **F0-3** MSSQL 构建器补全 | ✅ 已实现 | `mssql.c`：补全 `where` / `in` / `group` / `order` / `limit`（OFFSET/FETCH）/ `join` / `leftJoin` / `rightJoin` / `union` / `reset`，能力对齐 MySQL |

#### 功能补全（P1）

| 项 | 状态 | 落地说明 |
|---|------|----------|
| **F1-1** `Request::isDelete()` | ✅ 已实现 | `request.c:670`：注册 `isDelete`，与 `Controller` 镜像 |
| **F1-2** `Memory::incr()`/`decr()` | ✅ 已实现 | `memory.c:1276/1303`：在 `GENE_CACHE_WRLOCK` 内完成读-改-写 |
| **F1-3** `Router::match()` | ✅ 已实现 | `router.c`：纯匹配 API `match($method, $uri): array|false`，复用 dispatch 匹配逻辑但不执行 handler |
| **F1-4** `Pool::healthCheck()` | ✅ 已实现 | `pool.c:1048-1090`：对 idle 连接轻量探活，返回存活数 |
| **F1-5** `Di::instance()` | ✅ 已实现 | `di.c`：`instance($class, $params=[])` 走 `gene_class_instance` 但不入容器；解析结果改为 `zend_string_copy` 持有副本防 rehash 悬垂 |
| **F1-6** `Controller::forward()` | ✅ 已实现 | `controller.c`：`forward($controller, $action, $params=[])`，带转发深度上限（≤5）防无限递归 |
| **F1-7** Monitor 指标补全 | ✅ 已实现 | `monitor.c`：增补 `db_pool_cas_abandoned`、`db_pool_get_timeout`、`memory_cache_hit`/`miss`、`db_slow_query_count` + `gene.slow_query_ms` INI（默认 0 禁用，`pdo.c` 公共执行入口埋点） |
| **F1-8** PDO `lastInsertId()`/`rowCount()`/`quote()` | ✅ 已实现 | `pdo.c` + 四驱动：`lastInsertId`/`rowCount` 为 `lastId`/`affectedRows` 的 `PHP_MALIAS` 别名；`quote()` 经 `gene_pdo_quote()` 透传底层 `PDO::quote` |

#### 功能补全（P1 小面批 + P2 批）

| 项 | 状态 | 落地说明 |
|---|------|----------|
| `Di::alias()` | ✅ 已实现 | `di.c:495`：实例属性 `di_alias` 哈希表，`instance()` 先查别名再查注册表 |
| `Request::isSecure()` | ✅ 已实现 | `request.c:400`：Swoole 读 `server.https`/`server.server_port`；FPM 读 `HTTPS`/`SERVER_PORT` |
| `Memory::mget()`/`mset()` | ✅ 已实现 | `memory.c:1333/1378`：单次遍历哈希表，复用 get/set 逻辑 |
| `Monitor::reset()` + `prometheus()` | ✅ 已实现 | `monitor.c:196/297`：reset 归零全部全局计数器；Prometheus 文本格式含 `# HELP`/`# TYPE` |
| `View::render()`/`clearAssign()` | ✅ 已实现 | `view.c:810/854`：render 返回字符串不输出（`php_output_start_default` 返回值已检查）；clearAssign 清空 `view_vars` |
| `Response::getStatusCode()`/`isSent()`/`sendFile()` | ✅ 已实现 | `response.c`：Swoole 读 `response_status` ctx 字段；sendFile FPM 路径 8KB 分块流式输出（`ssize_t got` 防 SIZE_MAX 越界），仅接受本地普通文件（`php_stream_open_wrapper_ex` 拒绝 wrapper 防 SSRF），offset 越过 EOF 返回 false |
| `Session::clear()`/`all()` | ✅ 已实现 | `session.c:1098/1115`：clear 销毁 session 变量；all 返回 `$_SESSION` |
| `Validate::bail()`/`sometimes()` | ✅ 已实现 | `validate.c:489/506`：bail 标志使 validCheck 首错即停（含 group 模式）；sometimes 回调返回 false 跳过该字段全部规则。MINIT 已声明属性防动态属性拦截 |
| `Log::critical()`/`alert()`/`emergency()` + context | ✅ 已实现 | `log.c:282/304`：新增 LEVEL_CRITICAL/ALERT/EMERGENCY 常量；所有方法新增 `array $context` 参数，JSON 编码追加到日志行 |
| `SQLite::attach()`/`detach()` | ✅ 已实现 | `sqlite.c`：schema 标识符白名单校验防注入；path 单引号转义 + NUL 字节校验；成功判定排除 `IS_FALSE` |
| `Benchmark::mark()`/`lap()` | ✅ 已实现 | `benchmark.c:225/260`：基于 `gene_hrtime()` 纳秒时间戳；lap 返回 float 毫秒并重置 mark。注明 32 位平台 `zend_long` 纳秒约 4.3s 溢出，仅承诺 64 位构建 |
| `Application::stop()`/`isStopped()` | ✅ 已实现 | `application.c:1448/1461`：`app_stopped` 存于 `gene_request_context`（per-request/per-coroutine 隔离，ctx reset 时自动复位）；router 8 处检查点跳过后续派发 |

#### 落地过程中的缺陷修复（已全部 incorporated）

| 修复 | 说明 |
|------|------|
| `Monitor::stats()` 池分区引用计数 | `monitor.c`：`zend_hash_update` 转移所有权后不再 dtor，`EG(exception)` 中断保护 |
| `gene_db_*_do_join` 错误路径泄漏 | 4 个驱动文件补 `smart_str_free`，与正常路径对称 |
| `sendFile` 编译错误 + 大文件 OOM + ssize_t 越界 + SSRF | `response.c`：`REPORT_ERRORS` 修正、8KB 分块流式、`ssize_t got` 防 SIZE_MAX、`php_stream_open_wrapper_ex` 拒绝 wrapper |
| `app_stopped` Swoole 下永久跳过派发 | 从 module globals 迁入 `gene_request_context`，ctx reset 自动复位 |
| `Memory::set/mset` TTL 静默失效 | 新增进程级 `cache_expiry` 持久哈希表 + 惰性过期 + 每 32 次带 TTL 写入抽样清扫（`gene_memory_expiry_sweep_nolock`），空表短路零开销 |
| `Di::instance()` 别名解析 rehash 悬垂 | 解析结果改为 `zend_string_copy` 持有副本，出口 release |
| `View::render()` 输出缓冲启动失败 | 检查 `php_output_start_default` 返回值，失败时清理符号表后 `RETURN_FALSE` |
| `Log::exception()` json_ret 未初始化 | 补 `ZVAL_UNDEF(&json_ret)` 防御初始化 |
| `Response::isSent()` Swoole 非布尔真值 | 改用 `zend_is_true(&retval)` |
| `Response::redirect` 异常时仍记录状态 | 按 `EG(exception)` / `IS_FALSE` 门控后再记录 |

### 9.2 未实现 / 待立项项

| 项 | 状态 | 说明 |
|---|------|------|
| **F4** 路由级中间件管道 | ❌ 未实现 | 仍停留在建议阶段，不应在 O6 运行时验证全集打通前立项。见 `audit/plan/PLAN.md` |
| **F3** Controller 生命周期钩子 | ❌ 已回退 | v5.6.9 中移除，需重新设计。见 `audit/plan/PLAN.md` |
| Pool 连接泄漏检测 | ❌ 未实现 | `pool.c` 仅有超时溢出建连 + 归还自动收缩，无泄漏检测 |
| Hook 优先级 / `stopPropagation()` | ❌ 未实现 | `hook.c` 仅有 before/after/handle |
| Cache(Redis/Memcached) pipeline/multi | ❌ 未实现 | 可经 `call()` 透传，非硬缺口 |
| `Application::setResponse()` 无对应 getter | ❌ 未实现 | `setResponse` 已注册但无 `getResponse` |
| `gene.pool_max_overflow` / `gene.fn_cache_max` | ❌ 待立项 | PLAN.md 既有项 |
| C2/C3/C4/ML1/ML2/PF2~PF4 | ⏸ 观察项 | 需 profile/ZTS 证据后立项，不主动改动 |

### 9.3 配套同步

- **ide-helper**（`gene-ide-helper/Gene/*.php`）：Session / Db\Mysql / Db\Pgsql / Db\Sqlite / Db\Mssql / Request / Memory / Pool / Di / Router / Controller / Monitor / Application / View / Response / Validate / Log / Benchmark 等均已同步新增 API 与版本注解。
- **docs**（`docs/CONFIGURATION.md`）：补 `gene.slow_query_ms`、`closure_src_cache_max`、`swoole_auto_cleanup`、`cache_easy_ttl`、TTL 语义说明（FPM 惰性+抽样回收 vs Swoole 冻结后仅读掩蔽）、新增 API 章节。
- **测试**：新增 `test/DiTest.php`、`test/HookTest.php`；`test/DatabaseTest.php` SQLite 段补 `lastInsertId`/`rowCount`/`quote` 调用；9 个测试文件新增对应测试方法。

### 9.4 待运行时验证清单（全部待 Linux 环境）

1. Linux `phpize + make`（含 `--enable-debug` / ASAN）零告警编译；
2. C1 修复后多 worker（≥4）并发借还压测下 pool count 一致性断言（无负值、与 channel length 一致）；
3. MySQL / PgSQL / MSSQL `join` / `union` / `where` / `in` / `group` / `order` / `limit` 全量 SQL 回归（含标识符引用与参数绑定）；
4. `Session::regenerateId()` 并发场景下旧会话删除与新 cookie 刷新的一致性；
5. `Controller::forward()` 深度上限（≤5）在超限时的错误路径回归；
6. `Router::match()` 与 `Router::dispatch()` 匹配结果等价性回归；
7. `test/DatabaseTest.php` SQLite 段断言 `lastInsertId()` / `rowCount()` / `quote()` 行为；
8. `gene.slow_query_ms=1` 执行慢 SQL 断言 `Monitor::stats()['db_slow_query_count']` 递增；阈值 `0` 时计数恒为 0；
9. `sendFile` 对 >memory_limit/4 的大文件在 FPM 下断言 RSS 平坦；`sendFile($f, PHP_INT_MAX)` 断言返回 `false`；`sendFile("php://stdin")` 断言返回 `false`；
10. Swoole 下请求 A `Application::stop()` 后请求 B 正常派发，且并发协程 C 不受 A 影响；
11. FPM 常驻 worker 循环 `Memory::set("k$i", $v, 1)` 十万次不读取，断言 RSS 不单调增长（抽样清扫应使增长有界）；
12. `View::render()` 在输出缓冲嵌套满场景断言返回 false 且不污染外层输出。
