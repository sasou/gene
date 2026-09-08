# Gene 扩展极致并发优化 —— V2

> 版本：v7（2026-09-08）
> V1 已完成项目见 `PERFORMANCE_OPTIMIZATION_V1.md`。本文只保留尚待实现或验证的项目。

## 0. 执行原则

1. **不设置启动门槛**：不再以 profiling 占比、收益预测、风险等级、平台覆盖、前置设计或其他人工准入条件限制优化启动。
2. **只按实现复杂度排序**：复杂度综合代码改动范围、生命周期处理、兼容性影响和测试场景数量，不代表业务优先级或预期收益。
3. **实现与验收同行**：每个优化必须同时提交实现、功能回归和性能验收脚本；缺少自动化验收视为未完成。
4. **结果决定去留**：自动化脚本证明功能正确且性能达到标准则保留，否则继续调整或回退，不用事前论证替代实测。
5. **一次只验证一个变量**：优化应拆成可独立运行、独立对比、独立回退的小提交，避免多个变化互相掩盖。
6. **不预设收益**：报告原始数据、环境、命令和重复次数；不根据单次结果或局部指标宣称整体提升。

## 1. 自动化验收规范

### 1.1 每项优化必须提供

- `功能回归脚本`：验证原有 API、边界条件、异常路径和生命周期语义不变。
- `性能对比脚本`：在相同环境中运行优化前后的基准，输出机器可读结果。
- `结果判定脚本`：依据本节统一规则自动返回成功或失败，不依赖人工阅读日志。
- `场景说明`：记录负载规模、数据规模、并发度、预热次数、采样次数和运行环境。

建议统一放置：

```text
tools/perf/
  run.php|run.ps1|run.sh
  cases/<item>/
  compare.php
  thresholds.json
  results/
```

实际目录可沿用仓库现有测试结构，但调用入口和退出码必须统一。

### 1.2 统一判定规则

每个项目在 `thresholds.json` 中声明其主指标、保护指标和允许波动：

- **功能正确性**：所有既有测试和项目专项回归必须通过。
- **主指标**：至少一个目标指标达到该项目设定的改善阈值，例如 ns/op、CPU/请求、分配次数、RPS 或 p95/p99。
- **保护指标**：不得超过项目设定的退化阈值，例如 RSS、写方 p99、TTFB、错误率或连接数。
- **稳定性**：预热后至少重复 5 轮，以中位数比较；尾延迟同时报告 p95/p99。
- **退出码**：通过返回 `0`，功能失败、性能未达标或保护指标超限返回非 `0`。
- **留档**：保存 baseline、candidate、环境信息和比较结论，保证结果可复现。

不同项目的阈值由验收脚本显式配置，不在启动优化前设置人工准入门槛。

### 1.3 基础矩阵

脚本按项目适用范围自动选择场景，不要求所有项目先覆盖全部平台：

- 并发：1 / 32 / 128 / 512 / 1024。
- 批量：1 / N / 大批量。
- 生命周期：FPM 同 worker、Swoole 多协程/多 worker、workerStop、Pool close、`pcntl_fork`。
- 缓存：TTL、覆盖、删除、rehash、高 churn、冻结与未冻结。
- 路由：同层占位数 1 / 4 / 16 / 64，深度 3 / 6 / 10。
- I/O：本地 SQLite、Redis、真实池排队和突发扩容。
- 兼容：opcache 关闭、`opcache.file_cache_only=1`；ZTS 在正式支持时加入矩阵。

ASAN、反汇编和 profiling 可作为诊断工具，但不作为启动优化的前置条件；若项目涉及内存生命周期，验收脚本应加入可用平台上的 ASAN 任务。

---

## 2. 待优化项目：低复杂度

这些项目改动集中、接口边界清晰，优先实现。排序仅代表实现复杂度。

### 2.1 热路径 C API 化与固定成本削减

- `tool/monitor.c`、`tool/crypto.c`：使用 `zend_memory_peak_usage`、`gene_hrtime`、`php_base64_encode`、`php_random_bytes`。
- `common/common.c`：评估直调 `php_var_serialize/php_var_unserialize`；JSON 同类处理。
- `http/validate.c:781-970`：内置规则在 C 层实现，减少 `gene_factory_call` 回 PHP。
- `router.c`：仅在机器码确认仍有运行时成本时，将常量 `strlen` 改为 `ZEND_STRL`。
- ZTS 热函数中将只读 `GENE_G(runtime_type)` 等读取到局部变量。

**自动验收**：专项微基准输出 ns/op、CPU/操作和功能一致性；无机器码或指标改善的改动自动判失败。

### 2.2 每请求固定成本

- `orm/model.c:763-797`：`findMany(preserveOrder)` 预归一化 ids，减少重复 `zval_get_string`。
- `mvc/model.c:82-122`、`service/service.c:81-122`：缓存类名 `zend_string*`，遵守请求和模块生命周期。
- `cache/cache.c:563-619`：构造期预生成 `(sign,class,method)` key 前缀，运行时只追加 args。
- `log.c`：缓存秒级时间字符串；不改变 `error_log()` 的 SAPI、轮转和多进程语义。
- `orm/meta.c:147-236`：元数据命中后借用稳定字段，并用明确所有权标志管理释放。

**自动验收**：原 API 回归、跨请求生命周期测试、每请求分配次数和 ns/op 对比。

### 2.3 可观测指标补齐

依次实现以下低开销指标：

- `cache_read_locked` / `cache_read_lockfree` / `cache_lock_wait_us`
- `db_pool_wait_us` / `redis_pool_wait_us` 及分桶
- `route_pc_hit` / `route_pc_miss` / `route_pc_generation`
- `chird_scan_steps`
- `ctx_bytes_per_context`
- 专项构建下的每请求分配次数

热路径普通计数使用进程本地非原子计数；直方图等较高开销指标由专项构建或配置开启。

**自动验收**：指标值与构造负载匹配；开启观测后的性能开销不超过配置阈值。

### 2.4 池连接 fetch mode 一致性

`db/pool.c:288-366` 使用已有 `options[PDO::ATTR_DEFAULT_FETCH_MODE]`。只在用户提供配置时应用，不覆盖显式 `FETCH_BOTH/FETCH_NUM/FETCH_OBJ`，不修改输入配置数组。

**自动验收**：比较池/非池 `row/all`、原始 PDO 借出、数字下标、自定义模式及 `ATTR_CASE/ATTR_ORACLE_NULLS`。

### 2.5 PDO/PDOStatement 方法指针缓存

在精确内部 PDO/PDOStatement CE 上缓存方法指针；用户通过 `ATTR_STATEMENT_CLASS` 提供的类继续动态查找，避免跨请求持有用户类 CE。

**自动验收**：内部类与用户类功能回归、FPM 连续请求回归、本地 SQLite 微基准。

---

## 3. 待优化项目：中等复杂度

### 3.1 Webscan 去对象化

`app/application.c:418-447`、`http/webscan.c:85-161,224-267` 抽出接收配置的 C helper，由 application 直接调用，移除每请求 `object_init_ex`、构造、`check()` 和析构。不得改为 worker 级长期持有 PHP 对象。

递归深度和长度上限属于扫描策略变化，保持为独立项目。

**自动验收**：扫描样例结果完全一致；统计构造/析构、分配次数、CPU/请求和 RPS。

### 3.2 `Memory::mget()` 单次读锁

`memory.c:1782-1824` 在一次读锁内遍历全部 key 并完成深拷贝。保留 TTL、hit/miss、FPM 延迟删除及冻结语义。过期 key 在读锁内收集，释放后经写路径复查并删除；锁内不得 yield、调用用户代码或触发析构。

**自动验收**：1 / N / 大批量 key，TTL 边界、覆盖、删除、对象/数组值；主指标为批量 CPU，保护指标包含写方 p99。

### 3.3 上下文容量与内存标定

测量 `sizeof(gene_request_context)`，在 1/32/128/512/1024/2048 活跃协程下采样 worker RSS，自动拟合：

```text
worker RSS = 基线 RSS + 每协程增量 × 活跃协程数
```

同时验证 `co_contexts` sweep 单次耗时和 `swoole_auto_cleanup` 注册 defer 的成本，并生成给定 RSS 预算下的容量换算结果。

**自动验收**：拟合数据、误差、每协程字节数、sweep p99 和 defer 成本均由脚本输出并判定。

### 3.4 池健康检查、timer 合并与 TTL sweep

- `db/pool.c`：增加 `ping_on_get`，只对空闲超过阈值的连接探活。
- 将每 Pool 一个 `Timer::tick` 合并为单 worker timer。
- `cache/memory.c`：暴露 expiry sweep 配置；Swoole 后台 sweep 只操作业务表，与 worker 生命周期绑定，限制每次扫描量和耗时，锁内不 yield。

**自动验收**：覆盖低负载、突发扩容、满池排队、断线重连、多个 Pool、写少读多 TTL；检查 timer 数、探活往返、RSS、等待 p99 和过期项回收。

### 3.5 自动加载路径与重复 stat

`factory/load.c:78-165`：类自动加载可按规范化后的 `opened_path` 查询 `EG(included_files)`；视图文件仍允许使用不同 symbol table 重复执行；路径拼接改为 `memcpy`。

**自动验收**：类重复加载、视图重复渲染、相对/绝对/规范化路径、文件副作用及 stat/分配次数。

### 3.6 DI alias 解析缓存

保留原始 alias 边和现有 8 跳语义，增加请求/协程级解析缓存；alias 写入时递增 generation，使解析缓存失效。

**自动验收**：链式晚注册、中间节点重绑、环、超过 8 跳、构造函数内修改 alias、协程隔离和解析 ns/op。

### 3.7 FPM 持久连接（显式配置）

增加 `gene.db_fpm_persistent`，默认关闭；开启时设置 `PDO::ATTR_PERSISTENT=true`，归还前执行 `gene_db_tx_hygiene`。文档和测试明确 session variables、temporary tables、advisory locks、prepared statements、SQL mode、time zone、认证与断线状态不会被完整清理。

**自动验收**：默认行为不变；连接复用、事务回滚、断线恢复和已知残留状态均有脚本覆盖。

---

## 4. 待优化项目：高复杂度

### 4.1 ctx 级 arena

为 method/path/lang/module/controller/action 使用 ctx 级 arena，字段保存 offset/length；reset 时整体复位，超长值回退 heap 并记录所有权。使用独立标志区分 unset 与空串，保持 NUL 结尾，支持独立赋值、重复更新、显式路由参数和异常清理。

arena 扩容会使借用指针失效，实施时同步调整所有调用者，并与 `path_params` 的重建行为协调。

**自动验收**：所有赋值和清理路径、ASAN、每请求分配数；保护指标为 `sizeof(gene_request_context) × ctx_pool_max/活跃协程数` 对 RSS 的影响。

### 4.2 路由占位子树优化

`router/router.c:220-375` 当前静态 segment 已走 HashTable，目标是减少同层占位子路由 `chird` 的线性扫描。实现可选择注册期冲突检测、固定 suffix 判别索引、带约束 radix tree 或其他结构，但必须保持现有路由匹配和插入顺序语义，除非作为明确的 API 变更提交。

**自动验收**：同层占位数 1/4/16/64、深度 3/6/10；验证命中结果、优先级、`chird_scan_steps`、RPS 和 p99。

### 4.3 `route_precompile` 默认开关评估

保持现有 generation 失效机制和动态 controller/action 派发，不将不稳定的 action `zend_function*` 固定进描述符。通过脚本分别运行开关状态，自动决定候选默认值是否满足功能和性能阈值。

**自动验收**：与路由占位专项共用负载，增加 route clear/reload、dispatch 协程交错、opcache 模式和 route_pc hit/miss 检查。

### 4.4 `Memory::getBorrowed()` 借用读

仅供内部受控路径使用。借用期内必须保证无覆盖、删除、TTL 清理和协程切换；不以 persistent/interned 标志替代所有权约束。

**自动验收**：覆盖、淘汰、TTL、冻结、协程切换和高 churn；ASAN 与功能回归必须通过，未证明生命周期安全则脚本失败。

### 4.5 响应 buffered API

保留 `write()`、`end()`、SSE 和 chunked streaming 的现有流式语义。若需要合并小块写入，增加显式 buffered API 或只合并框架内部非流式输出，不将全量响应强制缓冲。

**自动验收**：普通响应、SSE、chunked、大响应和异常中止；主指标可为 syscall/吞吐，保护指标必须包含 TTFB、分块到达间隔和峰值内存。

### 4.6 HTTP 客户端跨请求连接复用

使用进程级 handle pool，而非请求 ctx 级 handle。每次复用执行完整 reset，清理 header、callback、POST body 和 private data，并处理 fork、DNS、TLS、异常及 worker 退出；不得长期持有请求级 zval。

**自动验收**：连接复用率、并发隔离、不同请求配置污染、异常恢复、fork/workerStop、TLS/DNS 变化、RSS 和 p99。

### 4.7 预处理语句 LRU

按物理连接缓存高频重复 SQL 的 prepared statement，限制缓存数量；关闭 cursor 后复用，连接重连后全部失效，并覆盖 native/emulated prepares、DDL、`SET` 和驱动差异。

**自动验收**：重复与非重复 SQL、游标未关闭、重连、淘汰、事务、四驱动兼容；统计 prepare 次数、服务端 statement 数、吞吐和 p99。

### 4.8 SQL 片段内部化

若减少 SQL/where/data 等 public 属性中转，必须保持属性读写、引用、clone、析构、GC、异常和 reset 的可观察语义；无法保持时作为新 API 提交，不伪装成透明优化。

**自动验收**：公开属性读写和引用行为、链式构造、clone、异常、重复执行、GC 泄漏及 SQL 结果一致性。

### 4.9 MemcachedPool

缓存 `Memcached` 方法指针并增加连接池，覆盖连接创建、借还、超时、失效、worker 生命周期和配置隔离。

**自动验收**：低负载、突发、满池、断线、多个实例和 workerStop；输出连接数、等待 p99、吞吐和错误率。

### 4.10 四驱动去重

将 `mysql.c/sqlite.c/pgsql.c/mssql.c` 的重复实现抽象为 `gene_db_dialect { oq, cq, callbacks }`。作为独立重构提交，不与其他优化混合。

**自动验收**：四驱动 SQL 生成和 CRUD 回归全部通过；性能不得超过配置的退化阈值。

### 4.11 编译与布局实验

脚本先记录实际编译命令，再分别构建 `-O2/-O3`、LTO、`/GL+/LTCG`、visibility、PGO 或布局候选。`-march=native` 只用于同机部署目标；检查导出符号和 ABI。

**自动验收**：记录 text size、启动时间、RPS、CPU/请求、p99、RSS 和导出符号；候选配置由比较脚本判定，不凭编译参数名称判断收益。

---

## 5. 平台与增强验证任务

以下任务直接执行，不作为其他优化的前置门槛：

1. 决定并实现 ZTS 支持矩阵，覆盖文件作用域 `zend_function*`、`zend_class_entry*`、`HashTable*` 生命周期。
2. 增加 opcache 关闭和 `opcache.file_cache_only=1` 的 FPM 同 worker 连续请求回归。
3. 增加 route clear/dispatch、缓存 rehash/churn、`pcntl_fork`、Swoole 多 worker/workerStop 与 Pool close 交错测试。
4. 增加 1/32/128/512/1024 并发、池突发扩容和满池排队、SQLite/Redis、worker RSS 趋势基准。
5. 在 Linux 增加 ASAN 任务；它用于发现实际执行路径问题，不替代功能和生命周期回归。

---

## 6. 明确保持不变的边界

以下方向不纳入当前性能实现：

- Cache RCU：回收 epoch 和裸指针生命周期成本过高。
- 无版本迁移的默认缓存哈希切换：会破坏滚动发布期间的缓存命中。
- 使用全量响应缓冲替代流式 `write()`：会破坏 SSE、chunked、TTFB 和大响应内存语义。
- worker 级长期持有 Webscan PHP 对象：请求生命周期和配置更新语义复杂。
- 仅取 64-bit 随机数替换 Session ID：Session ID 应使用至少 16 字节随机数，这是安全改进而非性能项目。

## 7. 推荐实施顺序

严格按实现复杂度从低到高推进：

1. §2 低复杂度项目；
2. §3 中等复杂度项目；
3. §4 高复杂度项目；
4. §5 平台和增强验证可与任意阶段并行。

同一复杂度内按文档顺序执行即可。任何项目都可以直接启动；是否完成只由该项目的自动化功能与性能验收脚本决定。
