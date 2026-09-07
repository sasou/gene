# Gene 扩展极致并发优化 —— V2：未开始条目

> 版本：v6-split（2026-09-07）。本文件由原 `PERFORMANCE_OPTIMIZATION.md`（v6，2026-09-06）
> 按执行状态拆分：
> - **V1（`PERFORMANCE_OPTIMIZATION_V1.md`）**：已修复 / 已实施 / 已结案条目及其待验证项。
> - **V2（本文件）**：尚未开始的条目与需专项设计的项目。
>
> 章节编号沿用原 v6 文档；跨文件引用以「→ V1」标注。
> 本文中 §1.x、§2.1–§2.3、§3.1、§3.4、§4.4、§5.1、§5.3、§6、§8.1–§8.2、§9、§10 见 V1。
>
> 准入原则、执行纪律与验证方法沿用 V1 的 §0 与 §9，不在此重复。
> 简述：D1 每协程固定成本 / D2 不可让出区间 / D3 每并发单位内存 / D4 排队与饱和；
> A 类正确性缺陷无门槛，B 类按 D1–D4 增长论证准入，C 类纯固定成本微优化为低风险背景任务。

---

## 2. 轨道 A：D1/D2 —— 每协程成本与不可让出区间（未开始部分）

> §2.1–§2.3 已实施/已消除，待验证项见 V1。

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
- **附带**：`webscan.c` 的 static 正则字符串与 `preg` 函数指针受 §1.4 → V1 约束。

### 2.7 【D1，低】其余每请求固定成本
- `orm/meta.c:147-236`：请求级元数据命中后仍 `zend_string_copy` 5 个字段
  → 出借指针 + `from_cache` 标志。
- `orm/model.c:763-797`：`findMany(preserveOrder)` 多次 `zval_get_string` → 预归一化 ids。
- `mvc/model.c:82-122`、`service/service.c:81-122`：`__get/__set` 每次类名 + DI 查找
  → 缓存 `zend_string*` 类名（受 §1.4 → V1 约束）。
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
  benchmark 部分已落地（§8.2 → V1）。**OpenSSL EVP 直接链接暂缓**（新增构建依赖）。
- `gene_preg_match` / Validate：PHP `preg_match` **本身已用 PCRE 编译缓存**，
  直调 `pcre_get_compiled_regex_cache()` 只省 PHP 调用帧。
  仍可做的是 `http/validate.c:781-970` 内置规则 C 层实现，避免 `gene_factory_call` 回 PHP。
- ZTS 下热函数内把 `GENE_G(runtime_type)` 等只读值读入局部变量一次。
- `router.c` 常量 `strlen(GENE_ROUTER_SAFE)` → `ZEND_STRL`：
  **先看反汇编**，编译器多半已折叠；没有运行时指令可省则不制造无效 diff。

**以上全部为 §0.2 → V1 的 C 类**：低风险、diff 小时顺手做，标注为专项路径收益，不占排期。

---

## 3. 轨道 B：D3 —— 每并发单位内存决定并发上限（未开始部分）

> §3.1、§3.4 已实施，待验证项见 V1。

### 3.2 【依赖 §3.1 → V1】`Memory::get()` 借用读
- **现状纠正**：
  - `Memory::get()` 走 `gene_memory_zval_local()`（`memory.c:1299`，深拷贝），
    不是 `_local_copy()`（后者用于 `Gene\Cache` 业务读）；
  - `LONG/DOUBLE/NULL/BOOL` **已经**是 `ZVAL_COPY_VALUE`。
- **字符串借用**不能用「persistent/interned 标志」简单实现：
  代码注释已记录零拷贝路径此前因业务缓存覆盖/淘汰导致 UAF 而被**主动撤销**
  （`memory.c:404-409`）。
- **保留项**：`getBorrowed()` 仅可用于「整个借用期内保证无覆盖、无删除、无 TTL 清理、
  无协程切换」的内部路径。**风险：高**，需先完成 §3.1 → V1 并明确框架表只读不变式。

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
     写入 §6.1 → V1 替代当前的示例值堆砌。
- **sweep 成本**：`co_contexts` 软阈值触发的冷却式 sweep（`gene.c:1207-1224`）
  已导出 `co_contexts_sweep_count/scanned/us/skipped`。
  高并发下需确认 sweep 的**单次耗时**不进入尾延迟；若进入，
  改为分摊式（每次请求扫固定小批）而非阈值触发的整表扫。
- **`swoole_auto_cleanup` 默认 0**：开启后为**每个新分配的上下文**注册一次
  `Swoole\Coroutine::defer`（`gene.c:1088-1094, 1240-1242`）——
  这是 D1 成本（每协程一次 PHP defer 注册）。
  需实测其代价，并在文档中明确它是「漏调 `cleanup()` 的兜底」而非推荐常态。
  v5 §7.1 直接建议 `= 1`，在极致并发下这个建议需要重新用数据支撑。

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

## 4. 轨道 C：D4 —— 排队、饱和与尾延迟（未开始部分）

> §4.4 已结案，见 V1。

### 4.1 【D4】路由复杂度：`chird` 占位子路由线性扫描
- **位置**：`router/router.c:220-375` `get_path_router_inner`
- **现状（已核对）**：静态 segment **已经**先走 HashTable 精确查找；
  线性扫描只发生在占位子路由（`chird`，按占位符名保存）上，
  且当前匹配代码不按「正则类型/首字符」检查当前 segment。
- **v1 方案作废**：「静态 → 正则 → 泛型分桶 + 首字符索引 → 近 O(1)」不成立 ——
  静态本就不在线性扫描里，泛型占位之间的区别通常在**后续路径分支**而非本段首字符。
- **为什么属 D4**：成本 ∝ 同层占位路由数 × 路径深度，且**每请求**发生。
  它不会出现在简单路由的 flamegraph 里（这正是 §10.1 → V1 的采样场景），
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
- **待决策只有**：① 默认值是否 0→1；② 失效机制（**见 §1.1 → V1，这是硬前置**）；
  ③ action 的 `zend_function*` 是否进描述符 —— **结论：不进**。
  同一 leaf 可经 `:c`/`:a` 派发到不同 controller/action，单指针缓存不成立（§8.2 → V1 已按此实现）。
- **排期**：§1.1 → V1 的 generation 失效机制落地并通过 ASAN 后，
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

## 5. 观测：让并发成本可见（未开始部分）

> §5.1 已导出现状与 §5.3 判读纪律见 V1。

### 5.2 待新增（按并发诊断价值排序）

| 优先级 | 指标 | 诊断什么 |
|---:|---|---|
| 1 | `cache_read_locked` / `cache_read_lockfree` / `cache_lock_wait_us` | §2.3 → V1 的性能悬崖是否已发生、锁路径占比（拆表后须区分框架表跳锁与业务表带锁） |
| 2 | `db_pool_wait_us` / `redis_pool_wait_us`（含 p99 或分桶） | §2.2 → V1 的 1 ms miss 与排队延迟 |
| 3 | `db_pool_idle_miss` / `redis_pool_idle_miss` | **已完成 2026-09-07**；§2.2 → V1 的 miss 率 |
| 4 | `route_pc_hit` / `route_pc_miss` / `route_pc_generation` | generation 已完成；hit/miss 待补，诊断 §4.2 有效性与 §1.1 → V1 失效 |
| 5 | `chird_scan_steps`（占位子路由线性扫描步数累计） | §4.1 是否真的在放大 |
| 6 | 每请求分配次数（低开销计数，可专项构建开启） | §2.5 / §2.6 的验收 |
| 7 | `ctx_bytes_per_context` + worker RSS 采样 | §3.3 的 D3 标定 |

原则：**热路径计数器必须是单个非原子 `GENE_G(x)++`**（进程本地，无同步成本，
仅监控用途），高开销的（如 `wait_us` 直方图）放在 opt-in 或专项构建后面。

---

## 7. 单独评审的 API / 兼容性项目（不混入透明优化批次）

### 7.1 SQL 历史 JSON 编码（开发模式）
`db/pdo.c:1153-1171`、`mysql.c:175-212`：`run_environment=0` 时每条 SQL `json_encode`
参数入历史。生产必须 `gene.run_environment >= 1`；当前默认已是 `1`，§6.1 → V1 生产样例固定为 `2`。
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
**必须在 §6.2 → V1 之后重新测量。**

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
同时受 §1.4 → V1 的 static 生命周期约束。
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

## 8. 执行顺序（未开始部分）

> §8.1 第零批状态与 §8.2 已完成历史见 V1。

### 8.3 轨道推进顺序

| 轨道 | 顺序 | 条目 | 主验收维度 |
|---|---:|---|---|
| A（D1/D2） | A1 | §2.1 → V1 级别 2 已实施；**级别 3 C-API 独立待评估** | 待补每 DB/Redis 操作 ns、`cas_abandoned=0` |
| A | A2 | §2.2 → V1 已实施 | 待补 p95/p99；miss 率指标已导出 |
| A | A3 | §3.1 → V1 拆表已实施；§2.3 → V1 分表锁观测待补 | 框架跳锁/业务带锁占比可见 |
| A | A4 | §2.6 Webscan 去对象化 | 构造/析构与分配计数 |
| A | A5 | §2.5 ctx arena | 分配数 + **RSS/协程**（D3 回归预算） |
| A | A6 | §2.4 `mget()` 单锁 | 批量 CPU **与写方 p99 双侧** |
| B（D3） | B1 | §3.3 容量标定与换算表 | RSS/并发拟合 |
| B | B2 | §3.4 → V1 PID 生命周期约束已实施；计数保留 Atomic | 待补 fork/Swoole 语义 + `cas_abandoned=0` |
| B | B3 | §3.1 → V1 拆表已实施 | 待补 Linux 锁路径、业务 rehash、ASAN/RSS |
| B | B4 | §3.5 timer 合并、`ping_on_get`、TTL sweep | timer 数、探活往返、RSS |
| C（D4） | C1 | §4.1 路由复杂度（先固化优先级语义） | 深层/多占位专项基准 |
| C | C2 | §4.2 route_pc opt-in 灰度（§1.1 → V1 完成后） | 同 C1 基准 |
| C | C3 | §4.3 buffered API 设计、§4.5 curl pool | TTFB、分块间隔、连接复用率 |

**§0.2 → V1 的 C 类条目（§2.7 全部）不占排期**，低风险时随手合入并标注收益范围。

---

## 附：验证与配置基线

未开始条目的验收统一遵循 V1 的 §9（验证方法：负载族、profiling、微基准规范、
压测与回归矩阵、ASAN 回归）与 §6（生产配置基线：Gene INI、宿主 OPcache、模板编译配置）。
执行任何条目前先确认 V1 对应「待验证」项的基线数据已就位。
