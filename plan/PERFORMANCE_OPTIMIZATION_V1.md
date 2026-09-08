# Gene 扩展极致并发优化 —— V1：已关闭

> 版本：v6-close（2026-09-08）。本文件由原 `PERFORMANCE_OPTIMIZATION.md`（v6，2026-09-06）
> 按执行状态拆分，并于 2026-09-08 完成 V1 收口：
> - **V1（本文件）**：已修复 / 已实施 / 已结案的第一批条目；支持矩阵内的实现与功能验收已完成。
> - **V2（`PERFORMANCE_OPTIMIZATION_V2.md`）**：尚未开始的优化，以及从 V1 移交的增强验证、收益量化和平台扩展。
>
> **关闭口径**：V1 的支持矩阵为 PHP 8.1 **NTS**（Windows x64 与 Linux Swoole）。关闭表示
> 目标缺陷已修复、功能回归和已执行的 Linux 并发验证通过；不表示已经证明所有负载下的整机收益。
> ZTS 多线程 SAPI 不在 V1 支持矩阵内，相关 static 缓存治理作为平台扩展移交 V2。ASAN 降为
> 建议性的补充诊断，不再作为关闭门槛；512/1024 梯度、RSS 长跑、专项 ns/op/p99、FPM/fork/
> opcache 配置矩阵也移交 V2，只有取得数据后才允许补充对应收益或平台兼容性声明。
>
> 章节编号沿用原 v6 文档，未纳入本文件的章节以「→ V2」标注。
> 本文中 §2.4–§2.7、§3.2、§3.3、§3.5、§4.1–§4.3、§4.5、§5.2、§7、§8.3 见 V2。
>
> —— 以下为原 v6 头部说明 ——
>
> **v6 是围绕「极致并发」的重新整理，不是 v5 的增量修订。**
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
> 4. 涉及生命周期/裸指针的改动优先运行 Linux `-fsanitize=address` 全量回归；ASAN 是建议性的补充诊断，不能替代功能、并发和生命周期不变式审查，也不单独阻塞 V1 关闭（执行方法见 §9.5）。
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
| 路由 | 预编译 dispatch 描述符 `route_pc`（`gene.route_precompile`，默认关，且要求 `runtime_type>=2 && worker_ready`；generation 失效 + 键式 closure 解析见 §1.1）；`Router::run()` 三键单锁读 `gene_memory_get_triple()` | `router.c:759-794, 1009-1073, 2072-2076` |
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

### 1.1 【已关闭 2026-09-08；压力矩阵移交 V2】`Router::clear()` 不失效 `route_pc`，留下悬垂描述符

> **修复实现**（`[GENE_FIX:2026-09-07 PC-GEN]`）：采纳方案 2 + 方案 3 的组合。
> - **generation**：新增 `GENE_G(route_pc_generation)`。`Router::clear()/delTree()/delEvent()`
>   调用 `gene_router_pc_invalidate()` 递增该计数；描述符记录解析时的生成号，
>   `get_router_info()` 查表命中后先比对，不匹配则**不执行**：从表中摘除（表已改为
>   **无 dtor**，摘除不释放内存）、挂入 `GENE_G(route_pc_retired)` 延迟回收链，
>   本次请求走 `get_router_info_slow()`。旧描述符只在 MSHUTDOWN 释放
>   （`gene_router_pc_destroy()` 同时清空表与回收链），因此协程在
>   `gene_route_pc_execute()` 中挂起时持有的描述符不会被抽走 —— 解决在途借用。
> - **不再缓存 closure zval**（方案 3）：描述符改存 `fn_cache` 的**键**
>   （`route_cl_key/before_cl_key/after_cl_key/hook_cl_key`，字符串位于持久路由树，
>   与其它借用指针同受 generation 保护），执行时再查 `fn_cache`。这样请求级
>   `fn_cache` 在 RSHUTDOWN 被释放不再使描述符悬垂（消除 §1.1「附带」的生命周期错配）。
>   四个 closure 在**任何 hook 执行之前**一次性解析完；若某个已登记的键查不到
>   （`fn_cache` 被清空且未重建），`gene_route_pc_execute()` 返回 `-1`，调用方
>   干净地回退慢路径，绝不会出现「hook 链执行一半」。
> - **可观测**：`Memory::stats()` / `Monitor::stats()` 新增 `route_pc_generation`、
>   `route_pc_retired`。
> - **验证**：`php -d gene.route_precompile=1 audit\repro\route_pc_clear_invalidate.php`
>   与 `=0` 输出逐行一致（行为等价），`generation` 随每次 `clear()` 推进、
>   `retired` 随之增长且 `items` 不再返回悬垂描述符；`test\RouterTest.php` 全通过。
>   注：脚本里 `workerReady()` 之后的 `clear()` 因进程缓存已冻结无法真正改写路由树，
>   慢路径本身会因 closure `object handle` 复用把 `/hello` 与 `/plain` 的处理器对调 ——
>   该现象在 `route_precompile=0` 下完全相同，属**既有**问题，不在本条范围内。
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

### 1.2 【已关闭 2026-09-08；高梯度长跑移交 V2】池计数器 CAS 放弃导致单调漂移

> **修复实现**：DB Pool 与 RedisPool 已删除 64 轮 `cmpset` 递减及放弃路径，连接槽的
> 成功预留/创建与销毁改为严格对称的 `Swoole\Atomic::add(+1)` / `sub(1)`；
> `*_cas_abandoned` 指标为兼容保留，新路径不再递增。修复同时处理了 `close()` 与
> 创建、回收、归还协程交错时的重复递减/负数计数，以及 Channel 关闭唤醒后误建
> overflow 连接的问题。前置生命周期约束见 §3.4。
>
> **验证状态**：
> - Windows PHP 8.1.30 NTS x64 Release 构建成功，`DatabaseTest` 39/39、`CacheTest` 63/63、
>   全量 `TestRunner` 884/884。
> - **Linux Swoole 验证（2026-09-07）**：PHP 8.1.34 NTS DEBUG + Swoole 6.1.9，Gene 6.2.1。
>   MySQL Pool 200 协程 × 1000 次借还，**0 failures**，`stats()` total=2/idle=2/using=0（计数无漂移）；
>   Redis Pool 同构 200 协程 × 1000 次，**0 failures**。
>   事务泄漏防护：借出者 1 开启事务后归还 → 框架自动 rollback → 借出者 2 收到干净连接 ✓。
>   全量 `TestRunner` **896/896**（100%）。
> - 现有结果满足 V1 功能关闭标准。512/1024 并发梯度、满池突发长跑及 Linux ASAN 移交 V2；补测前不回填性能收益数字。

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

### 1.3 【已关闭 2026-09-08；增强验证移交 V2】冻结表 tombstone 耗尽导致静默拒写

> **修复实现**：源码复核确认 Zend 公共 HashTable API 不支持在不移动 live bucket 的前提下
> 安全复用 tombstone；手写 `Bucket/HT_HASH` 插入会绑定 Zend 内部布局且无法证明无在途借用，
> 因此未采用该高风险临时方案。本轮直接落地 §3.1 拆表：冻结框架表与可变业务表分离，
> 公开 `Gene\Memory` / `Gene\Cache` 数据写入独立 `business_cache`，使用独立 rwlock、TTL 与
> LRU，可正常 rehash；框架表在 `workerReady()` 后保持只读和 `arData` 稳定。
> `Memory::clean()` 现只重建业务表，不触碰路由/DI/配置；业务读通过
> `gene_business_memory_get_copy()` 在锁内完成 owned deep copy，释放锁后不保留裸指针。
>
> **可观测与验证**：新增 `framework_cache_items`、`business_cache_items`、
> `business_cache_num_used`、`business_cache_table_size`；新增 5000 轮不同 key 的
> set/get/del churn 回归，`cache_insert_refused` 无新增且业务项回到基线。Windows 全量
> `TestRunner` 884/884。
> **Linux Swoole 验证（2026-09-07）**：全量 `TestRunner` **896/896**；
> 上下文隔离压测 10 万请求 / 500 并发（manual + auto 两轮），`cache_insert_refused=0`、
> `business_cache_items=0`、`framework_cache_items=0`，无 tombstone 耗尽或静默拒写。
> 以上证据满足 V1 的 NTS 功能关闭标准；Linux ASAN、长时间高 churn 与 RSS 趋势作为增强验证移交 V2，不据此声明内存收益。

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
- **修复前候选（拆表落地后退役）**：
  1. **插入时复用 tombstone**：冻结表插入前定位可复用的已删除 bucket，命中即原地复用，
     不增长 `nNumUsed`。硬约束：不得移动 `arData`、不得改变无锁读者可见的 bucket 顺序、
     复用前必须确认该 bucket 无在途借用（与 §2.4 → V2 借用读不变式一起论证）；
  2. 运维兜底：显式的业务表重建/清空入口（注意 `Memory::clean()` 在 `workerReady()` 后
     被**拒绝执行**，`memory.c:1869-1891` —— 兜底入口必须是新 API，不能复用 `clean()`）；
  3. 有界压实：**风险高**（移动 `arData`），仅在 §3.1 拆表方案确定不采纳时才评估。
- **不得被 §3.1 拆表阻塞**。拆表是高风险架构项，把本缺陷挂在它后面等于无限期推迟。
- **验收**：持续写删不同 key 超过 `reserve` 数倍，断言 `cache_insert_refused`
  **不随时间单调增长**且 `nNumUsed` 有界；配合 ASAN 与 RSS 趋势。

### 1.4 【V1 支持范围内已关闭 2026-09-08；ZTS 平台扩展移交 V2】文件作用域 `static` 缓存不是线程局部

> **已完成**：全库复核后，仍存在的直接
> `static zend_string* + zend_string_init_interned(..., 1)` 不安全模式集中在
> `cache/cache.c` 的 7 个方法/函数名缓存；现已统一改用 `GENE_INTERNED_STR()`，只在
> `IS_STR_PERMANENT` 时跨请求缓存，opcache 关闭或 `file_cache_only=1` 时不再保留请求级指针。
> Windows NTS 构建与全量 884/884 回归通过。
>
> **范围决策**：V1 只承诺 PHP 8.1 NTS，不把尚未验证的 ZTS 多线程 SAPI 纳入支持矩阵。
> 文件作用域 `zend_function*` / `zend_class_entry*` / `HashTable*` 的 ZTS 惰性初始化治理、
> ZTS 构建矩阵，以及 opcache 关/`file_cache_only=1` 的同 worker 跨请求专项回归移交 V2。
> 在这些验证完成前不得对外声明 ZTS 或上述 opcache 配置已获专项兼容性认证。

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

> §2.4–§2.7 尚未开始，见 V2。

### 2.1 【级别 2 已关闭 2026-09-08；收益量化移交 V2】池借还路径上的 PHP 方法调用

> **完成情况**：与 §1.2 同批删除 CAS 循环，正常销毁路径由最多 64 次 `cmpset` 收敛为
> 一次对称 `Atomic::sub(1)`；既有 Channel/Atomic 方法指针缓存与
> `zend_call_known_function` 路径继续保留。级别 3 的 Swoole C-API `dlsym` 直调未实施，
> 仍作为独立高风险 ABI 项。Windows 功能回归通过。
> **Linux Swoole 验证（2026-09-07）**：MySQL/Redis Pool 各 200 协程 × 1000 次借还均 **0 failures**，
> `db_pool_cas_abandoned` 无新增（CAS 循环已删除）。
> 本地 SQLite/Redis 的 1/32/128/512/1024 并发 ns/op 与 p99 移交 V2；测量完成前不声明整机收益。

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
     **单独立项，不进第一批。**（→ V2 待办）
- **专项基准**：本地 SQLite / 本地 Redis，1/32/128/512/1024 并发，
  测每操作 ns、`cas_abandoned`、p99；分离「借还开销」与「SQL 执行」。
- **风险**：1 低；2 中（与 §1.2 同批，需容量语义评审）；3 高（外部 ABI）。

### 2.2 【已关闭 2026-09-08；尾延迟量化移交 V2】`Pool::get()` 空闲队列 miss 的 1 ms 定时等待（原 §5.2 升级）

> **实现**：DB Pool/RedisPool 的 `get()` 先调用现有 Channel `isEmpty()`；空队列直接进入
> reserve/create 或饱和等待，不再执行 `pop(0.001)`。判空到 pop 之间没有 yield；
> 队列关闭、创建失败、饱和等待及 `close()` 唤醒路径保留原总等待预算并增加关闭后二次检查。
> DB Pool 直接构造与静态 `create()` 均恰好执行一次 min 预填。新增
> `db_pool_idle_miss` / `redis_pool_idle_miss`、Redis 等价 timeout 指标并导出到
> `Monitor::stats()` / Prometheus。Windows 编译和功能测试通过。
> **Linux Swoole 验证（2026-09-07）**：Pool min 预填恰好一次（total=2/idle=2），
> 200 协程 × 1000 次借还 **0 failures**，`db_pool_idle_miss` / `redis_pool_idle_miss` 无异常增长。
> Swoole 下低负载、突发扩容、满池排队三档的 p50/p95/p99 移交 V2；在数据补齐前只声明已消除固定 1 ms miss 路径，不声明端到端尾延迟收益。

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

### 2.3 【已关闭 2026-09-08；诊断指标移交 V2】进程缓存读路径：条件跳锁的覆盖率问题

> **完成情况**：§3.1 拆表落地后，业务写只设置/锁定独立 `business_cache`，不再改变框架表
> 的读锁策略；路由/DI/配置仍访问启动后只读的框架表，`workerReady()` 后可持续走跳锁路径，
> 原“一次业务写使框架读永久加锁”的性能悬崖已消除。业务读始终持独立 rwlock并在锁内深拷贝。
> `cache_read_locked/cache_read_lockfree/cache_lock_wait_us` 尚未新增；拆表后其诊断对象应改为
> 区分框架表跳锁与业务表带锁，而不是继续观测已退役的 `cache_business_dirty` 悬崖。

- **位置**：`memory.h:27-30`（`GENE_CACHE_RDLOCK` 在
  `worker_ready && !cache_business_dirty` 时跳锁）、`gene.h:39-57`（`gene_rwlock_t`，
  Windows `SRWLOCK` / 其余 `pthread_rwlock_t`）、`memory.c:869-895, 913-938, 952-997`
- **修复前源码事实（必须纠正历史上「Gene 缓存读是无锁的」的表述）**：
  - 无锁快路径**只在 Swoole 且业务从未写过缓存时成立**。
    `Memory::set/del/incr/decr/rateLimit/lock/unlock/mset` 任一次调用
    经 `GENE_CACHE_LAYER_MEMORY_WRITE_ENTER/LEAVE` 置 `cache_business_dirty=1`
    （`memory.c:1248-1250, 1426-1428, 1528-1530` 等），此后**框架读（路由/DI/配置）
    全部退回加锁路径**，且**没有回到 clean 状态的路径**；
  - **FPM 下 `worker_ready` 虽会被置 1**（`application.c:1383-1384`，
    v1 所称「FPM 永为 0」不成立），但 FPM 每请求重新 GINIT，
    实际效果是绝大多数读仍走锁。
- **修复前真正的 D2 问题是**：「只要应用用了一次 `Memory::set`，
  框架元数据读就永久变成带锁读」。这是一个**开关式的性能悬崖**，
  不是渐进退化 —— 且几乎所有真实应用都会踩到。
- **方案（这是 §3.1 拆表的真正动机，比「架构更整洁」有力得多）**：
  见 §3.1。在拆表落地前，**不要**为了绕过 `cache_business_dirty`
  去缩小写入口的标记范围 —— 标记范围小了就等于把 §2.4 → V2 的 UAF 放回来。
- **本项自身可做的低风险事**：新增计数器区分
  「跳锁读次数 / 加锁读次数 / 锁等待时长」，让悬崖**可被观测**（§5.2 → V2）。

---

## 3. 轨道 B：D3 —— 每并发单位内存决定并发上限

在极致并发下，**RSS/并发比往往先于 CPU 成为硬上限**。本轨道的验收指标是
「单 worker 可承载的活跃协程数 / 连接数」，不是 RPS。

> §3.2、§3.3、§3.5 尚未开始，见 V2。

### 3.1 【已关闭 2026-09-08；增强验证移交 V2】框架缓存与业务缓存拆表

> **落地结果**：新增 `GENE_G(business_cache)`、`business_cache_expiry` 与
> `business_cache_lock`。Router、Config、DI、Pool 配置等内部调用继续显式使用框架表；
> PHP-facing `Gene\Memory` 与 `Gene\Cache` 写入口通过业务作用域选择独立表，未按 key 前缀猜测。
>
> **当前验收**：Windows PHP 8.1.30 NTS x64 Release 编译成功；Cache 高 churn 5000 轮通过；
> 全量 884/884。
> **Linux Swoole 验证（2026-09-07）**：PHP 8.1.34 NTS DEBUG + Swoole 6.1.9，Gene 6.2.1。
> 全量 `TestRunner` **896/896**；上下文隔离 10 万请求 / 500 并发（manual + auto），
> `framework_cache_items=0`、`business_cache_items=0`，拆表后框架表与业务表各自独立运作正常。
> 以上证据满足 V1 的 PHP 8.1 NTS 功能关闭标准。Swoole 协程交错专项、FPM 同 worker 多请求、
> Linux ASAN 与 RSS/并发回归移交 V2；完成前不扩展平台认证范围，也不声明 RSS 收益。

- **位置**：`cache/memory.h:21-46`、`gene.h:296`（`cache_business_dirty`）、
  `memory.c:288, 716-758, 805-863, 1869-1891`
- **动机重述（v6 强化）**：v5 把拆表的收益写成「消除高并发读串行点」并自我批注为夸大.
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
- **关系**：§1.3 不得被本项阻塞。本项落地后 §1.3 的 tombstone 模型可退役、§3.2 → V2 可重新评估。
- **关闭后约束**：公开 API、指针所有权与存储模型已完成 Windows NTS 与 Linux Swoole 功能回归；
  协程交错、FPM 同 worker、ASAN 与 RSS 趋势继续作为 V2 增强验证管理，异常结果必须重开缺陷而非修改 V1 收益口径。

### 3.4 【已关闭 2026-09-08；fork 扩展矩阵移交 V2】Pool 生命周期与跨进程共享约束

> **实现**：DB Pool/RedisPool 构造时记录 `creatorPid`；`get/put/remove/recycleIdle/close/`
> `healthCheck/stats` 以及静态清理路径拒绝 PID 不匹配的跨 fork 使用并累计
> `db_pool_pid_mismatch` / `redis_pool_pid_mismatch`。两个 Pool 均为 final，私有 `__clone`，
> 并设置 `ZEND_ACC_NOT_SERIALIZABLE`。在该 PID 约束之后，§1.2 才移除 CAS 递减；计数仍保留
> `Swoole\Atomic`，未贸然改为结构体 `zend_long`。Windows NTS 已编译和回归。
> **Linux Swoole 验证（2026-09-07）**：MySQL/Redis Pool 各 200 协程 × 1000 次借还 **0 failures**，
> `db_pool_pid_mismatch` / `redis_pool_pid_mismatch` 无触发（worker 内构造，PID 一致）。
> creator PID 拒绝策略及 worker 内正常路径满足 V1 关闭标准；Linux `pcntl_fork`、Swoole 多 worker/workerStop 与关闭交错矩阵移交 V2。

- **位置**：`cache/redis_pool.c:453-491/580-594`；`db/pool.c:519-590`
- **修复前缺失的前提**：源码/API 当时**未阻止**用户在 `Server::start()` 前构造 Pool，
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

---

## 4. 轨道 C：D4 —— 排队、饱和与尾延迟

> §4.1、§4.2、§4.3、§4.5 尚未开始，见 V2。

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

---

## 5. 观测：让并发成本可见（第零批，持续补齐）

### 5.1 现状（已导出，不要重复提案）

- `Memory::stats()`（`memory.c:1915-1954`）：`cache_items`、`cache_num_used`、
  `cache_num_elements`、`cache_table_size`、`framework_cache_items`、`business_cache_items`、
  `business_cache_num_used`、`business_cache_table_size`、`cache_easy_items`、`cache_insert_refused`、
  `fn_cache_items`、`co_contexts_items`、`co_contexts_max`、`co_contexts_watermark`、
  `co_contexts_sweep_count/scanned/us/skipped`、`ctx_pool_size/max/hit/miss`、
  `cache_business_items`、`route_pc_items`、`closure_src_cache_items/flushes`、
  `cache_easy_ttl`、`cache_easy_expired`。
- `Pool::stats()` / `RedisPool::stats()`：`total`、`idle`、`using`、`overflow`、
  `min`、`max`、`closed`。
- `Monitor::stats()`（`monitor.c:111-194`）聚合上述，另加
  `requests.count/errors`、`redis_pool_cas_abandoned`、`db_pool_cas_abandoned`、
  `db_pool_get_timeout`、`redis_pool_get_timeout`、`db_pool_idle_miss`、`redis_pool_idle_miss`、
  `db_pool_pid_mismatch`、`redis_pool_pid_mismatch`、`memory_cache_hit/miss`、
  `db_slow_query_count`、`slow_query_ms`、`swoole_auto_cleanup_defers/reclaimed`、
  `cache_insert_refused`。
- `Monitor::prometheus()`（`monitor.c:309-341`）导出对应 counter/gauge。

**两个必须写进文档的语义陷阱**：
1. 所有计数器都在 `GENE_G` 中，**进程本地**。多 worker 必须在扩展外聚合。
2. `memory_cache_hit/miss` **只统计用户态 `Memory::get()`**（`memory.c:1298-1302`），
   路由/DI/配置的内部读**故意不计**。不能用它推断框架缓存命中率。

> §5.2 待新增观测项（锁路径、池等待时长、route_pc hit/miss 等）见 V2；
> 其中 `db_pool_idle_miss` / `redis_pool_idle_miss` 已完成（2026-09-07）。

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
> 示例值不是通用容量配置，应按 §3.3 → V2 的标定方法根据 RSS 预算换算。

| INI | 实际默认 | Swoole 生产示例 | 说明 |
|---|---:|---:|---|
| `gene.runtime_type` | 0 | `2` | Swoole 常驻模式 |
| `gene.run_environment` | 1 | `2` | 关闭 SQL 历史 / benchmark 采集（§7.1 → V2） |
| `gene.use_namespace` | — | `1` | |
| `gene.view_compile` | 0 | `1` | 离线预编译时设 `0` |
| `gene.view_compile_check_mtime` | **1**（v5 起） | `1` | 显式固定，防旧版扩展默认 `0` 的每请求重编译 |
| `gene.route_precompile` | 0 | `0` | **必须先完成 §1.1 失效机制**才可评估开启 |
| `gene.swoole_getcid_capi` | 1 | `1` | |
| `gene.swoole_auto_cleanup` | **0** | 待定 | 兜底而非常态；每协程一次 PHP defer，见 §3.3 → V2 |
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
  且**不可 reload**，与 `workerReady()` 引导顺序、DI 晚注册（§7.2 → V2）语义须一并验证。
  独立 opt-in 评估项。
- **与 §7.3 → V2 自动加载的关系**：若 `realpath_cache` + OPcache 已消除重复 stat 与重复编译，
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

## 7. 单独评审的 API / 兼容性项目

> §7.1–§7.12 全部尚未开始，见 V2。

---

## 8. 执行顺序

### 8.1 第零批（已关闭 2026-09-08）
1. **§1 正确性缺陷**：§1.1 route_pc 失效、§1.2 池 CAS 漂移、§1.3 tombstone 根治与
   §1.4 NTS 范围内的 interned string 悬垂均已修复并关闭。
2. **支持矩阵证据**：Windows PHP 8.1 NTS 全量 884/884；Linux PHP 8.1 NTS + Swoole 全量
   896/896、10 万请求上下文隔离、200 协程池借还与事务泄漏防护通过。
3. **移交 V2**：ZTS 平台扩展、ASAN、512/1024 梯度、RSS/churn 长跑、专项 ns/op/p99、
   FPM/fork/opcache 配置矩阵，以及锁路径/等待时长和 route_pc hit/miss 观测。
4. **声明边界**：V1 关闭只代表上述支持矩阵内的实现与功能验收，不产生未测量的整机收益、
   尾延迟、RSS、ZTS 或扩展配置兼容性声明；V2 补测若发现缺陷必须重新立项修复。

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
| 第三批 | §1.2 + §2.1 级别 2 | DB/Redis Pool 删除可放弃 CAS decrement，改对称 Atomic add/sub；关闭交错不重复递减 |
| 第三批 | §2.2 | 空闲 Channel 判空后仅在非空时 pop；新增 DB/Redis idle miss 与 timeout 指标 |
| 第三批 | §3.4 | Pool 绑定 creator PID，跨 fork 拒绝；禁止 clone/serialize；PID mismatch 可观测 |
| 第三批 | §1.3 + §2.3 + §3.1 | 框架/业务缓存拆表；业务表独立锁/TTL/LRU/rehash，框架表保持启动后只读 |
| 第三批 | §1.4（V1 范围） | `cache/cache.c` 7 处 static interned name 改为 `GENE_INTERNED_STR()`；V1 限定 PHP 8.1 NTS，ZTS 平台扩展移交 V2 |
| Linux 验证 | §1.2/§1.3/§2.1/§2.2/§3.1/§3.4 | Linux PHP 8.1.34 NTS DEBUG + Swoole 6.1.9，Gene 6.2.1。编译零警告；全量 `TestRunner` **896/896**；Swoole 矩阵 4 组合 ALL-PASS（~10.5K req/s debug 构建）；上下文隔离 10 万请求 / 500 并发（manual + auto）零泄漏、`ctx_pool_hit` 99.5%；MySQL/Redis Pool 200 协程 × 1000 次 **0 failures**；事务泄漏防护 rollback 验证通过 |

历史回归：Windows PHP 8.1.30 NTS x64 Release，`tools\build_all.bat x64 8.1` 构建成功；
`OrmTest` 177/177、`RouterTest` 38/38、`BenchmarkTest` 42/42、`CacheTest` 49/49；
显式加载 PDO SQLite 与 OpenSSL 后 `TestRunner` 861/861。

第三批回归（2026-09-07）：同一 Windows NTS x64 Release 构建成功；`CacheTest` 63/63、
`DatabaseTest` 39/39、`RouterTest` 42/42、`OrmTest` 177/177，全量 `TestRunner` **884/884**。
新增缓存 5000 轮高 churn、分表统计、Pool clone/serialize 与 min 预填功能覆盖。
**这些只证明 PHP 8.1 Windows NTS 的编译与功能兼容性，不构成性能收益或 Swoole 并发验收。**
§1.2/§2.1/§2.2/§3.1/§3.4 的 Linux Swoole 功能验证已于 2026-09-07 通过（见上表）。
ASAN、512/1024 并发梯度的 ns/op、RSS 与尾延迟已作为**非阻塞增强验证移交 V2**。

> §8.3 轨道推进顺序（未开始条目的排期）见 V2。

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
4. **深层 / 多同层占位路由**（§4.1 → V2 专用）；
5. **Memory/Cache 高 churn**（§1.3 专用，观察 `cache_insert_refused`）；
6. **DB/Redis 池扩容与饱和**（§2.1/§2.2/§1.2 专用）；
7. **本地 SQLite / 本地 Redis 高 QPS 小查询**（§2.1 专用 —— 剥离网络时间才能看到借还成本）；
8. 输出 / SSE（§4.3 → V2 专用，测 TTFB 与分块间隔）。

全部使用生产配置（§6.1 + §6.2），对并发、worker 数、数据规模、池容量、
同层占位数分档。**不再接受 2 threads / 32 connections 单档作为结论依据。**

### 9.2 profiling
- Linux Swoole worker：`perf record -F 999 -g -p <worker_pid>` +
  `perf script | stackcollapse-perf.pl | flamegraph.pl`；FPM 同法采样单 worker 连续请求。
- 同时记录 `gene.so` inclusive/self on-CPU、系统调用、锁等待与 **off-CPU**
  （§2.2 的 1 ms 等待只在 off-CPU 视图中可见 —— 这是旧 on-CPU-only 方法的盲区）。
- 对分配、哈希探测、深拷贝、锁、函数派发、stat、协程上下文解析增加低开销计数器（§5.2 → V2），
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
  池等待与 §5.2 → V2 新增计数器。
- 覆盖矩阵：
  - 路由：closure/hooks、404、**冻结后 `clear()`**、重复 `workerReady()`、协程交错；
  - 缓存：TTL、**长时间高 churn**、数组/对象转换、写后读取、
    **业务写触发 `cache_business_dirty` 前后对比**；
  - DB：用户 options、懒执行、事务、异常、非 Gene Db 回退、**池饱和与突发扩容**；
  - 宿主：opcache 开 / 关 / `file_cache_only=1` 三档（§1.4 第 2 类问题）。
- FPM 要在**同一 worker 内连续请求**；Swoole 要覆盖多 worker、清理与 reload；
  CLI 进程隔离测试不能替代它们。各模式先各自 A/B，再做模式比较，
  不把运行模型差异当成本项优化收益。

### 9.5 ASAN 补充诊断（建议执行，非 V1 关闭门槛）
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
- 占位路由线性扫描（§4.1 → V2）—— 该负载的路由结构过于简单，扫描步数接近 1；
- 缓存锁路径（§2.3）—— 取决于应用是否调过 `Memory::set`，与采样负载无关；
- tombstone 耗尽（§1.3）与 CAS 漂移（§1.2）—— 均为**时间/竞争累积型**，30 秒采样必然看不到；
- 每协程内存（§3.3 → V2）—— 与 CPU 占比无关，32 连接下 RSS 压力为零。

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
