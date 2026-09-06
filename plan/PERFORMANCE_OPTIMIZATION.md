# Gene 扩展极致并发优化 —— 源码复核与分轨执行方案

> 版本：v6（2026-09-06）。**v6 是围绕「极致并发」的重新整理，不是 v5 的增量修订。**
>
> 重整理的直接原因：v3–v5 遗留的**优化准入机制本身经过验证后被证明是限制项**。
> 具体地，v4 的「`gene.so` self on-CPU < 10% 即停止 C 层优化」和 v5 保留的
> 「§9.2 至少一个预声明主指标须稳定超过噪声才可合入」两条准入，均以
> **单点负载的平均量级**为判据。而 §10.4 实测的 6.15% / 1.89% 是在
> **2 threads / 32 connections** 下取得的 —— 这个参数下几乎所有并发放大成本
> （锁、原子、协程上下文、池排队、每协程内存）都还没进入放大区间。
> 用它做准入，等于**系统性地筛掉了全部只在高并发下显形的优化**，
> 而那恰好是本项目唯一值得做的一类优化。
>
> **v6 的准入原则改为「按伸缩性维度准入」**（见 §0.2）：
> 判据不是「在某个负载中占比多少」，而是「该成本随并发/协程数/路由规模/churn/池等待者数
> 如何增长」。平均占比只用于**排期**，不用于**准入**。
>
> v6 保留 v3–v5 全部经过源码复核的事实纠正与生命周期约束（这些是真实资产），
> 但重新分类：把「随规模放大」「串行化点」「每并发单位内存」「尾延迟」抽为一等公民，
> 把纯固定成本微优化降为背景任务。
>
> 本轮同时新增 v5 未立项的四类并发问题（§1.4、§2.1、§2.2、§3.4），
> 均来自 2026-09-06 的源码审计。行号为复核时定位参考，以函数名为准；路径相对 `src/`。
>
> 执行纪律（v6）：
> 1. **收益一律先测后填**，但基线必须建在**并发梯度**上（1/32/128/512/1024+），
>    单档结果不能作为收益或否决依据。
> 2. **profiling 只排期，不否决**。任何被判定为「随规模放大」的条目，
>    即使在当前负载 flamegraph 中不可见，也照常推进。
> 3. **正确性缺陷不受任何性能准入约束**（§1 全节）。
> 4. 涉及生命周期/裸指针/跨线程共享的改动，Linux `-fsanitize=address` 下跑全量回归（§7.4）。
> 5. 改变**公开 API 语义**的项目单独立项、单独评审，不混入透明优化批次。

---

## 0. 并发成本模型与准入原则

### 0.1 先确定「极致并发」在本项目里的具体含义

Gene 的目标运行模型是 **Swoole 常驻 worker + 协程**（`gene.runtime_type = 2`）。
在该模型下，单 worker 是**单线程协作调度**，因此「并发」放大的不是线程竞争，而是四类东西：

| 放大维度 | 随什么增长 | 直接体现 |
|---|---|---|
| **D1 每协程固定成本** | 活跃协程数 | CPU/请求、上下文查找命中率 |
| **D2 串行化 / 不可让出区间** | 并发请求数 × 临界区长度 | p95/p99、调度延迟 |
| **D3 每并发单位内存** | 峰值活跃协程/连接数 | worker RSS → 可承载并发上限 |
| **D4 排队与饱和行为** | 到达率相对容量 | 超时率、p99、饱和点位置 |

**多 worker / ZTS / fork 共享**只影响两处：进程缓存 rwlock（`gene.h:39-57`）和
`Swoole\Atomic` 计数（可能被 `Server::start()` 前构造的 Pool 经 fork 共享）。
除此之外 `src/` 内**没有任何** `__atomic` / `pthread_mutex` / spinlock / 信号量
（已全库 grep 确认）。因此 v3–v5 中「消除高并发读串行点」一类表述必须按 D2 重新表达：
真实成本是**固定原子/函数调用开销 + 不可让出区间长度**，不是线程争用。

### 0.2 准入原则（替代 v4 的 10% 门槛与 v5 的主指标门槛）

一个条目可以进入实现队列，只要满足**任一**条：

- **A. 正确性/可用性缺陷**：长跑会失效、会漂移、会悬垂、会静默拒绝服务。
  无需任何基准，最高优先级。
- **B. 属于 D1–D4 中某一维度，且可给出增长论证**：
  说明「该成本正比于 X」并给出 X 在目标负载下的取值范围。
  验收在**该维度自身的指标**上做（例如 D3 只需证明 `结构体增量 × 峰值协程数` 的 RSS 变化），
  不要求端到端 RPS 超过噪声。
- **C. 纯固定成本微优化**：只有在**风险低、diff 小、无语义变化**时才合入，
  且必须标注为「专项路径收益」，不得包装成整机收益。这类条目是背景任务，不占排期。

**明确废止**：
- v4 的 `gene.so >= 10%` 门槛；
- v5 §9.2 中「至少一个预先声明的主指标须稳定超过噪声」作为**准入**的用法
  （该要求降级为**收益陈述规范**：达不到就只能按 C 类标注，不是不许合入）。

**明确保留**：
- 所有回归预算（不得以 CPU 收益换 RSS/并发恶化，反之亦然）；
- A/B 双侧宿主配置必须完全一致（§6.2）；
- 大重构与性能优化分开提交。

### 0.3 已有优化基线（不得重复提案）

| 领域 | 已完成 | 位置 |
|---|---|---|
| 请求上下文 | `gene_request_context` 内联 `path_params`、struct 池复用（`ctx_pool`）、`co_contexts` 冷却式 sweep、`vm_stack` 同协程快路径跳过 `getcid()`、CID 复用时校验表指针身份 | `gene.c:767-905, 1123-1250` |
| 协程 ID | `dlsym` 直调 Swoole C-API `get_current_cid()`（`gene.swoole_getcid_capi=1`，默认开） | `gene.c:175-213` |
| 路由 | 预编译 dispatch 描述符 `route_pc`（`gene.route_precompile`，默认关，且要求 `runtime_type>=2 && worker_ready`）；`Router::run()` 三键单锁读 `gene_memory_get_triple()` | `router.c:759-794, 1009-1073, 2072-2076` |
| 类/函数查找 | `gene_lookup_class_str` 栈上小写缓冲直查 `EG(class_table)`；`GENE_CG_FN_LOOKUP` 缓存内部函数指针 | `gene.c:259-289`、`gene.h:73-90` |
| 进程缓存 | `workerReady()` 冻结前 `zend_hash_extend()` 预留 bucket（保持 `arData` 不动）+ 条件跳锁读；标量 `ZVAL_COPY_VALUE`；对象/资源在任意深度被拒 | `memory.h:27-30`、`memory.c:288, 716-758, 772-794` |
| 模板编译 | 编译结果以 `zend_string*` 流转；`view_compile_check_mtime` 增量重编译（v5 起默认 `1`） | `view.c:52-72, 610-630` |
| 视图渲染 | `render()` 每次调用**恰好一层** `php_output_start_default`；`display()` 完全不建 buffer；符号表用 COW `ZVAL_COPY` | `view.c:90, 736-799, 819-860` |
| 日志 | 级别过滤在任何分配之前；空 `context` 跳过 JSON 编码 | `log.c:177-238` |
| Session | `set()/del()` 仅置 dirty，`save()`/析构一次写后端；cookie `cookie_sent` 去重 | `session.c:593-600, 947-986, 1034-1055` |
| ORM/dispatch | 精确内部 CE 走 `zend_call_known_function`（§8.2 第二批） | `orm/meta.c`、`router.c:392-480` |
| 观测 | `Memory::stats()` / `Monitor::stats()` / `Monitor::prometheus()` 已导出 25+ 计数器（清单见 §5.1） | `memory.c:1915-1954`、`monitor.c:111-194, 309-341` |

---

## 1. 并发正确性缺陷 —— 无准入门槛，最高优先级

这一节的条目**全部**属于 §0.2 的 A 类：长跑 worker 在高并发下会失效、漂移或悬垂。
它们不是性能条目，但**在修完之前，高并发压测结果不可信**，因此排在所有性能轨道之前。

### 1.1 【已确认缺陷】`Router::clear()` 不失效 `route_pc`，留下悬垂描述符
- **位置**：`router/router.c:3057-3119`（`clear()`）、`759-794`（描述符构造与
  `gene_router_pc_destroy()`，仅在 MSHUTDOWN 调用）、`1009-1073`（`gene_route_pc_execute()`）
- **源码事实**（v5 只列为「优先排查的风险」，现已确认）：
  `clear()` 删除持久缓存中的 `:rt/:re/:cf` 三个键并**销毁 `fn_cache`**，
  但**不触碰 `GENE_G(route_pc)`**。而描述符里保存的是
  ① 借用自 leaf 路由数组的 `const char*`；② 借用自 `fn_cache` 的 closure zval
  （`fn_cache` 持强引用，`clear()` 后引用被释放）；只有 `eval_str` 是 owned。
  → `route_precompile=1` 的 worker 中调用 `Router::clear()`，后续命中同一 leaf 地址的
  dispatch 会读已释放内存。key 是 `HashTable*` 地址，**地址复用**会让命中概率非零。
- **修复方案（按侵入性递增）**：
  1. `clear()` 内同步销毁整个 `route_pc`（最简单，但需证明**无在途 dispatch**
     正持有描述符 —— 协程模型下 `clear()` 与 dispatch 可交错）；
  2. 引入 generation：`clear()` 递增 `route_pc_generation`，描述符携带生成号，
     `execute()` 前比对，不匹配则回退 `get_router_info_slow()`；旧描述符延迟回收。
     **推荐此方案** —— 它同时解决了在途借用问题；
  3. 彻底不缓存 closure zval，执行时解析。
- **验收**：Swoole 多 worker 下 `route_precompile=1` + 反复 `clear()` + 并发 dispatch，
  ASAN 无报错；`route_pc_items` 在 `clear()` 后归零或生成号推进。
  同时覆盖 FPM 同进程多次请求（显式 `workerReady()`）。
- **附带**：`route_pc` 只在 `worker_ready` 后启用，但请求级 `fn_cache` 在 RSHUTDOWN 释放，
  持久 `route_pc` 到 MSHUTDOWN 才销毁 —— 两种生命周期必须分别验证，不可混同。

### 1.2 【已确认缺陷】池计数器 CAS 放弃导致单调漂移
- **位置**：`db/pool.c:558-590`（`pool_decrement_count_cas`，64 轮 `cmpset` 后放弃并
  递增 `db_pool_cas_abandoned`）、`676-687`（`pool_increment_count_get`）；
  `cache/redis_pool.c` 同构
- **问题**：放弃后计数器**永久偏高**。`Pool::get()` 依据 `new_count <= max` 决定
  「创建新连接」还是「阻塞等待」（`pool.c:806-908`），计数偏高会让池**提前进入等待分支**，
  最终表现为可用容量单调收缩、`db_pool_get_timeout` 增长、p99 恶化。
  高并发正是 CAS 冲突概率最高的场景 —— 这是一个**在并发放大下自我恶化**的缺陷。
- **定性**：功能缺陷。`db_pool_cas_abandoned` 目前被当作观测项，实际上它非零即意味着
  容量已失真，不能只监控不修。
- **修复方向**：
  1. 放弃后**不能只计数**：至少要标记池计数为「不可信」并触发一次基于
     `Channel::length()` 与实际连接数的重同步；
  2. 更彻底：不再用「读-改-写 CAS」维护 count，改为
     `Atomic::add(+1)` / `Atomic::sub(1)` 的**无条件对称操作**
     （`Swoole\Atomic` 的 add/sub 本身是原子的，CAS 循环之所以存在是为了
     实现「不超过 max」的条件递增 —— 该条件判断应移到 Channel 容量或独立信号量上，
     而不是靠 CAS 重试）；
  3. 提高轮数或退避只是掩盖，不采纳为唯一措施。
- **前置约束**：Pool 可能在 `Server::start()` 前构造从而经 fork 跨 worker 共享
  （§3.4 有专项约束）。修复方案必须在「跨进程共享」与「worker-local」两种情形下都正确。
- **验收**：高并发借还压测（512/1024 并发、突发扩容 + 满池）下断言
  `db_pool_cas_abandoned == 0`，且 `stats()` 的 `total` 与实际连接数一致；
  长跑后容量不收缩。

### 1.3 【已确认缺陷，v5 §3.1b 延续】冻结表 tombstone 耗尽导致静默拒写
- **位置**：`cache/memory.c:841-845, 1644-1647, 1713-1716`（insert guard +
  `cache_insert_refused`）、`716-728`（`gene_cache_effective_reserve()`）、`744-758`（reserve）
- **问题**：冻结后新 key 插入依赖预留 bucket，删除留下的 tombstone **不降低 `nNumUsed`**。
  高 churn（写-删-写不同 key）会**单调**耗尽预留 bucket，最终所有新业务 key 被静默拒绝。
  注意 `cache_max_items` 默认为 **0**（业务项无上限、LRU 不启用），
  因此默认配置下**没有任何机制**回收 tombstone。
- **并发相关性**：churn 速率正比于并发与 QPS → 耗尽时间随并发线性缩短。
  这不只是「长跑问题」，是**并发放大的可用性问题**。
- **已完成（2026-09-06 首轮）**：候选 3 的观测部分 —— `cache_num_used`、
  `cache_num_elements`、`cache_table_size`、`cache_insert_refused` 已导出。
- **待做**：
  1. **插入时复用 tombstone**：冻结表插入前定位可复用的已删除 bucket，命中即原地复用，
     不增长 `nNumUsed`。硬约束：不得移动 `arData`、不得改变无锁读者可见的 bucket 顺序、
     复用前必须确认该 bucket 无在途借用（与 §2.3 借用读不变式一起论证）；
  2. 运维兜底：显式的业务表重建/清空入口（注意 `Memory::clean()` 在 `workerReady()` 后
     被**拒绝执行**，`memory.c:1869-1891` —— 兜底入口必须是新 API，不能复用 `clean()`）；
  3. 有界压实：**风险高**（移动 `arData`），仅在 §3.1 拆表方案确定不采纳时才评估。
- **不得被 §3.1 拆表阻塞**。拆表是高风险架构项，把本缺陷挂在它后面等于无限期推迟。
- **验收**：持续写删不同 key 超过 `reserve` 数倍，断言 `cache_insert_refused`
  **不随时间单调增长**且 `nNumUsed` 有界；配合 ASAN 与 RSS 趋势。

### 1.4 【新立项，ZTS/多线程正确性】文件作用域 `static` 缓存不是线程局部
- **位置**（已全库审计）：
  - `http/response.c:171-180`（`gene_swoole_resp_cache_*`）、`response.c:557`（`json_fn`）
  - `db/pool.c:62`（`gene_pool_named_cache`）、`190-193`（池方法 `zend_function*`）、
    `116-117, 155-156`（timer 函数指针）
  - `cache/redis_pool.c:51, 88-91`（同构）
  - `cache/cache.c:37-98`（static interned 方法/函数名字符串）
  - `router/router.c:1619`（`gene_closure_src_cache`，持久表，`closure_src_cache_max`
    默认 1024，满则整表 flush）
  - `cache/redis.c:603-604`、`http/webscan.c`、`tool/language.c`
- **两类问题**：
  1. **惰性初始化竞争**：`GENE_G` 在 ZTS 下是线程局部，但这些 `static` 是普通 C 全局，
     初始化无原子保护。ZTS 多线程 SAPI 下同时首次进入即为数据竞争
     （写 `zend_function*` / `zend_class_entry*` 指针）。
  2. **interned string 悬垂**：`GENE_INTERNED_STR()`（`gene.h:523-525`）经
     `gene_interned_str_persistent()` 只在 `IS_STR_PERMANENT` 时保留指针，其余情况失效缓存 ——
     **这是正确做法**。但部分文件（如 `cache/cache.c`）仍把
     `zend_string_init_interned(..., 1)` 的结果直接存进 `static zend_string*`。
     在 `opcache.file_cache_only=1` 或 opcache 关闭时，这些 interned string
     可能只有请求生命周期，跨请求保留即悬垂。
- **动作**：
  1. 先做**范围界定**：明确 Gene 是否支持 ZTS 多线程 SAPI。若**不支持**，
     在文档与 `MINIT` 中显式声明/检测并拒绝加载，本项的第 1 类问题即关闭
     （这是最省事且诚实的处理，优于给每个 static 加锁）；
  2. 无论 ZTS 结论如何，第 2 类问题**必须修**：把所有 `static zend_string*` 缓存
     统一改走 `gene_interned_str_persistent()` / `GENE_INTERNED_STR()`，
     并在 opcache 关闭、`file_cache_only=1` 两种配置下回归。
- **风险**：低（第 2 类为机械替换）；第 1 类的处理是决策而非编码。

---

## 2. 轨道 A：D1/D2 —— 每协程成本与不可让出区间（并发放大的核心）

这是 v6 的**主轨**。判据是「成本 ∝ 活跃协程数」或「成本处于不可让出区间内」。
不要求这些条目出现在 32 连接负载的 flamegraph 中。

### 2.1 【新立项，最高性能优先级】池借还路径上的 PHP 方法调用
- **位置**：`db/pool.c:558-590, 676-687, 806-976`；`cache/redis_pool.c:1214-1389`
- **源码事实（v5 完全未立项）**：池的并发原语**不是** C 原子或 C 队列，而是
  **`Swoole\Atomic` 与 `Swoole\Coroutine\Channel` 的 PHP 对象方法调用**。
  一次 `get()` + `put()` 在正常路径上至少包含：
  `Channel::pop` → `Atomic::add` →（等待路径再一次 `Channel::pop`）→
  `Channel::push` → CAS 循环内**若干次** `Atomic::cmpset`（最多 64 次）。
  每一次都是完整的 zend call frame + zval 装拆。
- **为什么这是 D1/D2 双重命中**：
  - D1：每次数据库/Redis 操作都借还一次，成本 ∝ 请求数 × 每请求 DB 操作数；
  - D2：CAS 循环在竞争下重试次数随并发上升，且 `Atomic` 操作本身是跨 worker 共享内存上的原子操作。
  在 32 连接下这被 SQL 网络时间完全掩盖（§8.4 的 1.89%），但在
  「本地 SQLite / Redis 命中 / 高 QPS 小查询」场景中它是**主成本之一**。
- **方案（分级，先做低风险级）**：
  1. **缓存方法指针**：`pool.c:190-193` 已缓存部分池方法 `zend_function*`；
     扩展到 `Atomic::add/sub/get/cmpset` 与 `Channel::push/pop/length`，
     统一走 `zend_call_known_function`。注意这些是 **Swoole 的 CE**，
     必须按精确 CE 严格相等判断，且不得跨请求持有用户子类的函数指针
     （与 §1.4 的 static 生命周期约束一起处理）。
  2. **减少调用次数**：与 §1.2 的修复合并 —— 用无条件 `add/sub` 替代 CAS 循环，
     直接消掉最多 64 次 `cmpset`；容量上界改由 Channel 容量表达。
  3. **直调 Swoole C-API**：仿照已有 `swoole_getcid_capi` 的 `dlsym` 模式，
     探测 Swoole 的 Channel/Atomic C 接口。**收益最大但依赖最强**，
     必须有 dlsym 失败回退到 PHP 调用的完整路径，并按 Swoole 版本矩阵验证。
     **单独立项，不进第一批。**
- **专项基准**：本地 SQLite / 本地 Redis，1/32/128/512/1024 并发，
  测每操作 ns、`cas_abandoned`、p99；分离「借还开销」与「SQL 执行」。
- **风险**：1 低；2 中（与 §1.2 同批，需容量语义评审）；3 高（外部 ABI）。

### 2.2 【新立项】`Pool::get()` 空闲队列 miss 的 1 ms 定时等待（原 §5.2 升级）
- **位置**：`db/pool.c:806-908`、`cache/redis_pool.c:1214-1311`
- **源码事实**：正常路径是 ① `pop(0.001)` 非阻塞尝试 → ② 未满则创建连接 →
  ③ 已满则以 `waitTimeout`（默认 3.0）阻塞 pop → ④ 超时则创建**溢出连接**并递增
  `db_pool_get_timeout`。准确描述是「**每次空闲队列 miss 多出约 1 ms 的定时等待**」，
  不是 v1 所称的「`max+2` 次忙等」。
- **为什么升级为 D2/D4 主项**：1 ms 是**固定加在关键路径上的调度延迟**。
  在池未预热、突发扩容或池容量略小于并发时，miss 比例可以很高 →
  直接抬高 p95/p99。它在低并发平均值里几乎不可见，正是被旧准入筛掉的典型条目。
- **方案**：
  1. 先核对**受支持 Swoole 版本**的 Channel timeout 语义，
     不能直接把 `0.001` 改 `0` 并假设非阻塞；
  2. 候选：用已有的 `rpool_channel_is_empty()` / `Channel::length()` 判空，
     仅非空时才 pop。**必须证明判空到 pop 之间无 yield / 无可重入抢占**
     （协程模型下这是可论证的，但要写清楚），并覆盖：队列关闭、取消、
     无效元素、创建失败、饱和等待五种路径；
  3. 保留**总等待预算**，避免重试反复重置 `waitTimeout`；
  4. 预建 min 连接：已有 `rpool_fill()` / `pool_recycle_idle()` 的 refill 路径
     （`pool.c:720-804`），先核对是否已覆盖首次预热，不重复实现。
     是否**异步**预热是单独策略。
- **验收**：低负载命中、扩容突发、满池排队三档分别测 p50/p95/p99 与 miss 率。

### 2.3 【D2 核心】进程缓存读路径：条件跳锁的覆盖率问题
- **位置**：`memory.h:27-30`（`GENE_CACHE_RDLOCK` 在
  `worker_ready && !cache_business_dirty` 时跳锁）、`gene.h:39-57`（`gene_rwlock_t`，
  Windows `SRWLOCK` / 其余 `pthread_rwlock_t`）、`memory.c:869-895, 913-938, 952-997`
- **源码事实（必须纠正一切「Gene 缓存读是无锁的」表述）**：
  - 无锁快路径**只在 Swoole 且业务从未写过缓存时成立**。
    `Memory::set/del/incr/decr/rateLimit/lock/unlock/mset` 任一次调用
    经 `GENE_CACHE_LAYER_MEMORY_WRITE_ENTER/LEAVE` 置 `cache_business_dirty=1`
    （`memory.c:1248-1250, 1426-1428, 1528-1530` 等），此后**框架读（路由/DI/配置）
    全部退回加锁路径**，且**没有回到 clean 状态的路径**；
  - **FPM 下 `worker_ready` 虽会被置 1**（`application.c:1383-1384`，
    v1 所称「FPM 永为 0」不成立），但 FPM 每请求重新 GINIT，
    实际效果是绝大多数读仍走锁。
- **因此真正的 D2 问题是**：「只要应用用了一次 `Memory::set`，
  框架元数据读就永久变成带锁读」。这是一个**开关式的性能悬崖**，
  不是渐进退化 —— 且几乎所有真实应用都会踩到。
- **方案（这是 §3.1 拆表的真正动机，比「架构更整洁」有力得多）**：
  见 §3.1。在拆表落地前，**不要**为了绕过 `cache_business_dirty`
  去缩小写入口的标记范围 —— 标记范围小了就等于把 §2.4 的 UAF 放回来。
- **本项自身可做的低风险事**：新增计数器区分
  「跳锁读次数 / 加锁读次数 / 锁等待时长」，让悬崖**可被观测**（§5.2）。

### 2.4 【D2，高风险，先设计】`mget()` 逐 key 加读锁
- **位置**：`memory.c:1782-1824`
- **方案**：一次 `GENE_CACHE_RDLOCK()` 内遍历全部 key，结束后一次解锁；锁内禁止 yield。
- **必须保留的语义**（v1 遗漏）：
  1. TTL 判断（过期项返回 miss）；
  2. **深拷贝必须在释放锁之前完成**；
  3. FPM 下的延迟删除行为一致（注意 `memory.c:882-889` 的 lazy delete
     **仅在未冻结时**发生）；
  4. hit/miss 计数语义不变。
- **参考边界**：`gene_memory_get_triple()`（`memory.c:913-938`）只能参考
  「单锁 + TTL 查找」形态 —— 它返回**裸指针**，没有锁内深拷贝、没有 hit/miss 更新、
  没有 FPM 延迟删除，**不能照搬**。
- 过期 key 在读锁内只**收集**；释放后走写路径重新检查过期状态再删除，
  禁止读锁内升级写锁。锁内避免用户回调/析构。
- **D2 权衡（必须实测）**：N 次锁 → 1 次锁，但**持锁时间变长**，
  大批量 key 会拉长不可让出区间，**恶化写方尾延迟**。
  这是典型的「吞吐换尾延迟」，必须同时报告两侧指标。
- **验收**：1 / N / 大批量 key 三档；TTL 边界、覆盖与删除、对象/数组值；**并测写方 p99**。

### 2.5 【D1/D3】ctx 级 arena 替代 m/c/a 多次堆分配
- **位置**：`router/router.c:124-168`（`setMca`，为 module/controller/action
  各分配新缓冲并首字母大写、记录 `*_len`）、`router.c:220-375`、`http/request.c:78-103`、
  `gene.c:603-707`（`gene_request_context_free_fields()` 逐字段 `efree`）
- **现状**：一次请求约 8–10 次小块 emalloc/efree 仅用于持有
  method/path/lang/module/controller/action。成本 ∝ 请求数（D1）。
- **方案（v5 已收敛的版本，v6 保持）**：
  1. **不**给每字段各挂定长数组 —— 那会抬高**所有**活跃协程与 ctx pool 的常驻内存（D3 恶化）；
  2. 改为 ctx 级**单个小型 arena**，字段只存 `offset + length`；
  3. reset 时整体复位游标；超长才回退 heap 并打所有权标记；
  4. 用独立设置标志区分 unset 与空串（**不能**用 length 判定）；
     保持现有 `char*` 调用边界的 NUL 结尾。
- **额外约束**：URI 不是 m/c/a 的唯一来源，必须支持独立赋值、重复更新、
  显式路由参数、异常清理。arena 扩容会使所有借用指针失效 → 必须审计全部调用者。
  注意 `path_params` 已内联在 ctx 中，且 `gene_router_reset_path_params()`
  在超过 128 bucket 时重建（`router.c:171-194`）—— arena 设计要与之协调。
- **D3 验收硬要求**：报告 `sizeof(gene_request_context)` 变化 ×
  `ctx_pool_max`（默认 **256**）与峰值活跃协程数（`co_contexts_max` 默认 **1024**）
  的 RSS 影响。CPU 收益不得以 RSS/并发上限为代价。
- **风险**：中（生命周期，需 ASAN）。

### 2.6 【D1】Webscan 每请求实例化
- **位置**：`app/application.c:418-447`、`http/webscan.c:85-161, 224-267`
- **现状**：`object_init_ex` + 7 参数 `__construct` + `check()` + dtor，每请求一次。
- **方案**：**去对象化** —— 抽出接收配置参数的 C helper，application 直接调用。
  **不**采用「worker 级长期持有 PHP 对象」（属性 zval 生命周期、配置热更新、
  RINIT/RSHUTDOWN、Swoole/FPM 差异全部要处理，收益不值）。
- **拆分立项**：flatten 增加递归深度/长度上限会**改变安全扫描覆盖范围**，
  属安全策略变更，单独评审，不在性能批次内。
- **附带**：`webscan.c` 的 static 正则字符串与 `preg` 函数指针受 §1.4 约束。

### 2.7 【D1，低】其余每请求固定成本
- `orm/meta.c:147-236`：请求级元数据命中后仍 `zend_string_copy` 5 个字段
  → 出借指针 + `from_cache` 标志。
- `orm/model.c:763-797`：`findMany(preserveOrder)` 多次 `zval_get_string` → 预归一化 ids。
- `mvc/model.c:82-122`、`service/service.c:81-122`：`__get/__set` 每次类名 + DI 查找
  → 缓存 `zend_string*` 类名（受 §1.4 约束）。
- `orm/query.c:124-140/185+`：ops 数组 push + 线性 apply → C 侧链表或直接生成 SQL。
- `cache/cache.c:563-619` `gene_cache_key`：构造期预生成 `(sign,class,method)` 前缀
  `zend_string`，运行时仅追加 args。
  **明确不做**：不改默认哈希算法 —— 会使现有 APCu/Redis/Memcached key 全部变化，
  滚动发布期间命中率瞬时归零。若要切换，只能以**显式配置 + key 版本号前缀**提供。
- `log.c`：`gene_log_get_datetime` 每条重新格式化 → 秒级时间字符串缓存。
  **纠正**：直接用 C `write()` 替代 `error_log()` 会改变 SAPI/error_log 配置、
  日志轮转、多进程追加与 Windows 行为，**不是低风险**，须 opt-in 且默认关。
- `common/common.c:743-793`：`serialize/unserialize` **已**用
  `zend_call_known_function`（不是最慢的 `call_user_function`）。
  改直调 `php_var_serialize/php_var_unserialize` 只省调用帧，
  **收益主要在大量小值场景**。igbinary 直连降级为可选（新增可选扩展 ABI 与构建探测）。
- `common/common.c:743-763` JSON：同上，属「兼容性换性能」的低-中优先级项。
- `tool/benchmark.c` / `tool/monitor.c` / `tool/crypto.c`：C API 化
  （`zend_memory_peak_usage`、`gene_hrtime`、`php_base64_encode`、`php_random_bytes`）。
  benchmark 部分已落地（§8.2）。**OpenSSL EVP 直接链接暂缓**（新增构建依赖）。
- `gene_preg_match` / Validate：PHP `preg_match` **本身已用 PCRE 编译缓存**，
  直调 `pcre_get_compiled_regex_cache()` 只省 PHP 调用帧。
  仍可做的是 `http/validate.c:781-970` 内置规则 C 层实现，避免 `gene_factory_call` 回 PHP。
- ZTS 下热函数内把 `GENE_G(runtime_type)` 等只读值读入局部变量一次。
- `router.c` 常量 `strlen(GENE_ROUTER_SAFE)` → `ZEND_STRL`：
  **先看反汇编**，编译器多半已折叠；没有运行时指令可省则不制造无效 diff。

**以上全部为 §0.2 的 C 类**：低风险、diff 小时顺手做，标注为专项路径收益，不占排期。

---

## 3. 轨道 B：D3 —— 每并发单位内存决定并发上限

在极致并发下，**RSS/并发比往往先于 CPU 成为硬上限**。本轨道的验收指标是
「单 worker 可承载的活跃协程数 / 连接数」，不是 RPS。

### 3.1 【架构项，D2+D3 双收益，需设计文档】框架缓存与业务缓存拆表
- **位置**：`cache/memory.h:21-46`、`gene.h:296`（`cache_business_dirty`）、
  `memory.c:288, 716-758, 805-863, 1869-1891`
- **动机重述（v6 强化）**：v5 把拆表的收益写成「消除高并发读串行点」并自我批注为夸大。
  **真正的动机是 §2.3 的性能悬崖**：单次业务 `Memory::set` 会让框架元数据读
  永久退回加锁路径，且不可恢复。拆表是**唯一**能让框架读保持无锁、
  同时让业务表正常 rehash/LRU/TTL 的方案。这是 D2 收益。
  D3 收益是业务表可以独立设容量与淘汰，不再和框架元数据抢冻结前的预留 bucket。
- **方案**：
  - **框架表**：启动阶段可写，`workerReady()` 后只读；不允许业务覆盖、无 TTL、无 LRU；
    保持无锁读与 `arData` 稳定；
  - **业务表**：承接公开 `Memory`/`Cache` 用户数据；独立锁 / LRU / TTL；允许正常 rehash；
  - 拆表前**列出全部 `gene_memory_*` 调用者**并显式指定表归属，
    不靠 key 前缀猜测（现有后缀约定：`:rt/:re/:cf` 路由、`:easy` 等）；
  - 明确兼容策略：既有 `Memory` 读取框架 key、`clear()`、`stats()` 统计口径、
    容量配置、同名 key 冲突。
- **安全前置（不可跳过）**：`gene_memory_get()` / `get_triple()` 返回**解锁后的裸指针**
  （`memory.c:869-895, 913-938`），重新加锁**不等于**保护借用期。
  - 业务表：必须在**锁内**完成 owned copy，允许 rehash 前移除其全部借用路径；
  - 框架表：无锁读须证明整个借用期无覆盖/删除且销毁顺序安全。
  - 单线程不 yield 的路径不能仅因「解锁了」就断言已触发 UAF，
    应分别构造可重入/协程切换与真实共享线程场景复现。
- **明确移除**：**RCU 方案不做**。单 worker 协作调度下收益有限，
  回收 epoch 与裸指针风险很大。
- **关系**：§1.3 不得被本项阻塞。本项落地后 §1.3 的 tombstone 模型可退役、§3.2 可重新评估。
- **风险**：高（公开 API、指针所有权、存储模型同时变化）。先出设计文档。

### 3.2 【依赖 §3.1】`Memory::get()` 借用读
- **现状纠正**：
  - `Memory::get()` 走 `gene_memory_zval_local()`（`memory.c:1299`，深拷贝），
    不是 `_local_copy()`（后者用于 `Gene\Cache` 业务读）；
  - `LONG/DOUBLE/NULL/BOOL` **已经**是 `ZVAL_COPY_VALUE`。
- **字符串借用**不能用「persistent/interned 标志」简单实现：
  代码注释已记录零拷贝路径此前因业务缓存覆盖/淘汰导致 UAF 而被**主动撤销**
  （`memory.c:404-409`）。
- **保留项**：`getBorrowed()` 仅可用于「整个借用期内保证无覆盖、无删除、无 TTL 清理、
  无协程切换」的内部路径。**风险：高**，需先完成 §3.1 并明确框架表只读不变式。

### 3.3 【D3】上下文与池的容量/内存标定
- **默认值纠正（v5 §7.1 的示例值与实际默认差距很大，必须在文档中区分）**：
  `co_contexts_max` 默认 **1024**（v5 示例写 8192）；`ctx_pool_max` 默认 **256**（示例 512）；
  `cache_max_items` 默认 **0**（无上限、LRU 不启用）；`cache_reserve` 默认 **4096**；
  池 `waitTimeout` 默认 **3.0**。
- **要做的不是调大默认值，而是建立标定方法**：
  1. 测 `sizeof(gene_request_context)` 与 arena（§2.5）方案下的实际每协程字节数；
  2. 在 1/32/128/512/1024/2048 活跃协程下采样 worker RSS，
     拟合「基线 RSS + 每协程增量 × 并发」；
  3. 给出「给定 RSS 预算 → 推荐 `co_contexts_max` / `ctx_pool_max` / 池 max」的换算表，
     写入 §6.1 替代当前的示例值堆砌。
- **sweep 成本**：`co_contexts` 软阈值触发的冷却式 sweep（`gene.c:1207-1224`）
  已导出 `co_contexts_sweep_count/scanned/us/skipped`。
  高并发下需确认 sweep 的**单次耗时**不进入尾延迟；若进入，
  改为分摊式（每次请求扫固定小批）而非阈值触发的整表扫。
- **`swoole_auto_cleanup` 默认 0**：开启后为**每个新分配的上下文**注册一次
  `Swoole\Coroutine::defer`（`gene.c:1088-1094, 1240-1242`）——
  这是 D1 成本（每协程一次 PHP defer 注册）。
  需实测其代价，并在文档中明确它是「漏调 `cleanup()` 的兜底」而非推荐常态。
  v5 §7.1 直接建议 `= 1`，在极致并发下这个建议需要重新用数据支撑。

### 3.4 【D3/正确性，与 §1.2 同批】Pool 生命周期与跨进程共享约束
- **位置**：`cache/redis_pool.c:453-491/580-594`；`db/pool.c:519-590`
- **前提缺失**：源码/API 目前**并未阻止**用户在 `Server::start()` 前构造 Pool，
  此时 `Swoole\Atomic` 会经 fork 被**多 worker 共享** —— 这正是 CAS 循环存在的原因。
  直接把计数改成 worker-local `zend_long` 会**改变跨 worker 语义**。
- **执行顺序（不可跳步）**：
  1. 明确约定 Pool 只能在 `WorkerStart` 内创建，并在构造处检测/记录 PID；
  2. 禁止 `clone` / `serialize`；
  3. 运行期 PID 变化时报错或降级；
  4. **之后**才可把计数改为结构体内 `zend_long`（届时 §1.2 的 CAS 问题一并消失）。
- **注意**：`Pool::closeAll()` 依赖静态 `instances` 注册表（`pool.c:1145-1203`），
  该注册表也是文件作用域 static（§1.4）。
- **风险**：中高（跳过 1–3 则不可接受）。

### 3.5 【D3/D4】池健康检查与 timer 合并
- `db/pool.c:368-404`：探活用 `PDO::getAttribute(ATTR_SERVER_INFO)`，**借出时不探活**
  → 提供 `ping_on_get` 可选配置；**仅空闲超阈值才探活**（避免每次借出加一次往返）。
- `db/pool.c:720-804`：**每个 Pool 一个 `Timer::tick`**（间隔 `idleTimeout * 500` ms）
  → 单 worker 合并为一个全局 timer。池数量多时这是 D3（timer 对象）+ D1（回调）双成本。
- `cache/memory.c:830-831` TTL sweep：每 32 次 TTL 写扫最多 64 项。
  写少读多时过期 key 只表现为 miss、存储不回收 → 暴露 `gene.cache_expiry_sweep_*` 配置；
  Swoole 可加后台 sweep。**`Timer::tick` 硬约束**：只操作业务表；与 worker 生命周期绑定；
  **不为每个实例创建 timer**；限制单次扫描时长；不在锁内 yield。
- `cache/memcached.c:115-173`：每次查方法指针 + **无池** → 缓存 `zend_function*`
  并增加 `MemcachedPool`（独立立项）。

---

## 4. 轨道 C：D4 —— 排队、饱和与尾延迟

### 4.1 【D4】路由复杂度：`chird` 占位子路由线性扫描
- **位置**：`router/router.c:220-375` `get_path_router_inner`
- **现状（已核对）**：静态 segment **已经**先走 HashTable 精确查找；
  线性扫描只发生在占位子路由（`chird`，按占位符名保存）上，
  且当前匹配代码不按「正则类型/首字符」检查当前 segment。
- **v1 方案作废**：「静态 → 正则 → 泛型分桶 + 首字符索引 → 近 O(1)」不成立 ——
  静态本就不在线性扫描里，泛型占位之间的区别通常在**后续路径分支**而非本段首字符。
- **为什么属 D4**：成本 ∝ 同层占位路由数 × 路径深度，且**每请求**发生。
  它不会出现在简单路由的 flamegraph 里（这正是 §8.4 的采样场景），
  但在真实 REST API（深层嵌套 + 多同层占位）中会成为主成本。
  **这是被旧准入筛掉的最典型条目。**
- **候选方向（需专项设计文档，择一）**：
  1. 注册期检测同层等价泛型路由并报冲突，从源头限制 children 规模；
  2. 对占位子树的后续固定 suffix 建立判别索引；
  3. 重构为带约束的 radix tree；
  4. **先显式定义多可变路由的优先级语义**（当前隐式依赖插入顺序）——
     这是前置项，语义不固化就不能改匹配顺序。
- **验收**：专用基准「深层 REST + N 个同层占位」，N = 1/4/16/64，深度 3/6/10。
- **风险**：中高。

### 4.2 【D4，opt-in】`route_precompile` 默认开关评估
- **位置**：`router/router.c:759-794`（描述符）、`1009-1073`（execute）、`1366-1395`（入口）
- **现状纠正**：hook 解析结果**已经**存入 `gene_route_pc`（`is_before/is_after`、
  before/after/hook src 与对应 closure、route src、eval 程序），
  v1 所述「待做 hook 预编译」不成立。`gene_route_pc_execute()` 是**零哈希查找**路径。
- **待决策只有**：① 默认值是否 0→1；② 失效机制（**见 §1.1，这是硬前置**）；
  ③ action 的 `zend_function*` 是否进描述符 —— **结论：不进**。
  同一 leaf 可经 `:c`/`:a` 派发到不同 controller/action，单指针缓存不成立（§8.2 已按此实现）。
- **排期**：§1.1 的 generation 失效机制落地并通过 ASAN 后，
  以 **opt-in 灰度**做 A/B；不因当前 6.15% 占比而永久关闭，也不在未证明安全时改默认。
- **收益预期**：零哈希查找路径的收益 ∝ 路由深度与 hook 数量 → 与 §4.1 同一维度，
  基准场景共用。

### 4.3 【D4，API 语义，需专项设计】响应输出与流式语义
- **位置**：`http/response.c:817-854`（`write` / `gene_response_write_chunk`）、
  `627-660`（`end`）、`857-956`（SSE）
- **源码事实**：**不存在**内部响应缓冲。`json`/`write`/`end`/`sseEvent`
  直接经 `php_write` 或 Swoole `$response->write/end` 输出。
  `success()/error()/data()`（`response.c:488-538`）**只返回数组**，不发送输出。
- **v1 方案作废**：把所有 `write()` 累积到 `smart_str` 再于 `end()` 输出，
  会破坏 SSE、chunked streaming、大响应恒定内存、及时 flush 与 TTFB。
  **这是 API 语义变更，不是透明优化。**
- **另外**：`sseStart()` 循环丢弃 output buffer 的目的正是**确保 SSE 不被缓冲**，
  并非低效逻辑。
- **可接受方向**：① 保留 `write()` 流式语义不变；② 另加显式 buffered API
  （`writeBuffered()` / `setBuffering(true)`）由调用方选择；③ 或仅合并框架内部产生的非流式小块写入。
- **D4 验收**：必须测**首字节到达时间与分块到达间隔**，不能只看总 RPS。
- **风险**：高（API 语义）。**排期**：暂缓，先出设计。

### 4.4 【结案】视图渲染的 output buffer 层数
- **位置**：`mvc/view.c:736-799`（`display/displayExt`）、`819-860`（`render`）
- **源码事实（v5 的待验证项现已结案）**：
  `render()` 每次调用创建**恰好一层** `php_output_start_default`，
  渲染后 `php_output_get_contents()` + `php_output_discard()`；
  `display()`/`displayExt()` **完全不创建 buffer**，直接输出。
  嵌套子视图若用 `contains`/include 则天然写入当前 buffer，**不产生额外层**；
  只有**显式嵌套调用 `render()`** 才会产生多层。
- **结论**：v1 的「嵌套子视图共享一层 buffer」优化**前提不成立，本项关闭**。
  符号表用 COW `ZVAL_COPY`（`view.c:90`）已是正确做法，
  共享 HashTable 不安全（模板执行会修改符号表）。

### 4.5 【D4，独立架构立项】HTTP 客户端跨请求连接复用
- **位置**：`http/http.c:1359-1407`、`1838-1950`
- **方案错误纠正**：FPM 每请求销毁 ctx，**ctx 级 handle 只能在同一请求内复用**，
  根本解决不了「每请求 `curl_easy_init`」。
- **真要跨请求复用需要**：process-level handle pool、每次 `curl_easy_reset`、
  清除 header/callback/POST body/private data、处理 fork/DNS/TLS/异常、
  绝不长期持有请求级 zval。直接链接 libcurl 还会新增构建依赖。
- **`CURLOPT_TCP_KEEPALIVE`** 用于空闲连接探测，**不等同于** HTTP 连接复用，
  不解决每请求初始化成本。
- **定性**：这是独立架构功能，不是中风险性能优化。**独立立项。**

---

## 5. 观测：让并发成本可见（第零批，持续补齐）

### 5.1 现状（已导出，不要重复提案）

- `Memory::stats()`（`memory.c:1915-1954`）：`cache_items`、`cache_num_used`、
  `cache_num_elements`、`cache_table_size`、`cache_easy_items`、`cache_insert_refused`、
  `fn_cache_items`、`co_contexts_items`、`co_contexts_max`、`co_contexts_watermark`、
  `co_contexts_sweep_count/scanned/us/skipped`、`ctx_pool_size/max/hit/miss`、
  `cache_business_items`、`route_pc_items`、`closure_src_cache_items/flushes`、
  `cache_easy_ttl`、`cache_easy_expired`。
- `Pool::stats()` / `RedisPool::stats()`：`total`、`idle`、`using`、`overflow`、
  `min`、`max`、`closed`。
- `Monitor::stats()`（`monitor.c:111-194`）聚合上述，另加
  `requests.count/errors`、`redis_pool_cas_abandoned`、`db_pool_cas_abandoned`、
  `db_pool_get_timeout`、`memory_cache_hit/miss`、`db_slow_query_count`、`slow_query_ms`、
  `swoole_auto_cleanup_defers/reclaimed`、`cache_insert_refused`。
- `Monitor::prometheus()`（`monitor.c:309-341`）导出对应 counter/gauge。

**两个必须写进文档的语义陷阱**：
1. 所有计数器都在 `GENE_G` 中，**进程本地**。多 worker 必须在扩展外聚合。
2. `memory_cache_hit/miss` **只统计用户态 `Memory::get()`**（`memory.c:1298-1302`），
   路由/DI/配置的内部读**故意不计**。不能用它推断框架缓存命中率。

### 5.2 待新增（按并发诊断价值排序）

| 优先级 | 指标 | 诊断什么 |
|---:|---|---|
| 1 | `cache_read_locked` / `cache_read_lockfree` / `cache_lock_wait_us` | §2.3 的性能悬崖是否已发生、锁路径占比 |
| 2 | `db_pool_wait_us` / `redis_pool_wait_us`（含 p99 或分桶） | §2.2 的 1 ms miss 与排队延迟 |
| 3 | `pool_idle_miss_count`（`pop(0.001)` 未命中次数） | §2.2 的 miss 率 |
| 4 | `route_pc_hit` / `route_pc_miss` / `route_pc_generation` | §4.2 有效性与 §1.1 失效是否生效 |
| 5 | `chird_scan_steps`（占位子路由线性扫描步数累计） | §4.1 是否真的在放大 |
| 6 | 每请求分配次数（低开销计数，可专项构建开启） | §2.5 / §2.6 的验收 |
| 7 | `ctx_bytes_per_context` + worker RSS 采样 | §3.3 的 D3 标定 |

原则：**热路径计数器必须是单个非原子 `GENE_G(x)++`**（进程本地，无同步成本，
仅监控用途），高开销的（如 `wait_us` 直方图）放在 opt-in 或专项构建后面。

### 5.3 判读纪律

使用**同一 worker/PID 的区间增量与每请求比率**，不是累计值是否增长：
- `cache_insert_refused`：正常容量负载应无新增；容量不足或 tombstone 耗尽都可能导致拒绝。
- `db_pool_cas_abandoned`：**非零即为 §1.2 缺陷已发生**，不是容量提示。
- `db_pool_get_timeout`：结合注入失败/饱和测试的预期判断。
- `co_contexts_sweep_*`、`ctx_pool_miss`：可正常增长，结合 sweep 耗时、存活上下文、RSS 与延迟判断。

---

## 6. 生产配置基线

### 6.1 Gene 扩展 php.ini（Swoole 模式）

> **注意**：下表左列为**扩展实际默认值**，右列为示例值。
> v5 的示例值曾被误读为默认值 —— 两者差距很大，必须区分。
> 示例值不是通用容量配置，应按 §3.3 的标定方法根据 RSS 预算换算。

| INI | 实际默认 | Swoole 生产示例 | 说明 |
|---|---:|---:|---|
| `gene.runtime_type` | 0 | `2` | Swoole 常驻模式 |
| `gene.run_environment` | 1 | `2` | 关闭 SQL 历史 / benchmark 采集（§7.1） |
| `gene.use_namespace` | — | `1` | |
| `gene.view_compile` | 0 | `1` | 离线预编译时设 `0` |
| `gene.view_compile_check_mtime` | **1**（v5 起） | `1` | 显式固定，防旧版扩展默认 `0` 的每请求重编译 |
| `gene.route_precompile` | 0 | `0` | **必须先完成 §1.1 失效机制**才可评估开启 |
| `gene.swoole_getcid_capi` | 1 | `1` | |
| `gene.swoole_auto_cleanup` | **0** | 待定 | 兜底而非常态；每协程一次 PHP defer，见 §3.3 |
| `gene.co_contexts_max` | **1024** | 按 RSS 标定 | 是 sweep 软阈值，**不是**并发准入上限 |
| `gene.ctx_pool_max` | **256** | 按 RSS 标定 | |
| `gene.ctx_pool_prewarm` | — | = `ctx_pool_max` | `workerReady()` 仅在 `runtime_type>=2` 且池空时预热 |
| `gene.cache_max_items` | **0**（无上限） | 按业务标定 | 为 0 时业务表 LRU **不启用**（与 §1.3 相关） |
| `gene.cache_reserve` | **4096** | `≥ max_items + max(64, max_items/4)` | `workerReady()` 会自动向上矫正并告警 |
| `gene.slow_query_ms` | — | `200` | 可选 |
| 池 `waitTimeout` | **3.0** | 按 SLO | 超时后创建**溢出连接**，不是失败 |

### 6.2 宿主 PHP 配置（**优先于本文档全部 C 层优化**）

对一个 PHP 框架，宿主配置通常压倒扩展内部优化。**必须先调优并固定，再谈 C 层提速** ——
否则 A/B 结果会被宿主噪声淹没，且会把本该由配置解决的问题错误归为 C 代码问题。

```ini
opcache.enable                  = 1
opcache.enable_cli              = 1     ; Swoole 常驻进程按 CLI SAPI 运行，必须显式开
opcache.memory_consumption      = 256
opcache.interned_strings_buffer = 32
opcache.max_accelerated_files   = 50000 ; 需 >= 实际 PHP 文件数（含编译后的视图产物）
opcache.validate_timestamps     = 0     ; 生产关闭；发布后须重启/reload worker
opcache.save_comments           = 1     ; 使用注解时关闭会破坏功能

realpath_cache_size             = 4096k
realpath_cache_ttl              = 600
```

- **`opcache.jit`**：**不预设收益**。对典型 IO/DB 密集 Web 请求收益常接近 0，
  甚至因 tracing 开销为负。做独立 A/B（`tracing` / `function` / off 三档），
  有数据再开；开启须同时给 `opcache.jit_buffer_size`。
- **preload**：可省每请求类加载与自动加载路径构造。约束：preload 类在各 worker 间共享
  且**不可 reload**，与 `workerReady()` 引导顺序、DI 晚注册（§7.2）语义须一并验证。
  独立 opt-in 评估项。
- **与 §7.3 自动加载的关系**：若 `realpath_cache` + OPcache 已消除重复 stat 与重复编译，
  §7.3 的收益会显著缩水。**§7.3 必须在本节调优之后重新测量。**
- **`opcache.file_cache_only` / opcache 关闭**：见 §1.4 第 2 类问题 ——
  这两种配置下 static interned string 缓存会悬垂，**必须列入回归矩阵**。
- A/B 双侧必须使用**完全相同**的本节配置，并把配置连同 PHP/扩展版本记入原始结果。

### 6.3 模板编译配置（历史坑，保留说明）

- **代码事实**：只有 `isCompile || GENE_G(view_compile)` 为真才进入编译分支；
  `view_compile_check_mtime = 0` 时 `view_compile_needs_rebuild()`（`view.c:52-72`）
  **直接 return 1**，即**每次请求都重编译**（28 轮 `php_pcre_replace`），
  且编译产物写了却永不被使用。普通 `display()` 走 `gene_view_display()`，
  不进 `displayExt()` 的模板编译分支。
- **v1 曾推荐 `view_compile=1 + check_mtime=0`，与优化目标完全相反。**
- **正确配置（二选一）**：
  - 运行时编译缓存：`view_compile=1` + `view_compile_check_mtime=1`；
  - 离线预编译：构建期生成实际 app root 下的 `Cache/Views/*.php`，运行时
    `view_compile=0`，且调用方**不能传 `isCompile=true`**（编译条件是二者逻辑或）。
- **已落地（2026-09-06）**：`check_mtime` 的 INI 与 GINIT 默认值由 `0` 改为 `1`；
  显式 `0` 保留为旧行为回退；CHANGELOG 已记录。
- **部署验收**：区分 `display()` 与 `displayExt()`；覆盖缓存缺失、源文件更新、编译失败、
  多 worker 首次编译、OPcache 更新。离线产物采用发布版本目录/原子切换；
  mtime 仅比较时间戳，不能识别保留旧时间戳的内容更新，也不保证并发写入安全。

---

## 7. 单独评审的 API / 兼容性项目（不混入透明优化批次）

### 7.1 SQL 历史 JSON 编码（开发模式）
`db/pdo.c:1153-1171`、`mysql.c:175-212`：`run_environment=0` 时每条 SQL `json_encode`
参数入历史。生产必须 `gene.run_environment >= 1`；当前默认已是 `1`，§6.1 生产样例固定为 `2`。
无需 C 代码变更。

### 7.2 DI alias 注册期展平
`di/di.c:136-150, 497-510` 当前允许晚注册、重新绑定，解析上限为 8 跳；环不会强制 miss，
而是使用第 8 跳落点。注册时直接保存最终目标会改变这些行为。
保留原始边；若基准显示瓶颈，可另做请求/协程级解析缓存，每次 alias 写入递增 generation
使全部解析缓存失效，且保留 8 跳语义和调用用户代码前的 owned string。
回归覆盖链式晚注册、中间节点重绑、环、超过 8 跳、构造函数内改 alias、协程隔离。

### 7.3 自动加载路径构造与重复 stat
`factory/load.c:78-128`（编译+执行+加入 `EG(included_files)`，未先判断是否已包含）、
`load.c:131-165`（`estrdup`+`replaceAll`+`snprintf`）。
1. `EG(included_files)` 短路**只用于类自动加载路径**，且必须以规范化后的 `opened_path` 为键；
2. **视图文件必须允许重复执行**（不同 symbol table），绝不可给通用 `gene_load_import()`
   无条件加短路；
3. 路径拼接改 `memcpy`。
**降级**：`workerReady()` 预 include 整个控制器/模型目录会改变加载顺序与文件副作用，
风险非「低」，作为独立 opt-in 配置，默认关。
**必须在 §6.2 之后重新测量。**

### 7.4 池连接 fetch mode
`db/pool.c:288-366` 未设置 `ATTR_DEFAULT_FETCH_MODE`；四驱动非池路径均设 `FETCH_ASSOC`
（mysql:268、sqlite:280、pgsql:276、mssql:264）。未显式指定的池连接可能使用 PDO 默认
`FETCH_BOTH` → 数字键与关联键增加 bucket（**这是 D3 项**：结果集内存随行数×列数放大）。
**第一步**只用现有 `options[PDO::ATTR_DEFAULT_FETCH_MODE]` 显式选择，不新增开关也不改默认。
若统一默认，按兼容性变更独立评审；仅在 options 未提供该键时补默认，
绝不覆盖用户显式 `FETCH_BOTH/FETCH_NUM/FETCH_OBJ`。
验收：比较池/非池 `row/all` 与原始 PDO 借出，验证数字下标、显式 fetch 参数、自定义模式、
输入配置数组 COW 不被修改；同时核查 `ATTR_CASE/ATTR_ORACLE_NULLS`。

### 7.5 FPM 持久连接（opt-in）
`db/pool.c` 在 `runtime_type < 2` 时直接返回 0（不建池）。提供显式配置
`gene.db_fpm_persistent`（默认 **0**），开启后设 `PDO::ATTR_PERSISTENT=true`；
归还前调用现有 `gene_db_tx_hygiene` 回滚未提交事务。
**必须在文档列明 `tx_hygiene` 无法清除的残留状态**：session variables、temporary tables、
advisory locks、prepared statements、SQL mode / time zone、认证与断线状态。
**不把 `ATTR_PERSISTENT=true` 作为框架默认。**

### 7.6 PDO/PDOStatement 方法指针缓存
`db/pdo.c` 约 **19 处** `zend_hash_str_find_ptr(&ce->function_table, ...)`，典型如 `pdo.c:856-875`。
按 `ce` 首次解析并缓存，之后 `zend_call_known_function`。
**安全条件**：`PDOStatement` 可经 `ATTR_STATEMENT_CLASS` 替换为**用户类**，其 CE/method
在 FPM 下可能只有请求生命周期 → 进程级静态缓存**只允许对精确内部 CE 生效**
（`Z_OBJCE_P(x) == php_pdo_get_dbh_ce()/statement ce` 严格相等），其余走动态查找。
同时受 §1.4 的 static 生命周期约束。
**优先级低**：相对真实 SQL 网络与 DB 执行时间，一次 HashTable lookup 占比极小；
须用真实内部 PDO/PDOStatement 的本地 SQLite 基准测量。

### 7.7 预处理语句 LRU 复用
`db/pdo.c:856-866`、`db/mysql.c:349/365`。在 native prepares 下只省掉**重复 SQL 的
server prepare 往返**，**不省 execute 往返**。
适用前提：SQL 高度重复、同一物理连接、正确关闭 cursor、连接重连后全部失效、
控制服务端 prepared statement 数量、处理 DDL/`SET`/驱动差异。**风险高，独立设计。**

### 7.8 SQL 片段属性中转
`db/mysql.c:158-173, 301-343, 1755-1767`；四驱动同构。SQL/where/data 等属性是 public，
用户可读写取引用。只在 C 侧维护字段会使公开属性与实际执行 SQL 不一致，
不能宣称「对外 API 不变」。须先设计属性读写/引用同步、clone/析构/GC、异常和 reset 规则；
否则作为新 API 或版本迁移项目。**风险高。**

### 7.9 四驱动去重（长期，独立分支）
`mysql.c/sqlite.c/pgsql.c/mssql.c` 各 ~60 KB，仅引号字符与少量方言方法不同
（`pdo.c:1269-1285` `makeWhere` 甚至靠 `strstr(class_name,"Pgsql")` 判断引号）。
方案：抽象 `gene_db_dialect { oq, cq, callbacks }`。
**依赖关系纠正**：这**不是** §7.6/7.7/7.8 的技术前提 —— 三者均可先在公共 `pdo.c` 或
shared helper 中实现。**大重构与性能优化必须分开提交。**

### 7.10 Session ID 熵
`session.c:334-375` 的 `gettimeofday+snprintf+MD5` 可换 `php_random_bytes`，
但**不能只取 64-bit**（熵偏低）。应取**至少 16 字节随机数**再 hex 编码为 32 字符。
这是安全项，不是性能项。

### 7.11 编译与布局（独立实验分支）
- `src/config.m4`（仅探测 `clock_gettime`）、`src/config.w32`（仅 `/I` `/utf-8`）。
  **不能**由「config.m4 没写优化标志」推出「当前没有优化」——
  扩展通常**继承 PHP 构建环境的标志**。
- **执行前置**：先 dump 实际编译命令行（`make V=1` / MSBuild 详细日志），确认现有 `-O` 级别。
  Windows 侧已核查：Release、x64、NTS、VS2019，PGO disabled。
- 若确需覆盖，逐项验证：`-O3` 未必快于 `-O2`；LTO 对 PHP 扩展收益常很小；
  `/GL` 必须与 `/LTCG` 配套；`-fvisibility=hidden` 需检查所有导出符号（配合 `PHP_GENE_API`）；
  `-march=native` 仅限同机部署。PGO profile 必须来自 §2/§3/§4 的负载族。
- 记录 text size、启动时间、RPS、CPU/请求与 p99；用 `perf annotate`/反汇编确认机器码变化。
- 评估 cache-line 布局、热冷字段拆分和 per-worker 预分配时，必须同时计算
  `结构体增量 × 峰值活跃协程/连接数`（D3），防止 CPU 小幅收益换来 RSS/并发恶化。

### 7.12 明确不做
- **Cache RCU**：单 worker 协作调度下收益有限，回收 epoch 与裸指针风险很大。
- **无兼容迁移的默认哈希切换**（§2.7）：滚动发布期间缓存命中率归零。
- **响应全量缓冲替代流式 `write()`**（§4.3）：API 语义破坏。
- **worker 级长期持有 PHP 对象**（§2.6 Webscan）：生命周期成本高于收益。

---

## 8. 执行顺序

### 8.1 第零批（进行中，最先落地且持续补齐）
1. **§1 全部正确性缺陷**：§1.1 route_pc 失效、§1.2 池 CAS 漂移、§1.3 tombstone 复用、
   §1.4 static 缓存生命周期。**在这些修完之前，高并发压测结果不可信。**
2. §5.2 观测项 1–4（锁路径、池等待、pool idle miss、route_pc hit/miss）——
   低风险、无语义变化，且是后续所有 A/B 的前提。
3. §6.2 宿主配置固定；§6.1 默认值 vs 示例值的文档修正。
4. 每轮保存构建参数、PHP/扩展版本、CPU/RSS、原始压测结果和 flamegraph。

### 8.2 已完成（历史）

| 批次 | 条目 | 结果 |
|---|---|---|
| 首轮 | §6.3 `view_compile_check_mtime` 默认 0→1 | 已落地，CHANGELOG 已记录 |
| 首轮 | §1.3 观测部分 | `cache_num_used/num_elements/table_size/insert_refused` 已导出 |
| 首轮 | §7.1 `run_environment` | 默认已为 `1`，生产样例固定 `2`，无代码变更 |
| 首轮 | §7.11 Windows 前置核查 | Release/x64/NTS/VS2019/PGO disabled；未擅自覆盖标志 |
| 第二批 | §2.7 ORM known-function | `gene_orm_db_call()` 对精确 `Gene\Db\*` CE 走 `zend_call_known_function`；非 Gene Db/mock 回退 |
| 第二批 | §4.2 action 单次查找 | direct dispatch 合并为一次 `zend_hash_str_find_ptr()`；新增 `gene_factory_call_1_known()`；函数指针**不**写入 `route_pc` |
| 第二批 | §2.7 Benchmark C API | 计时改 `gene_hrtime()` 单调纳秒；峰值内存改 `zend_memory_peak_usage(0)`；输出格式不变 |

回归：Windows PHP 8.1.30 NTS x64 Release，`tools\build_all.bat x64 8.1` 构建成功；
`OrmTest` 177/177、`RouterTest` 38/38、`BenchmarkTest` 42/42、`CacheTest` 49/49；
显式加载 PDO SQLite 与 OpenSSL 后 `TestRunner` 861/861。
**这些只证明 PHP 8.1 Windows NTS 的编译与功能兼容性，不构成性能收益数字。**
§2.7 / §4.2 的 ns/op、CPU/请求、RPS 与尾延迟收益继续标记为**待测**。

### 8.3 轨道推进顺序

| 轨道 | 顺序 | 条目 | 主验收维度 |
|---|---:|---|---|
| A（D1/D2） | A1 | §2.1 池借还 PHP 调用（级别 1+2，与 §1.2 同批） | 每 DB/Redis 操作 ns、`cas_abandoned=0` |
| A | A2 | §2.2 池 idle miss 1 ms | p95/p99、miss 率 |
| A | A3 | §2.3 观测 + §3.1 拆表设计文档 | 锁路径占比可见 |
| A | A4 | §2.6 Webscan 去对象化 | 构造/析构与分配计数 |
| A | A5 | §2.5 ctx arena | 分配数 + **RSS/协程**（D3 回归预算） |
| A | A6 | §2.4 `mget()` 单锁 | 批量 CPU **与写方 p99 双侧** |
| B（D3） | B1 | §3.3 容量标定与换算表 | RSS/并发拟合 |
| B | B2 | §3.4 Pool 生命周期约束 → 计数 worker-local | 语义正确 + `cas_abandoned` 消失 |
| B | B3 | §3.1 拆表实施（设计通过后） | 锁路径归零 + 业务表可 rehash |
| B | B4 | §3.5 timer 合并、`ping_on_get`、TTL sweep | timer 数、探活往返、RSS |
| C（D4） | C1 | §4.1 路由复杂度（先固化优先级语义） | 深层/多占位专项基准 |
| C | C2 | §4.2 route_pc opt-in 灰度（§1.1 完成后） | 同 C1 基准 |
| C | C3 | §4.3 buffered API 设计、§4.5 curl pool | TTFB、分块间隔、连接复用率 |

**§0.2 的 C 类条目（§2.7 全部）不占排期**，低风险时随手合入并标注收益范围。

---

## 9. 验证方法

### 9.0 顺序纪律
微基准回答局部路径改善，flamegraph 回答**某一负载**的权重，
并发/复杂度/饱和压测回答**成本如何随规模放大**。三者互补，**任何一个都不拥有否决权**。

默认顺序：**负载族基线 → 热点与放大维度映射 → 目标条目专项基准 → 独立实现 A/B → 端到端复核**。

当前负载未命中但具备 D1–D4 放大论证的条目，**先建能触发该路径的专项基准**，
再回到高并发端到端场景验证。禁止只有源码推断没有测量；
**同样禁止用单次低参数 flamegraph 否决框架核心优化**（这是 v6 重整理的直接原因）。

### 9.1 负载族（替代 v4 的两条固定 URL）
必须覆盖，且每条都按并发梯度分档：
1. 纯静态路由；
2. 路由 + 模板；
3. DB + ORM + 模板；
4. **深层 / 多同层占位路由**（§4.1 专用）；
5. **Memory/Cache 高 churn**（§1.3 专用，观察 `cache_insert_refused`）；
6. **DB/Redis 池扩容与饱和**（§2.1/§2.2/§1.2 专用）；
7. **本地 SQLite / 本地 Redis 高 QPS 小查询**（§2.1 专用 —— 剥离网络时间才能看到借还成本）；
8. 输出 / SSE（§4.3 专用，测 TTFB 与分块间隔）。

全部使用生产配置（§6.1 + §6.2），对并发、worker 数、数据规模、池容量、
同层占位数分档。**不再接受 2 threads / 32 connections 单档作为结论依据。**

### 9.2 profiling
- Linux Swoole worker：`perf record -F 999 -g -p <worker_pid>` +
  `perf script | stackcollapse-perf.pl | flamegraph.pl`；FPM 同法采样单 worker 连续请求。
- 同时记录 `gene.so` inclusive/self on-CPU、系统调用、锁等待与 **off-CPU**
  （§2.2 的 1 ms 等待只在 off-CPU 视图中可见 —— 这是旧 on-CPU-only 方法的盲区）。
- 对分配、哈希探测、深拷贝、锁、函数派发、stat、协程上下文解析增加低开销计数器（§5.2），
  给出每请求/每协程/每业务操作的次数，避免只依赖采样概率。
- 产出**热点与放大维度映射表**：flamegraph top-20 → 条目编号；
  另单独记录未进 top-20 但随路由数/并发数/批量 key 数/池等待者数增长的路径。
- 线上采集入口：`tools/acceptance/linux_swoole_profile.sh`
  （需扩展为支持 §9.1 的 8 类负载与并发梯度，当前只支持两条 URL）。

### 9.3 微基准与收益陈述规范
- 每项独立基准，断言结果正确并**确认命中被优化分支**；冷热启动分别记录；
  **不把 PHP mock 当内部 CE 快路径**。
- 固定真实 warm-up 工作量；交替 A/B 与 B/A、多独立进程多轮，记录样本量与原始输出。
- 报告批次 ns/op 的 median、离散程度/置信区间、分配次数和内存；
  **少数轮次的均值分布不能冒充单请求 p99** —— p95/p99 用足够请求样本的端到端延迟分布计算。
- 固定/记录 CPU、亲和性、频率策略、worker 数、PHP/扩展/数据库、编译命令与提交 ID；
  OPcache/JIT/realpath 缓存须按 §6.2 调优并在 A/B 双侧完全一致。
- **收益陈述规范（不是准入门槛）**：
  - 端到端指标稳定超噪声 → 可写「整机收益」，须注明负载、并发与指标；
  - 仅专项 ns/op、分配数、复制量、锁等待或 RSS/协程改善 → 只能写「专项路径收益」+ 范围；
  - 全部指标无改善且无 D1–D4 论证 → 不合入。
- Windows NTS 能验证局部功能，**不能替代** Linux Swoole、FPM 跨请求或 ZTS 路径。

### 9.4 压测与回归矩阵
- 并发 1/32/128/512/1024（必要时 2048），固定 worker/CPU 资源并记录压测端瓶颈。
  `wrk --latency -t8 -c1024 -d60s "$TARGET_URL"` 仅单档示例；
  单次 60 秒闭环 wrk 不能充分证明尾延迟，另以**可控到达率**测过载/排队行为（D4）。
- 同时记录成功 RPS、错误率/超时、p50/p95/p99、CPU/请求、worker RSS、
  池等待与 §5.2 新增计数器。
- 覆盖矩阵：
  - 路由：closure/hooks、404、**冻结后 `clear()`**、重复 `workerReady()`、协程交错；
  - 缓存：TTL、**长时间高 churn**、数组/对象转换、写后读取、
    **业务写触发 `cache_business_dirty` 前后对比**；
  - DB：用户 options、懒执行、事务、异常、非 Gene Db 回退、**池饱和与突发扩容**；
  - 宿主：opcache 开 / 关 / `file_cache_only=1` 三档（§1.4 第 2 类问题）。
- FPM 要在**同一 worker 内连续请求**；Swoole 要覆盖多 worker、清理与 reload；
  CLI 进程隔离测试不能替代它们。各模式先各自 A/B，再做模式比较，
  不把运行模型差异当成本项优化收益。

### 9.5 ASAN 回归（Linux 独立构建，不用于性能测量）
1. 干净独立构建树 + 匹配的 phpize/php-config，在其 `src/` 执行：
   ```bash
   export CFLAGS="-fsanitize=address -fno-omit-frame-pointer -O1 -g"
   export LDFLAGS="-fsanitize=address"
   phpize && ./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
   make -j"$(nproc)"
   ```
2. 优先使用同样启用 ASAN 的 PHP；仅检测扩展时也须确保匹配 runtime 最先加载。
   GCC 必要时 `LD_PRELOAD="$(gcc -print-file-name=libasan.so)"`；Clang 用匹配 runtime，
   不混用工具链。设 `USE_ZEND_ALLOC=0` 使 Zend 请求分配进入系统分配器，提升 UAF 可检测性。
3. 仓库根目录按实际构建路径运行：
   ```bash
   export USE_ZEND_ALLOC=0
   export GENE_TEST_PHP_ARGS="-n -d extension=$PWD/src/modules/gene.so"
   php -n -d "extension=$PWD/src/modules/gene.so" --ri gene
   php -n -d "extension=$PWD/src/modules/gene.so" test/TestRunner.php
   ```
   构建路径含空格时在 `GENE_TEST_PHP_ARGS` 内保留 shell 引号。
   验证主进程与子进程加载相同产物，检查 skip/未覆盖测试而不只看退出码。
   **Swoole 专项必须另外加载匹配 Swoole 并运行对应脚本** —— §1.1 与 §3.1 的验证依赖它。
4. ASAN 通过只说明已执行路径未检出内存错误，**不证明线程安全或并发覆盖完整**。
   生命周期变更必须额外做同进程重复请求与长时间 churn/RSS 趋势回归；
   性能测试使用正常 release 构建。

### 9.6 复用现有手段，不从零重复建设
- `test/BenchmarkTest.php` 是**功能测试**，不是性能基准。
- `tools/acceptance/swoole_benchmark.php` 已有 getcid/route_precompile 四组合摘要一致性检查，
  是**正确性矩阵**，不等于 HTTP 性能测量。
- `tools/acceptance/fpm_benchmark.php` 可执行多轮外部 benchmark_command，
  但 warmup 目前是 `sleep`，**并不实际预热目标**；须由压测命令/前置步骤发送真实请求。
- `tools/acceptance/linux_swoole_verify.sh` 已有构建、协程/池验收、HTTP 压测和 RSS 采样入口，
  优先扩展；性能测试须显式使用生产 `run_environment`（脚本默认 0）。
- `tools/acceptance/linux_swoole_profile.sh` 已支持单 worker PID + 两条 URL 的
  warm-up / 压测 / `perf record` / DSO 占比 / 符号 top-20 / FlameGraph / 打包。
  **待扩展**：§9.1 的 8 类负载、并发梯度、off-CPU 采样。
- `audit/repro/` 已有 Swoole 缓存 UAF、workerReady、路由复现，可作回归起点，
  不能替代性能基准。§1.1 与 §1.2 应在此新增复现脚本。

---

## 10. 历史 profiling 结果与 v6 的判读

### 10.1 首轮线上 profiling（2026-09-06）

| 场景 | `gene.so` self on-CPU | 采样参数 |
|---|---:|---|
| `/profile/route`（路由 + 模板） | 6.15% | perf 30s、**2 threads / 32 connections** |
| `/profile/db`（DB + ORM + 模板） | 1.89% | 同上 |

环境：CentOS 7.9、Swoole worker PID 28903，两个场景均生成 FlameGraph。
原始结果归档 `gene-swoole-profile-20260906-211117.tar.gz`。

### 10.2 v6 判读（与 v4/v5 的关键分歧）

这两个数字**只说明**：在 2 threads / 32 connections、简单路由、单一 DB 查询的负载下，
Gene 的 on-CPU 可见占比较低。它**不能说明**：

- 池借还的 PHP 方法调用成本（§2.1）—— 该负载下被 SQL 网络时间完全掩盖；
- 池 idle miss 的 1 ms 等待（§2.2）—— 属 **off-CPU**，on-CPU 采样看不见；
- 占位路由线性扫描（§4.1）—— 该负载的路由结构过于简单，扫描步数接近 1；
- 缓存锁路径（§2.3）—— 取决于应用是否调过 `Memory::set`，与采样负载无关；
- tombstone 耗尽（§1.3）与 CAS 漂移（§1.2）—— 均为**时间/竞争累积型**，30 秒采样必然看不到；
- 每协程内存（§3.3）—— 与 CPU 占比无关，32 连接下 RSS 压力为零。

**结论**：v4 用这两个数字设置 10% 停止门槛，v5 虽废止门槛但仍保留了
「主指标须超噪声」的准入 —— 两者都把**低并发平均值**当作**高并发成本**的代理指标。
这是 v6 重新整理的唯一原因。profiling 继续做（§9.2），但只用于排期；
准入改按 §0.2 的 D1–D4 增长论证。

### 10.3 后续执行条件

固定 §6.2 配置与 Linux 环境后，按 §9.1 扩展负载族与并发梯度，
回填 inclusive/self、**off-CPU**、分配与锁指标。执行不要求 `gene.so` 达到任何百分比：
- 真实热点 → 优先队列；
- D1–D4 放大项 → 专项压力队列（**这是 v6 的主队列**）；
- 低风险固定成本 → 连续优化队列（不占排期）。

每项须有独立基线、正确性回归和可回滚提交；有运行时开关的项目先 opt-in 灰度。
