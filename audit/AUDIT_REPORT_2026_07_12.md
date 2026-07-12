# Gene 扩展性能与内存优化技术实施方案（2026-07-12）

> 适用版本：Gene 5.6.8（develop）
>
> 范围：PHP-FPM 高并发、Swoole 高并发、内存泄漏与高内存占用
>
> 方法：源码静态分析、历史审计交叉复核、现有测试与验证脚本盘点
>
> 重要说明：本方案不负责 Windows、Linux 扩展编译及运行环境搭建。扩展二进制、PHP/FPM/Swoole 环境、php.ini 和服务地址由人工准备并确认可用；方案执行侧负责制定验收脚本、执行测试、采集证据和输出验收结论。文中的性能收益均为待验证目标；README 和源码注释中的历史 QPS、纳秒级数据不能替代本轮基准。

---

## 一、结论摘要

### 1.1 当前状态

Gene 已具备较完整的双运行时优化基础：

- FPM 走 `default_ctx` 快路径，不承担协程识别成本；请求级对象由 RINIT/RSHUTDOWN 管理。
- Swoole 已具备 `vm_stack` 快路径、协程 ID C-API 快路径、协程上下文池、`co_contexts` sweep、DB/Redis 连接池、workerReady 后缓存免锁读、可选路由预编译。
- 5.6.8 已修复 DB 池协程识别、Redis 池计数 CAS、`co_contexts_max=0` 无界增长等关键问题。
- FPM 未发现已确认的经典请求级泄漏；Swoole 的主要风险是常驻数据结构缺少容量策略或业务未遵守 cleanup 生命周期。

当前最主要的缺口不是继续堆叠微优化，而是：

1. 缺少可复现的 FPM/Swoole 基准体系和变更前基线；
2. 缺少面向人工提供环境的自动验收脚本，以及 ASAN/Valgrind、长跑 RSS 的统一结果判定；
3. 部分已有优化默认关闭，生产可用性尚未通过完整运行时证据闭环；
4. persistent 缓存仍有无上限分区；
5. T2/T3 优化项尚未经过 profiling 证明其真实占比。

### 1.2 总体策略

实施顺序必须遵循：

**先建立基线和可观测性 → 再封住无界内存 → 验证已有快路径 → 按 profile 优化热点 → 最后考虑结构性重构。**

优先级建议：

- P0：明确人工交接清单，建设一键验收脚本、标准 benchmark harness 和 24 小时 RSS 采样。
- P1：FPM 闭包源码缓存容量上限、Swoole Cache 上限提醒、Swoole cleanup/上下文观测。
- P1：验证并灰度启用 `route_precompile`，增加 worker 启动预热。
- P2：配置路径解析、SQL 构造链、路由注册期优化。
- P3：视图解析状态机、C 层连接池计数器、路由递归迭代化。

---

## 二、现状与证据

### 2.1 运行时与生命周期

核心 INI 定义位于 `src/gene.c:140-154`：

- `runtime_type < 2`：FPM/CLI；
- `runtime_type >= 2`：Swoole/协程；
- `co_contexts_max=1024`；
- `ctx_pool_max=256`；
- `cache_max_items=0`（业务缓存不限）；
- `route_precompile=0`（默认关闭）。

请求上下文结构位于 `src/gene.h:108-153`。主要生命周期：

- MINIT：初始化 persistent cache、锁及子模块；
- RINIT：初始化或重置 `default_ctx`，Swoole 下准备 `co_contexts` 和 ctx pool；
- RSHUTDOWN：销毁请求级上下文及相关字段；
- MSHUTDOWN：销毁 persistent cache、LRU、路由预编译缓存、闭包源码缓存、连接池静态资源。

相关实现：

- `src/gene.c:285-478`：request context 初始化、reset、destroy；
- `src/gene.c:480-611`：Swoole ctx pool；
- `src/gene.c:722-868`：`co_contexts` sweep 与 `gene_request_ctx()`；
- `src/gene.c:919-1145`：请求级、进程级关闭及模块钩子。

### 2.2 已有性能机制

FPM：

- `gene_request_ctx()` 直接返回 `default_ctx`，见 `src/gene.c:789-792`；
- 闭包路由源码跨请求缓存，见 `src/router/router.c:1568-1639`；
- 路由缓存批量读取、字符串长度缓存、栈缓冲等热路径优化已存在；
- 生产模式可通过 `run_environment=2` 跳过 DB benchmark；
- 模板编译缓存可避免重复解析。

Swoole：

- `EG(vm_stack)` 身份快路径，见 `src/gene.c:793-798`；
- dlsym 直调 Swoole `get_current_cid()`，失败自动回退 PHP 方法，见 `src/gene.c:104-198`；
- `co_contexts` 默认软上限和存活 sweep，见 `src/gene.c:722-780,845-857`；
- ctx 结构体池与预热，见 `src/gene.c:480-611`；
- 路由预编译描述符缓存，见 `src/router/router.c:1337-1366`；
- workerReady 后配置/路由缓存冻结，读路径可跳过锁；
- DB Pool、RedisPool 支持协程复用，Redis 计数递减已改 CAS。

### 2.3 已有内存保护

- `path_params` 和 `request_attr` 小 HashTable 复用；超过 128 桶时主动丢弃 backing storage，见 `src/gene.c:304-358`；
- `di_regs`、响应、视图变量、DB history 在 reset 时完全析构，避免 ctx pool 长期持有大数组；
- ctx pool 有 `ctx_pool_max` 硬上限；
- `co_contexts_max=0` 已回退到 1024，不再表示无限；
- `Gene\Cache` 已支持可选近似 LRU，见 `src/cache/memory.c:336-544`；
- persistent cache、route_pc、closure cache 在 MSHUTDOWN 有析构路径。

### 2.4 工程与验证缺口

- 当前无 CI 配置；
- 无 `.phpt` 测试；
- `test/` 是自研 PHP 测试框架，不是 PHPUnit；
- README 中历史 QPS 缺少同机、同配置、可复现脚本；
- `test/README.md` 声称 PHP 7.4+，但 `src/config.m4:28-32` 实际要求高于 PHP 8.0.0，文档不一致；
- `audit/AUDIT_REPORT_2026_07_03.md:702-710` 中 SQL 回归、CAS 压测、dlsym 可见性仍待执行；Linux 编译验证改由人工完成并提供环境确认单。

### 2.5 本方案执行边界与人工交接

本方案执行时不做以下工作：

- 不编译 Windows DLL；
- 不执行 Linux `phpize/configure/make`；
- 不安装或升级 PHP、FPM、Swoole、数据库、Redis；
- 不修改生产服务配置；
- 不替人工判断扩展 ABI、编译参数和目标机器兼容性。

人工介入需要为每套验收环境提供：

- 操作系统、CPU、内存和容器/虚拟机信息；
- PHP 版本、SAPI、NTS/ZTS、Zend Module API；
- Gene 扩展版本、二进制路径及 `php --ri gene` 输出；
- Swoole 版本及 `php --ri swoole` 输出（Swoole 环境）；
- 实际生效的 php.ini、FPM pool、Swoole server 配置；
- FPM/Swoole 基线与候选版本的独立访问地址；
- DB/Redis 测试账号和专用测试库；
- ASAN/Valgrind 构建由人工准备，若无法提供则对应安全验收项标记为“未执行”，不得判定通过；
- 环境准备完成确认，以及允许开始/停止测试服务的时间窗口。

验收脚本在启动负载前必须执行 preflight；版本、扩展、服务、权限或配置不满足时立即失败，不尝试自动编译或修复环境。

---

## 三、问题分级

### 3.1 已确认问题

#### P1：Swoole 业务缓存默认可无界增长

`gene.cache_max_items=0` 默认不启用 LRU。若业务将 `Gene\Cache` 当作进程内高基数缓存，worker RSS 会持续增加。

证据：

- `src/gene.c:152`；
- `src/cache/memory.c:336-357,515-516`。

该问题属于容量管理缺失，不是忘记 `efree` 的传统泄漏。

#### P1：Swoole cleanup 依赖业务正确调用

协程请求若未在 `finally` 中调用 `Application::cleanup(true)`，上下文会留在 `co_contexts`，直到达到 cap 后触发 sweep。

现有 cap/sweep 能避免无限增长，但不能替代正确生命周期；sweep 触发时还会带来 O(N) 扫描和尾延迟毛刺。

证据：

- `src/gene.c:722-780,845-857`；
- `demo/public/swoole.php:60-76`。

#### P2：FPM 闭包源码持久缓存无容量上限

`gene_closure_src_cache_put()` 直接写入 persistent HashTable，没有最大条目数。

证据：`src/router/router.c:1625-1639`。

通常路由集合有限，因此风险低于 Swoole 业务缓存；但热更新、动态生成路由或多租户路由会使 FPM worker RSS 随不同 key 增长。

#### P2：`Gene\Memory::set` 与 `cache_easy` 不受业务 LRU 约束

LRU 仅跟踪 `cache_layer_memory_write_depth > 0` 的 `Gene\Cache` 业务写。普通 `Memory::set` 和文件元数据 `cache_easy` 无独立容量上限。

证据：

- `src/cache/memory.c:512-516`；
- `src/cache/memory.c:671-692`。

这属于 API 设计约束，需要通过分区统计、文档和独立容量策略治理。

### 3.2 待运行时验证的风险

- `route_precompile=1` 的借用指针在全路由、hook、error、闭包、路由清理场景下是否始终无 UAF；
- dlsym 的 Swoole C++ 符号在目标 Swoole 5.x/6.x 发行构建中是否可见；
- RedisPool CAS 在极端争用下是否出现计数偏高或容量收缩异常；
- Swoole 运行方式下 RINIT/RSHUTDOWN 的真实触发频率是否符合当前生命周期假设；
- PHP 8.0–8.4 各版本下 persistent interned string 和 HashTable 自定义析构是否均通过 ASAN/Valgrind。

### 3.3 不应误判为泄漏

- Zend MM 已释放但 RSS 未下降：通常是 allocator arena/碎片化或 libc 未还给 OS；
- ctx pool、OPcache、persistent route/config cache 的稳定常驻内存；
- 单次大请求后 RSS 高水位，但 `Memory::stats()`、live allocation 和上下文条目已回落；
- 有 TTL 但未到清理时机的业务缓存；
- worker 退出时的 `still reachable`，需与 `definitely lost` 分开判断。

因此必须同时观察：

1. `memory_get_usage(true)`；
2. 进程 RSS/PSS；
3. `Gene\Memory::stats()`；
4. ASAN/Valgrind live allocation；
5. 业务缓存和连接池实际对象数量。

---

## 四、性能优化技术方案

## 4.1 FPM 高并发

### FPM-P0：建立生产基线

新增统一压测入口，至少包含：

1. 空路由 `/bench/ping`；
2. MCA 字符串路由；
3. 闭包路由；
4. 带 before/after hook 路由；
5. 单次配置读取与 10 次配置读取；
6. 单条 DB SELECT；
7. 模板渲染冷/热路径。

固定环境：

- PHP 8.2 和 8.4 NTS；
- OPcache 开启；
- FPM static 与 dynamic 两种 pm 配置分开测；
- 固定 worker 数、CPU affinity、数据库连接方式、日志级别；
- 预热 60 秒，正式采样至少 5 分钟，重复 5 轮；
- 输出 QPS、错误率、p50/p95/p99、CPU/request、RSS/worker。

基准工具建议：`wrk2` 或 `wrk`；延迟验收优先使用固定吞吐率，避免 coordinated omission。

### FPM-P1：生产配置收敛

推荐基线：

```ini
gene.runtime_type=1
gene.run_environment=2
gene.view_compile=1
gene.view_compile_check_mtime=0
```

同时启用和固定 OPcache 配置。禁止以 README 历史 QPS 作为验收值，所有结果使用同机 A/B 相对变化。

### FPM-P2：闭包源码缓存有界化

实施：

- 新增 INI：`gene.closure_src_cache_max`，默认建议 1024；
- 非正值表示禁用缓存，而不是无限；
- 超限策略优先采用“整表清空后写入”，避免在低价值冷路径实现复杂 LRU；
- `Memory::stats()` 增加 `closure_src_cache_items`；
- MSHUTDOWN 析构保持不变。

兼容策略：

- 默认值为 1024；为避免无上限持久缓存，负值与 `0` 均禁用缓存；
- 第一版也可不新增 INI，固定 1024，后续再开放配置。

验收：

- 动态生成 5000 个不同闭包位置/key，缓存条目不超过配置上限；
- FPM worker RSS 进入平台期；
- 命中场景性能不低于基线 2%；
- Valgrind `definitely lost: 0 bytes`。

### FPM-P3：路由注册期字符串构造优化

目标：将 `Router::__call` 路由注册中的重复 `snprintf` 改为长度计算 + 栈缓冲/单次分配 + `memcpy`。

前置条件：profile 证明路由注册在 FPM CPU 中占比显著。若标准 FPM 部署由 OPcache 加载但每请求仍执行路由配置，优化才有价值。

实施要求：

- 抽取统一 key builder，避免 20+ 处手工修改；
- 小于固定阈值走栈缓冲，长 key 单次 `emalloc`；
- 所有长度使用 `size_t`，先做溢出检查；
- 覆盖全部 HTTP verb、group、hook、error 路由。

验收：

- 路由注册微基准耗时降低至少 20%；
- 端到端 FPM 空路由吞吐提升至少 3%，否则不合并复杂重构；
- ASAN 无 OOB/UAF。

### FPM-P4：DB 与 SQL 构造热点

FPM 不适合在 Gene 内实现跨请求连接池。连接开销应优先通过：

- PDO persistent connection 的受控使用；
- ProxySQL/PgBouncer 等外部连接代理；
- 合理的 FPM worker 与数据库连接上限；
- 减少每请求建连和 DNS/TLS 开销。

扩展内部只优化 SQL 构造：

- profile `zend_read_property`、`zend_update_property_*`、`smart_str` 扩容占比；
- 缓存 property offset，直接访问已声明属性槽；
- 对字段、where、order 预估容量，减少 `smart_str` 扩容；
- 保留类型检查和对象布局兼容分支。

验收：

- 10 段链式 SQL 构造微基准 CPU 降低至少 10%；
- 完整 DB 请求 p99 不退化；
- PHP 8.0–8.4 矩阵回归。

---

## 4.2 Swoole 高并发

### SW-P0：生命周期标准化

所有请求必须采用：

```php
try {
    \Gene\Application::getInstance()->run();
} finally {
    \Gene\Application::cleanup(true);
}
```

Worker 生命周期必须包含：

- workerStart：加载路由/配置、创建连接池、调用 `workerReady()`；
- workerExit：停止池定时器；
- workerStop：`closeAll()`；
- request：先 `waitWorkerReady()`。

实施：

- 保留 `demo/public/swoole.php` 作为权威模板；
- 在 `cleanup()` 缺失无法自动检测时，通过 `co_contexts_items` 高水位告警间接检测；
- 文档明确 cleanup 必须位于 `finally`，不能只放成功分支。

### SW-P1：现有快路径验证与灰度启用

验证四组组合：

1. C-API off + route precompile off；
2. C-API on + route precompile off；
3. C-API off + route precompile on；
4. C-API on + route precompile on。

要求：

- `tools/verify_5_6_6_swoole.php` 全部通过；
- 四组 `RESULT-DIGEST` 一致；
- 覆盖直派、闭包、动态参数、before/after hook、404、异常路由；
- ASAN 运行至少 100 万请求；
- Swoole 5.x、6.x 各验证一个生产目标版本。

灰度：

- 第一期仅 5% worker 开启 `route_precompile=1`；
- 观察 crash、错误率、p99、RSS、route_pc 条目数；
- 连续 24 小时无回归再扩大。

### SW-P2：route_pc 启动预热

现状为首次命中某路由时 lazy resolve，可能造成首批请求延迟毛刺。

实施：

- 在 `workerReady()` 冻结路由树后遍历全部 leaf；
- 对每个 leaf 调用独立的 `gene_route_pc_resolve()`，只构建描述符，不执行 handler；
- 预热失败时记录 WARNING 并保留慢路径；
- 增加预热计数、耗时、失败数到 stats 或启动日志；
- 不在 RINIT 或请求中重复预热。

风险：

- route_pc 借用路由树/fn_cache 指针，必须确保 workerReady 后二者不可变；
- Router::clear/load 若允许在 workerReady 后执行，必须禁止、使 route_pc 失效或重建；
- 预热会增加 worker 启动时间和常驻内存。

验收：

- 首 1000 请求 p99 比 lazy 方案降低至少 15%；
- 稳态 QPS 不退化；
- route_pc 条目数等于可派发 leaf 数；
- 预热后路由变更尝试被拒绝或正确失效；
- ASAN 无借用指针 UAF。

### SW-P3：协程上下文 sweep 降毛刺

现有 sweep 达到 cap 才全表扫描，且现已仅清理运行时确认死亡的协程上下文；超过 cap 但仍存活的上下文会告警并保留。进一步降低扫表毛刺的改进方向：

- `cleanup()` 正常时保持零额外成本；
- 增加高水位/低水位：达到 80% cap 时每次最多扫描固定 batch；
- 保存 sweep cursor，分批处理，避免单请求 O(N)；
- C-API 可用时使用 `get_by_cid`；
- C-API 不可用时保留 PHP fallback；
- 不以释放内存为由破坏正在运行协程的上下文正确性。

验收：

- 10 万协程风暴下单次 sweep CPU 时间受 batch 上限约束；
- 正常 cleanup 时 sweep 次数接近 0；
- 故意遗漏 cleanup 时条目最终回到安全范围；
- 活跃协程状态零丢失。

### SW-P4：配置路径解析缓存

`gene_memory_get_by_config()` 每次复制 path、`strtok`、逐段 hash 查询。

实施选项：

1. 启动期将高频路径编译为 segment 数组；
2. workerReady 后为完整 path 建立结果指针缓存；
3. 缓存 key 使用 config root key + path；
4. 只在配置冻结后启用；
5. 配置清理/重载时整体失效。

结果指针缓存收益更高，但依赖配置不可变；segment 缓存风险更低。建议先实现 segment 缓存并 profile，再决定是否缓存最终 zval 指针。

验收：

- 每请求 10 次、50 次配置读取两档；
- 微基准 CPU 降低至少 15%；
- workerReady 前后、配置缺失、非数组中间节点结果与旧实现一致；
- 失效后无 UAF。

### SW-P5：连接池热路径

短期：

- 完成 RedisPool CAS 高并发验证；
- 暴露 DB/Redis 池 active/idle/waiters/total/capacity/timer 回收统计；
- 明确每 worker 独立池，容量按 worker 数计算；
- 连接池等待必须有超时，不能无限挂起。

长期：

- profile 证明 Swoole\Atomic PHP 调用占比显著后，再将进程内计数迁到 C11/GCC 原子；
- 若计数必须跨进程共享，不可直接换成进程内原子；
- 保留实现切换开关和一致性校验期。

验收：

- 500 协程并发、每协程 10000 次借还；
- 计数永不为负，不超过 max；
- 压测结束 active=0，idle 与 total 一致；
- 无等待协程遗留；
- 连接失败、超时、异常、取消路径均归还计数。

### SW-P6：请求上下文 char* arena

当前每请求 method/path/module/controller/action 等字段分别分配和释放。源码已说明，直接保留 buffer 会破坏“指针非 NULL 表示已设置”的语义。

因此不建议直接池化旧指针。若 profile 证明 allocator 占比足够高，采用：

- ctx 内增加单一 request arena；
- 字段仍在 reset 时置 NULL；
- arena 内存由独立 base/capacity/used 管理；
- 新请求 reset 仅 `used=0`，字段指针全部清 NULL；
- 超过阈值的大块请求后主动 shrink；
- 禁止 arena 指针逃逸到 persistent cache。

合并门槛：

- 端到端空路由 QPS 提升至少 3% 或 CPU/request 降低至少 5%；
- ASAN 全量通过；
- 重点验证字段 bleed、yield 后指针有效性、异常 cleanup。

若收益低于门槛，维持现状。

---

## 五、内存优化技术方案

### 5.1 先区分泄漏、无界缓存和碎片化

诊断分类：

- 真泄漏：对象不可达但未释放，Valgrind/ASAN live allocation 随请求增长；
- 有界常驻：ctx pool、route/config、OPcache 达到稳定平台；
- 无界缓存：条目数与 RSS 同时随不同 key 单调增长；
- 碎片化：逻辑条目回落、Zend usage 回落，但 RSS 不回落；
- 业务持有：用户 static/global/singleton/第三方库持有对象。

每个内存问题必须记录：

- 触发请求类型；
- 请求总数与并发；
- `Memory::stats()` 各分区条目；
- Zend used/real memory；
- RSS/PSS；
- allocator 配置；
- cleanup、GC、cache cap 配置；
- worker 启停时间。

### 5.2 persistent 缓存分区治理

扩展 `Memory::stats()`，至少增加：

- `cache_items_total`；
- `cache_business_items`；
- `cache_easy_items`；
- `closure_src_cache_items`；
- `fn_cache_items`；
- `route_pc_items`；
- `co_contexts_items/max`；
- `ctx_pool_size/max`；
- DB/Redis pool stats。

容量策略：

- `Gene\Cache`：生产必须显式设置 `cache_max_items`；
- 未设置时 Swoole workerReady 输出一次 NOTICE，不在每请求重复；
- `Memory::set`：增加独立分区/统计，默认禁止 workerReady 后动态高基数写；
- `cache_easy`：增加 max/TTL 或按文件 mtime 失效；
- `closure_src_cache`：固定或可配置 cap；
- `fn_cache`、route_pc：条目应与路由规模同阶，超出预期报警。

### 5.3 Swoole RSS 控制

建议生产默认值不是固定照抄，而是按单 worker 峰值并发计算：

```ini
gene.runtime_type=2
gene.ctx_pool_max=<单 worker P95 并发协程数>
gene.ctx_pool_prewarm=<单 worker常态并发或其 50%-100%>
gene.co_contexts_max=<单 worker允许的最大活跃协程数 + 安全余量>
gene.cache_max_items=<业务容量预算 / 单条平均字节>
```

容量预算方法：

1. 采样 1000 个典型 cache entry 的平均 persistent 字节；
2. 为 Gene cache 分配每 worker 内存预算；
3. `max_items = floor(预算 / P95 entry size)`；
4. 保留 20%–30% 余量给 HashTable bucket 和 allocator overhead；
5. worker_num 翻倍时总预算也会翻倍。

### 5.4 大请求后的高水位回收

现有 HashTable 超 128 桶即丢弃策略应保留，并扩展到所有可复用容器：

- request_attr/path_params：已有；
- 若未来复用 view_vars/DI history，必须同样设置 shrink 阈值；
- arena 若实施，超过阈值后不进入 pool；
- 单次超大 header/body 不应保存在 request ctx；
- 连接池中异常大响应对象不得作为连接属性长期持有。

### 5.5 Worker 回收是最后防线

即使没有泄漏，长期运行仍可能因第三方扩展、libc arena 或业务静态对象造成 RSS 高水位。生产应设置：

- FPM `pm.max_requests`；
- Swoole `max_request` + 随机抖动，避免全部 worker 同时重启；
- RSS 超预算的优雅 worker reload；
- reload 前输出 Memory stats，便于区分 Gene 与业务问题。

Worker 回收不能替代修复确认的无界缓存或 UAF。

---

## 六、基准、压测与泄漏检测方案

### 6.1 测试矩阵

以下矩阵由人工提供已安装、已启动的环境，验收侧只做识别、校验和测试：

- PHP：8.0、8.1、8.2、8.3、8.4，Linux x86_64 NTS；
- FPM：OPcache on，Gene on/off；
- Swoole：目标 5.x、6.x 各一个版本；
- 二进制类型：release；如需内存安全验收，由人工另行提供 `--enable-debug`、ASAN/Valgrind 可运行环境；
- 开关：C-API 0/1、route_precompile 0/1、cache cap 0/有限值。

不要求一次提供全部 PHP 小版本。最低有效验收集为：

1. 一个生产目标 PHP/FPM 环境；
2. 一个生产目标 PHP/Swoole 环境；
3. 基线扩展与候选扩展各一套相同规格实例；
4. 如要给出“无内存泄漏/UAF”结论，必须额外提供 ASAN 或 Valgrind 环境。

### 6.2 验收脚本总体设计

建议新增 `tools/acceptance/`，脚本只消费人工准备的环境：

```text
tools/acceptance/
├── preflight.php                 # 版本、扩展、INI、依赖和服务检查
├── functional.php                # TestRunner、verify、路由和 SQL 回归编排
├── fpm_benchmark.php             # FPM 场景准备、探针和结果归档
├── swoole_benchmark.php          # Swoole 四组开关与 digest 验收
├── swoole_context_soak.php       # cleanup/co_contexts 长跑
├── cache_memory_soak.php         # Cache cap、closure cache、RSS 长跑
├── pool_concurrency.php          # DB/Redis 池并发、异常与计数验收
├── collect_process_metrics.php   # RSS/PSS/CPU/FD/线程/协程采样
├── compare_results.php           # 基线与候选版本统计比较
└── run_acceptance.php            # 总入口，按 profile 编排并生成报告
```

统一调用方式：

```bash
php tools/acceptance/run_acceptance.php \
  --profile=fpm \
  --config=tools/acceptance/config/fpm.json \
  --output=audit/results/fpm-<run-id>

php tools/acceptance/run_acceptance.php \
  --profile=swoole \
  --config=tools/acceptance/config/swoole.json \
  --output=audit/results/swoole-<run-id>
```

配置文件只记录服务地址、并发、时长、预期版本、指标阈值及测试数据源；不得包含生产密码。敏感参数通过环境变量注入。

每次执行固定产出：

- `environment.json`：人工环境声明与脚本实测信息；
- `preflight.json`：前置检查结果；
- `functional.json`：功能用例结果；
- `benchmark.json`：每轮 QPS/延迟/错误率/CPU；
- `memory.csv`：时间序列 RSS/PSS/Zend/业务条目；
- `comparison.json`：基线与候选版本的差异和置信区间；
- `acceptance.md`：通过、失败、阻塞、未执行项及证据路径。

### 6.3 FPM 性能测试

建议命令示例：

```bash
wrk -t4 -c100 -d5m --latency http://127.0.0.1/bench/ping
wrk -t8 -c400 -d5m --latency http://127.0.0.1/bench/ping
```

每轮：

- 预热 60 秒；
- 正式采样 5 分钟；
- 重复 5 轮；
- 删除最高/最低值后取中位数；
- 记录 FPM slowlog、CPU、context switch、worker RSS。

不得只看 QPS；错误率和 p99 必须同时满足。

### 6.4 Swoole 性能测试

场景：

- 单 worker：测扩展热路径上限；
- 多 worker：测实际吞吐和调度；
- 固定连接数 100/500/1000；
- 路由简单/复杂、配置读取密集、DB/Redis 池三类；
- 冷启动首 1000 请求与预热后稳态分开。

观测：

- QPS、p99、event loop lag；
- coroutine count；
- `co_contexts_items`；
- ctx pool hit/miss（建议新增）；
- pool waiters、borrow latency；
- worker RSS/PSS；
- sweep 次数、扫描条目与耗时（建议新增）。

### 6.5 FPM 内存验证

Valgrind 环境及扩展由人工提供，验收脚本只负责执行：

```bash
USE_ZEND_ALLOC=0 valgrind \
  --leak-check=full \
  --show-leak-kinds=all \
  --track-origins=yes \
  php -d extension=gene.so tools/verify_5_6_6.php
```

要求：

- `definitely lost: 0 bytes`；
- `indirectly lost: 0 bytes`；
- 对 PHP/系统库已知 reachable 建 suppressions；
- 单请求、1000 循环、异常路径分别执行。

FPM 长跑：

- 10 万、100 万请求两档；
- 固定 `pm.max_requests=0` 观察自然趋势；
- 再使用生产 `pm.max_requests` 验证回收策略；
- 动态闭包 key 专项验证 cache cap。

### 6.6 Swoole 内存验证

四组专项：

1. 正常 cleanup，100 万请求；
2. 1%、10%、50% 请求故意遗漏 cleanup；
3. `Gene\Cache` 写入 100 万不同 key，cap=1000；
4. 路由预编译、连接池、异常/超时/取消混合流量。

长跑：

- 最少 24 小时；
- 每 10 秒采样一次；
- 以预热后第 1 小时作为基线；
- 对 RSS 做线性回归，不能仅比较起止两点；
- 记录 worker reload，排除重启造成的假稳定。

### 6.7 Profiling

人工提供具备权限和对应工具的 Linux 环境后，验收侧执行：

- `perf record/perf report`：C 热点；
- eBPF/off-CPU：连接池等待和 syscall；
- heaptrack 或 jemalloc profiling：分配热点与碎片；
- ASAN/LSAN：UAF、OOB、泄漏；
- Valgrind：退出时精确泄漏核对。

优化项只有在目标函数达到以下任一条件时进入开发：

- 占 CPU samples ≥3%；
- 贡献 p99 明显毛刺；
- 分配次数占总分配 ≥5%；
- 导致 RSS 随请求单调增长。

---

## 七、量化验收标准

### 7.1 正确性门禁

- 人工提供的扩展可被目标 PHP 正常加载，`php --ri gene`、版本、SAPI、NTS/ZTS 和预期配置完全匹配；
- preflight 0 失败，基线与候选环境除 Gene 版本/目标开关外无影响比较的配置漂移；
- `php test/TestRunner.php` 0 失败；
- FPM/Swoole verify 脚本全部通过；
- Swoole 四组开关 RESULT-DIGEST 一致；
- 全路由、hook、404、异常、SQL quote 回归全部通过；
- ASAN：0 UAF、0 OOB、0 double-free；
- Valgrind：`definitely lost: 0 bytes`。

### 7.2 性能门禁

采用相对基线：

- 任意优化不得使错误率增加；
- p99 不得退化超过 3%；
- CPU/request 不得退化超过 3%；
- 仅微基准提升但端到端提升小于 2%的复杂优化不进入主线；
- route_pc 预热目标：冷启动首批 p99 降低 ≥15%；
- 配置路径缓存目标：配置密集微基准 CPU 降低 ≥15%；
- SQL 构造优化目标：10 段链式构造 CPU 降低 ≥10%；
- 路由注册优化目标：注册微基准耗时降低 ≥20%。

这些是进入/保留优化的工程门槛，不是预先保证的结果。

### 7.3 内存门禁

- 正常 cleanup 的 Swoole 100 万请求：`co_contexts_items` 回到接近 0 或稳定活跃值；
- 24 小时长跑：预热后 RSS 线性斜率不显著为正；建议初始门槛 `<1 MB/worker/hour`，随后用真实业务基线收紧；
- `cache_max_items=N`：业务跟踪条目不超过 N；
- closure cache：条目不超过配置上限；
- ctx pool：不超过 `ctx_pool_max`；
- 单次病态请求后，逻辑条目在 cleanup 后回落；
- 连接池结束时 active=0、waiters=0、total 在配置范围内。

不能简单规定“RSS 增长小于 5%”适用于所有 worker 大小；绝对斜率、逻辑条目和 live allocation 必须联合判断。

### 7.4 验收结论规则

总报告只允许四种状态：

- **PASS**：必选用例全部执行且达标，无正确性或安全失败；
- **FAIL**：任一必选用例失败、结果 digest 不一致、性能越过退化阈值，或发现 crash/UAF/OOB/leak；
- **BLOCKED**：人工环境、权限、服务、测试数据或扩展二进制不满足前置条件；
- **PARTIAL**：功能和性能已执行，但 ASAN/Valgrind、24 小时长跑等非当前批次必选项未提供环境。

判定约束：

- BLOCKED 不得写成 FAIL，也不得自动修改环境后重试；
- 未提供 ASAN/Valgrind 环境时，不得输出“无内存泄漏/UAF”的结论；
- 基线和候选环境配置漂移时，性能比较无效，必须重新配照；
- 单轮数据不得判定性能通过，必须满足预热、重复次数和统计规则；
- 每个失败项必须记录命令、时间、环境、原始输出和最小复现步骤。

### 7.5 标准执行顺序

1. 人工提交环境交接单和基线/候选地址；
2. 执行 `preflight.php`，不通过则输出 BLOCKED；
3. 执行功能回归，失败则停止性能测试并输出 FAIL；
4. 执行 FPM 或 Swoole 性能 profile；
5. 执行内存压力和长跑 profile；
6. 在人工提供的诊断环境执行 ASAN/Valgrind/profile；
7. `compare_results.php` 生成差异；
8. `run_acceptance.php` 汇总为最终验收报告；
9. 人工复核结果并决定是否进入灰度。

---

### 7.6 Windows 本地验收记录（2026-07-12）

本轮在 Windows 本地人工准备环境执行，未进行扩展编译、依赖安装或服务配置修改。

环境信息：

- OS：Windows 10.0.26200；
- PHP：8.1.34 CLI，NTS，x64；
- Gene：5.6.8，扩展已加载；
- `gene.runtime_type=1`；
- Swoole：未安装或未加载；
- `wrk`：未安装或不在 PATH。

执行命令：

```bash
php test/TestRunner.php
php tools/verify_5_6_6.php
php tools/acceptance/run_acceptance.php --profile=fpm --config=tools/acceptance/config/fpm.example.json --output=audit/results/local-fpm-20260712
php tools/acceptance/run_acceptance.php --profile=swoole --config=tools/acceptance/config/swoole.example.json --output=audit/results/local-swoole-20260712
php -d gene.cache_max_items=100 tools/acceptance/cache_memory_soak.php --keys=5000
php tools/acceptance/profile_gate.php tools/acceptance/config/profile.example.json
```

结果：

- FPM preflight：**PASS**；
- FPM functional：**PASS**。`tools/verify_5_6_6.php` 的 15 项回归全部通过；
- FPM benchmark：**FAIL**。功能回归完成，但 5 轮 benchmark 均因 Windows 环境找不到 `wrk` 退出，未产生有效 QPS/延迟数据；
- Swoole profile：**BLOCKED**。`swoole` 扩展未加载，且当前 `gene.runtime_type=1`；
- Cache capacity soak：**PASS**。5000 个 key 在 `gene.cache_max_items=100` 下业务条目稳定为 100；
- profile gate：4 个候选优化项均为 **DEFER**，当前没有达到 profile 准入阈值；
- PHP 脚本语法检查：全部通过。

测试套件特别说明：

`test/TestRunner.php` 返回退出码 0，但本轮输出显示 12 个测试文件均为 `0 passed, 0 failed`，总测试数为 0。因此只能确认测试入口未报错，不能据此宣称框架测试用例已实际覆盖通过。直接在仓库根目录执行该命令会因工作目录错误报告测试文件不存在，正确工作目录为 `test/`。

证据目录：

- `audit/results/local-fpm-20260712/`；
- `audit/results/local-swoole-20260712/`。

本轮不能给出 Swoole、FPM 压测、ASAN/Valgrind、百万请求或 24 小时 RSS 无泄漏结论。FPM 结果按规则记为 **FAIL（性能工具缺失）**，Swoole 结果记为 **BLOCKED（环境前置条件不满足）**，不将环境缺失误判为 Gene 功能缺陷。

---

## 八、分阶段实施计划

### 阶段 0：验收脚本与人工环境交接（第 1 周）

状态：**部分完成（工具已落地，目标环境验收待人工交接）**

交付：

- 已新增 `tools/acceptance/preflight.php`、`run_acceptance.php`、`functional.php`、FPM/Swoole benchmark 编排、RSS 采样、结果比较及示例 profile 配置；
- 已统一输出 `environment.json`、`preflight.json`、`functional.json`、`benchmark.json`、`memory.csv`、`acceptance.json`；
- 已复用 `test/TestRunner.php`、`tools/verify_5_6_6.php` 与 `tools/verify_5_6_6_swoole.php`，并修正 `test/README.md` 的 PHP 最低版本为 8.0；
- 标准 benchmark endpoint、人工环境交接单、基线/候选服务地址及真实基线结果仍待人工提供。

退出条件：

- [x] 脚本在不满足扩展、SAPI、INI 或服务前置条件时输出 `BLOCKED`，且不尝试编译、安装或修复环境；
- [x] 一条命令可运行指定 profile 并生成结构化验收结果；
- [ ] 人工提供的基线/候选扩展均通过 preflight；
- [ ] 同一 commit 连续 5 轮结果波动可控；
- [ ] 已保存优化前基线。

### 阶段 1：内存护栏（第 2 周）

状态：**代码与专项脚本已落地，运行时安全验收待目标环境执行**

交付：

- [x] 新增 `gene.closure_src_cache_max`（默认 `1024`；`0` 表示禁用）；新 key 达到上限时清空闭包源码缓存后写入，并通过 `closure_src_cache_flushes` 记录次数；
- [x] Swoole `workerReady()` 后，对 `gene.cache_max_items=0` 每 worker 仅输出一次 NOTICE，未改变既有无限容量语义；
- [x] `Memory::stats()` 新增 closure cache、route_pc、业务缓存分区、ctx pool hit/miss、`co_contexts` 高水位及 sweep 次数/扫描量/耗时；
- [x] 新增 `cache_memory_soak.php`、`swoole_context_soak.php` 与 `pool_concurrency.php` 专项脚本；
- [ ] ASAN/Valgrind、百万请求和 24 小时 RSS 结论待人工提供的 Linux 环境执行，不得提前标记为通过。

退出条件：

- [x] closure source cache 专项条目具备配置上限；业务 `Gene\Cache` 可通过既有 `gene.cache_max_items` 限制；
- [ ] Valgrind/ASAN 通过；
- [ ] 100 万请求无单调 live allocation 增长。

### 阶段 2：验证已有优化（第 3 周）

状态：**验证编排与灰度规范已落地，运行时矩阵待目标环境执行**

交付：

- [x] `swoole_benchmark.php` 覆盖 C-API 与 `route_precompile` 四组开关，并校验 `RESULT-DIGEST` 一致性；
- [x] `tools/acceptance/README.md` 明确了 5% worker、24 小时观察及异常立即关闭开关的灰度流程；
- [x] `demo/public/swoole.php` 已保持 `try/finally cleanup(true)`、workerReady、连接池停止和关闭的标准生命周期；
- [ ] Swoole 5.x/6.x、ASAN、RedisPool CAS 与 DB Pool 高并发结果待目标 Linux 环境执行。

退出条件：

- [ ] 四组开关结果一致；
- [ ] 连接池计数不漂移；
- [ ] 24 小时灰度无 crash/UAF/RSS 异常。

### 阶段 3：高 ROI 性能优化（第 4–6 周）

状态：**未启动代码优化；profile 准入门禁已落地**

按 profiling 结果选择，不固定全部实施：

1. route_pc workerReady 预热；
2. 配置路径 segment/result 缓存；
3. SQL property slot + smart_str 容量；
4. FPM 路由 key builder。

已新增 `tools/acceptance/profile_gate.php`：仅在 CPU samples ≥3%、分配占比 ≥5%、p99 毛刺或 RSS 单调增长任一证据满足时，才将对应项标为 `IMPLEMENT_IN_INDEPENDENT_PR`；当前示例 profile 对全部候选项输出 `DEFER`。在没有真实 profile 前，不实施 route_pc 预热、配置缓存、SQL 构造或路由 key builder。

### 阶段 4：结构性优化（后续版本）

状态：**未启动，维持候选池**

候选：

- 视图 28 次正则替换改状态机；
- C 层连接池原子计数；
- request arena；
- 路由递归改迭代；
- `cache_easy` 独立 TTL/LRU。

只有 profile 和阶段 0–3 数据证明收益后才启动。

---

## 九、任务拆分

### WP-01 验收编排与结果判定

状态：**部分完成**

- [x] 实现 `preflight.php`、`run_acceptance.php`、profile 配置和失败退出码；
- [x] 实现 FPM/Swoole benchmark 编排、JSON 归档、RSS 采样和基线/候选 JSON 比较；
- [x] 修正 PHP 版本文档；
- [ ] 人工环境交接模板、火焰图归档、ASAN/Valgrind 实跑、真实基线/候选比较及 `acceptance.md` 仍待目标环境执行。

### WP-02 内存可观测性

状态：**完成（连接池逐实例 stats 沿用既有 API）**

- [x] 扩展 `Memory::stats()`；
- [x] 新增 sweep 次数/耗时、ctx pool hit/miss、协程上下文高水位；
- [x] 暴露 closure source、route_pc、业务缓存等 persistent 分区条目；
- [x] `Gene\Pool::stats()` 与 `Gene\Cache\RedisPool::stats()` 已提供 total/idle/using/max 等逐实例统计，专项脚本负责采集和断言。

### WP-03 容量护栏

状态：**部分完成**

- [x] closure cache cap；
- [x] Cache unlimited NOTICE；
- [x] `Gene\Cache` 高基数写入专项及 `gene.cache_max_items` 验证；
- [ ] `cache_easy` 独立 TTL/LRU 设计仍属于阶段 4 候选，未在本期实现。

### WP-04 Swoole 生命周期与并发

状态：**部分完成**

- [x] cleanup 漏调专项、四开关矩阵编排、CAS/pool 压测入口与灰度操作规范；
- [x] sweep 仅清理已确认死亡的协程上下文；超过 cap 且仍存活时告警，不再按插入顺序淘汰 live context；
- [ ] route_pc 全树预热未实现：该项须先由 profile 证明首批请求收益，并验证 workerReady 后路由树/fn_cache 冻结不变量；
- [ ] C-API 兼容矩阵、CAS/pool 压测与完整 route_pc 回归待目标 Linux 环境执行。

### WP-05 FPM 热路径

状态：**准入门禁完成，优化实现待 profile**

- [x] 新增 profile 准入脚本，阻止没有证据的热路径重构进入主线；
- [ ] 路由注册 key builder 与 SQL 构造优化待 profile 达标后作为独立变更实施；
- [ ] persistent PDO/外部代理部署建议、FPM worker RSS 与 `pm.max_requests` 验收待人工提供生产等价环境和基线数据。

---

## 十、风险与回退

### 10.1 兼容风险

- 修改 `cache_max_items=0` 语义会破坏兼容性，因此本期只输出 NOTICE，不直接改变默认；
- closure cache 清空可能造成短时 Reflection/File I/O 毛刺，必须记录清空次数；
- route_pc 预热依赖路由冻结，动态路由用户必须保留关闭开关；
- property slot 直取依赖类布局和 PHP 版本，必须保留标准 Zend API fallback；
- C 层原子计数不能替代跨进程 Swoole\Atomic 语义。

### 10.2 回退要求

- 所有中高风险优化必须有 INI kill-switch；
- 默认值先保持旧路径，完成灰度后再讨论默认开启；
- 单项回退不应要求回滚整个扩展；
- stats 和 benchmark 不受快路径开关影响；
- 发现 crash、UAF、结果 digest 不一致时立即关闭优化，不以性能收益抵消正确性风险。

---

## 十一、生产推荐配置原则

FPM：

```ini
gene.runtime_type=1
gene.run_environment=2
gene.view_compile=1
gene.view_compile_check_mtime=0
```

Swoole：

```ini
gene.runtime_type=2
gene.run_environment=2
gene.swoole_getcid_capi=1
gene.route_precompile=0
gene.ctx_pool_max=512
gene.ctx_pool_prewarm=256
gene.co_contexts_max=8192
gene.cache_max_items=10000
gene.view_compile=1
gene.view_compile_check_mtime=0
```

说明：

- 上述 Swoole 数字仅为起始示例，必须按单 worker 并发和内存预算调整；
- `route_precompile` 在阶段 2 验证前保持关闭；
- `co_contexts_max` 不能盲目设大，避免掩盖 cleanup 漏调；
- `ctx_pool_prewarm` 不必总等于 max，应以常态并发为准；
- `cache_max_items` 应由 entry P95 大小和每 worker 预算推导。

---

## 十二、最终建议

下一版本不应先投入视图状态机或路由递归等高成本重构。建议立即启动：

1. 人工准备并配照扩展环境，方案侧实现标准验收脚本并执行 benchmark、ASAN/Valgrind 和 24 小时长跑；
2. closure cache cap、Cache unlimited NOTICE、Memory stats 扩展；
3. route_precompile/C-API/CAS 的目标环境运行时闭环；
4. 根据 perf/heap profile 决定 route_pc 预热、配置缓存和 SQL 构造优化的真实优先级。

只有通过可复现数据，才能判断 Gene 的瓶颈是在扩展派发、Zend 分配、DB/Redis I/O，还是业务本身。当前源码已经具备较好的优化基础，下一阶段的核心目标应从“继续静态推测优化”转为“建立证据、控制容量、稳定启用已有能力”。

---

## 附录：关键源码索引

- `src/gene.h:23`：扩展版本；
- `src/config.m4:28-32`：PHP 最低版本约束；
- `src/gene.c:140-154`：INI；
- `src/gene.c:285-478`：请求上下文；
- `src/gene.c:480-611`：ctx pool；
- `src/gene.c:722-868`：co_contexts 与请求上下文快路径；
- `src/gene.c:919-1145`：模块生命周期；
- `src/cache/memory.c:149-221`：persistent cache；
- `src/cache/memory.c:336-544`：业务缓存 LRU；
- `src/cache/memory.c:546-657`：缓存读取与配置路径；
- `src/router/router.c:1337-1366`：route_pc；
- `src/router/router.c:1568-1639`：FPM closure source cache；
- `src/db/pool.c:85-96`：DB 池协程识别；
- `src/cache/redis_pool.c:427-454`：Redis CAS；
- `demo/public/swoole.php:22-88`：Swoole 标准生命周期；
- `tools/verify_5_6_6.php`：FPM/CLI 验证；
- `tools/verify_5_6_6_swoole.php`：Swoole 验证；
- `audit/AUDIT_REPORT_2026_07_03.md:669-711`：既有未完成事项。
