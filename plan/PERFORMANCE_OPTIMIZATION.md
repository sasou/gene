# Gene 扩展并发性能优化 —— 源码复核与分阶段执行方案

> 版本：v3（2026-09-06）。本次依据当前 `src/`、`test/`、`tools/acceptance/` 静态复核，
> 修正 v2 中默认行为变更、生命周期约束、优先级与验证盲区。v2 提到的
> `PERFORMANCE_OPTIMIZATION_CHECK.md` 当前仓库未找到，不作为本版证据。
> 本版只修订方案，未实施 C 代码改动、未运行性能压测；收益均未实测。
> 下文源码路径除特别注明外相对 `src/`，行号为复核时定位参考，以函数名为准。
>
> 执行纪律：
> 1. **收益一律先测后填**。本文档不再保留任何未经基准验证的百分比/倍数；标注 `收益：待测` 的项目
>    必须先产出基线数据才能进入性能改造；正确性修复与文档纠错不受收益门禁限制。
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

### 1.1 【高风险，先验证生命周期】`route_precompile` 默认开关评估
- **位置**：`router/router.c:755-797`（描述符）、`1005-1066`（resolve）、`1364-1392`（缓存键）
- **现状纠正**：hook 解析结果**已经**存入 `gene_route_pc`，v1 所述「待做 hook 预编译」不成立。
  真正待决策的只有三件事：
  1. 默认值是否从 0 改 1；
  2. action 的 `zend_function *` 是否值得进描述符（见 1.2，结论：**暂不**）；
  3. 失效机制。
- **前置硬约束**：`route_pc` 以 leaf `HashTable*` 地址为 key，借用路由树与 `fn_cache` 指针。
  现有 `gene_memory_write_allowed()` 已限制部分冻结后框架写，不能把「没有独立 frozen 字段」
  等同于「没有写保护」；但必须审计所有修改入口与对象生命周期，不能只检查 `bind()`。
- **优先排查的正确性风险**：`Router::clear()` 在底层删除被拒绝后仍释放 `fn_cache`
  （`router.c:3081-3114`），而 `route_pc` 没有同步失效；此外请求级 `fn_cache` 在
  RSHUTDOWN 释放，持久 `route_pc` 到 MSHUTDOWN 才销毁。需分别验证长驻 Swoole worker
  与同进程多次 FPM 请求（显式调用 `workerReady()`）的可达路径，不把两种请求生命周期混同。
- **方案**：拒绝写必须无副作用；若允许更新，先设计 generation/失效与在途 dispatch 生命周期。
  closure 指针优先不缓存或执行时解析；仅销毁整表也可能使在途描述符悬垂，不能机械加 destroy。
  保持默认关闭，先补 closure/clear/重复请求/worker reload 的回归，再决定是否值得开启。
- **收益**：哈希查找与分配节省均待计数、压测。**风险**：高（生命周期）；风险复现与修复优先于提速。

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
  4. 使用独立设置标志区分 unset 与空串，不能统一用 length 判定；保持现有 `char*` 调用边界的 NUL 结尾。
- **额外约束**：URI 并非 module/controller/action 等字段唯一来源，必须支持独立赋值、重复更新、
  显式路由参数与异常清理。若 arena 扩容，所有借用指针都会受影响；使用 offset 的同时审计全部调用者，
  严格证明 backing 生命周期覆盖借用期。该方案先原型测量，不预设为单一 URI buffer 即可解决。
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

### 1.7 【低，先测】局部字符串与函数查找
- `router/router.c:2147/2222/2478` 常量 `strlen(GENE_ROUTER_SAFE)` → `ZEND_STRL` 可统一风格，
  但编译器通常已常量折叠，不计为已证明的运行时收益。
- `factory/factory.c:213-219` 已算 `action_len` 的 `ZVAL_STRING` → `ZVAL_STRINGL`。
- `http/json.c:74/103` 函数查找复用 `GENE_CG_FN_LOOKUP`，保留现有 ZTS 分支和异常传播；
  直调内部 JSON API 则归入 §6.1，不混为同一低风险修改。
- `gene.c:410-435` URI 拼接先统计实际分配，再比较精确长度单次分配与 `smart_str`；
  后者可能扩容，不能直接承诺「单次分配」。

### 1.8 【暂缓】DI alias 注册期展平
- `di/di.c:136-150, 497-510` 当前允许晚注册、重新绑定，解析上限为 8 跳；环不会强制 miss，
  而是使用第 8 跳落点。注册时直接保存最终目标会改变这些行为。
- 保留原始边；若基准确实显示瓶颈，可另做请求/协程级解析缓存，每次 alias 写入递增 generation，
  使全部解析缓存失效，且保留 8 跳语义和调用用户代码前的 owned string。
- 回归覆盖链式晚注册、中间节点重绑、环、超过 8 跳、构造函数内改 alias、协程隔离。
  不进入第一批纯局部优化。

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
  - 使用离线预编译产物：构建期生成实际 app root 下的 `Cache/Views/*.php`，运行时
    `gene.view_compile=0`，且调用方不能传 `isCompile=true`（编译条件是二者逻辑或）。
- **动作**：先修正文档与部署检查。`check_mtime=0` 是保留的合法旧行为，不称为矛盾配置；
  如新增生产诊断，应单独评审并仅在初始化时记录一次，Swoole 使用 `gene_log_diag()`，
  不每请求触发 warning 或用户错误处理器。
- **部署验收**：区分 `display()` 与 `displayExt()`；覆盖缓存缺失、源文件更新、编译失败、
  多 worker 首次编译和 OPcache 更新。离线产物采用发布版本目录/原子切换；mtime 仅比较时间戳，
  不能识别保留旧时间戳的内容更新，也不保证并发写入安全。**优先级**：先纠正文档，代码诊断另评。

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

### 3.1 【高风险，先设计】框架缓存与业务缓存隔离
- **位置**：`cache/memory.h:21-46`、`gene.h:296` `cache_business_dirty`
- **现状**：公开 `Gene\Memory` 写入口和 `Gene\Cache` 业务写都需纳入审计；首次业务写设置
  `cache_business_dirty=1` 后，读（含路由、配置、DI）回到加锁路径，不能只隔离 `Gene\Cache`。
- **方案**：按用途拆为框架表与业务表，分别定义锁与生命周期：
  - 框架表：启动阶段可写，`workerReady()` 后只读，不允许业务覆盖、TTL 或 LRU 淘汰；
  - 业务表：承接公开 Memory/Cache 用户数据，独立锁 / LRU / TTL，允许正常 rehash；
  - 拆表前列出全部 `gene_memory_*` 调用者，显式指定表归属，不仅靠 key 前缀猜测；
    明确既有 Memory 读取框架 key、`clear()`、统计、容量和同名 key 的兼容策略。
- **安全前置**：`gene_memory_get/get_triple` 返回解锁后的裸指针，重新加锁不等于保护借用期。
  业务表必须在锁内完成 owned copy，允许 rehash 前移除其全部借用路径；框架表无锁读取则须
  证明整个借用期无覆盖/删除且销毁顺序安全。单线程不 yield 的路径不能仅因解锁就断言已触发 UAF，
  应分别构造可重入/协程切换与真实共享线程场景复现。
- **v1 遗漏的关键问题（必须一并解决）**：冻结后新 key 插入依赖预留 bucket，而删除留下的 tombstone
  **不会降低 `nNumUsed`**（`memory.c:833-865`）。长期高 churn 即使有 LRU 也会逐步耗尽预留 bucket，
  表现为 `cache_insert_refused` 持续增长。拆表的目标之二就是让业务表摆脱「冻结 + 预留 bucket」模型。
- **表述纠正**：「消除高并发读串行点」在单 Swoole worker（单线程协作调度）下**夸大** ——
  短且不 yield 的 rwlock 临界区主要是固定原子/函数调用开销，未必存在严重线程竞争。真实收益待测。
- **明确移除**：RCU 方案。单 worker 协程模型下收益有限，回收 epoch 与裸指针风险很大。**不做**。
- **风险**：高（公开 API、指针所有权和存储模型同时变化）。先修复可复现的生命周期问题，
  再以独立提交拆表；不因架构更整洁就预设其吞吐收益。

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
- **参考边界**：`gene_memory_get_triple()`（`memory.c:901-938`）仅可参考单锁和 TTL 查找形态；
  它返回裸指针，没有锁内深拷贝、hit/miss 更新或 FPM 延迟删除，不能照搬作为完整实现。
- 过期 key 在读锁内只收集；释放后走写路径重新检查过期状态再删除，禁止读锁内升级写锁。
  锁内避免用户回调/析构；保持 key 转换、重复 key、返回顺序及 Memory 原有值转换语义。
- **收益**：N 次锁 → 1 次，但无锁快路径收益可能很小，大批量持锁时间会增加。
  **风险**：中；测试小/大批量、TTL 边界、覆盖与删除、对象/数组值，并测写方尾延迟。

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
- **动作**：先确认该 HashTable 的真实共享范围、回调重入和锁所有权。若证明 NTS FPM 中无其他线程
  访问该表，可独立评估锁分支，不必为了优化强行让 `workerReady()` 提前返回（会改变初始化语义）。
  ZTS 暂保留锁但仍需说明保护的共享对象，不能仅凭宏名证明线程安全。**风险**：中，收益待测。

---

## 4. 数据库（Pool / PDO / 驱动）

### 4.1 【兼容性评审，不直接改默认】池连接 fetch mode
- **位置**：`db/pool.c:246-285` 未设置 `ATTR_DEFAULT_FETCH_MODE`；四驱动非池路径均设
  `19 => 2 (FETCH_ASSOC)`（mysql:268、sqlite:280、pgsql:276、mssql:264）。
- **后果**：未显式指定模式的池连接可能使用 PDO 默认 `FETCH_BOTH`。数字键与关联键增加 bucket，
  但并不等于值内容、字符串分配或总内存翻倍，须按列类型/宽度测量。
- **方案**：第一步用现有 `options[PDO::ATTR_DEFAULT_FETCH_MODE] = PDO::FETCH_ASSOC` 显式选择，
  不新增开关也不直接改变默认。若决定统一默认，按兼容性变更独立评审；仅在 options 未提供该键时
  补默认，绝不覆盖用户显式 `FETCH_BOTH/FETCH_NUM/FETCH_OBJ`，缺省与已有 options 两分支都覆盖。
- **验收**：比较池/非池 `row/all` 与原始 PDO 借出，验证数字下标、显式 fetch 参数、自定义模式、
  输入配置数组 COW 不被修改；同时核查 `ATTR_CASE/ATTR_ORACLE_NULLS`，不顺手改变其他选项。
- **收益**：待测（峰值内存、分配量、取行耗时）。**风险**：中高（返回结构兼容性），非 P0 提速项。

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
  **须用真实内部 PDO/PDOStatement 的本地 SQLite 基准测量；用户类 mock 仅验证回退，不进第一批。**

### 4.4 【P1，第一批】ORM 通过 `call_user_function` 调用 Db 方法
- **位置**：`orm/meta.c:341-351` `gene_orm_db_call`（`ZVAL_STRING(&fname)` + `call_user_function`），
  被 `orm/model.c`、`orm/query.c` 数十处调用。
- **可行性**：Gene 的四个 Db 类为 **final**，按精确 CE 缓存函数指针相对安全（优于 4.3）。
- **方案**：参考 `gene_orm_db_kind()` 的精确 CE 判断，缓存对应类的 public 方法，
  命中才走 `zend_call_known_function`；非精确 CE、自定义 Db/mock 保留 `call_user_function`。
  保持参数、返回值初始化、异常和失败状态语义，不长期缓存用户类函数指针。
- **验证**：PHP mock 只验证回退路径，不能证明 final Db 快路径提速。用真实 `Gene\Db\Sqlite`
  的内存数据库验证全链路，并单独重复测量内部 Db 链式构建调用，分离 SQL 执行与派发成本。
- **收益**：待测；多次调用可能累计节省，但不预断言优于 §4.3。**风险**：低-中。

### 4.5 【暂缓】预处理语句 LRU 复用
- **位置**：`db/pdo.c:856-866`、`db/mysql.c:349/365`
- **收益表述纠正**：在 native prepares 下只省掉**重复 SQL 的 server prepare 往返**，
  **不省 execute 往返**（v1「省一次 DB 往返」表述含糊）。
- **适用前提**：SQL 高度重复、同一物理连接、正确关闭 cursor、连接重连后全部失效、
  控制服务端 prepared statement 数量、处理 DDL/`SET`/驱动差异。
- **风险**：高。**排期**：独立设计，不依赖 §4.8 全面去重；先在公共 helper 中验证生命周期与收益。

### 4.6 【暂缓，API 专项】SQL 片段属性中转
- **位置**：`db/mysql.c:158-173, 301-343, 1755-1767`；四驱动同构。
- SQL/where/data 等属性是 public；用户可读取、写入、取引用。只在 C 侧维护字段会使公开属性与
  实际执行 SQL 不一致，不能宣称「对外 API 不变」，仅在 execute 时写回也不够。
- 如保留优化，须先设计属性读写/引用同步、clone/析构/GC、异常和 reset 规则；否则作为新 API
  或版本迁移项目。优先测量属性访问成本，不承诺固定节省次数。**风险**：高。

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
- **方案**：先核对受支持 Swoole 版本的 Channel timeout 语义，不能直接把 `0.001` 改 `0`
  并假设非阻塞。候选为已有 `rpool_channel_is_empty()` 判空后，仅非空时取出；必须证明判空到
  pop 间无 yield/可重入抢占，并覆盖关闭队列、取消、无效元素、创建失败和饱和等待。
- 保留总等待预算，避免重试反复重置超时。已有 `rpool_fill()`，预建 min 先核对构造/填充路径，
  不重复实现；是否异步预热是单独策略。
- **收益**：空闲 miss 可引入约 1 ms 的定时等待，但受调度影响，不是固定成本；
  只有该路径占足够比例才可能改善 p99。测低负载命中、扩容突发、满池三类场景。

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
- **再次纠正**：`gene_session_auto_cookie()`（`session.c:593-600`）已经首先检查 `cookie_sent`
  并早返回。在 `set()/del()` 外层重复预检并不能省掉该属性读取，从待执行项删除。
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
- **现状**：源码注释已记录内部 API 兼容性与函数指针缓存的权衡；注释中的调用耗时估计
  不是本次基准结果，不能据此判断收益。
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
- **可选配置评估**：`CURLOPT_TCP_KEEPALIVE` 用于空闲连接探测，不等同于 HTTP 连接复用，
  不解决每请求初始化成本；按实际 libcurl/系统支持验证，不作为立即提速项。

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

以上为示例而非通用容量配置：按 worker 峰值活跃上下文、ctx pool 实际命中率和 RSS 预算调整。
`co_contexts_max` 是触发 sweep 的软阈值，不是最大并发准入限制；自动 cleanup 是兜底，
需验证 defer 可用与应用清理路径。离线视图仍须避免调用方显式 `isCompile=true`。

运行期使用同一 worker/PID 的区间增量与每请求比率，而不是累计值是否增长：
- `cache_insert_refused`：正常容量负载应无新增；容量不足或 tombstone 耗尽都可能导致拒绝。
- `db_pool_get_timeout`、`redis_pool_cas_abandoned`：结合注入失败/饱和测试的预期判断，不能一概归因配置。
- `co_contexts_sweep_count`、`ctx_pool_miss`：可正常增长，结合 sweep 耗时、存活上下文、RSS 与延迟判断。
- 后续可补 `cache_business_dirty`、`nNumUsed/nNumOfElements/nTableSize` 与 route_pc hit/miss 指标；
  这些是待新增观测项，不假定当前 `stats()` 已提供。

---

## 8. 落地顺序（按证据与风险门禁）

### 第零批：基线与正确性，不等待性能收益证明
1. §9 复用现有验收入口，补真正命中优化路径的微基准，保存版本与原始结果。
2. §2.1 修正文档/部署检查；新增运行时诊断另评，不直接加入每请求 warning。
3. §1.1 / §3.1 补 route_pc、clear、借用指针与 churn 最小复现；已复现的内存安全或数据正确性
   问题作为修复单独处理，不绑在拆表或默认开关调整上。

### 第一批：低风险候选，基线通过后逐项提交
| 项目 | 准入条件与验收 |
|---|---|
| §1.2 action 单次查找 | 保留方法可见性、未命中与异常行为，不跨 dispatch 缓存 |
| §1.7 字符串长度/Json 函数缓存 | 排除 DI 展平和内部 JSON API 直调；证实确有运行时工作可省 |
| §4.4 ORM known-function | 精确 CE + 非 Gene Db 回退；真实内部 Db 测快路径，mock 测兼容性 |
| §5.7 Benchmark C API | 保持单位、精度、start/end 和峰值内存语义；外部 hrtime 基准不依赖此改动 |

### 第二批：专项验证通过后实施
1. §3.3 `mget()` 锁内复制与批量读，补删除锁序和大批量尾延迟验证。
2. §5.2 RedisPool 消除空队列定时等待，先验证 Channel 版本语义和总超时。
3. §1.5 Webscan 去对象化，保持扫描顺序、配置类型转换、异常与拦截结果一致。
4. §1.4 ctx arena，核对 unset/空串、NUL 结尾、字段独立更新与容量增长后指针稳定性；
   不能把 `length==0` 当成通用 unset，记录峰值协程数 × ctx/pool 常驻内存。
5. §5.6 仅日志时间格式缓存，处理时区变化；缓冲/输出后端另立项。
6. §6.1 JSON / §6.2 编译标志：仅在 profiling 支持时试验，不预设收益。

### 架构/API 专项：不混入透明优化批次
- §3.1 拆表 → 明确公开 Memory 兼容契约与所有权 → 再评估 §3.2 借用读。
- §1.1 route_precompile：先完成生命周期修复与模式矩阵，不预定默认开启。
- §4.1 fetch 默认、§4.2 持久连接、§4.6 SQL public 属性、§5.1 Pool 生命周期限制，分别评审。
- §1.3 路由索引、§1.8 DI 解析缓存、§1.6 响应缓冲、§2.2 View buffer、§4.5 Statement LRU、
  §4.7 探活/定时器、§4.8 驱动去重、§5.4 MemcachedPool、日志缓冲、Session ID 随机源均独立立项。
- §6.5 HTTP 连接复用、OpenSSL 直连暂缓；Cache RCU 与无兼容迁移的默认哈希切换不采用。
- 每项单独提交与回滚；有运行时开关的项目先 opt-in 灰度，无开关的局部改动回滚对应构建产物。

---

## 9. 验证方法（前置任务，需先建设）

### 9.1 复用现有手段，不从零重复建设
- `test/BenchmarkTest.php` 是功能测试，不是 Router / Memory / PDO 性能基准。
- `tools/acceptance/swoole_benchmark.php` 已有 getcid/route_precompile 四组合摘要一致性检查，
  主要是正确性矩阵，不等于 HTTP 性能测量。
- `tools/acceptance/fpm_benchmark.php` 可执行多轮外部 benchmark_command，但 warmup 目前是 sleep，
  并不实际预热目标；须由压测命令/前置步骤发送真实请求。
- `tools/acceptance/linux_swoole_verify.sh` 已有构建、协程/池验收、HTTP 压测和 RSS 采样入口，
  优先扩展；先检查参数与依赖，性能测试显式使用生产 run_environment（脚本默认值为 0）。
- `audit/repro/` 已有 Swoole 缓存 UAF、workerReady、路由复现，可用作回归起点，不能替代性能基准。

### 9.2 微基准套件要求与准入标准
- 每项独立基准，断言结果正确并确认命中被优化分支；冷热启动分别记录，不把 PHP mock 当内部 CE 快路径。
- 固定真实 warm-up 工作量；交替 A/B 与 B/A、多独立进程多轮运行，记录样本量与原始输出。
- 微基准报告批次 ns/op 的 median、离散程度/置信区间、分配次数和内存；少数轮次的均值分布
  不能冒充单请求 p99。p95/p99 用足够请求样本的端到端延迟分布计算。
- 固定/记录 CPU、亲和性、频率策略、worker 数、PHP/扩展/OPcache/JIT、数据库、编译命令与提交 ID；
  Windows NTS 能验证局部功能，但不能替代 Linux Swoole、FPM 跨请求或 ZTS 相关路径。
- 性能结果必须可重复且超过测量噪声；每项测试前定义业务 SLO 和可接受 CPU/RSS/尾延迟回归预算。
  无显著收益则不合入性能改动；正确性修复不受收益门禁限制。

### 9.3 分场景基准
| 领域 | 场景划分 |
|---|---|
| 路由 | 静态路径 / 单占位 / 多同层冲突占位 / 深层嵌套 |
| Memory | 无业务写（无锁快路径）/ 首次 dirty 之后 / 带 TTL / 高 churn（观察 `cache_insert_refused`）|
| DB 函数派发 | 真实内部 Db/PDO 本地基准测快路径；PHP mock 仅测回退兼容性 |
| DB 真实 SQL | 单独测吞吐与连接建立成本 |
| 输出 / SSE | 必须测**首字节到达时间与分块到达间隔**，不能只看总 RPS |

### 9.4 压测与回归矩阵
- 并发从低负载逐级增加到饱和（例如 1/32/128/512/1024），固定 worker/CPU 资源并记录压测端瓶颈。
  `wrk --latency -t8 -c1024 -d60s "$TARGET_URL"` 仅作单档示例，TARGET_URL 由实际环境提供；
  单次 60 秒与闭环 wrk 不能充分证明尾延迟，另以可控到达率测试过载/排队行为。
- 同时记录成功 RPS、错误率/超时、p50/p95/p99、CPU/请求、worker RSS 与池等待；
  正常负载拒绝应无新增，故障/饱和注入按预期计数，sweep/miss 不要求为零。
- 路由覆盖 closure/hooks、404、冻结后 clear、重复 workerReady、协程交错；缓存覆盖 TTL、
  churn、数组/对象转换、写后读取；DB 覆盖用户 options、懒执行、事务、异常和非 Gene Db 回退。
- FPM 要在同一 worker 内连续请求；Swoole 要覆盖多 worker、清理与 reload；CLI 进程隔离测试不能替代它们。
  各模式先各自 A/B，再做模式比较，不把运行模型差异当成本项优化收益。

### 9.5 ASAN 回归（Linux 独立构建，不用于性能测量）
1. 使用干净的独立构建树及匹配的 phpize/php-config，避免旧对象未重编译；在其 `src/` 执行：
   ```bash
   export CFLAGS="-fsanitize=address -fno-omit-frame-pointer -O1 -g"
   export LDFLAGS="-fsanitize=address"
   phpize && ./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
   make -j"$(nproc)"
   ```
2. 优先使用同样启用 ASAN 的 PHP；仅检测扩展时也须确保匹配的 runtime 最先加载。
   GCC 构建必要时设置 `LD_PRELOAD="$(gcc -print-file-name=libasan.so)"`；Clang 使用匹配 runtime，
   不混用工具链。设置 `USE_ZEND_ALLOC=0` 使 Zend 请求分配进入系统分配器，提升 UAF 可检测性。
3. 在仓库根目录，按实际构建路径运行（所需 PDO/SQLite 等依赖可能还需显式加载）：
   ```bash
   export USE_ZEND_ALLOC=0
   export GENE_TEST_PHP_ARGS="-n -d extension=$PWD/src/modules/gene.so"
   php -n -d "extension=$PWD/src/modules/gene.so" --ri gene
   php -n -d "extension=$PWD/src/modules/gene.so" test/TestRunner.php
   ```
   构建路径含空格时，在 `GENE_TEST_PHP_ARGS` 内也须保留 shell 引号。验证主进程与子进程加载相同产物，
   并检查 skip/未覆盖测试，而不只看退出码。Swoole 专项必须另外加载匹配 Swoole 并运行对应脚本。
4. ASAN 通过只能说明已执行路径未检出内存错误，不证明线程安全或并发覆盖完整。
   生命周期变更必须额外做同进程重复请求与长时间 churn/RSS 趋势回归；性能测试使用正常 release 构建。
