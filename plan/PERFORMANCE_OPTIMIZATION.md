# Gene 扩展并发性能优化 —— 最终执行版

> 版本：v2（2026-09-06）。v1 为静态审计草案，经 `PERFORMANCE_OPTIMIZATION_CHECK.md` 复核后，
> 已删除/纠正其中「与代码现状相反」「已实现」「会造成语义回归」「无数据支撑的收益数字」的条目。
>
> 执行纪律：
> 1. **收益一律先测后填**。本文档不再保留任何未经基准验证的百分比/倍数；标注 `收益：待测` 的项目
>    必须先产出微基准数据才能进入实施。
> 2. 涉及生命周期/裸指针的改动，Linux `-fsanitize=address` 下跑全量回归（见 §9）。
> 3. 任何改变**公开 API 语义**的项目单独立项、单独评审，不得混入「透明性能优化」批次。

---

## 0. 已有优化基线（不得重复提案）

| 领域 | 已完成 | 位置 |
|---|---|---|
| 请求上下文 | `gene_request_context` 内联 `path_params`、struct 池复用（`ctx_pool`）、`co_contexts` 冷却式 sweep、`vm_stack` 同协程快路径跳过 `getcid()` | `gene.c:1125-1250` |
| 协程 ID | `dlsym` 直调 Swoole C-API `get_current_cid()`（`gene.swoole_getcid_capi=1`） | `gene.c:175-213` |
| 路由 | 预编译 dispatch 描述符 `route_pc`：**已缓存** `is_before/is_after`、before/after/hook src、对应 closure、route src、eval 程序（`gene.route_precompile`，默认关） | `router.c:755-797, 1005-1066` |
| 类查找 | `gene_lookup_class_str` 栈上小写缓冲直查 `EG(class_table)` | `gene.c:259-289` |
| 内部函数 | `GENE_CG_FN_LOOKUP` 缓存 `json_encode` 等函数指针（非 ZTS） | `gene.h:73-90` |
| 进程缓存 | `workerReady()` 冻结 bucket + 无锁读快路径（`GENE_CACHE_RDLOCK` 条件跳锁）；标量 `ZVAL_COPY_VALUE` 零拷贝 | `memory.h:27-30`、`memory.c:396-485` |
| 模板编译 | 编译结果以 `zend_string*` 流转（省 28× estrndup/efree）；`view_compile_check_mtime` 增量重编译 | `view.c:52-72, 610-630` |
| 日志 | 级别过滤已在任何分配之前；空 `context` 已跳过 JSON 编码 | `log.c:177-238` |
| Session | `set()/del()` 仅置 dirty，`save()`/析构一次写后端；cookie 有 `cookie_sent` 去重 | `session.c:947-986, 1034-1055` |
| 字符串 | `gene_strreplace_fast`、ctx 内缓存 `*_len` 避免 `strlen` | `common.c`、`gene.h:163-170` |

---

## 1. 请求热路径（Router / Dispatch / Request / Response）

### 1.1 【中】`route_precompile` 默认开关评估
- **位置**：`router/router.c:755-797`（描述符）、`1005-1066`（resolve）、`1364-1392`（缓存键）
- **现状纠正**：hook 解析结果**已经**存入 `gene_route_pc`，v1 所述「待做 hook 预编译」不成立。
  真正待决策的只有三件事：
  1. 默认值是否从 0 改 1；
  2. action 的 `zend_function *` 是否值得进描述符（见 1.2，结论：**暂不**）；
  3. 失效机制。
- **前置硬约束**：`route_pc` 以 leaf `HashTable*` 地址为 key，并**借用**路由树内部指针。
  默认开启的前提是「`workerReady()` 之后路由树绝对只读」必须在代码层强制（增加 frozen 标志，
  冻结后 `bind()` 直接报错或失效整表），否则会出现 stale descriptor / UAF。
- **收益**：每请求省若干次哈希查找 + 1 次 alloc，**待测**。**风险**：中（生命周期）。

### 1.2 【低】action 双重哈希探测
- **位置**：`router/router.c:465` `zend_hash_str_exists(function_table)` → `factory/factory.c:276`
  再 `zend_hash_str_find_ptr` 同一 key。
- **方案**：`gene_factory_call_1` 增加 `zend_function *fn` 入参（或返回未命中状态），dispatch 侧
  `find_ptr` 一次并传入。
- **明确不做**：不把该 `zend_function *` 缓存进 `route_pc`。同一 leaf 可经 `:c`/`:a` 派发到不同
  controller/action，单指针缓存不成立。保留为**纯局部微优化**。
- **收益**：每请求省 1 次哈希探测。**风险**：低。

### 1.3 【中高】`chird` 占位子路由线性扫描
- **位置**：`router/router.c:300-374` `get_path_router_inner`
- **现状（已核对）**：静态 segment **已经**先走 HashTable 精确查找；线性扫描只发生在占位子路由
  （`chird`，按占位符名保存）上，且当前匹配代码不按「正则类型/首字符」检查当前 segment。
- **v1 方案作废**：「静态 → 正则 → 泛型分桶 + 首字符索引 → 近 O(1)」不成立 —— 静态本就不在
  线性扫描里，泛型占位之间的区别通常在**后续路径分支**而非本段首字符。
- **重新设计的候选方向**（需专项设计文档，择一）：
  1. 注册期检测同层等价泛型路由并报冲突，从源头限制 children 规模；
  2. 对占位子树的后续固定 suffix 建立判别索引；
  3. 重构为带约束的 radix tree；
  4. 显式定义多可变路由的优先级语义（当前隐式依赖插入顺序，改动前必须固化）。
- **收益**：待测（须用「深层 REST + 多同层占位」专用基准）。**风险**：中高。

### 1.4 【中】URI 解析与 m/c/a 多次堆分配
- **位置**：`router/router.c:233-290`、`router.c:124-159` `setMca`、`http/request.c:78-103`、
  `gene.c:603-620`（reset 逐字段 `efree`）
- **现状**：一次请求约 8–10 次小块 emalloc/efree 仅用于持有 method/path/lang/module/controller/action。
- **方案（相较 v1 已收敛）**：不给每字段各挂一个定长数组（会抬高**所有**活跃协程与 ctx pool 的常驻内存），
  改为：
  1. ctx 级**单个小型 arena / 单一 URI backing buffer**；
  2. 各字段只存 `offset + length`；
  3. reset 时整体复位游标，超长才回退 heap 并打所有权标记；
  4. 所有 `ctx->module != NULL` 的「是否已设置」判断改为长度/标志位。
- **额外约束**：若字段借用 URI 内容（`ptr,len`），必须严格证明 URI backing 的生命周期覆盖整个请求。
- **收益**：待测（分配器调用次数可直接计数）。**风险**：中（生命周期，需 ASAN）。

### 1.5 【中】Webscan 每请求实例化
- **位置**：`app/application.c:418-447`、`http/webscan.c:85-161, 224-267`
- **现状**：`object_init_ex` + 7 参数 `__construct` + `check()` + dtor，每请求一次。
  `check()` 主要读对象配置，未见明显请求级状态写入。
- **方案调整**：**不**采用「worker 级长期持有 PHP 对象」（涉及属性 zval 生命周期、配置热更新、
  RINIT/RSHUTDOWN、Swoole/FPM 差异）。改为**去对象化**：抽出一个接收配置参数的 C helper，
  application 直接调用，完全省掉对象构造。
- **拆分立项**：flatten 增加递归深度/长度上限会**改变安全扫描覆盖范围**，属安全策略变更，
  必须单独评审，不在性能批次内。
- **收益**：省一次对象构造与 7 参数装拆，待测。**风险**：中。

### 1.6 【暂缓 / 需专项设计】响应输出逐块进入 SAPI 层
- **位置**：`http/response.c:817-853`
- **v1 方案作废**：把所有 `write()` 累积到 `smart_str` 再于 `end()` 输出，会破坏 SSE、chunked
  streaming、大响应恒定内存、调用方期望的及时 flush 与 TTFB。**这是 API 语义变更，不是透明优化。**
- **另外**：v1 引用的 `response.c:863-865` 循环丢弃 output buffer 属于 `sseStart()`
  （`response.c:857-894`），其目的正是**确保 SSE 不被缓冲**，并非低效逻辑。
- **可接受的方向**：
  1. 保留 `write()` 流式语义不变；
  2. 另加显式 buffered API（如 `writeBuffered()` / `setBuffering(true)`）由调用方选择；
  3. 或仅合并框架内部产生的非流式小块写入。
- **风险**：高（API 语义）。**排期**：暂缓，先出设计。

### 1.7 【低】零散 `strlen` / 未缓存函数指针（可立即做）
- `router/router.c:2147/2222/2478` `zend_read_property(..., strlen(GENE_ROUTER_SAFE))` → `ZEND_STRL`。
- `factory/factory.c:213-219` 已算 `action_len` 却又 `ZVAL_STRING` 重算 → `ZVAL_STRINGL`。
- `http/json.c:74/103` 每次 `zend_hash_str_find_ptr(CG(function_table))` → 复用 `GENE_CG_FN_LOOKUP`
  或直调 `gene_json_encode`。
- `di/di.c:143-165` alias 链最多 8 跳 → `Di::alias()` 注册期解析到最终目标。
- `gene.c:410-435` `gene_get_router_uri` 最多 4 次 alloc → 单次 `smart_str` 后 `ZVAL_STR`。

---

## 2. 视图 / 自动加载

### 2.1 【高】模板编译配置纠正（**v1 此处结论是反的，最严重错误**）
- **位置**：`mvc/view.c:52-72`（`view_compile_needs_rebuild`）、`445-475`、`623-630`
- **代码事实**：
  - 只有 `isCompile || GENE_G(view_compile)` 为真才进入编译分支；
  - `view_compile_check_mtime = 0` 时 `view_compile_needs_rebuild()` **直接 return 1**，即**每次请求都重编译**；
  - 普通 `display()` 走 `gene_view_display()`，不进 `displayExt()` 的模板编译分支。
- **因此 v1 推荐的 `view_compile=1 + view_compile_check_mtime=0` 会导致每请求跑 28 轮
  `php_pcre_replace`，与优化目标完全相反。**
- **正确配置**（二选一）：
  - 使用运行时编译缓存：`gene.view_compile=1` + **`gene.view_compile_check_mtime=1`**；
  - 使用离线预编译产物：构建期生成 `app/Cache/Views/*.php`，运行时 `gene.view_compile=0`。
- **动作**：修正 §7 生产配置、修正对外文档、补一条断言「check_mtime=0 且 view_compile=1」
  在 `run_environment>=2` 时发 warning。**风险**：低。**优先级**：立即。

### 2.2 【暂缓 / 收益未证明】视图渲染的 output buffer 捕获
- **位置**：`mvc/view.c:813-859`
- 模板可执行任意 PHP 并通过 `echo`/include/扩展输出，直接捕获到 `smart_str` 最终仍需接入 PHP
  output handler，不一定能省掉 output subsystem。
- 「嵌套子视图共享一层 buffer」需**先证明**当前确实创建了多层 buffer —— 普通 include/contains
  通常天然写入当前 buffer。
- **动作**：先加计数验证多层 buffer 是否真实发生；未证实前不列为收益项。

### 2.3 【低-中】自动加载路径构造与重复 stat
- **位置**：`factory/load.c:76-127`（编译+执行+加入 `EG(included_files)`，但未先判断是否已包含）、
  `load.c:131-165`（`estrdup`+`replaceAll`+`snprintf`）
- **方案（已加约束）**：
  1. `EG(included_files)` 短路**只用于类自动加载路径**，且必须以规范化后的 `opened_path` 为键；
  2. **视图文件必须允许重复执行**（不同 symbol table），绝不可给通用 `gene_load_import()`
     无条件加短路；
  3. 路径拼接改 `memcpy`。
- **明确降级**：`workerReady()` 预 include 整个控制器/模型目录会改变加载顺序与文件副作用，
  风险非「低」，作为独立 opt-in 配置，默认关。

---

## 3. 进程缓存（Gene\Memory / Gene\Cache）

### 3.1 【高，架构级第一优先】框架缓存与业务缓存共用一张表、一把锁
- **位置**：`memory.h:21-46`、`gene.h:296` `cache_business_dirty`
- **现状**：首次 `Gene\Cache` 业务写将 `cache_business_dirty=1`，此后**所有**读（含路由、配置、DI）
  永久回到 `rwlock` 路径。
- **方案**：拆为两张 `HashTable` + 两把锁：
  - 框架表：`workerReady()` 后真正 write-once、永久无锁；
  - 业务表：独立锁 / LRU / TTL，并且**允许正常 rehash**。
- **v1 遗漏的关键问题（必须一并解决）**：冻结后新 key 插入依赖预留 bucket，而删除留下的 tombstone
  **不会降低 `nNumUsed`**（`memory.c:833-865`）。长期高 churn 即使有 LRU 也会逐步耗尽预留 bucket，
  表现为 `cache_insert_refused` 持续增长。拆表的目标之二就是让业务表摆脱「冻结 + 预留 bucket」模型。
- **表述纠正**：「消除高并发读串行点」在单 Swoole worker（单线程协作调度）下**夸大** ——
  短且不 yield 的 rwlock 临界区主要是固定原子/函数调用开销，未必存在严重线程竞争。真实收益待测。
- **明确移除**：RCU 方案。单 worker 协程模型下收益有限，回收 epoch 与裸指针风险很大。**不做**。
- **风险**：中。

### 3.2 【降级 / 大部分已实现】`Memory::get()` 拷贝策略
- **现状纠正**：
  - `Memory::get()` 调用的是 `gene_memory_zval_local()`（`memory.c:1291-1303`），不是
    `gene_memory_zval_local_copy()`（后者用于 `Gene\Cache` 业务读）；
  - `LONG/DOUBLE/NULL/BOOL` **已经**是 `ZVAL_COPY_VALUE`（`memory.c:415-421, 469-475`）。
  - **v1 的第一项建议是已完成项，删除。**
- **字符串借用**：不能用「persistent/interned 标志」简单实现。代码注释已记录：零拷贝路径此前
  因业务缓存覆盖/淘汰导致 UAF 而被**主动撤销**（`memory.c:404-409`）。
- **保留项**：`getBorrowed()` 仅可用于「整个借用期内保证无覆盖、无删除、无 TTL 清理、无协程切换」
  的内部路径。**风险：高**（v1 标「中」不足），需先完成 3.1 拆表并明确框架表只读不变式。

### 3.3 【中】`mget()` 逐 key 加读锁
- **位置**：`memory.c:1782-1824`
- **方案**：一次 `GENE_CACHE_RDLOCK()` 内遍历全部 key，结束后一次解锁；锁内禁止 yield。
- **v1 遗漏、必须保留的语义**：
  1. TTL 判断（过期项返回 miss）；
  2. **深拷贝必须在释放锁之前完成**；
  3. FPM 下的延迟删除行为保持一致；
  4. hit/miss 计数语义不变。
- **参考实现**：`gene_memory_get_triple()`（`memory.c:901-938`）已具备正确形态，照此扩展。
- **收益**：N 次锁 → 1 次。**风险**：低（前提是照搬上述四条）。

### 3.4 【低】缓存键生成
- **位置**：`cache.c:563-619` `gene_cache_key`（已支持 FNV / xxHash64 / FarmHash / Murmur / TurboHash）
- **可做**：构造期预生成 `(sign,class,method)` 前缀 `zend_string`，运行时仅追加 args。**风险**：低。
- **明确不做**：**不改默认哈希算法**。改默认会使现有 APCu/Redis/Memcached key 全部变化 →
  滚动发布期间新旧实例无法共享缓存、命中率瞬时归零。若要切换，只能以**显式配置 + key 版本号前缀**
  的方式提供，并在文档标注为破坏性变更。

### 3.5 【低】TTL 采样清理
- `memory.c:560-585`：每 32 次写扫 64 项，写少读多时过期 key 只表现为 miss、存储不回收 →
  暴露 `gene.cache_expiry_sweep_*` 配置；Swoole 可加后台 sweep。
- **`Timer::tick` 硬约束**：只操作业务表；与 worker 生命周期绑定；**不为每个实例创建 timer**
  （单 worker 一个全局 timer）；限制单次扫描时长；不在锁内 yield。
- `GENE_G(x)++` 计数器仅监控用途，可接受现状。

### 3.6 【低 / 条件不成立，需先补前置】FPM 非 ZTS 跳锁
- **现状纠正**：`workerReady()` 在 `runtime_type < 2` 时**并未**提前返回，仍会 reserve 并置
  `worker_ready=1`（`application.c:1317-1387`）。因此 v1 前提「FPM 下 `worker_ready` 永远为 0」**不成立**。
- **动作**：若要做此优化，必须先在 `workerReady()` 中显式定义 FPM 语义（提前返回，或明确置位含义），
  再以 `#ifndef ZTS && runtime_type<2` 跳锁。ZTS 必须保留锁。**风险**：中。

---

## 4. 数据库（Pool / PDO / 驱动）

### 4.1 【P0，立即做】池化连接未设置 `ATTR_DEFAULT_FETCH_MODE`
- **位置**：`db/pool.c:246-285` `pool_normalize_config()` 只设 ERRMODE(3) / EMULATE_PREPARES(20) /
  Swoole 下 PERSISTENT(12)；非池路径 `db/mysql.c:265-279` 已设 `19 => 2 (FETCH_ASSOC)`。
- **后果**：池连接落到 PDO 默认 `FETCH_BOTH`，每行双键（数字+字符串），内存与 `zend_string` 分配翻倍。
- **方案**：`add_index_long(&z, 19, 2)`，**两个分支都要加**（options 缺省分支与已有 options 分支，
  后者位于 `pool.c:278-285`）。
- **附带核查**：确认 Pgsql/Sqlite/Mssql 非池路径的 `ATTR_CASE`、`ATTR_ORACLE_NULLS` 是否需与池路径一致。
- **收益**：高（可用行数×列数直接量化）。**风险**：低。

### 4.2 【P1，opt-in，不改默认】FPM 模式不走连接池 / 持久连接
- **位置**：`db/pool.c:1551-1564`（`runtime_type < 2` 直接返回 0）、`db/mysql.c:215-290`
- **方案调整**：提供**显式配置** `gene.db_fpm_persistent`（默认 **0**），开启后设
  `PDO::ATTR_PERSISTENT=true`；归还前调用现有 `gene_db_tx_hygiene` 回滚未提交事务。
- **必须在文档中列明持久连接残留状态**（`tx_hygiene` **无法**清除）：session variables、temporary
  tables、advisory locks、prepared statements、SQL mode / time zone、认证与断线状态。
- **明确不做**：不把 `ATTR_PERSISTENT=true` 作为框架默认。**风险**：中高。

### 4.3 【P2】PDO/PDOStatement 方法指针缓存
- **位置**：`db/pdo.c` 约 **19 处**（v1 写 12 处，计数错误）`zend_hash_str_find_ptr(&ce->function_table, ...)`，
  典型如 `pdo.c:856-875`。
- **方案**：按 `ce` 首次解析并缓存到静态结构体，之后 `zend_call_known_function`。
- **必须的安全条件**：`PDOStatement` 可经 `ATTR_STATEMENT_CLASS` 替换为**用户类**，其 CE/method
  在 FPM 下可能只有请求生命周期 → 进程级静态缓存**只允许对精确的内部 CE 生效**
  （`Z_OBJCE_P(x) == php_pdo_get_dbh_ce()/statement ce` 严格相等判断），其余走动态查找。
- **优先级下调理由**：相对真实 SQL 网络与 DB 执行时间，一次 HashTable lookup 占比极小。
  **除非纯内存 mock 微基准证明 CPU 已成瓶颈，否则不进第一批。**

### 4.4 【P1，第一批】ORM 通过 `call_user_function` 调用 Db 方法
- **位置**：`orm/meta.c:341-351` `gene_orm_db_call`（`ZVAL_STRING(&fname)` + `call_user_function`），
  被 `orm/model.c`、`orm/query.c` 数十处调用。
- **可行性**：Gene 的四个 Db 类为 **final**，按精确 CE 缓存函数指针相对安全（优于 4.3）。
- **方案**：缓存 select/where/limit/row/all/cell/lastId/affectedRows 等，改 `zend_call_known_function`。
- **收益**：一次 ORM 查询叠加多次链式调用，累积效应大于 4.3；**须用「纯内存 mock Db」微基准测量**，
  不能用真实远程 SQL 延迟掩盖结果。**风险**：低-中。

### 4.5 【暂缓】预处理语句 LRU 复用
- **位置**：`db/pdo.c:856-866`、`db/mysql.c:349/365`
- **收益表述纠正**：在 native prepares 下只省掉**重复 SQL 的 server prepare 往返**，
  **不省 execute 往返**（v1「省一次 DB 往返」表述含糊）。
- **适用前提**：SQL 高度重复、同一物理连接、正确关闭 cursor、连接重连后全部失效、
  控制服务端 prepared statement 数量、处理 DDL/`SET`/驱动差异。
- **风险**：高。**排期**：驱动公共 helper（4.8）落地后再评估。

### 4.6 【P2】SQL 片段全部经 PHP 对象属性中转
- **位置**：`db/mysql.c:158-173`（reset 11 个属性）、`mysql.c:301-343`（execute 读 9 个属性）；四驱动同构。
- **方案**：链式构建期在 C 侧对象结构体维护 `smart_str`/字段，`execute` 时组装；对外 API 不变。
- **收益**：每条 SQL 省 10+ 次 `zend_read/update_property`，待测。**风险**：中。

### 4.7 【P2】健康检查与回收
- `db/pool.c:368-404`：探活用 `getAttribute(ATTR_SERVER_INFO)`，借出时不探活 →
  提供 `ping_on_get` 可选配置；仅空闲超阈值才探活。
- `db/pool.c:720-804`：每个 Pool 一个 `Timer::tick` → 单 worker 合并为一个全局 timer。

### 4.8 【P2，长期，独立分支】四驱动去重
- `mysql.c/sqlite.c/pgsql.c/mssql.c` 各 ~60 KB，仅引号字符与少量方言方法不同
  （`pdo.c:1269-1285` `makeWhere` 甚至靠 `strstr(class_name,"Pgsql")` 判断引号）。
- **方案**：抽象 `gene_db_dialect { oq, cq, callbacks }`。
- **依赖关系纠正**：这**不是** 4.3/4.5/4.6 的技术前提 —— 三者均可先在公共 `pdo.c` 或 shared helper
  中实现。**大重构与性能优化必须分开提交**，否则回归时无法区分是抽象错误还是优化错误。

### 4.9 【高（开发模式）】SQL 历史 JSON 编码
- `db/pdo.c:1153-1171`、`mysql.c:175-212`：`run_environment=0` 时每条 SQL `json_encode` 参数入历史。
  生产必须 `gene.run_environment >= 1`，文档强调。

### 4.10 其他（低）
- `orm/meta.c:147-236` 请求级元数据命中后仍 `zend_string_copy` 5 个字段 → 出借指针 + `from_cache` 标志。
- `orm/model.c:763-797` `findMany(preserveOrder)` 多次 `zval_get_string` → 预归一化 ids。
- `mvc/model.c:82-122`、`service/service.c:81-122` `__get/__set` 每次类名 + DI 查找 → 缓存 `zend_string *` 类名。
- `orm/query.c:124-140/185+` ops 数组 push + 线性 apply → C 侧链表或直接生成 SQL。

---

## 5. Redis / Memcached / Session / Log / 工具

### 5.1 【中，需先加生命周期约束】RedisPool 计数走 `Swoole\Atomic`
- **位置**：`cache/redis_pool.c:453-491/580-594`；`db/pool.c:519-590` 同构。
- **前提缺失**：源码/API 目前**并未阻止**用户在 server start 前构造 Pool，直接换成 worker-local
  `zend_long` 会改变跨 worker 语义。
- **执行顺序（不可跳步）**：
  1. 明确约定 Pool 只能在 `WorkerStart` 内创建，并在构造处检测/记录 PID；
  2. 禁止 `clone` / `serialize`；
  3. 运行期 PID 变化时报错或降级；
  4. 之后才把计数改为结构体内 `zend_long`。
- **风险**：中高（跳过 1–3 则不可接受）。

### 5.2 【中】RedisPool `get()` 空闲队列 miss 的 1 ms 延迟
- **位置**：`cache/redis_pool.c:1230-1307`
- **现状纠正**：并非「`max+2` 次 1 ms 忙等」。正常路径是：① 尝试一次 1 ms pop → ② 未满则创建连接 →
  ③ 已满则以 `waitTimeout` 阻塞 pop；只有无效队列项/创建失败等路径才重试。
  准确描述是「**每次空闲队列 miss 多出约 1 ms 延迟**」。
- **方案**：首次改为**非阻塞** pop；未命中直接进入创建或 `waitTimeout` 阻塞；`workerStart` 预建 `min` 个连接。
- **收益**：降 p99（不是降 CPU 忙等），待测。

### 5.3 【低-中】序列化走 PHP `serialize()/unserialize()`
- **位置**：`common/common.c:743-793`、`cache/redis.c:496-515`、`memcached.c:369`
- **现状纠正**：**已经**缓存 PHP 函数指针并用 `zend_call_known_function`，不是最慢的 `call_user_function`。
- **方案**：直调 `php_var_serialize/php_var_unserialize`（按 `PHP_VERSION_ID` shim）仅省调用帧；
  大对象的主要成本是递归遍历与分配，**收益预期主要在大量小值场景**。
- **igbinary 直连降级为可选**：会引入可选扩展 ABI、头文件与构建探测依赖，收益/复杂度比不佳。

### 5.4 【中】Memcached 驱动每次查方法指针 + 无池
- `cache/memcached.c:115-173`：缓存 `zend_function *`，增加 `MemcachedPool` 复用连接。

### 5.5 【降级 / 大部分已实现】Session
- **现状纠正**：`Session::set()` 只改内存数组并置 dirty，`save()`/析构才写 handler
  （`session.c:947-986, 1034-1055`）；cookie 有 `cookie_sent` 去重。**「写合并」已存在，v1 该项删除。**
- **仍可做**：`set()/del()` 每次都调用 `gene_session_auto_cookie()` 做属性检查 → 提前用 `cookie_sent`
  标志短路，省掉属性读取。**风险**：低。
- **ID 生成纠正**：`session.c:334-375` 的 `gettimeofday+snprintf+MD5` 可换 `php_random_bytes`，
  但**不能只取 64-bit**（v1 的 `gene_u64_to_hex` 方案熵仅 64 bit，偏低）。应取
  **至少 16 字节随机数**再 hex 编码为 32 字符。

### 5.6 【低-中】Log
- **现状纠正**：级别过滤**已在任何分配之前**（`log.c:186-188`），空 `context` **已跳过 JSON**
  （`log.c:228`）。v1 的两条主要建议均为已完成项，删除。
- **仍然有效**：
  1. 秒级时间字符串缓存（`gene_log_get_datetime` 每条都重新格式化）；
  2. 减少 `error_log()` 的 PHP 调用帧；
  3. 可选 `gene.log_buffer` 批量刷。
- **风险纠正**：直接用 C `write()` 替代 `error_log()` 会改变 SAPI/error_log 配置、日志轮转、
  多进程追加与 Windows 行为，**不是低风险**，须作为 opt-in 且默认关。
- **删除**：v1「吞吐 5–10×」无任何基准依据。

### 5.7 【低】Benchmark / Monitor / Crypto
- `tool/benchmark.c:69-115`：`start()/end()` 调 PHP `memory_get_peak_usage` → `zend_memory_peak_usage()`
  C API；计时改 `gene_hrtime()`。
- `tool/monitor.c:43-187`：`stats()` 逐池调 PHP `stats()` → 各池导出 C 结构体计数器。
- `tool/crypto.c:67-222`：`base64/bin2hex/random_bytes` → `php_base64_encode`/`php_random_bytes`
  （这些是 PHP 内部 C 函数，安全）。**OpenSSL EVP 直接链接暂缓**（新增构建依赖，独立立项）。

---

## 6. 通用 / 横切

### 6.1 【低-中】JSON 编解码走 PHP 调用帧
- **位置**：`common/common.c:743-763`
- **现状**：源码注释已记录权衡 —— PHP 8.x 小版本间内部 API 签名变化、函数指针已缓存、
  约 0.5 µs 调用帧可接受。
- **优先级纠正**：v1 标「高」不合理。**除非压测显示每请求存在大量 JSON 编解码调用**，
  否则这是一个「兼容性换性能」的低-中优先级项。**风险**：中（多版本兼容）。

### 6.2 【待验证，不得先改】编译优化标志
- **位置**：`src/config.m4`（仅探测 `clock_gettime`）、`src/config.w32`（仅 `/I` `/utf-8`）
- **推理错误纠正**：扩展编译通常**继承 PHP 构建环境的优化标志**，不能由「config.m4 没写」
  推出「当前没有优化」。
- **执行前置**：先 dump 实际编译命令行（`make V=1` / MSBuild 详细日志），确认现有 `-O` 级别，
  再决定是否需要覆盖。
- **若确需覆盖，逐项验证**：`-O3` 未必快于 `-O2`；LTO 对 PHP 扩展收益常常很小；`/GL` 必须与
  `/LTCG` 配套；`-fvisibility=hidden` 需检查所有导出符号（配合已有 `PHP_GENE_API`）；
  `-march=native` 仅限同机部署。
- **删除**：v1「中-高收益」无数据支撑。

### 6.3 【低】`gene_preg_match` 与 Validate 正则
- **现状纠正**：经 PHP `preg_match` 调用时，PHP **本身已使用 PCRE 编译缓存**。直接调
  `pcre_get_compiled_regex_cache()` 主要省的是 **PHP 调用帧**，不是「从不缓存变成缓存」。
- **仍可做**：`http/validate.c:781-970` 内置规则在 C 层实现，避免 `gene_factory_call` 回调 PHP。
- 若期望跨 Swoole 请求复用，须先证明 PHP PCRE 缓存与正则字符串的生命周期。

### 6.4 【低】ZTS 下 `GENE_G()` 热路径访问
- 热函数内把 `GENE_G(runtime_type)` 等只读值读入局部变量一次；已有 `ZEND_ENABLE_STATIC_TSRMLS_CACHE=1`。

### 6.5 【暂缓 / 独立立项】HTTP 客户端 FPM 下每次 `curl_easy_init`
- **位置**：`http/http.c:1359-1407`、`1838-1950`
- **方案错误纠正**：FPM 每请求销毁 ctx，**ctx 级 handle 只能在同一请求内复用**，
  根本解决不了「每请求 `curl_easy_init`」。
- **真要跨 FPM 请求复用需要**：process-level handle pool、每次 `curl_easy_reset`、
  清除 header/callback/POST body/private data、处理 fork/DNS/TLS/异常、
  绝不长期持有请求级 zval。
- 直接链接 libcurl 还会新增构建依赖。**这是独立架构功能，不是中风险性能优化。**
- **可立即做的小项**：开启 `CURLOPT_TCP_KEEPALIVE`。

---

## 7. 生产推荐 php.ini（Swoole 模式）—— 已修正

```ini
gene.runtime_type             = 2
gene.run_environment          = 2     ; 关闭 SQL 历史 / benchmark 采集
gene.use_namespace            = 1

; —— 视图：以下两行必须成对；check_mtime=0 会导致每请求重编译（见 §2.1）——
gene.view_compile             = 1
gene.view_compile_check_mtime = 1
; 若改用「构建期离线预编译 app/Cache/Views」，则应设 gene.view_compile = 0

gene.route_precompile         = 0     ; 默认保持关闭；开启前须完成 §1.1 路由树冻结约束
gene.swoole_getcid_capi       = 1
gene.swoole_auto_cleanup      = 1     ; 防漏调 cleanup() 导致 co_contexts 堆积
gene.co_contexts_max          = 8192  ; ≈ 单 worker 峰值协程数
gene.ctx_pool_max             = 512
gene.ctx_pool_prewarm         = 512
gene.cache_max_items          = 10000
gene.cache_reserve            = 12560 ; ≥ max_items + max(64, max_items/4)
gene.slow_query_ms            = 200   ; 可选，用于定位慢 SQL
```

运行期观测：`Gene\Monitor::stats()` 中 `cache_insert_refused`、`db_pool_get_timeout`、
`redis_pool_cas_abandoned`、`co_contexts_sweep_count`、`ctx_pool_miss` 持续增长即为配置不足信号。
其中 `cache_insert_refused` 增长还可能是 §3.1 的 tombstone 耗尽预留 bucket，需一并排查。

---

## 8. 落地顺序（最终版）

### 第一批：低风险、可立即执行
| # | 项目 | 说明 |
|---|---|---|
| 1 | §4.1 池连接 `FETCH_ASSOC` | **两个 options 分支都要加** |
| 2 | §2.1 修正 View 生产配置 + 文档 + 矛盾配置 warning | v1 最严重错误的纠正 |
| 3 | §1.2 action 单次 HashTable lookup | **不**缓存进 `route_pc` |
| 4 | §1.7 零散 `ZEND_STRL` / `ZVAL_STRINGL` / Json fn 缓存 / Di alias 展平 | 纯局部 |
| 5 | §3.3 `mget()` 单次加锁 | 必须保留 TTL + 锁内深拷贝 + 计数语义 |
| 6 | §4.4 ORM final Db 类 known-function 调用 | 需 mock Db 微基准 |
| 7 | §5.5 Session `auto_cookie` 短路 | |
| 8 | §5.7 Benchmark 改 `gene_hrtime()` + Zend memory C API | 顺带让基准工具本身可信 |
| 9 | §9 建立微基准套件 | **其余批次的前置条件** |

### 第二批：中风险，需回归 + 数据支撑
1. §3.1 框架缓存 / 业务缓存拆表（含 tombstone 问题）
2. §1.5 Webscan 去对象化（flatten 上限单独评审）
3. §1.4 ctx arena + offset/length 方案
4. §4.2 FPM 持久连接（显式 opt-in，默认关）
5. §5.1 Pool worker-local 计数（先完成生命周期约束 1–3 步）
6. §5.6 Log 时间缓存 + 可选缓冲（opt-in）
7. §4.6 / §4.7 驱动公共 helper 与池健康检查
8. §1.1 `route_precompile` 默认开（先做路由树冻结强制）
9. §6.1 JSON 直调（若基准显示 JSON 调用密集）
10. §6.2 编译标志（先 dump 实际命令行）

### 暂缓 / 先出专项设计
1. §1.6 响应缓冲（API 语义变更）
2. §1.3 `chird` 索引（原方案作废，需重新设计）
3. §2.2 View output buffer（收益未证明）
4. §4.5 Statement LRU
5. §4.8 四驱动抽象（与性能改造**分开提交**）
6. §6.5 libcurl 直连 / process-level handle pool
7. §3.2 `getBorrowed()`（依赖 §3.1）
8. **已废弃**：Cache RCU；修改默认缓存哈希算法；OpenSSL EVP 直连

---

## 9. 验证方法（前置任务，需先建设）

### 9.1 现有手段不足
`test/BenchmarkTest.php` 是 `Gene\Benchmark` 的**功能测试**（含大量 `usleep`、打印、普通 PHP 操作），
**不能**用于验证 Router / Memory / PDO 的性能变化。v1 把它当微基准是错误的。

### 9.2 微基准套件要求
- 每个优化项一个**独立**的 C/PHP 微基准；
- 固定 warm-up 轮数；
- 多轮运行，报告 **median、p95、p99、标准差**（不看单次均值）；
- CPU pinning，或至少固定 worker 数与关闭频率调节；
- 基线与优化版使用**完全相同**的 PHP / OPcache / Swoole / 数据库配置与同一次构建工具链。

### 9.3 分场景基准
| 领域 | 场景划分 |
|---|---|
| 路由 | 静态路径 / 单占位 / 多同层冲突占位 / 深层嵌套 |
| Memory | 无业务写（无锁快路径）/ 首次 dirty 之后 / 带 TTL / 高 churn（观察 `cache_insert_refused`）|
| DB 函数派发 | **mock / 本地无网络** 基准（避免真实 SQL 延迟掩盖 CPU 差异）|
| DB 真实 SQL | 单独测吞吐与连接建立成本 |
| 输出 / SSE | 必须测**首字节到达时间与分块到达间隔**，不能只看总 RPS |

### 9.4 压测
Swoole 模式 `wrk -t8 -c1024 -d60s`，关注 RPS 与 p99；FPM 模式同参数对照。
压测期间轮询 `Gene\Monitor::stats()` / `Gene\Memory::stats()`，确认拒绝/超时/sweep 计数为 0。

### 9.5 ASAN 回归（命令需完整）
v1 的 ASAN 命令不完整。正确做法：
1. **configure、编译、链接三个阶段**都要带 sanitizer 参数：
   ```bash
   export CFLAGS="-fsanitize=address -fno-omit-frame-pointer -O1 -g"
   export LDFLAGS="-fsanitize=address"
   phpize && ./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
   make -j"$(nproc)"
   ```
2. 确保 PHP 进程能加载 ASAN runtime（必要时 `LD_PRELOAD=$(gcc -print-file-name=libasan.so)`）；
3. `test/*.php` **不是**一条能自动跑全部测试的命令 —— 使用 `TestRunner.php`，
   并按 `AGENTS.md` 通过 `GENE_TEST_PHP_ARGS` 传入 `-n -d extension=...` 保证子进程加载被测 DLL/so，
   否则会产生假通过/假失败。
