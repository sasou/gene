# Gene 扩展框架三向审计报告（内存泄漏 / 并发性能 / 功能完善）

> 审计版本：5.6.9（HEAD `f198821`）
> 审计日期：2026-08-06
> 审计方式：纯静态代码审查 + 与 `AUDIT_REPORT_2026_07_30.md`、`audit/plan/PLAN.md` 交叉复核；**本轮仅出报告，未修改任何源码**
> 审计范围：`src/` 全模块（约 1.1MB C 代码，24 个子模块），另抽查 `test/`、`docs/`、`gene-ide-helper/`
> 环境约束：本机为 Windows，**无法执行编译、ASAN/Valgrind、压测与长跑**；所有结论标注「静态确认」或「需运行时验证」

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

`src/gene.c:141-158` 共 16 项 `gene.*` INI 指令。

| 关注项 | 当前默认 | 评估 |
|--------|----------|------|
| `gene.cache_max_items` | `0`（无界） | ⚠️ 危险默认。已有启动 NOTICE，但生产建议在 `docs/CONFIGURATION.md` 中给出明确推荐值（如 10000）并解释内存换算方式 |
| `gene.closure_src_cache_max` | `1024` | ⚠️ **缺文档**。应补入 `docs/CONFIGURATION.md` |
| `gene.co_contexts_max` | `1024` | 高并发下偏小；M1 cooldown 落地后已不再引发 O(N²)，但仍建议文档注明生产参考值 |
| `gene.ctx_pool_max` | `256` | 同上 |

建议新增：`gene.slow_query_ms`（慢查询阈值，配合 F1-7）、`gene.pool_max_overflow`（连接池硬熔断，PLAN.md 既有项）、`gene.fn_cache_max`（fn_cache LRU 容量治理，PLAN.md 既有项）。

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

## 九、实施状态回写（2026-08-06 落地）

> 本节由实施方在完成 §七 建议顺序的 1~4 步后回写。所有改动均为静态实施，**运行时验证（编译/ASAN/压测）仍因 Windows 环境约束而悬置**，待 Linux 环境补齐后承接 PLAN.md O6/O7。

### 9.1 已落地条目

| 条目 | 状态 | 落地点 | 说明 |
|---|---|---|---|
| **C1** | ✅ 已实施 | `src/db/pool.c` `pool_decrement_count` / `put`；`src/gene.h`、`src/gene.c`、`src/tool/monitor.c` | 将 get→sub 的 TOCTOU 序列改为 CAS 循环，对齐 `redis_pool.c`；新增 `db_pool_cas_abandoned` 全局计数并导出至 Monitor。保留无 `cmpset` 时的 get→sub 回退路径。 |
| **PF1** | ✅ 已实施（与 C1 同批） | `src/db/pool.c` `put()` | 合并 `pool_get_count` / `pool_get_max` / `pool_decrement_count` 的重复跨界读，`pool_get_max` 提到循环外。 |
| **F0-1** | ✅ 已实施 | `src/session/session.c` `regenerateId()` | 生成新 ID、按需删除旧会话、刷新内部 ID 与 cookie、标记 dirty。 |
| **F0-2** | ✅ 已实施 | `src/db/mysql.c`、`src/db/pgsql.c` | 新增 `join()` / `leftJoin()` / `rightJoin()` / `union()` / `reset()`，含标识符引用、ON 条件拼接、UNION 子查询参数合并、`reset()` 复位构建器状态。PgSQL 镜像 MySQL 实现。 |
| **F0-3** | ✅ 已实施 | `src/db/mssql.c`、`src/db/mssql.h` | 补全 `where` / `in` / `group` / `order` / `limit`（OFFSET/FETCH 语法）/ `join` / `leftJoin` / `rightJoin` / `union` / `reset`，能力对齐 MySQL。 |
| **F1-1** | ✅ 已实施 | `src/http/request.c` | 注册 `Request::isDelete()`。 |
| **F1-2** | ✅ 已实施 | `src/cache/memory.c` | `incr($key, $step=1)` / `decr($key, $step=1)`，在 `GENE_CACHE_WRLOCK` 内完成读-改-写。 |
| **F1-3** | ✅ 已实施 | `src/router/router.c` `match()` | 纯匹配 API `match($method, $uri): array\|false`，复用 dispatch 匹配逻辑但不执行 handler；已注册 arginfo 与方法表。 |
| **F1-4** | ✅ 已实施 | `src/db/pool.c` `healthCheck()` | `pool_health_check_channel` 对 idle 连接轻量探活，返回存活数。 |
| **F1-5** | ✅ 已实施 | `src/di/di.c` `instance()` | `instance($class, $params=[])` 走 `gene_class_instance` 但不入容器。 |
| **F1-6** | ✅ 已实施 | `src/mvc/controller.c` `forward()` | `forward($controller, $action, $params=[])`，带转发深度上限（≤5）防无限递归。 |
| **F1-7** | ✅ 已实施 | `src/tool/monitor.c` | 增补 `db_pool_cas_abandoned`（配合 C1）、`db_pool_get_timeout`（pool 获取超时次数）、`memory_cache_hit` / `memory_cache_miss`（用户态 Memory 命中率）。慢查询计数未纳入本轮（依赖 `gene.slow_query_ms` 配置项，留待后续）。 |
| **F1-8** | ⚠️ 部分实施（2026-08-07 复核更正） | `src/db/pdo.c` | 底层助手 `gene_pdo_last_insert_id` / `gene_pdo_statement_row_count` 存在于 `pdo.c` 并被四个驱动的 `lastId()` / `affectedRows()` 调用，但报告中声称的 `lastInsertId()` / `rowCount()` 方法**并未注册**。等价能力已由既有 `lastId()` / `affectedRows()` 覆盖，`quote()` 仍缺失。详见 §十。 |
| **测试基础设施** | ✅ 已实施 | `test/DiTest.php`、`test/HookTest.php`、`test/TestRunner.php` | 新增 Di / Hook 回归测试并纳入 TestRunner；已在本机 `php -l` 与运行验证通过。 |
| **ide-helper 同步** | ✅ 已实施 | `gene-ide-helper/Gene/*.php` | Session / Db\Mysql / Db\Pgsql / Db\Sqlite / Db\Mssql / Request / Memory / Pool / Di / Router / Controller / Monitor / Application 均已同步新增 API 与版本注解。 |

### 9.2 仍未落地 / 留待后续

| 条目 | 状态 | 原因 |
|---|---|---|
| F1-7 慢查询计数 | ⏸ 暂缓 | 依赖 `gene.slow_query_ms` 配置项与慢查询埋点，改动面较大，留待后续立项。memory 命中率（hit/miss）已在本轮落地。 |
| C2 / C3 / ML1 / ML2 / PF2~PF4 | ⏸ 观察项 | 按 §七 第 5 步，等 profile 或 ZTS 需求出现后再评估，不主动改动。 |
| 运行时验证（O6/O7） | ⏸ 悬置 | Windows 环境无法编译/ASAN/压测，待 Linux 环境补齐。 |

### 9.3 后续验证清单（承接 PLAN.md O6/O7）

1. Linux `phpize + make`（含 `--enable-debug` / ASAN）零告警编译。
2. C1 修复后多 worker 并发借还压测下 pool count 一致性断言。
3. MySQL / PgSQL / MSSQL `join` / `union` / `where` / `in` / `group` / `order` / `limit` 全量 SQL 回归（含标识符引用与参数绑定）。
4. `Session::regenerateId()` 并发场景下旧会话删除与新 cookie 刷新的一致性。
5. `Controller::forward()` 深度上限在超限时的错误路径回归。
6. `Router::match()` 与 `Router::dispatch()` 匹配结果等价性回归。

---

## 十、完成情况复核与缺陷修复回写（2026-08-07）

> 本节为 2026-08-07 的落地复核：对 §9.1 全部「已实施」条目逐条源码核实，并对 08-06 批次新增代码做第二轮内存/并发审查，修复新发现的缺陷。仍为静态实施，运行时验证约束不变。

### 10.1 §9.1 条目核实结果

| 条目 | 核实结果 |
|---|---|
| C1（db/pool.c CAS 对称化 + `db_pool_cas_abandoned` 遥测） | ✅ 属实。`pool_atomic_cmpset` / `pool_count_cas_fns` / `pool_decrement_count_cas`（`src/db/pool.c:476-547`）与 redis_pool.c 修法对称；64 轮上限 + once 告警 + 计数导出（`monitor.c:167`）、RINIT 重置（`gene.c:1017-1018`）均到位；无 `cmpset` 时的 get→sub 回退保留。 |
| PF1（put() 跨界调用合并） | ✅ 属实。`put()`（`pool.c:899-924`）单次 `Atomic::get` 复用于溢出判据与 CAS 递减，`pool_get_max` 为普通属性读，无原子语义误用。 |
| F0-1（`Session::regenerateId()`） | ✅ 属实。`session.c:1175-1225`：`old_id` 的 `zend_string_copy`/`release` 配对、`hash_val` dtor、`RETURN_STR_COPY` 均正确。 |
| F0-2 / F0-3（MySQL/PgSQL/MSSQL 构建器） | ✅ 属实，但本轮在错误路径上新发现 4 处泄漏，已修复（见 10.2-A）。 |
| F1-1 / F1-2 / F1-3 / F1-5 / F1-6 | ✅ 属实。`Request::isDelete` 已注册；`Memory::incr/decr` 在 `GENE_CACHE_WRLOCK` 内完成读-改-写；`Router::match` 各分支（method/rkey/path）释放配对正确；`Di::instance` 与 `Controller::forward`（深度 ≤5，RSHUTDOWN 复位）refcount 配对正确。 |
| F1-4（`Pool::healthCheck()`） | ✅ 属实。经复核 `pool.c:1048-1090` **无泄漏**：`conn_zv` 指向 `item` 数组内部元素，`zval_ptr_dtor(&item)` 会释放连接引用（PDO 析构即关闭连接）；push 回通道经 `pool_channel_push` 内部重新打包 `[conn, lastUsed]` 并 addref，所有权平衡。审查中「push 失败泄漏连接」的候选结论**驳回**（见 10.3）。 |
| F1-7（Monitor 指标） | ✅ 属实。`db_pool_cas_abandoned` 等已导出。慢查询计数维持暂缓。 |
| F1-8（PDO `lastInsertId()` / `rowCount()`） | ⚠️ **原状态不实，已更正**（§9.1 行已改写）。`lastInsertId()` / `rowCount()` 未注册为方法；既有 `lastId()` / `affectedRows()` 覆盖同等语义。`test/DatabaseTest.php:99` 原调用不存在的 `lastInsertId()`（会抛 `Error` 且不被 `catch (Exception)` 捕获），已改为 `lastId()`。 |

### 10.2 本轮新修复的缺陷

| # | 位置 | 问题 | 修复 |
|---|------|------|------|
| A | `src/db/mysql.c` / `pgsql.c` / `mssql.c` / `sqlite.c` 的 `gene_db_*_do_join`（08-06 批次新增） | 错误返回路径（非法 JOIN type、`build_on` 失败）未释放 `smart_str frag` / `on_str`；`build_on` 部分写入后失败时 `on_str` 为实际泄漏 | 4 个文件的 2 条错误路径均补 `smart_str_free`（对空 smart_str 安全），与正常路径对称 |
| B | `test/DatabaseTest.php:99` | 调用未注册的 `lastInsertId()` | 改为已存在的 `lastId()`（`php -l` 通过） |

### 10.3 本轮驳回的候选发现

| 候选 | 驳回理由 |
|------|----------|
| `Pool::healthCheck()` push 失败泄漏连接对象 | **不成立**。`conn_zv` 是 `item` 数组内部元素的指针，`zval_ptr_dtor(&item)` 已释放其唯一引用；push 失败时 PDO 随析构关闭，无需显式 close。 |
| `mssql.c` `where()` / `in()` 的 E_ERROR 路径泄漏 `smart_str` / `estrndup` | **不修**。`E_ERROR` 触发 `zend_bailout` 终止请求，泄漏无运行时意义；且该形态是 `mysql.c:680/686/783/847` 等处既有全仓约定，仅改 mssql 会造成不对称。记录为约定项，若未来统一治理再一并处理。 |

### 10.4 更新后的待办清单

| 条目 | 状态 | 说明 |
|---|---|---|
| F1-8 补注册 `lastInsertId()` / `rowCount()` 别名或文档注明等价 API | ⏸ 待决策 | 等价能力已存在（`lastId()` / `affectedRows()`）；若追求与 PDO 命名对齐可加别名方法，属兼容性新增，非缺陷。`quote()` 仍缺失，可与此同批。 |
| F1-7 慢查询计数 | ⏸ 暂缓 | 依赖 `gene.slow_query_ms` 配置项立项。 |
| C2 / C3 / ML1 / ML2 / C4 / PF2~PF4 | ⏸ 观察项 | 维持 §七 第 5 步结论，需 profile/ZTS 证据后立项。 |
| 运行时验证（§9.3 六项 + PLAN.md O6/O7） | ⏸ 悬置 | Windows 无法编译/ASAN/压测；10.2-A 的修复需在 Linux `--enable-debug` + ASAN 下回归确认零告警。 |
