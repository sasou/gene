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
| **F1-8** | ✅ 已实施（2026-08-07 二批补全） | `src/db/pdo.c`、四个驱动 | `lastInsertId()` / `rowCount()` 已以 `PHP_MALIAS` 注册为 `lastId()` / `affectedRows()` 的别名（mysql/pgsql/mssql/sqlite 四个驱动）；`quote()` 经新增 `gene_pdo_quote()` 透传底层 `PDO::quote`。详见 §十一。 |
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
| F1-8 补注册 `lastInsertId()` / `rowCount()` 别名 + `quote()` | ✅ 已落地（2026-08-07 二批） | 见 §十一。 |
| F1-7 慢查询计数 | ✅ 已落地（2026-08-07 二批） | `gene.slow_query_ms` 配置项 + `db_slow_query_count` 计数器，见 §十一。 |
| C2 / C3 / ML1 / ML2 / C4 / PF2~PF4 | ⏸ 观察项 | 维持 §七 第 5 步结论，需 profile/ZTS 证据后立项。 |
| 运行时验证（§9.3 六项 + PLAN.md O6/O7） | ⏸ 悬置 | Windows 无法编译/ASAN/压测；10.2-A 的修复需在 Linux `--enable-debug` + ASAN 下回归确认零告警。 |

---

## 十一、待办清单落地回写（2026-08-07 二批）

> 本节对应 §10.4 更新后的待办清单中两个可静态落地项的完成记录。仍为静态实施，运行时验证约束不变。

### 11.1 本批落地内容

| 条目 | 状态 | 落地点 | 说明 |
|---|---|---|---|
| **F1-8**（`lastInsertId()` / `rowCount()` / `quote()`） | ✅ 已实施 | `src/db/pdo.c` / `pdo.h`；`src/db/mysql.c` / `pgsql.c` / `mssql.c` / `sqlite.c` | 新增助手 `gene_pdo_quote()` 透传 `PDO::quote`；四个驱动各注册 `lastInsertId` / `rowCount` 为 `lastId` / `affectedRows` 的 `PHP_MALIAS` 别名，并新增 `quote(string $str, int $paramType = PDO::PARAM_STR): string\|false`（无连接时返回 `false`）。 |
| **F1-7b**（慢查询计数） | ✅ 已实施 | `src/gene.c`（INI + GINIT + globals 初始化）、`src/gene.h`（module globals）、`src/db/pdo.c`（埋点）、`src/tool/monitor.c`（导出） | 新增 INI `gene.slow_query_ms`（`PHP_INI_SYSTEM`，毫秒，默认 `0`=禁用）；在 `gene_pdo_exec` 与 `gene_pdo_statement_execute` 两个全驱动共用的执行入口埋点（`gene_hrtime` 纳秒计时，超阈值 `db_slow_query_count++`）；`Monitor::stats()` 新增导出 `db_slow_query_count` 与 `slow_query_ms`。 |
| **文档 / ide-helper / 测试同步** | ✅ 已实施 | `docs/CONFIGURATION.md`、`gene-ide-helper/Gene/Db/*.php`、`gene-ide-helper/Gene/Monitor.php`、`test/DatabaseTest.php` | CONFIGURATION.md 补 `gene.slow_query_ms` 及 §4.4 点名的 `closure_src_cache_max` / `swoole_auto_cleanup` / `cache_easy_ttl` 三个缺文档项；ide-helper 四驱动补 `lastInsertId/rowCount/quote` 声明、Monitor 补新导出键；DatabaseTest 的 SQLite 段（内存库，真实可运行）补三个新 API 调用。 |

### 11.2 设计要点与自查结论

- **别名语义**：`lastInsertId()` 是 `lastId()` 的别名，语义含「先执行构建器中待执行的 SQL 再取 ID」，与裸 PDO 的纯取值语义略有差异，已在 ide-helper 注明别名关系。这保留了构建器注入防护路径，是有意为之。
- **零开销门控**：`gene.slow_query_ms=0`（默认）时执行热路径仅多一次 long 比较，不取时钟；`slow_query_ms` 与既有 `cache_easy_ttl` 等同属「php.ini 填充、init_globals 禁止清零」类，`GINIT` 中补了默认值 `0`。
- **计数语义**：`db_slow_query_count` 为进程级累计（与其他遥测计数器同生命周期，仅在 `php_gene_init_globals` 清零）；埋点位于 `pdo.c` 公共助手而非四个驱动各自实现，天然覆盖 pool 借出的连接与断线重连路径；超时/异常 SQL 同样计数（执行慢即慢，无论成败）。
- **内存安全自查**：`gene_pdo_quote` 的 `params[0]` 为借用语义（`ZVAL_STR` 不 addref，`zend_call_known_function` 不消耗实参），无需 dtor；两个埋点块无新分配；四驱动 `quote()` 在 `pdo` 属性非对象时短路返回，不会解引用空指针。
- **可验证性**：本机（PHP 8.1 NTS + 已装 gene 扩展旧构建）对全部改动 PHP 文件执行 `php -l` 通过；C 改动无法在本机编译验证，承接 §9.3 清单。

### 11.3 新增运行时验证需求（并入 §9.3）

7. Linux 编译后运行 `test/DatabaseTest.php` SQLite 段，断言 `lastInsertId()` / `rowCount()` / `quote()` 行为（`quote("it's")` 应返回 `'it''s'`）。
8. 设置 `gene.slow_query_ms=1` 执行一条 `SELECT SLEEP(...)`（MySQL）或等价慢 SQL，断言 `Monitor::stats()['db_slow_query_count']` 递增；阈值 `0` 时断言计数恒为 0。

### 11.4 剩余待办

| 条目 | 状态 | 说明 |
|---|---|---|
| C2 / C3 / ML1 / ML2 / C4 / PF2~PF4 | ⏸ 观察项 | 维持 §七 第 5 步结论，需 profile/ZTS 证据后立项。 |
| `gene.pool_max_overflow` / `gene.fn_cache_max` | ⏸ 待立项 | PLAN.md 既有项，本轮未动。 |
| 运行时验证（§9.3 全部 + 11.3 新增 2 项 + PLAN.md O6/O7） | ⏸ 悬置 | Windows 环境约束不变。 |

---

## 十二、§4.1 模块完备度缺口剩余待办（2026-08-07 补录）

> 本节对照 §4.1「模块完备度总览」逐行复核源码（方法表 / `PHP_METHOD` 注册），列出三批落地后**仍未实现**的功能缺口。已落地项（F0-1~F0-3、F1-1~F1-8，见 §9.1 / §10.4 / §11.1）不再重复；观察项与 PLAN.md 既有项仍以 §11.4 为准。

### 12.1 未实现缺口清单

| 模块 | 未实现缺口 | §4.1 优先级 | 核实依据 |
|---|---|---|---|
| Application | `stop()` 优雅停机；`setResponse()` 无对应 getter | P2 | `application.c` 方法表无 `stop` / 无 response getter |
| Router | 中间件管道（F4，未实现；`match()` 已落地） | P1 | `router.c` 无中间件注册/执行链 |
| Controller | 生命周期钩子（F3 已 revert，需重新设计；`forward()` 已落地） | P1 | `controller.c` 无钩子派发 |
| View | `render()` 返回字符串；`clearAssign()` | P2 | `view.c` 方法表仅有 display/assign/contains 等 |
| Hook | 钩子优先级、`stopPropagation()` | P2 | `hook.c` 仅有 before/after/handle |
| Di | `alias()`（`instance()` 已落地） | P1 | `di.c` 方法表无 `alias` |
| Request | `isSecure()`（`isDelete()` 已落地） | P1 | `request.c` 方法表无 `isSecure` |
| Response | `getStatusCode()` / `isSent()` / `sendFile()` | P2 | `response.c` 方法表均无 |
| Session | `clear()` / `all()`（`regenerateId()` 已落地） | P0 行内余量 | `session.c` 方法表均无 |
| Cache(Memory) | `mget()` / `mset()`（`incr()/decr()` 已落地） | P1 | `memory.c` 方法表均无 |
| Cache(Redis/Memcached) | pipeline / multi（可经 `call()` 透传，非硬缺口） | P2 | 代理透传维持现状 |
| Db(SQLite) | `attach` | P2 | `sqlite.c` 无 `attach` |
| Db(Pool) | overflow 硬熔断（承接 `gene.pool_max_overflow`，§11.4 已列）；连接泄漏检测 | P1 | `pool.c` 仅有超时溢出建连 + 归还自动收缩，无硬熔断与泄漏检测 |
| Validate | `bail()` 首错即停；`sometimes()` | P2 | `validate.c` 方法表均无 |
| Log | `critical()` / `emergency()`；context 结构化字段 | P2 | `log.c` 方法表仅 debug~exception |
| Monitor | `reset()`；Prometheus 文本导出（等待队列/慢查询/命中率指标已落地） | P1 | `monitor.c` 仅有 `stats()` |
| Benchmark | `mark()` / `lap()` | P2 | `benchmark.c` 仅有 start/end/time/memory |

### 12.2 不立项项

| 模块 | 缺口 | 结论 |
|---|---|---|
| Model | ORM / ActiveRecord | 维持 §4.1 定级说明：属架构选择（P2），新架构立项而非补功能，不进入待办。 |

### 12.3 建议批次

1. **P1 批**：`Di::alias()`、`Request::isSecure()`、`Memory::mget()/mset()`、`Monitor::reset()` + Prometheus 导出 —— 均为小面改动，可静态实施；`Request::isSecure()` 建议与 §4.3 的 API 镜像约定同批。
2. **P1 设计批**：Router 中间件管道（F4）、Controller 生命周期钩子（F3 重设计）、Pool 连接泄漏检测 —— 需先出设计草案再动代码。
3. **P2 批**：View `render()/clearAssign()`、Response 三方法、Session `clear()/all()`、Validate `bail()/sometimes()`、Log 级别与 context、SQLite `attach`、Benchmark `mark()/lap()`、Application `stop()` —— 按需求驱动立项。

---

## 13. 补全批次实施记录（2026-08-07）

本批次完成 §12.1 表中 P1 小面批 + §12.3 第 3 项 P2 批的全部缺口，静态实施于 C 源码，同步更新 ide-helper、docs 与测试。

### 13.1 已实现方法清单

| 模块 | 方法 | 优先级 | 源文件 | 要点 |
|------|------|--------|--------|------|
| Di | `alias($alias, $target)` | P1 | `di.c` / `di.h` | 实例属性 `di_alias` 哈希表，`instance()` 先查别名再查注册表 |
| Request | `isSecure()` | P1 | `request.c` / `request.h` | Swoole 读 `server.https`/`server.server_port`；FPM 读 `HTTPS`/`SERVER_PORT` |
| Memory | `mget(array $keys)` / `mset(array $items, $ttl)` | P1 | `memory.c` / `memory.h` | 单次遍历哈希表，复用 get/set 逻辑 |
| Monitor | `reset()` / `exportPrometheus()` | P1 | `monitor.c` / `monitor.h` | reset 归零全部全局计数器；Prometheus 文本格式含 `# HELP`/`# TYPE` |
| View | `render($template, $vars)` / `clearAssign()` | P2 | `view.c` / `view.h` | render 返回字符串不输出；clearAssign 清空 `view_vars` |
| Response | `getStatusCode()` / `isSent()` / `sendFile($path, $filename, $headers)` | P2 | `response.c` / `response.h` | Swoole 读 `response_status` ctx 字段；sendFile 设头+输出文件 |
| Session | `clear()` / `all()` | P2 | `session.c` / `session.h` | clear 销毁 session 变量；all 返回 `$_SESSION` |
| Validate | `bail()` / `sometimes($field, callable)` | P2 | `validate.c` / `validate.h` | bail 标志使 validCheck 首错即停（含 group 模式）；sometimes 回调返回 false 跳过该字段全部规则 |
| Log | `critical()` / `alert()` / `emergency()` + 所有方法新增 `array $context` | P2 | `log.c` / `log.h` | 新增 LEVEL_CRITICAL(6)/ALERT(7)/EMERGENCY(8) 常量；context JSON 编码追加到日志行 |
| SQLite | `attach($path, $schema)` / `detach($schema)` | P2 | `sqlite.c` / `sqlite.h` | schema 标识符白名单校验防注入；path 单引号转义 |
| Benchmark | `mark($name)` / `lap($name)` | P2 | `benchmark.c` / `benchmark.h` | 基于 `gene_hrtime()` 纳秒时间戳；lap 返回 float 毫秒并重置 mark |
| Application | `stop()` / `isStopped()` | P2 | `application.c` / `gene.h` / `gene.c` / `router.c` | 全局 `app_stopped` 标志；router 在 before-hook/action 后检查，跳过后续派发；RSHUTDOWN 归零 |

### 13.2 基础设施变更

- **`gene.h`**：`gene_request_context` 新增 `di_alias`(zval)、`bench_marks`(zval)、`response_status`(zend_long) 字段；`gene_globals` 新增 `app_stopped`(zend_bool)。
- **`gene.c`**：ctx 初始化（`ZVAL_UNDEF`）、ctx reset 清理（`zval_ptr_dtor`）、RSHUTDOWN 归零 `app_stopped`。
- **`router.c`**：三处派发路径（PC_DIRECT/PC_CLOSURE 缓存路径、direct 路径、closure 路径）均插入 `GENE_G(app_stopped)` 检查点。

### 13.3 同步更新

- **ide-helper**（`gene-ide-helper/Gene/*.php`）：12 个文件新增对应方法签名与 docblock。
- **docs**（`docs/CONFIGURATION.md`）：新增"新增 API"章节，按 P1/P2 分表列出全部方法。
- **test**（`test/*.php`）：9 个测试文件新增对应测试方法并注册到 `runAllTests()`。

### 13.4 未立项项（维持 §12.2）

- Model ORM/ActiveRecord：架构选择，不进入补全批次。
- Router 中间件管道（F4）、Controller 生命周期钩子（F3 重设计）、Pool 连接泄漏检测：需设计草案，归入 §12.3 第 2 项"P1 设计批"。
- Cache(Redis/Memcached) pipeline/multi：经 `call()` 透传维持现状。

---

## 14. §13 批次落地复核与缺陷修复回写（2026-08-07 三批）

> 本节为 2026-08-07 三批：对 §13.1 全部新增 API 做第二轮独立复核（功能合理性 + 内存/并发安全），修复确认的问题。审查方式：三路并行逐方法静态审查 + 主线逐条原文核实。运行时验证约束不变（Windows 无法编译/ASAN）。

### 14.1 复核总体结论

- **未发现新的内存泄漏、重复释放或悬垂指针**。ctx 新字段（`di_alias` / `bench_marks` / `response_status`）的 init/reset/destroy 配对、`sendFile` 流关闭配对、router 检查点 goto 与 cleanup 标签关系、`benchmark` 惰性建表与 `lap` 指针时序、`monitor reset()` 对 gene.h 全部遥测计数器的覆盖，均核实通过。
- **发现 1 处确定性编译错误、1 处跨请求状态泄漏（功能级高危）、1 处 TTL 静默失效（历史既有缺陷被新 API 继承）**，均已修复（§14.2）。
- 功能合理性上确认 1 处需求/实现签名偏差：`sendFile` 实际签名为 `sendFile(string $file, int $offset = 0, int $length = 0)`，无 headers 参数；按现状收敛（headers 可经 `Response::header()` 先行设置），不视为缺陷。

### 14.2 本批修复清单

| # | 位置 | 问题 | 修复 |
|---|------|------|------|
| G1 | `src/http/response.c:801` | **编译错误**：`php_stream_open_wrapper` 使用不存在的 `REPORT_PATH`（全仓其余 5 处均为 `REPORT_ERRORS`） | 改为 `REPORT_ERRORS` |
| G2 | `src/gene.c` RSHUTDOWN（`php_gene_close_request_globals`） | **`app_stopped` 跨请求泄漏**：只在 MINIT 归零，FPM 下某请求调用 `stop()` 后同 worker 后续所有请求派发被永久跳过；且 `gene.h`（process lifetime）与 `application.c`/`router.c`（per-request）注释语义互相矛盾 | RSHUTDOWN 补 `GENE_G(app_stopped) = 0`，统一为 per-request 语义（与 router 检查点意图一致） |
| G3 | `src/router/router.c:1179` | direct 路径（`get_router_info_slow`）只在 action 之后检查 `app_stopped`，PC_DIRECT/closure 路径在 action 之前检查——同套路由开关 `route_precompile` 行为不一致 | dispatch 前补检查点（goto direct_cleanup，无资源跳过，已核实） |
| G4 | `src/http/response.c:810` | `sendFile` FPM 路径 `php_stream_copy_to_mem` 整文件读入单个 `zend_string`，大文件 OOM；与 Swoole 内核 sendfile 路径内存特征差异巨大 | 改为 8KB 分块 `php_stream_read` + `php_write` 流式输出，遵守 `length` 上限递减 |
| G5 | `src/cache/memory.c` / `src/gene.h` / `src/gene.c` | **TTL 静默失效**：`Memory::set/mset` 的 `validity` 参数传至 `gene_memory_set` 后被完全丢弃（`set()` 的既有缺陷，`mset()` 继承），文档却声称「TTL 0 表示永久」 | 新增进程级 `cache_expiry` 持久哈希表（key → 到期 unix ts）：set 时在 WRLOCK 内写入/清除；`gene_memory_get` / `gene_memory_exists` 读路径判过期（覆盖 get/mget/exists）；过期键在写未冻结时惰性 `del`；`del_core`/`clean()` 同步清理；GINIT 置 NULL、MSHUTDOWN 用 `zend_hash_destroy + pefree` 析构（键由表自身 pemalloc 复制，无需 manual key dance） |
| G6 | `src/http/validate.c` MINIT | `bail` / `sometimes` 属性未声明，成为动态属性，子类 `__set` 会拦截内部写入 | MINIT 补 `zend_declare_property_bool(bail, 0)` / `zend_declare_property_null(sometimes)` |
| G7 | `src/db/sqlite.c` `attach()`/`detach()` | 未拒绝 path 内 NUL 字节（SQL 以 C 串传给 `gene_pdo_exec`，NUL 会静默截断路径）；成功判定 `Z_TYPE != IS_UNDEF` 过宽——`PDO::exec` 失败返回 false 也被判为成功 | 补 `memchr` NUL 校验；成功判定排除 `IS_FALSE`（两处同修） |
| G8 | `src/tool/log.c` `gene_log_write_message` | `zval json_ret` 未初始化即传给 `gene_json_encode` 后判 `Z_TYPE`，若调用方未填充则为 UB | 补 `ZVAL_UNDEF(&json_ret)` 防御初始化 |
| G9 | `src/http/response.c` `isSent()` | Swoole `isWritable()` 返回非 bool 真值（如 int 1）时被误读为「未发送」 | 改用 `zend_is_true(&retval)` |
| G10 | `src/http/response.c` redirect | Swoole redirect 调用抛异常/返回 false 时仍记录 `response_status` | 按 `EG(exception)` / `IS_FALSE` 门控后再记录 |
| G11 | `src/mvc/view.c` `render()` | `php_output_start_default()` 返回值未检查，失败时模板输出污染外层输出缓冲/客户端 | 检查返回值，失败时按正常路径同款 `zend_hash_destroy + FREE_HASHTABLE` 清理符号表后 `RETURN_FALSE` |
| G12 | `src/di/di.c` `instance()` | 不像 `get()/has()` 那样解析别名，别名注册后显式实例化行为不一致 | 入口处经 `gene_di_resolve_alias()` 解析（借用指针，无所有权变更） |
| G13 | `src/gene.c:544-553` debug 块 | pool acquire 的 `#ifndef NDEBUG` 不变式文档块缺 `di_alias` / `bench_marks` 的 `ZVAL_UNDEF`（无实际 bug，destroy 已保证） | 补齐两行，维持块声称的不变式完整 |

### 14.3 本批驳回 / 不改项

| 候选 | 结论 |
|------|------|
| `sometimes` 守卫 fail-open（回调失败时继续校验该字段） | **不改**。回调失败的回退方向是「继续校验」，即更严格而非更宽松，属安全方向；语义已在方法注释中声明 |
| `gene_json_encode` 缺 fn NULL 守卫 | **不改**。`GENE_CG_FN_LOOKUP` 对 `json_encode` 的解析是全仓既有约定（common.c 全部 helper 同型），仅改 log.c 会造成不对称；记录为约定项 |
| `sendFile` 缺 headers 参数 | **收敛为文档偏差**。实际签名 `(file, offset, length)` 自洽；响应头可经 `Response::header()` 先设 |
| `stop()` 非 static 方法被静态调用的崩溃面 | **不改**。Zend 对静态调用实例方法本身有 deprecated 保护，且 `getThis()==NULL` 场景在全仓实例方法中普遍存在，不单独立项 |

### 14.4 新增运行时验证需求（并入 §9.3）

9. Linux `--enable-debug` 编译需确认 G1 修复后零告警（本批唯一编译错误项）。
10. FPM 下同一 worker 连续两请求：请求 A 调 `Application::stop()`，请求 B 断言路由正常派发（验证 G2）。
11. `Memory::set('k','v',1)` 后 `sleep(2)`，断言 `get/exists/mget` 返回 miss 且 `Monitor` 命中率计数正确；`set('k','v',0)` 覆盖 TTLed 键后断言永久生效（验证 G5）。
12. `sendFile` 对 >memory_limit/4 的大文件在 FPM 下断言 RSS 平坦（验证 G4）。
13. `View::render()` 在输出缓冲嵌套满（`output_buffering` 极限）场景断言返回 false 且不污染外层输出（验证 G11）。

### 14.5 剩余待办（合并 §11.4 / §13.4）

| 条目 | 状态 | 说明 |
|---|---|---|
| C2 / C3 / ML1 / ML2 / C4 / PF2~PF4 | ⏸ 观察项 | 维持 §七 第 5 步结论 |
| `gene.pool_max_overflow` / `gene.fn_cache_max` | ⏸ 待立项 | PLAN.md 既有项 |
| Router 中间件管道（F4）、Controller 生命周期钩子（F3 重设计）、Pool 连接泄漏检测 | ⏸ 设计批 | §12.3 第 2 项，需先出设计草案 |
| 运行时验证（§9.3 全部 + 11.3 两项 + 14.4 五项 + PLAN.md O6/O7） | ⏸ 悬置 | Windows 环境约束不变；G1~G13 全部修复需 Linux `--enable-debug` + ASAN 回归 |

---

## 15. 落地功能合理性与内存泄漏独立复核（2026-08-07 四批）

> 本节为对 §13（`0c182a3`）与 §14（`d1e4265`）两个提交**全部落地代码**的第四轮独立静态复核，审查目标为两条：**落地功能是否合理**、**是否存在内存泄漏或内存安全问题**。审查方式：逐 diff 原文核实 + 完整文件上下文交叉验证。**本节仅出结论，未修改任何源码。** 运行时验证约束不变（Windows 无法编译 / ASAN / 压测）。

### 15.1 总体结论

1. **§14 的 G1~G13 修复全部核实属实**，方向正确。特别是 G4（`sendFile` 分块化）、G5（TTL 落地）、G2（`app_stopped` 复位）三处均切中真实缺陷。
2. **但 G4 与 G2 两处修复本身各自引入/遗留了一个更高severity的问题**（见 N1、N2），属于「修复不完整」而非「修复错误」：G4 换成分块循环时引入了有符号/无符号返回值缺陷；G2 只覆盖了 FPM，Swoole 模式下复位点根本不会触发。
3. **G5（TTL）在功能语义上成立，但两种运行模式下的实际行为与文档承诺存在偏差**，且在 FPM 常驻场景下引入了一条新的无界增长路径（N3）。这是本轮唯一的内存增长类发现。
4. **未发现新的确定性内存泄漏、重复释放或悬垂指针。** 具体已核实通过的配对见 15.4。
5. 本轮**驳回 6 项候选发现**（见 15.5），均经原文复核后确认不成立。

### 15.2 新发现问题清单

| # | 位置 | 严重级别 | 问题 |
|---|------|----------|------|
| **N1** | `src/http/response.c:827` | **高危（内存安全）** | `php_stream_read()` 在 PHP 8 中返回 **`ssize_t`**，此处赋给 `size_t got`。读失败时底层返回 `-1`，转换后 `got == SIZE_MAX`，`if (got == 0)` 无法拦截，随即执行 `php_write(buf, SIZE_MAX)` —— 对 8KB 栈缓冲的巨量越界读，直接导致段错误或**栈内存内容泄露到 HTTP 响应体**。G4 把整读改分块时引入。 |
| **N2** | `src/gene.c:1134`、`src/app/application.c:1443`、`src/gene.h:385` | **高危（功能）** | `app_stopped` 的复位点在 `php_gene_close_request_globals()`（RSHUTDOWN）。但 `gene.c:1156-1157` 的注释自己写明「**In Swoole mode RSHUTDOWN fires once at worker exit**」—— 即 Swoole 下该复位每 worker 只发生一次。后果：**Swoole 部署中任一请求调用 `Application::stop()` 后，该 worker 的后续所有请求都会被 router 的 8 个检查点永久跳过派发**，表现为服务静默失效。G2 实际只修好了 FPM 路径，`application.c:1443` 的「per-request and reset in RSHUTDOWN」在 Swoole 下不成立。<br>叠加问题：`app_stopped` 是 module global（`gene.h:385`）而非 `gene_request_context` 字段，与同批新增的 `di_alias` / `bench_marks`（均正确放入 ctx 以隔离协程）**取舍不一致**；即便复位问题解决，同 worker 内并发协程之间仍会互相串扰。 |
| **N3** | `src/cache/memory.c:575-577`、`620-636` | **中危（内存增长）** | FPM 下 `GENE_G(cache)` 是 pemalloc 的进程级表，跨请求存活。G5 的 TTL 只有**惰性回收**（`memory.c:628` 仅在该键被再次读取时才 `del`），而 `memory.c:575-577` 已明确 userland `Memory::set` **不参与 `cache_max_items` 的 LRU 可淘汰分区**。两者叠加：`Memory::set("rate:$ip", $v, 60)` 这类轮转键一旦写入后不再读，value 与 expiry 条目**永不释放**，进程级持久堆随请求量单调增长。TTL 落地前该 API 无实际用途所以不暴露，落地后成为可用功能，增长面随之打开。 |
| **N4** | `src/cache/memory.c:620-636` | **中危（性能）** | `gene_memory_get()` 是路由 / 配置 / DI 查找的核心热路径（其 610-618 行注释已自述该定位）。G5 使每次调用都无条件先做一次 `zend_hash_str_find(cache_expiry, ...)`，即**热路径哈希查找次数翻倍**。绝大多数部署不使用 TTL，此开销纯属浪费。建议加 `zend_hash_num_elements(GENE_G(cache_expiry)) == 0` 的短路判断（一次整型比较）。按 PLAN.md 准入约束，此项需 profile 证据后再动。 |
| **N5** | `src/cache/memory.c:628` | **低危（功能语义）** | Swoole 模式下 TTL 实质退化为「读掩蔽」而非「过期回收」：写入必须在 workerReady 冻结前完成（`gene_memory_write_allowed` 门控），而 628 行在冻结后显式跳过删除。因此过期条目的内存**永久占用**，只是读路径报 miss。这个取舍本身正确（冻结后不能写），但与 `docs/CONFIGURATION.md` 中「TTL 0 表示永久」的表述并列时，用户会误以为非 0 值可回收内存。应在文档中明写两种模式的差异。 |
| **N6** | `src/di/di.c` `gene_di_resolve_alias()` | 低危（观察项） | 返回的是 `Z_STR_P(target)` —— 指向 `ctx->di_alias` 哈希表内部 zval 的**借用指针**。若解析之后、使用 `name` 之前发生对该表的写入（例如 `gene_di_get()` 惰性加载的类，其构造函数中再次调用 `Di::alias()`），rehash 会使指针悬垂。当前调用链未见此路径，故记为观察项而非缺陷；若后续要加固，在 `gene_di_get()` 内对解析结果做 `zend_string_copy` 并在出口 release 即可。 |
| **N7** | `src/di/di.c` `gene_di_resolve_alias()` 注释 | 低危（注释不实） | 注释声称环形别名「degrades to a registry miss」。实际 8 跳上限只是**停止**遍历：`a=>b, b=>a` 在偶数跳后落回 `a`，是一个合法名字，会正常命中注册表而非 miss。行为无害，但注释描述与实现不符，应改为「解析到环上的某个名字后停止」。 |
| **N8** | `src/http/response.c:815` | 低危（功能） | `php_stream_seek()` 对普通文件允许 seek 到 EOF 之后并返回 0。因此 `sendFile($f, 999999999)` 会通过校验，随后循环立即 EOF，函数**返回 `true` 但响应体为空**——调用方无法区分「成功发送空范围」与「offset 越界」。建议 seek 后比对 `php_stream_tell()` 或文件大小。 |
| **N9** | `src/http/response.c:811` | 低危（安全，观察项） | `php_stream_open_wrapper()` 接受全部已注册 wrapper（`http://`、`php://`、`data://` 等）。若 `$file` 直接来自用户输入，`Response::sendFile()` 即成为 SSRF 与任意流读取入口。虽然「不要把用户输入当文件路径」是调用方责任，但作为框架的响应输出 API，建议改用 `php_stream_open_wrapper_ex()` 并限定 plain files，或至少在 ide-helper / 文档中明确警示。 |
| **N10** | `src/tool/benchmark.c` `mark()` / `lap()` | 低危 | `ZVAL_LONG(&ts, (zend_long)gene_hrtime())`：`gene_hrtime()` 为 uint64 纳秒，32 位平台上 `zend_long` 仅 32 位，约 **4.3 秒即溢出**，`lap()` 将返回负值或错误的巨大值。64 位平台无此问题。建议注明平台约束，或改存 double 毫秒。 |
| **N11** | `src/router/router.c:1173-1184` | 低危（可读性） | G3 插入检查点时把 `if (hook_src) { ... }` 整块的缩进降了一级，与同函数其余代码（`1162-1171`、`1186-1196`）的缩进层级不一致。无功能影响，建议随下次改动一并回正。 |

### 15.3 修复建议（按优先级）

1. **N1（必须优先）**：`got` 改为 `ssize_t`，判据改为 `if (got <= 0) break;`。这是本轮唯一的内存安全缺陷，且触发条件（磁盘 I/O 错误、NFS 中断）在生产中真实可达。
2. **N2**：将 `app_stopped` 从 `gene_globals` 迁入 `gene_request_context`，与 `di_alias` / `bench_marks` 同批字段对齐，天然获得协程隔离与 ctx 复用时的复位；`Application::stop()` / `isStopped()` 改读 ctx。若暂不迁移，则至少在 Swoole 请求入口（`Application::run()` 起点或 ctx acquire）补一次归零，并修正 `application.c:1443` 的注释。
3. **N3**：为 `cache_expiry` 增加主动清扫（如每 N 次 `gene_memory_set` 抽样扫描一批过期键并 `del`），或让带 TTL 的 userland 写入进入 `cache_max_items` 的 LRU 分区。二者取其一即可封闭增长面。
4. **N5 + N4**：先补文档（零成本），性能短路待 profile 证据。
5. **N6~N11**：随手改项，无需单独立项。

### 15.4 已核实通过的配对（无问题）

- `cache_expiry` 的 `PALLOC_HASHTABLE` / `zend_hash_destroy` + `pefree` 配对：GINIT 置 NULL（`gene.c:1065`）、`gene_memory_init()` 建表（`memory.c:177-180`）、MSHUTDOWN 析构（`gene.c:1259-1263`）、`clean()` 析构后经 `memory.c:1379` 的 `gene_memory_init()` 重建 —— 四处闭合，键由表自身 pemalloc 复制，无需手工 key dance，注释所述属实。
- `gene_memory_del_core`（`memory.c:442-444`）与 LRU 淘汰（`memory.c:523`）均同步清理 expiry 条目，无孤儿。
- `gene_memory_get` 过期分支的锁序：先 `GENE_CACHE_RDUNLOCK()` 再调用取 WRLOCK 的 `gene_memory_del()`，无递归加锁，无死锁。
- `router.c` `direct_cleanup` 标签：`dispatch_result` 在 1165 行已 `ZVAL_NULL`，`hookname_alloc` 在 1195 行统一 `efree`，三个新增 goto 均不跳过任何已分配资源。
- `response.c` `sendFile` 的 `php_stream_close()`：`!stream`（1 处 return 前无需关闭）、seek 失败（816 行已关闭）、正常结束（837 行）—— 全部 return 路径配对完整。
- `response.c` `sendFile` Swoole 分支：`zfile` 的 `ZVAL_STR_COPY` / `zval_ptr_dtor` 配对，`retval` 在 `IS_FALSE` 与非 `UNDEF` 两条路径均 dtor，无泄漏。
- `benchmark.c` `lap()` 的时序：`prev_ns` 在 `add_assoc_zval_ex` 触发可能的 rehash **之前**已按值取出，不存在旧指针解引用。
- `di.c` `gene_di_aliases()`：`IS_NULL` 分支先 `zval_ptr_dtor` 再 `array_init_size`，`IS_UNDEF` 直接 init，无重复释放。
- `view.c` `render()` 的 `php_output_start_default()` 失败分支：`table` 经 `zend_hash_destroy` + `FREE_HASHTABLE` 清理后才 `RETURN_FALSE`，与正常路径同款。
- `log.c` `gene_log_write_message` 的 `ZVAL_UNDEF(&json_ret)` 防御初始化到位。
- `sqlite.c` `attach()` / `detach()` 的 `memchr` NUL 校验与 `IS_FALSE` 成功判定收紧，两处对称。
- `gene.c:550-553` ctx 池 acquire 路径的 `di_alias` / `bench_marks` `ZVAL_UNDEF` 已补齐，与 destroy 侧一致。

### 15.5 本轮驳回的候选发现

| 候选 | 驳回理由 |
|------|----------|
| `Memory::clean()` 将 `cache_expiry` 置 NULL 后 TTL 永久失效 | **不成立**。`memory.c:1373` 置 NULL 后，紧接着 `memory.c:1379` 调用 `gene_memory_init()` 重建该表，与 `cache` 主表的处理完全对称。 |
| `gene_memory_set` 先写 expiry 再写值，插入失败会留下孤儿 expiry 条目 | **不成立**。原文复核 `memory.c:585-605`：585 行之后的两条分支（`copyval == NULL` 新建 / 非 NULL 编辑）都必然完成写入，中间无失败返回路径，不产生孤儿。 |
| LRU 淘汰绕过 expiry 清理，导致 `cache_expiry` 无界增长 | **不成立**。`gene_cache_lru_evict_nolock`（`memory.c:523`）经 `gene_memory_del_core` 淘汰，后者 442 行已含 expiry 删除。 |
| `cache_expiry` 用 `zend_hash_str_*` 而主表用 `zend_symtable_str_*`，数字串键（如 `"123"`）命名空间错配 | **不构成缺陷**。expiry 表的 set（`zend_hash_str_update`）/ find（`zend_hash_str_find`）/ del（`zend_hash_str_del`）三处**一致**使用字符串键，表内自洽；主表是否把 `"123"` 折叠为整型索引不影响 expiry 的查得率。 |
| `router.c` 新增的 `goto direct_cleanup` 跳过资源释放 | **不成立**。见 15.4，`dispatch_result` 与 `hookname_alloc` 均由 cleanup 标签统一处理。 |
| `gene_memory_get` 在读锁内触发写操作（惰性 del）导致死锁或数据竞争 | **不成立**。`memory.c:624` 已先 `GENE_CACHE_RDUNLOCK()` 才调用 `gene_memory_del()`，且 628 行的门控确保仅在写仍被允许的阶段（FPM / 冻结前）执行。 |

### 15.6 新增运行时验证需求（并入 §9.3）

14. N1 修复后，在 FPM 下对一个读取过程中被截断的文件（或 `EIO` 注入）调用 `sendFile()`，断言进程不崩溃且响应体不含栈残留数据。
15. Swoole 多请求场景：请求 A 调 `Application::stop()`，请求 B（同 worker）断言路由**正常派发**（验证 N2）；并发协程场景下断言 A 的 `stop()` 不影响并发中的 C。
16. FPM 常驻 worker 下循环 `Memory::set("k$i", $v, 1)` 十万次且不读取，断言进程 RSS 不单调增长（验证 N3）。
17. 32 位构建下 `Benchmark::mark()` + `sleep(5)` + `lap()`，断言返回值为正且约 5000ms（验证 N10）。

### 15.7 更新后的剩余待办（合并 §14.5）

| 条目 | 状态 | 说明 |
|---|---|---|
| **N1**（`sendFile` `ssize_t`） | 🔴 待修 | 内存安全，最高优先级 |
| **N2**（`app_stopped` 迁入 ctx） | 🔴 待修 | Swoole 下功能静默失效 |
| **N3**（TTL 键主动回收） | 🟡 待修 | FPM 无界增长 |
| N4 / N5 / N6~N11 | 🟡 待修 | N4 需 profile 证据；其余为随手改项与文档项 |
| C2 / C3 / ML1 / ML2 / C4 / PF2~PF4 | ⏸ 观察项 | 维持 §七 第 5 步结论 |
| `gene.pool_max_overflow` / `gene.fn_cache_max` | ⏸ 待立项 | PLAN.md 既有项 |
| Router 中间件管道（F4）、Controller 生命周期钩子（F3 重设计）、Pool 连接泄漏检测 | ⏸ 设计批 | §12.3 第 2 项 |
| 运行时验证（§9.3 全部 + 11.3 两项 + 14.4 五项 + 15.6 四项 + PLAN.md O6/O7） | ⏸ 悬置 | Windows 环境约束不变 |
