# Gene 未实现功能与待验证项计划

> 本文件整理自 audit 历史报告，**仅存放尚未落地或待验证的待办项**。已完成的条目不再保留，避免与审计报告的「落地情况」节重复。
> 来源审计报告：`AUDIT_REPORT_2026_07_30.md`、`AUDIT_REPORT_2026_08_06.md`（及更早的 05-25 / 06-20 / 07-03 / 07-12 / 07-13 报告）。
> 维护约定见文末 §九。

---

## 一、已 revert / 未实现的功能（需设计后立项）

### F3 `Gene\Controller::init()` 生命周期钩子（重设计）

- **来源**：`AUDIT_REPORT_2026_07_30.md` §6；`AUDIT_REPORT_2026_08_06.md` §4.1
- **现状**：在 v5.6.9 中已回退（commit `fc10274`）。`Gene\Controller` 不再提供 `init()` 方法，控制器继续使用 `__construct` / `beforeAction`。
- **回退原因**：该钩子带来的复杂度超过收益，且与现有生命周期冲突。
- **待决策**：若后续确有 Yaf 语义需求，可重新设计为不破坏 `__construct` 约定的可选钩子。属「P1 设计批」，需先出设计草案再动代码。

### F4 路由级中间件管道

- **来源**：`AUDIT_REPORT_2026_07_30.md` §6；`AUDIT_REPORT_2026_08_06.md` §4.1
- **现状**：仅停留在建议阶段，未立项实现。`router.c` 无中间件注册/执行链。
- **方案**：路由配置支持 `route.middleware = "Auth,RateLimit"`（或 `$router->middleware()`），派发前按序执行各 `Gene\Hook` 子类 `handle()`，任一返回 false 即中断并走 error 路径。复用现有 `gene_router_exec_hook_direct` 直派机制。
- **约束**：**不应在 O6 运行时验证全集打通前立项** —— 中等规模的派发链改造，在缺回归证据时改派发路径风险收益比不成立。属「P1 设计批」。

---

## 二、模块完备度缺口

> 基线：08-06 §4.1 模块完备度总览逐行复核源码后剩余的未实现缺口。

### 2.1 P1 设计批（需先出设计草案再动代码）

| 模块 | 缺口 | 说明 |
|------|------|------|
| Router | 中间件管道 | = F4，见 §一 |
| Controller | 生命周期钩子 | = F3 重设计，见 §一 |
| Db(Pool) | 连接泄漏检测 | 借出未归还的连接追踪与告警；`pool.c` 仅有超时溢出建连 + 归还自动收缩，无泄漏检测 |

### 2.2 P2 批（按需求驱动立项）

| 模块 | 缺口 |
|------|------|
| Application | `setResponse()` 无对应 getter |
| Hook | 钩子优先级、`stopPropagation()` |
| Cache(Redis/Memcached) | pipeline / multi（可经 `call()` 透传，非硬缺口） |

---

## 三、性能 / 压测量化后待立项的优化

> **准入约束**：本节所有项必须先通过 `tools/acceptance` 的 profile 准入脚本拿到证据，否则不得进入主线。

### route_pc 全树预热

- **来源**：`AUDIT_REPORT_2026_07_12.md` WP-04
- **现状**：`gene.route_precompile=1` 当前为惰性填充（按叶子 HashTable 指针 memoize），首次请求仍需解析。
- **待实现**：在 `workerReady()` 后遍历路由树，预先生成所有叶子对应的 `gene_route_pc` 描述符。
- **前提**：先由 profile 证明首批请求收益，并验证 `workerReady` 后 `fn_cache` / 路由树冻结不变量。

### sweep batch/cursor 方案（已缓解，降级为可选）

- **来源**：`AUDIT_REPORT_2026_07_12.md` SW-P3；`AUDIT_REPORT_2026_07_30.md` M1；`AUDIT_REPORT_2026_08_06.md` §一.3
- **现状**：M1 cooldown 已落地（`gene.c:907-935`，`cap/4` 分配数 + 表增长双触发，`co_contexts_sweep_skipped` 遥测已导出），O(N²) 放大已缓解。F1 协程 defer 自动 cleanup 落地后，M1 的现实触发场景（漏调 cleanup）已被抽掉，冷却逻辑作为纯防御层保留。
- **结论**：batch/cursor 化**降级为可选优化**，除非压测显示 cap≥8192 时仍有毛刺，否则不主动改动。

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

### fn_cache / 连接池可选演进

- **来源**：`AUDIT_REPORT_2026_05_25.md` §9.1；`AUDIT_REPORT_2026_08_06.md` §9.2
- **待评估**：
  - `gene.fn_cache_max`：fn_cache LRU 容量治理。
  - `gene.pool_max_overflow`：连接池 overflow 硬熔断。
  - ~~`named_cache` 改用 `pemalloc`~~（08-08 已落地：持久堆 + 持久 key 副本 + `runtime_type >= 2` 门禁，见 ML1）。

### 借还路径跨界调用与可观测性（08-07 核查新增，需 profile 达标）

- **来源**：`audit/audit_landing_verification.20260807.md` 第三批
- **背景**：静态计数下，DB 池「借 + 还」稳态 3 次跨界（`Channel::pop` + `Atomic::get` + `Channel::push`），理论下限 2 次。PF1 已把 put() 从 3 次压到 1~2 次，以下是剩余空间。

| # | 位置 | 优化 | 备注 |
|---|------|------|------|
| P-1 | `src/db/pool.c:908` | 正常归还路径的 `Atomic::get` 仅用于 `cur > max` 溢出判断；溢出连接唯一来源是超时补偿，可改为「先 push，仅当 `db_pool_get_timeout` 计数非零时才做溢出检查」，稳态压到 1 次跨界 | 剩余空间最大的一处 |
| P-2 | `src/db/pool.c:852-854`、`src/cache/redis_pool.c:1248-1251` | `php_error_docref` 的实参在 C 里无条件求值，E_NOTICE 被屏蔽时仍白付 1-2 次 `Atomic::get` | 改为先判 `EG(error_reporting)` 再取值 |
| P-3 | `src/router/router.c:286` | `get_path_router_init` 在无 prefix / 无 langs 时返回 `str_init(path)`，复制出与入参相同的字符串后调用方 efree 原件；改为返回 `path` 本身、调用方用指针相等判断（该模式在 match 路径已存在） | 每请求省 1 次 emalloc + memcpy |
| P-4 | `src/cache/memory.c:200` | TTL 表非空时 get 热路径每次调 `time(NULL)`，可换 `sapi_get_request_time` 或缓存的秒级时间戳 | 仅影响启用 TTL 的部署 |
| P-5 | `src/tool/monitor.c:305` | Prometheus 导出缺 `worker_id` label，多 worker 抓取剧烈抖动（计数器本身是 per-worker module globals，语义正确） | 可观测性缺陷，非性能项，可不受 profile 准入约束单独立项 |

### 观察项（需 profile / ZTS 证据后立项，不主动改动）

| # | 位置 | 问题 | 来源 |
|---|------|------|------|
| C2 | `src/gene.c:89-90、102-103` | dlsym 解析结果为进程级 static，ZTS 下并发解析；`resolved` 标志与指针写入间无内存屏障，弱内存序架构（ARM）理论瑕疵。Swoole 不支持 ZTS，实际风险极低 | 08-06 §3.2 |
| C3 | `src/db/pool.c:55`、`src/cache/redis_pool.c:42` | 进程级 static `HashTable *named_cache` 在 ZTS 下跨线程共享且无锁。同 C2，与 §五 function-local static 合并处理 | 08-06 §3.2 |
| C4 | `src/cache/memory.c:565-570` | `GENE_CACHE_RDLOCK()` 依据 `worker_ready` 跳过加锁；仅在引入多线程 worker 时需重新评估 | 08-06 §3.2 |
| ML1 | `src/db/pool.c:35-84`、`src/cache/redis_pool.c:36-71` | ~~`named_cache` 用 `emalloc` 而非 `pemalloc`~~ **08-08 已关闭**：表改持久堆、key 改持久副本，并只在 `runtime_type >= 2` 下填充（多请求 SAPI 不再跨请求持有请求期对象）。C3 的 ZTS 竞争仍在观察 | 08-06 §2.2 |
| ML2 | `src/gene.c:748` | sweep `emalloc(sizeof(zend_ulong) * total)` 瞬态分配；M1 cooldown 后触发频率大幅下降，除非压测显示 cap≥8192 时进入火焰图 | 08-06 §2.2 |
| PF2 | `src/router/router.c:2233-2544` | `snprintf` 拼接改 `memcpy`；**几乎全在路由注册/编译阶段（冷路径）**，收益仅冷启动/reload | 08-06 §3.3 |
| PF3 | `src/router/router.c:245-278` | `strtok` 字符串复制；仅在多语言路由中执行，核心匹配已是指针扫描 | 08-06 §3.3 |
| PF4 | `src/router/router.c:656` | 闭包 fn_cache key 改 `zend_hash_index_find`；闭包路由 1-2% 收益 | 08-06 §3.3 |

---

## 四、待 Linux 环境验证的阻塞项

> **环境约束**：本机 Windows，无法执行编译、ASAN/Valgrind、压测与长跑。以下全部待 Linux 环境补齐后承接。

### O6 运行时验证全集

- **来源**：`AUDIT_REPORT_2026_07_30.md` §七
- **内容**：
  - ZTS/NTS 双构建零告警。
  - ASAN/Valgrind 无内存错误。
  - 百万请求/24h RSS 长跑。
  - CAS/pool 压测。
  - dlsym 符号可见性验证。
- **状态**：代码已静态实施，`php -l` 通过；所有运行时验证均因缺少 Linux 环境而悬置。

### O7 Linux 回归失败项

- **来源**：`AUDIT_REPORT_2026_07_30.md` §七 / `AUDIT_REPORT_2026_07_12.md` §7.7
- **待处理**：
  - DB 驱动 `connect()` 二选一逻辑。
  - Cache / Language / Http 夹具 triage。
  - Mvc 失败项：F3 已回退，相关用例需重新评估或移除对 `init()` 的依赖。

### 08-06 / 08-07 新增验证需求

- **来源**：`AUDIT_REPORT_2026_08_06.md` §9.4
- **清单**：
  1. Linux `phpize + make`（含 `--enable-debug` / ASAN）零告警编译。
  2. C1 修复后多 worker（≥4）并发借还压测下 pool count 一致性断言（无负值、与 channel length 一致）。
  3. MySQL / PgSQL / MSSQL `join` / `union` / `where` / `in` / `group` / `order` / `limit` 全量 SQL 回归（含标识符引用与参数绑定）。
  4. `Session::regenerateId()` 并发场景下旧会话删除与新 cookie 刷新的一致性。
  5. `Controller::forward()` 深度上限（≤5）在超限时的错误路径回归。
  6. `Router::match()` 与 `Router::dispatch()` 匹配结果等价性回归。
  7. `test/DatabaseTest.php` SQLite 段断言 `lastInsertId()` / `rowCount()` / `quote()` 行为。
  8. `gene.slow_query_ms=1` 执行慢 SQL 断言 `Monitor::stats()['db_slow_query_count']` 递增；阈值 `0` 时计数恒为 0。
  9. `sendFile` 大文件 RSS 平坦 + offset 越界返回 false + wrapper 拒绝。
  10. Swoole 下 `Application::stop()` 后同 worker 后续请求正常派发 + 并发协程隔离。
  11. FPM 常驻 worker 循环 TTL 写入十万次不读取，断言 RSS 不单调增长。
  12. `View::render()` 输出缓冲嵌套满场景断言返回 false 且不污染外层输出。

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

## 五、代码迁移 / 清理

### 剩余 function-local static 函数指针缓存迁移

- **来源**：`AUDIT_REPORT_2026_07_03.md` §10.2 / 状态表；`AUDIT_REPORT_2026_08_06.md` C3
- **现状**：`GENE_CG_FN_LOOKUP` 基础设施已建，`benchmark.c`、`log.c`、`common.c`、`pdo.c`、`validate.c` 已迁移。
- **待迁移**：`redis_pool.c` 等文件中剩余的 `static zend_function *` 局部缓存。Swoole 不支持 ZTS，实际风险低，但应逐步对齐仓库约定。与 §三 C2/C3 合并处理。

---

## 六、文档与测试缺口

### 文档

- **来源**：`AUDIT_REPORT_2026_08_06.md` §4.4 / §4.5
- **待补**：
  - 新增 `docs/API_REFERENCE.md`。
  - 新增 `docs/ARCHITECTURE.md`（尤其是 Swoole 协程上下文模型与 `co_contexts` 语义，本扩展最难理解的部分）。
  - 新增 `docs/PERFORMANCE_TUNING.md`（INI 调参 + Monitor 指标解读）。
  - `docs/CONFIGURATION.md` 说明 `model.success/error` 的业务码空间（2000/4000）与 HTTP 状态码不同源。
- `gene-ide-helper/Gene/Application.php` 版本注解为 5.4.3，实际 5.6.9，需更新。
- `Router::getRouterUri()` 在 C 层有实现但 ide-helper 中无声明。

### 测试

- **来源**：`AUDIT_REPORT_2026_08_06.md` §4.5
- **现状**：`test/` 已有 16 个用例文件 + `TestRunner.php`。
- **仍缺独立测试的模块**：**Monitor、Exception、Factory、Memcached、RedisPool、MSSQL**。

---

## 七、可选演进（05-25 记录）

- **来源**：`AUDIT_REPORT_2026_05_25.md`
- **待评估**：
  - Swoole RSHUTDOWN 守卫：明确 RSHUTDOWN 仅在 worker 退出/reload 触发，防止误配置导致协程上下文被提前销毁。
  - `file_cache_only` 压测回归：opcache `file_cache_only=1` 场景下的跨请求 UAF 回归。
  - persistent PDO / 外部代理部署建议：生产部署文档与灰度验收模板。
  - FPM worker RSS 与 `pm.max_requests` 验收：依赖人工提供生产等价环境和基线数据。

---

## 七之二、2026-08-09 审计新增待办（ORM v1 + 近两周优化）

> 来源：`AUDIT_REPORT_2026_08_09.md`（**含运行时实测证据**，复现脚本在 `audit/repro/`）。
> 前 3 项为实锤缺陷，应作为一个修复批优先落地，不进「待评估」队列。

### 7.2.1 待修复缺陷（P0）

| # | 位置 | 问题 | 修法 |
|---|------|------|------|
| H1 | `src/db/pdo.c:513,516` | 缺 `username`/`password` 时 NULL 解引用 → 段错误（4 驱动全中，`test/OrmTest.php` 直接崩溃） | 参照 `pool.c:239-259` 的 `pool_normalize_config()` 补空值兜底 |
| H2 | `src/router/router.c:2293-2301` | `match()` 未命中分支漏 `zval_ptr_dtor(&ctx->path_params)`，实测 56 B/call | 补对称释放；两段恢复代码抽 `restore:` 标签 |
| H3 | `src/orm/meta.c:312` | `timestamps` 用 `sapi_get_request_time()`，常驻进程下时间戳冻结（全仓唯一一处，其余 11 处均 `time(NULL)`） | 改 `time(NULL)` |

### 7.2.2 ORM v1 语义补齐（P1，建议同批或紧随）

- **M1 hydrate 缺口（结构性）**：`find()`/`findAll()`/`Query::row()` 只返回数组，`exists` 仅能由成功 insert 置位
  → `(new M)->fill(M::find($id))->save()` 走 INSERT 且带主键，造成唯一键冲突或静默重复行（已复现）。
  方案：`fill()`/新增 `hydrate()` 在含非空主键时置 `exists=1`；并提供 `find($id, true)` / `Query::first()` 返回模型。
- **M2 返回类型**：`create()`/`save()` 透传 PDO 的字符串 `lastId`，且把字符串主键写回 attributes，
  与 `find()` 的 `int` 主键、`Query::count()` 的 int 归一化不一致 → 统一为 `ZVAL_LONG`。
- **M4 方言判定**：`gene_orm_db_limit()` 用 `strstr(cname,"Mysql")` 判 MySQL，而 `gene_orm_db_reset()` 用
  `ce ==`；非标准/包装/子类化驱动会拿到颠倒的 offset/limit，`paginate()` 静默返回错误页 → 统一 `ce ==`。
- **M5 db 句柄借用**：`gene_orm_get_db()` 返回 DI 借用指针，静态方法跨多次 `call_user_function` 持有；
  期间用户代码 `Di::del('db')` 即 UAF（`Query` 路径持强引用，已安全）→ 入口 `Z_TRY_ADDREF_P`、出口 dtor。
- **M3 异常门禁**：`gene_orm_db_call()` 包 `call_user_function`，抛异常仍返回 SUCCESS，后续 SQL 继续下发
  → 每个调用点后加 `if (UNEXPECTED(EG(exception))) goto cleanup;`。
- **M7**：实现 `__isset`/`__unset`（当前 `isset($m->attr)` 恒 false、`unset()` 静默无效）。
- 去掉 `Query` 的 `ALLOW_DYNAMIC_PROPERTIES`（`query.c:440-442`），与 `final` + protected 状态的封闭设计冲突。

### 7.2.3 ORM 能力缺口（P2，按需求驱动）

- `Query` 缺 `join/group/having/union/offset/first/exists/pluck/update/delete`；无事务、关联、软删除、属性转换。
  底层 Db 层 08-06/08-07 刚补齐 `join/union/group/having`，ORM 未透出，落差明显。
- `$fields` 目前仅作 SELECT 列表，`fill()`/`__set()` 不校验字段名（数字键亦可写入）→ 缺 mass-assignment 白名单。
- API 对称性：有 `Model::updateBy()` 无 `Query::update()`；`Model::destroy($id)` 与实例 `delete()` 语义重叠。

### 7.2.4 文档 / 测试待办

- `test/DatabaseTest.php:191` 调用不存在的 `Sqlite::connect()`（另有 `getHistory()` 亦不存在），
  测试套件在此 fatal —— 测试文件与 6.0.0 API 已脱节，需同步。
- `Response::sendFile()`：FPM 分支拒绝非 plain-file wrapper，**Swoole 分支无任何校验**（`response.c:786-805`）；
  两分支均不做路径规范化。建议把 wrapper 校验前置到分支之前，并在文档写明路径校验为调用方责任。
- 文档需写明：ORM 不可跨协程共享同一 Db 实例（应走 `pool` 配置）；生产环境建议 `gene.run_environment=1`
  以关闭默认开启的 SQL history（单驱动单请求上限 200 条 ≈ 177 KB，非泄漏但会干扰内存 profiling）。
- `create()` = insert + lastId 两次独立调用，无事务包裹，需在文档说明并发下的 id 语义。

---

## 八、如何维护本计划

- 每轮审计完成后，将**仍未实现/未验证**的项从审计报告迁移到本文件。
- **已落地或验证通过的项应及时删除**，不在本文件保留「✅ 已完成」条目；完成记录留在审计报告的「落地情况」节。
- 新立项前必须在 `tools/acceptance` 拿到 profile/ASAN 证据，避免无依据的大范围改动。
