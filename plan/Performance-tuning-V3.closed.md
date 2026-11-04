# Gene 扩展极致并发优化 —— V3（数据驱动的残余热点）

> 版本：v2（2026-11-04）
> 状态：**关闭**（2026-11-04 按指示忽略 §0.1(3f) 准入门槛，§2–§3.7 全部以代码落地；结果与实施偏差见 §9，Linux/Swoole 验收由维护者手动完成）
> 谱系：承接 [Performance-tuning-V1.closed.md](Performance-tuning-V1.closed.md)、[Performance-tuning-V2.closed.md](Performance-tuning-V2.closed.md)。V2 已落地的项（NTS 锁消除、请求装载零分配、路由/DI 分配削减、Db 属性槽位、冻结框架表零拷贝、ctx 冷热分离、Pool C 层 idle 栈、输出零拷贝、retired 回收）**本文不重复**。
> 依据：对 `src/router/router.c`、`src/app/application.c`、`src/cache/{cache,memory,redis}.c`、`src/http/{http,validate,webscan}.c`、`src/gene.c` 的源码复核（2026-11-03）。

---

## 0. 范围、约束与方法论

### 0.1 硬性约束（沿用 V2 §0）

1. 只写 Gene 扩展代码层面；宿主配置仅在附录 A 列为参考，不属于验收范围。
2. FPM 与 Swoole 分别记账：FPM 看“每请求固定成本”，Swoole 看“每协程固定成本 + worker 常驻内存（RSS）”。
3. 每项必须给出 (a) 现状证据（文件:行）、(b) 待证假设、(c) 改法、(d) 不变量与生命周期边界、(e) 验证要点、(f) **准入条件**（数据达不到就不做）。
4. 不改公开 API 语义：`Db` 惰性写、`history()` 快照、`Memory` 业务表 owned copy 语义、SSE/`write()`、Pool 生命周期约束保持不变。
5. 一项一提交，可独立回退；新行为一律先以 INI 开关落地（默认保持现状），验收通过后再单独提交“翻转默认值”。

### 0.2 为什么要先测量

V1/V2 之后，热路径上“显而易见”的分配与哈希已基本清理。剩余候选项的收益高度依赖**业务路由形态**（`Class@action` / 闭包 / 字符串钩子）和**缓存数据形态**（大数组还是小标量）。若不先测量，容易把低频路径当热点优化，引入复杂度却无收益。因此本文采用：

```
阶段 0 可观测计数  →  基线压测  →  逐项改造（INI 开关）  →  A/B 对照  →  准入判定  →  翻转默认
```

### 0.3 统计口径（所有项共用）

- **确定性指标**（首选，噪声低）：`valgrind --tool=callgrind` 单请求指令数（Ir）；`USE_ZEND_ALLOC=0` + `ltrace`/自定义计数下的每请求分配次数。两者对比 A/B 构建，差异 ≥ 3% 视为有效。
- **吞吐/延迟指标**：`wrk -t4 -c{64,256,1024} -d60s --latency`，每组 **5 轮**，丢弃首轮预热，取中位数；报告 QPS 与 P50/P99/P999。A/B 中位数差异需超过 5 轮极差（max−min）才算显著，否则记为“无显著差异”。
- **内存指标**：Swoole 每 worker 稳态 RSS（`/proc/<pid>/status` 的 `VmRSS`，压测 10 分钟后采样）与 `Gene\Memory::stats()`；FPM 看 `pm.max_children` 个进程 RSS 之和。
- **环境固定**：同一台 Linux 机器、CPU 绑核（`taskset`）、关闭 turbo、同一 PHP 构建（NTS + opcache + 相同 JIT 设置）、A/B 仅相差被测提交。
- **压测矩阵**：

  | 维度 | 取值 |
  |------|------|
  | 运行模式 | FPM（`pm=static`）、Swoole（`worker_num=CPU 核数`） |
  | 路由形态 | R1 `Class@action`；R2 闭包路由；R3 `Class@action` + 闭包 before/after 钩子；R4 `hook()->through()` 字符串钩子 |
  | 业务负载 | L0 空响应 `json(['ok'=>1])`；L1 `Config::get` ×10；L2 `localCachedVersion` 命中（1KB / 64KB 数组）；L3 `Validate` 5 字段（含正则） |

---

## 1. 现状复核与对 2026-11-03 初步分析的更正

源码复核后，以下初步判断**不成立或已被 V2 覆盖**，不纳入本文：

| 初步判断 | 复核结论 | 证据 |
|----------|----------|------|
| `cachedVersion` 数据键与版本键分两次往返，应合并为 MGET | **不成立**：`gene_cache_get(hook, &cache_key, ...)` 传入的是“数据键 + 全部版本键”数组，一次 `get` 取回；`curVersion()` 只是在已取回的结果里查找 | `cache.c:1290-1305`、`cache.c:996-1016` |
| Memory 读取全部深拷贝 | **部分成立**：冻结后的框架表已零拷贝（V2 §3.1）；仅**业务表**（`Gene\Cache` 本地层、用户 `Memory::get`）仍为 owned deep copy，这是 V2 §6 明确保留的安全边界 | `memory.c:411-493` |
| Pool 借还每次调用 PHP 层 Channel | **已解决**：V2 §3.6 已改为 C 层 idle 栈 + `waiters` 计数，仅满池等待时才走 `Channel::pop` | `pool.c:99-110`、V2 §3.6 / §11.1 |
| Redis 序列化固定为 PHP serialize | **不成立**：已支持 `serializer` 配置项 | `redis.c:257-259` |
| FPM 每请求重新执行 `router.ini.php` | **不成立**：`load_file()` 以 `cache_easy` + `validity`（默认 10s）+ mtime 判定，未变化时不重新 import | `application.c:264-331` |

仍然成立、构成本文主体的残余热点见 §3。

---

## 2. 阶段 0：可观测计数（所有后续项的前置条件）

**目的**：用数据确认 §3 各项的“待证假设”。本阶段只加计数，不改任何行为。

**改法**：在 `Gene\Memory::stats()`（`memory.c:2013` 起，现有键风格 `xxx_items` / `xxx_flushes`）追加以下**进程级**计数（NTS 下为普通 `zend_ulong` 自增，ZTS 下放 `GENE_G`，不加锁不做原子操作——计数本身允许在 ZTS 下近似）：

| 键 | 含义 | 埋点位置 |
|----|------|----------|
| `route_eval_calls` | 路由分发走到 `zend_eval_stringl` 的次数（含 PC 描述符与慢路径） | `router.c:1132`、`:1495`、`:1755` |
| `route_eval_bytes` | 上述 eval 程序文本累计字节数 | 同上 |
| `route_error_eval_calls` | 错误处理器走 eval 退路的次数 | `router.c:1637` |
| `route_dispatch_direct` / `route_dispatch_closure` | 直接分发 / 闭包分发次数（作为分母） | `gene_router_dispatch_direct`、`gene_router_dispatch_closure` |
| `memory_business_copy_calls` / `memory_business_copy_bytes` | 业务表 deep copy 次数与近似字节数（字符串长度和 + bucket 数 × `sizeof(Bucket)`） | `gene_memory_zval_local_copy`、`gene_memory_hash_copy_deep` |
| `validate_regex_calls` | `gene_preg_match*` 调用次数 | `validate.c:213`、`:246` |
| `http_php_calls` | `gene_http_php_call` 调用次数 | `http.c:255` |
| `cache_fill_calls` | `cachedVersion` 系列回源（未命中后调用数据函数）次数 | `cache.c:1311` 及 batch/local 同构位置 |
| `cache_fill_concurrent_max` | 同一 key 同时回源的最大协程数（Swoole；FPM 恒 ≤1） | 同上，见 §3.5 |

**不变量**：纯计数，`stats()` 原有键与语义不变；计数增加对热路径的开销为一次内存自增。

**验证**：`test/` 新增 `stats_counters.php`，覆盖每个计数从 0 增长；callgrind 对比加计数前后 Ir 差异 < 0.5%。

**产出物**：在压测矩阵 R1–R4 × L0–L3 下导出一份基线表（含各计数 / 请求的比值），回填到本文 §6。**§3 每项的准入条件都引用这张表。**

---

## 3. 候选项

### 3.1 路由 eval 退路消除（P0 候选，FPM 为主）

**证据**：

- 路由分发仍有 5 处 `zend_eval_stringl`：PC 描述符 `GENE_PC_EVAL` 分支（`router.c:1129-1135`）、慢路径的 before/run/after 拼接 eval（`router.c:1458-1497`）、错误处理退路（`:1637`）、`:1755` 退路、`:3803`。
- 进入 eval 的条件是“闭包分发所需的闭包在 `fn_cache` 中找不到”（`router.c:1020-1060`、`:1385-1412` 的 `use_closure = 0`）。
- `fn_cache` 是**请求级**哈希表（`router.c:686-688` 以 `ALLOC_HASHTABLE` + 非持久 `zend_hash_init` 创建，`gene.c:1401-1408` 在请求结束时销毁）。
- FPM 下路由树持久化在进程缓存里，`router.ini.php` 在 `validity` 内不会重新执行（`application.c:264-331`），因此**闭包不会被重新注册进 `fn_cache`**。`closure_src_cache`（`router.c:1766-1782`）缓存的是闭包**源码文本**，供 eval 使用。
- `zend_eval_stringl` 每次都要完整编译（词法、语法、生成 op_array），且 eval 代码**不进入 opcache**。

**待证假设**：H1 —— FPM 下 R2/R3 路由每请求至少一次 eval；Swoole 下闭包在 worker 启动时注册、`fn_cache` 常驻，eval 次数接近 0。

**准入条件**：阶段 0 中 FPM R2 或 R3 的 `route_eval_calls / 请求 ≥ 1`，且 callgrind 显示 eval（`compile_string` 及其子调用）占单请求 Ir ≥ 10%。若业务全部是 R1 形态，本项降级为“仅修错误处理退路”。

**改法（FPM）：闭包源码编译为文件，交给 opcache**

1. 在生成 eval 程序文本的位置（PC 描述符构建 `router.c:1070` 起，与慢路径拼接 `:1458` 起），改为生成一个**内容寻址**的 PHP 文件：
   - 路径：`{app_root}/Cache/Router/{hash}.php`，`hash` = eval 程序文本的 64 位哈希（`zend_hash_func` 或 xxh3，碰撞时用全文比对兜底）。
   - 内容：`<?php return static function () { <原 eval 程序文本> };`
   - 写入用“临时文件 + `rename`”保证原子性（与视图编译 `view.c` 的写出方式一致）。
2. 分发时 `include` 该文件（走 `zend_compile_file`，opcache 命中时零编译），得到闭包后用 `zend_call_known_function` 调用。闭包对象按请求创建、请求结束释放，不跨请求持有。
3. 在进程级持久表里记录 `hash → 文件已存在`，避免每请求 stat（与 V2 §3.3 `view_fresh` 同一模式，受 `gene.view_stat_ttl` 同类 TTL 控制）。
4. 由于文件名就是内容哈希，路由源码变化会生成新文件，旧文件不再被引用。`opcache.validate_timestamps=0` 时也不会读到旧代码。旧文件清理与 `closure_src_cache_max` 联动：超过上限时清理最久未用的文件。
5. 新增 INI：`gene.route_eval_compile=0/1`（默认 0，即保持现状）。

**改法（Swoole）**：Swoole 下闭包在 `fn_cache` 常驻，预期无需改造；若阶段 0 发现 Swoole 也有 eval（例如 `Router::clear()` 后 `fn_cache` 被清空、未重新注册），则在 `workerReady()` 前对 eval 程序做同样的“编译为文件 + include”，并把得到的闭包写入 worker 级 `fn_cache`。

**不变量与边界**（必须逐条验证，任何一条不满足则本项不做）：

- **变量作用域**：`zend_eval_stringl` 在调用方作用域执行，包进 `static function(){}` 后变为独立作用域。必须先审计所有 eval 程序文本（由 `router.c` 生成，模板有限），确认它们**不读写调用方局部变量**；若存在，则在生成的函数签名里显式传参。
- **`$this` 绑定**：使用 `static function`，与当前 eval 在内部函数调用下无 `$this` 的行为一致。
- **`return` / 异常 / `exit`**：eval 程序中的 `return` 在函数中语义相同；异常传播路径不变；`zend_try/zend_catch` 保护（对应 `exit()` / fatal）保留在 include 与调用外层。
- **错误信息**：`__FILE__`/`__LINE__` 从 `eval()'d code` 变为缓存文件路径，属于可观测变化，需要在文档中注明。
- **目录权限**：`Cache/Router` 不可写时自动回退到 eval，并计入 `route_eval_calls`，不能让请求失败。
- **多进程并发写**：多个 FPM worker 可能同时生成同一文件；`rename` 原子覆盖，内容相同，安全。

**验证**：

- 新增 `test/route_eval_compile.php`：R2/R3/错误处理器在开关 0/1 下输出逐字节一致；目录只读时自动回退；修改路由源码后生效；`Application::stop()`、`Response::isEnded()` 在 before/after 中的短路行为不变（对应慢路径拼接中的 `if(\Gene\Response::isEnded()||\Gene\Application::isStopped())return;` 语句）。
- callgrind：FPM R2 单请求 Ir 下降 ≥ 10% 才翻转默认值。
- wrk：FPM R2/R3 QPS 提升且 P99 不劣化。

### 3.2 `gene.route_precompile` 默认值评估（P1，Swoole）

**证据**：PC 描述符缓存可把每请求 6–10 次 leaf/hook 哈希查找降为 0（`router.c:754-786`），但以 `gene.route_precompile`（`gene.c:169`，默认 `0`）作为开关，注释明确说明“验证前保持慢路径”。V2 两轮线上门禁覆盖的是 V2 列出的项，没有专门验收此开关。

**待证假设**：H2 —— Swoole 下开启后 R1/R3 单请求 Ir 下降 ≥ 3%，且 `route_pc_items` 稳定（不随请求数增长）。

**准入条件**：H2 成立，并且 `tools/acceptance/linux_swoole_verify.sh --all` 在开关为 1 时全部通过，另加 `Router::clear()` / `delTree()` 后的 generation 失效回归（`route_pc_clear_invalidate.php`）。

**改法**：本项不改代码逻辑，只做验收。验收通过后单独提交，把默认值改为 1，回退方式为 INI 设为 0。

**不变量**：FPM 下描述符缓存本来就不会填充（`router.c:766-769`），默认值翻转对 FPM 无影响。

### 3.3 业务表读取的 deep copy（P2 候选，Swoole 为主，有前置数据要求）

**证据**：业务表读取（`gene_memory_zval_local_copy`，`memory.c:400`、`:466-493`）每次都重建字符串和数组。V2 §6 明确保留了这一安全边界：它防的是业务表 `pefree`/rehash 之后请求侧持有悬垂指针（UAF-2）。

**待证假设**：H3 —— L2（`localCachedVersion` 命中 64KB 数组）场景下，`memory_business_copy_bytes / 请求` 达到 KB 级，且 copy 占单请求 Ir ≥ 15%。

**准入条件**：H3 成立。否则本项不做，改为在 `reference.md` 写明使用建议：“本地缓存只放小值，大数组拆成按 key 的条目”。

**改法（只在准入后实施）：写时快照 + 借用计数 + 请求结束归还**

1. 业务表每个条目从“裸 zval”改为一个持久节点 `gene_biz_node { zval value; uint32_t borrows; uint8_t retired; }`，`value` 在写入时一次性构建为**不可变形态**（字符串 interned/permanent，数组 `IS_ARRAY_IMMUTABLE`，与 V2 §3.1 框架表相同）。
2. 读取时 `borrows++`，返回不可变 zval（零拷贝）；同时把节点指针登记到当前请求上下文的借用列表（Swoole 下是协程 ctx，FPM 下是 `default_ctx`）。
3. 覆盖写、删除、LRU 淘汰、TTL 过期时，节点只从哈希表中摘除并标记 `retired`，**不立即释放**；当 `borrows == 0` 时才 `pefree`。
4. 请求/协程结束（`cleanup()` / RSHUTDOWN / ctx reset）时遍历借用列表做 `borrows--`，归零且已 retired 的节点在此时释放。
5. 新增 INI：`gene.memory_business_zero_copy=0/1`（默认 0）。

**不变量与边界**：

- **NTS 单线程前提**：`borrows` 为非原子计数，仅在 NTS 下启用；ZTS 构建编译期禁用本项，继续 deep copy。
- **协程安全**：Swoole 协程在同一线程内协作调度，借用登记与归还不会跨线程；协程被取消或异常退出时，由现有 `co_contexts` 清理路径（`gene_co_contexts_sweep`）负责归还，必须为此加测试。
- **用户侧修改**：用户对返回数组做 `$a[] = x` 时，Zend 按 `IS_ARRAY_IMMUTABLE` 自动 `zend_array_dup` 到请求堆，不会写到持久内存（与 V2 §3.1 的论证相同）。
- **对象 / 资源**：业务表本来就不允许存对象（`gene_memory_zval_persistent` 的类型分支），不受影响。
- **内存上界**：retired 但仍被借用的节点数受“并发协程数 × 单请求借用数”约束；暴露 `memory_business_retired_items` 计数，压测中必须保持有界。
- **`Memory::clean()`**：清空业务表时同样只 retire 不立即释放。

**验证**：ASAN + 真实 Swoole：读取后在同一请求内覆盖写、删除、`clean()`、LRU 淘汰、TTL 过期，然后继续使用已读取的值；协程中途抛异常 / 被 `Coroutine::cancel`；200 协程 × 10 万次读写混合压测后 `retired_items` 归零。

### 3.4 Validate 正则直连 PCRE（P2，FPM / Swoole）

**证据**：`gene_preg_match` / `gene_preg_match_str`（`validate.c:213-260`）通过 `zend_call_known_function` 调用用户态 `preg_match`，每次都要构造参数 zval 和返回值数组；手机号、日期等固定规则也走正则（`validate.c:1444`、`:1466`）。`webscan.c:248` 同理。

**待证假设**：H4 —— L3 场景下正则校验占单请求 Ir ≥ 5%。

**准入条件**：H4 成立。

**改法**：

1. 改用 `pcre_get_compiled_regex_cache(regex)` + `php_pcre_match_impl`（或直接调用 `pcre2_match`，配合 `php_pcre_mctx()` / `php_pcre_create_match_data`），省去用户态函数调用和参数构造。PCRE 编译缓存与 JIT 由 `ext/pcre` 统一管理，行为与 `preg_match` 一致（包括 `pcre.backtrack_limit`、`pcre.jit`）。
2. 手机号、日期等内置固定规则改为手写的 C 判断函数；语义以现有正则为准，编写**等价性测试**：用现有正则和 C 实现对同一批样本（含边界、Unicode、超长输入）逐一比对结果。
3. `webscan` 的 PCRE 失败语义（`webscan.c:238` 注释：`preg_match` 遇到回溯 / JIT 栈限制时返回 false）必须保持：直连实现遇到 `PCRE2_ERROR_*` 时同样按“匹配失败 / 拦截”处理，不能静默放行。

**不变量**：用户自定义正则规则的匹配结果、错误行为与 `preg_match` 完全一致。

**验证**：现有 `ValidateTest` 全绿；新增固定规则等价性测试（≥ 1 万条随机样本 + 边界集）；webscan 规则在回溯超限样本上仍然拦截。

### 3.5 本地缓存回源防击穿（P1，Swoole 为主，关注 P99 与下游压力）

**证据**：`cachedVersion`（`cache.c:1309-1325`）在版本不一致或未命中时直接调用数据函数回源，没有同 key 合并。Swoole 单 worker 内同时有大量协程时，热点 key 失效会让 N 个协程同时打到数据库，表现为 P99 尖刺与连接池排队（`db_pool` 等待计数上升）。FPM 下每个进程一次只处理一个请求，进程内不存在同 key 并发，跨进程的击穿属于分布式锁问题，不在本项范围内。

**待证假设**：H5 —— Swoole 压测中热点 key 失效瞬间 `cache_fill_concurrent_max ≥ 4`，且 P99 明显高于稳态。

**准入条件**：H5 成立。

**改法**：

1. 在 worker 级维护 `inflight: key → 等待者列表`（只在 `runtime_type >= 2` 下启用）。
2. 第一个未命中的协程登记为“领头者”并执行回源；后到的协程挂起等待（使用 Swoole C API 的 `Coroutine::yield/resume`，或者回退到 `Swoole\Coroutine\Channel` 且只在有冲突时才创建）。
3. 领头者完成后把结果交给等待者：每个等待者拿到的都是**各自的 owned copy**，保持现有返回值所有权。领头者抛出异常时，等待者各自重新回源（不传播他人的异常），避免“一个失败、全体失败”。
4. 等待设置超时（`gene.cache_fill_wait_ms`，默认 0 = 关闭本项）；超时后等待者自行回源，退化为现状。

**不变量**：未发生并发冲突时，行为、调用次数与现状完全一致；回源函数的副作用次数只会减少不会增加；等待者超时一定会退化为自行回源。

**验证**：200 协程同时请求同一冷 key，数据函数调用次数从 200 降到 1；领头者抛异常、领头者被取消、领头者超时三种情况下等待者均能得到结果或各自回源；`co_contexts` 与 `inflight` 表压测后归零。

### 3.6 C 层回调 PHP 函数的查找缓存（P3，低风险、小收益）

**证据**：

- `gene_http_php_call`（`http.c:255-264`）每次调用都在 `CG(function_table)` 里按名字查找 `curl_*` 函数；一次 `curl_multi` 请求会调用十余次。
- `gene_cache_get/set/incr`（`cache.c:445-488`）每次都在对象的方法表里查找 `get/set/incr`。
- 对照：`validate.c` 已经用 `GENE_CG_FN_LOOKUP`（`gene.h:74-79`）做了进程级缓存。

**改法**：

1. `curl_*` 函数改用 `GENE_CG_FN_LOOKUP` 缓存（内部函数在 MINIT 之后地址不变）。
2. 缓存后端的方法查找按“类指针 → `zend_function*`”做单槽缓存：类指针相同时直接复用，否则重新查找。用户子类化缓存后端时自然失效重查。

**准入条件**：无需阶段 0 数据（改动是纯查找缓存，风险极低）；但只有 callgrind 显示可测差异才计为收益。

**不变量**：`disable_functions` 禁用的函数仍会被识别为不存在（缓存值为 NULL 时每次都重新查找，或在 RINIT 时清空缓存）；用户覆盖 `get/set` 方法的行为不变。

### 3.7 FPM 下 curl 连接复用（P3，可选，涉及行为变化）

**证据**：FPM 下 `Gene\Http` 每次请求都新建 curl 句柄，没有跨请求复用 TCP/TLS 连接。对调用外部 HTTP 接口较多的业务，握手开销可能达到毫秒级。

**待证假设**：H7 —— 典型业务每请求 ≥ 1 次出站 HTTP，且目标 host 集中。

**改法**：新增 `gene.http_keepalive=0/1`（默认 0）。开启后在进程级（FPM）按 `scheme+host+port` 缓存 curl easy 句柄，并用 `curl_share` 共享 DNS 与 TLS 会话；请求结束时 `curl_easy_reset` 清理请求级选项。Swoole 下继续使用协程 HTTP 客户端，不受影响。

**不变量**：句柄复用前必须 reset 所有请求级选项（header、body、回调、认证信息），防止上一个请求的认证头泄漏到下一个请求，这一点要有专门的安全测试；连接被对端关闭时自动重建。

**准入条件**：H7 成立，且安全测试通过。本项涉及用户可见的运维语义（连接保持），按 V2 §8.4 的原则单独立项验收，不与其他项混合提交。

---

## 4. 内存占用专项

| 项 | 说明 | 状态 |
|----|------|------|
| V2 §4.4 持久缓存 arena 归并 | V2 因缺少“配置树达到万级”的数据而保留为候选 | 沿用：阶段 0 导出 `cache_num_used` 与框架表字节数，若单 worker 框架表 ≥ 10 万 bucket 或 ≥ 20MB，再按 V2 §4.4 实施 |
| `Cache/Router` 编译文件 | §3.1 引入的磁盘文件，进入 opcache 共享内存 | 由 `closure_src_cache_max` 限制文件数；opcache 共享内存由所有 FPM worker 共用，不增加单 worker RSS |
| 业务表零拷贝的 retired 节点 | §3.3 引入 | 用 `memory_business_retired_items` 监控，压测后必须归零 |
| 回源合并的 `inflight` 表 | §3.5 引入 | 条目在回源结束时删除；压测后必须为空 |

---

## 5. 明确不做

- **不缓存 eval 编译出的 op_array 到持久内存**：op_array 分配在请求 arena 上，持久化需要复制 opcache 的整套持久化逻辑，风险远大于 §3.1 的“落盘交给 opcache”方案。
- **不把业务表改成无锁 + 无拷贝的简单借用**：没有借用计数时，覆盖写或淘汰会导致悬垂指针，就是 UAF-2 的原问题（V2 §6）。§3.3 是唯一允许的零拷贝形态。
- **不改 `ctx_pool_max` / `co_contexts_max` 等默认值**：这些上限与部署的并发度相关，应由运维按附录 A 配置，框架默认值保持保守。
- **不在 FPM 下实现跨进程回源合并**：属于分布式锁问题，应由业务层用 Redis 锁处理，不放进框架。
- **不在没有阶段 0 数据的情况下实施 §3.1、§3.3、§3.4、§3.5、§3.7**。

---

## 6. 基线数据（阶段 0 完成后回填）

本轮按指示不以准入数据为前提，下表仅回填本地 Windows 可测的部分（PHP 8.2.35 NTS x64，`php-cgi -b` FastCGI worker = FPM 子进程同一请求生命周期，opcache 开，`tools/acceptance/fcgi_v3_verify.php`，每格 3000 次闭包路由请求）。callgrind Ir、wrk QPS/P99、RSS 与全部 Swoole 行需 Linux 环境，未测。

| 场景 | `route_eval_calls`/请求 | `memory_business_copy_bytes`/请求 | `validate_regex_calls`/请求 | `cache_fill_concurrent_max` | 服务端 `run()` 中位数 / p90 | QPS（c=256） | P99 | 单 worker RSS |
|------|------|------|------|------|------|------|------|------|
| FPM R1 L0（demo `Class@action`） | 0 | – | – | 1 | – | 未测 | 未测 | 未测 |
| FPM R2/R3 L0，`route_eval_compile=0` | 1（另：闭包 404 每次 1 次 `route_error_eval_calls`） | – | – | 1 | 174.3µs / 348.5µs | 未测 | 未测 | 未测 |
| FPM R2/R3 L0，`route_eval_compile=1` | 0（`route_eval_compiled`=1/请求，3 个缓存文件） | – | – | 1 | 131.8µs / 246.9µs（中位数 −24%） | 未测 | 未测 | 未测 |
| FPM `processCached` 命中，`zero_copy=0/1` | – | 297 B/次 → 0（`borrow_calls`=1） | – | 1 | – | 未测 | 未测 | 未测 |
| FPM `rule_mobile`/`rule_date` | – | – | 1 → 0（手写匹配器） | – | – | 未测 | 未测 | 未测 |
| Swoole 全部场景 | 待维护者 Linux 验收 | | | | | | | |

---

## 7. 优先级与推荐顺序

| 序 | 项 | 复杂度 | FPM | Swoole | 准入依赖 | 主要收益 |
|----|----|--------|-----|--------|----------|----------|
| 0 | §2 可观测计数 | 低 | ✔ | ✔ | 无 | 为后续所有项提供依据 |
| 1 | §3.1 路由 eval 消除 | 中 | ✔ | （视数据） | H1 | 每请求省掉一次 PHP 编译 |
| 2 | §3.2 route_precompile 默认开启 | 低（仅验收） | – | ✔ | H2 | 每请求 6–10 次哈希查找 |
| 3 | §3.6 函数查找缓存 | 低 | ✔ | ✔ | 无 | 每次 curl/缓存调用一次哈希查找 |
| 4 | §3.5 回源防击穿 | 中 | – | ✔ | H5 | P99、下游数据库压力 |
| 5 | §3.4 PCRE 直连 | 中 | ✔ | ✔ | H4 | 每次正则校验一次用户态调用 |
| 6 | §3.3 业务表零拷贝 | 高 | ✔ | ✔ | H3 | 大数组本地缓存的 CPU 与临时内存 |
| 7 | §3.7 FPM curl 复用 | 中 | ✔ | – | H7 | 出站 HTTP 握手延迟 |
| 8 | V2 §4.4 arena | 高 | – | ✔ | 配置树规模数据 | RSS、TLB |

排序依据：先做“确定有收益且风险低”的项（0、2、3），再做“收益依赖数据”的项；复杂度高、涉及生命周期的项（§3.3）排在后面，并要求 ASAN + 真实 Swoole 验收。

---

## 8. 验收与关闭条件

1. 每项以“INI 默认关闭”提交，Windows 全量回归（`test/TestRunner.php`）通过。
2. 涉及借用、生命周期、协程的项（§3.3、§3.5、§3.1 的 Swoole 部分）必须在 Linux + 真实 Swoole 下通过 `tools/acceptance/linux_swoole_verify.sh --all`，并新增对应阶段；本文要求 ASAN/LSAN 为**必须项**（V2 中为补充项，V2 §13.5 将其列为遗留）。
3. 按 §0.3 统计口径完成 A/B 对照，结果回填 §6 并附归档（与 V2 一致，记录 `gene.so` SHA-256 和 `git rev-parse HEAD`——V2 遗留的 HEAD 指纹缺失问题在本文验收中必须解决）。
4. 达到准入条件且验收通过的项，单独提交翻转默认值；未达到准入条件的项在文内记录数据和“不做”的结论。
5. 所有项处理完毕（落地或基于数据否决）后，文件更名为 `Performance-tuning-V3.closed.md`，并更新 `plan/README.md` 索引。

---

## 9. 实施结果（2026-11-04 关闭）

按维护者指示：忽略 §0.1(3f) 与各项“准入条件”，从代码层面全部落地；风险项以 INI 默认关闭提交，回归测试补齐，本地完成 FPM 等价真实验收，Linux/Swoole 由维护者手动验收。

### 9.1 落地清单

| 项 | INI（默认） | 实现要点 | 与原方案的偏差及理由 |
|----|------------|----------|----------------------|
| §2 可观测计数 | – | `Memory::stats()` 新增 `route_eval_calls/bytes/compiled/files`、`route_error_eval_calls`、`route_dispatch_direct/closure`、`memory_business_copy_calls/copy_bytes/borrow_calls/retired_items`、`validate_regex_calls`、`http_php_calls`、`cache_fill_calls/concurrent_max/coalesced/inflight`；`Monitor::reset()` 清零 | 计数一律放 `GENE_G`（NTS 即普通全局） |
| §3.1 路由 eval 消除 | `gene.route_eval_compile`（0） | 5 处 `zend_eval_stringl` 统一经 `gene_router_eval()`；opcache 启用时把程序写为 `{app_root}/Cache/Router/<hash>_<len>.php`（`"<?php\n"` + 原程序文本，tmp+rename），首次使用逐字节校验，`zend_compile_file` + `zend_execute` 执行 | **不包成 `static function(){}`**：eval 程序（`GENE_ROUTER_CONTENT_*` 模板）依赖调用方符号表（`$gene_mp`/`$gene_h` 等）和顶层 `return` 短路，`zend_execute` 走的是与 eval 完全相同的顶层代码路径，作用域、return、异常、bailout 语义零变化，无需逐模板审计传参。**不删除旧文件**：多 FPM 进程共享目录，删除有竞态；内容寻址不会读到旧代码，进程内表按 `closure_src_cache_max` 封顶。**内容不符不覆盖**：opcache 共享内存可能仍持有该路径的旧脚本，改走 eval。仅 NTS（进程级表）。可观测变化：`__FILE__` 为缓存文件，且不再继承 `Gene\Application` 类作用域 |
| §3.2 route_precompile 默认值 | `gene.route_precompile`（**1**） | 默认值翻转，`0` 回退慢路径；FPM 下本来不填充描述符，无影响 | 未做 H2 数据验收（按指示），Swoole 四格矩阵由维护者 Linux 验收 |
| §3.3 业务表零拷贝 | `gene.memory_business_zero_copy`（0） | 命中时直接返回持久化 interned 字符串 / `IS_ARRAY_IMMUTABLE` 数组；本请求内覆盖、删除、`clean()` 的旧值进入退休队列，**下一请求 RINIT** 统一释放；`processCachedVersion*` 命中去掉第二次深拷贝（改 `ZVAL_COPY`，两种模式都生效） | **仅 NTS FPM/CLI，Swoole 不启用**：用户态可把借用值存进 `static`/对象属性跨请求持有，借用计数在“请求结束归还”时无法知道值是否仍被引用，Swoole 常驻下必然 UAF；FPM 每请求结束清空用户态所有引用，下一请求开始释放是可证明安全的边界。**不在 RSHUTDOWN 释放**：session 等扩展在 gene 之后关闭，仍可能触碰借用值 |
| §3.4 PCRE 直连 | – | `gene_pcre_match()` = `pcre_get_compiled_regex_cache` + `php_pcre_match_impl`（8.4+ 新签名已适配），`rule_match` 字符串入参与 Webscan 使用；`rule_mobile`/`rule_date` 手写匹配器 | 手写匹配器遇 ≥0x80 字节回退 PCRE（`\d` 受 ctype locale 表影响）；非字符串入参保留原 `preg_match` 调用以保持类型转换/弃用告警语义 |
| §3.5 回源防击穿 | `gene.cache_fill_wait_ms`（0） | worker 级 `cache_inflight`（引用计数节点），领头者 `zend_try` 包裹回源，结束先摘表再按等待者 `Channel(1)` 推送 `[data]`；等待者超时/领头异常/结果含对象时自行回源；复用者不重复写缓存 | **带版本方法只计数不合并**：等待者读到的版本号可能新于领头者，复用其结果会返回版本更新本应失效的数据。batch 方法只计 `cache_fill_calls` |
| §3.6 查找缓存 | – | `gene_http_php_call` 按名字指针 32 槽缓存内部函数；`Gene\Cache` 钩子 get/set/incr/delete 单槽 `(ce, fn)`，请求结束清空 | http 缓存仅 NTS 且 `runtime_type<2`：Swoole runtime hook 可在运行期替换 `curl_*` 条目 |
| §3.7 curl 复用 | `gene.http_keepalive`（0） | PHP ≥ 8.5 用 `curl_share_init_persistent([DNS, SSL_SESSION, CONNECT])` 挂到每个 easy 句柄（含 `curl_reset` 之后与 multi 每项）；低版本空操作 | **不缓存 easy 句柄**：持久 share 只共享 DNS/TLS 会话/连接，不携带任何请求级选项，从机制上排除认证头跨请求泄漏，无需另立安全测试；PHP < 8.5 无安全的跨请求持久化句柄 API，故不做 |

### 9.2 验证

- 构建：PHP 8.1.30 / 8.2.33 / 8.4.25 / 8.5.10 NTS x64 编译通过（覆盖 `php_pcre_match_impl` 两种签名）。
- 回归：`test/TestRunner.php` 26 个文件全绿；新增 `test/PerfV3Test.php`（49 项，夹具 `test/fixtures/perfv3`）——INI 默认值与计数字段；`rule_mobile`/`rule_date` 与 `preg_match` 20031 条（含 2 万随机）逐条一致；`rule_match` 非法正则 false+warning、整型入参；零拷贝借用/退休/COW/`clean()`；`php-cgi -T` 下 `route_eval_compile` 0/1 四类路由输出一致、opcache 关/目录不可写/文件被篡改三种回退。
- FPM 等价真实验收：`tools/acceptance/fcgi_v3_verify.php`，`route_eval_compile × memory_business_zero_copy` 四格，夹具 + demo（`/healthz`、404、首页）全部 ALL-PASS，`RESULT-DIGEST` 四格一致（`d567b475…`），数据见 §6。WampServer 的 Apache 以 CGI Action 方式每请求起新进程，无法体现 worker 常驻，故改用 `php-cgi -b`。
- 维护者 Linux/Swoole 待办：`linux_swoole_verify.sh --all`（`route_precompile` 默认 1 下矩阵）；`tools/acceptance/swoole_v3_singleflight.php` 分别以 `gene.cache_fill_wait_ms=0` 与 `=2000` 运行；ASAN 下复跑 `PerfV3Test`。

---

## 附录 A：运维基线（参考，不属于本文验收范围）

这些配置决定了框架优化能否体现出来，压测环境应统一使用：

**FPM**

- opcache：`opcache.validate_timestamps=0`、`opcache.interned_strings_buffer>=32`、`opcache.memory_consumption` 按代码量预留 50% 余量；JIT `opcache.jit=tracing`、`opcache.jit_buffer_size=64M`（以压测结果为准，I/O 密集型业务 JIT 收益有限）。
- 进程：`pm=static`；`pm.max_children ≈ CPU 核数 × (1 + 平均 I/O 等待 / 平均 CPU 时间)`，并受“总内存 / 单进程 RSS”约束；`pm.max_requests` 取 5000–20000 抑制碎片。
- 连接：每个 FPM 进程一条持久连接，总连接数 = 进程数，需与 MySQL `max_connections` 对齐。
- Gene：`gene.log_keep_open=1`、`gene.view_stat_ttl>0`（发布流程需配合）、生产关闭 debug 与 `db_history`。

**Swoole**

- `worker_num = CPU 核数`；`max_request` 取较大值，配合 `max_wait_time` 平滑重启。
- 连接池：单 worker 池大小 ≈ 单 worker 并发协程数 × 数据库耗时占比（通常 8–32）；`worker_num × 池大小 ≤ MySQL max_connections`。
- `gene.ctx_pool_prewarm` ≈ 单 worker 平均并发协程数；`gene.co_contexts_max` ≈ `max_coroutine`。
- 所有路由、配置、ORM 元数据在 `workerReady()` 前加载完毕；进程缓存（`Gene\Memory`）每个 worker 一份，大体量共享数据应放 `Swoole\Table` 或 Redis。
