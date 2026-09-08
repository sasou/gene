# Gene 扩展极致并发优化 —— V1 已完成归档

> 版本：v7-close（2026-09-08）
> V1 所列项目均已完成落地并通过当时支持范围内的功能验收。尚待实现的优化、自动化性能验收增强和平台扩展见 `PERFORMANCE_OPTIMIZATION_V2.md`。

## 0. 归档原则

1. **只记录已完成结果**：本文不再保留候选方案、启动门槛、准入分类或未实施设计。
2. **只按实现复杂度归档**：已完成项目按低、中、高复杂度排列，不按收益预测、风险等级、D1–D4 或历史实施批次排序。
3. **不补写未测收益**：已有数据证明功能正确或特定路径得到改善，不自动等同于整机 RPS、p99 或 RSS 提升。
4. **验收结果可复现**：保留构建环境、测试数量、专项场景和关键指标；后续统一接入 V2 的自动化验收入口。
5. **增强验证不影响已落地状态**：ZTS、ASAN、高并发梯度、FPM/fork/opcache 矩阵等补充验证在 V2 独立推进；发现缺陷时重新立项修复。
6. **兼容性边界继续有效**：公开 API、缓存所有权、流式响应、Pool 生命周期和配置默认值不得因归档简化而改变。

## 1. V1 完成范围

### 1.1 支持范围

V1 已完成以下环境中的实现和功能验收：

- Windows PHP 8.1.30 NTS x64 Release。
- Linux PHP 8.1.34 NTS DEBUG + Swoole 6.1.9，Gene 6.2.1。

V1 不包含 ZTS 多线程 SAPI 的正式认证。opcache 关闭、`opcache.file_cache_only=1`、Linux ASAN、512/1024 并发梯度、FPM 同 worker、`pcntl_fork`、RSS 长跑和专项 ns/op/p99 由 V2 的自动化验收任务继续覆盖。

### 1.2 总体验收结果

| 环境 | 验收结果 |
|---|---|
| Windows NTS x64 Release | 构建成功；`TestRunner` **884/884** |
| Linux NTS + Swoole | 编译零警告；`TestRunner` **896/896** |
| Swoole 上下文隔离 | 10 万请求 / 500 并发，manual + auto 两轮通过 |
| MySQL Pool | 200 协程 × 1000 次借还，0 failures |
| Redis Pool | 200 协程 × 1000 次借还，0 failures |
| 事务泄漏防护 | 未提交事务归还时自动 rollback，后续借出连接状态干净 |
| 缓存 churn | 5000 轮不同 key 的 set/get/del 通过，无新增拒写 |

这些结果证明 V1 支持范围内的构建、功能和已执行并发场景通过，不代表所有负载下均有相同幅度的性能收益。

---

## 2. 已完成项目：低复杂度

### 2.1 模板编译检查默认值修正

**实现**：`gene.view_compile_check_mtime` 默认值由 `0` 改为 `1`。运行时模板编译不再因默认关闭 mtime 检查而每次请求重复执行 28 轮 `php_pcre_replace`。显式配置 `0` 仍保留旧行为，CHANGELOG 已记录。

**配置语义**：

- 运行时编译缓存：`gene.view_compile=1`、`gene.view_compile_check_mtime=1`。
- 离线预编译：构建期生成 `Cache/Views/*.php`，运行时设置 `gene.view_compile=0`，调用方不传 `isCompile=true`。

**已执行验收**：覆盖缓存缺失、源文件更新、编译失败及 `display()` / `displayExt()` 分支；全量回归通过。

### 2.2 Benchmark 热路径 C API 化

**实现**：Benchmark 计时改用 `gene_hrtime()` 单调纳秒，峰值内存改用 `zend_memory_peak_usage(0)`，对外输出格式保持不变。

**已执行验收**：`BenchmarkTest` **42/42**，全量回归通过。

### 2.3 ORM known-function 调用

**实现**：`gene_orm_db_call()` 对精确内部 `Gene\Db\*` CE 使用 `zend_call_known_function`；非 Gene Db 和 mock 对象继续走动态回退路径。

**已执行验收**：`OrmTest` **177/177**，内部类和 mock 回退行为一致。

### 2.4 路由 action 单次查找

**实现**：direct dispatch 合并为一次 `zend_hash_str_find_ptr()`；新增 `gene_factory_call_1_known()`。action 函数指针不写入持久 `route_pc`，避免动态 controller/action 派发时缓存错误目标。

**已执行验收**：`RouterTest` **42/42**，全量回归通过。

### 2.5 static interned string 生命周期修复

**实现**：`cache/cache.c` 中 7 处文件作用域 static 方法/函数名缓存统一改用 `GENE_INTERNED_STR()`。仅在 `IS_STR_PERMANENT` 时跨请求保留指针，其他情况下不缓存请求级字符串。

**已执行验收**：Windows PHP 8.1 NTS 构建成功，`TestRunner` **884/884**。

**边界**：V1 只认证 NTS；文件作用域 `zend_function*`、`zend_class_entry*`、`HashTable*` 的 ZTS 初始化和多线程生命周期验证归入 V2。

### 2.6 观测指标补齐

**实现**：新增或保留以下指标：

- 缓存：`cache_num_used`、`cache_num_elements`、`cache_table_size`、`cache_insert_refused`。
- 分表：`framework_cache_items`、`business_cache_items`、`business_cache_num_used`、`business_cache_table_size`。
- 上下文：`co_contexts_*`、`ctx_pool_*`、`swoole_auto_cleanup_defers/reclaimed`。
- Pool：`total`、`idle`、`using`、`overflow`、`min`、`max`、`closed`。
- Pool 诊断：`db_pool_idle_miss`、`redis_pool_idle_miss`、`db_pool_get_timeout`、`redis_pool_get_timeout`、`db_pool_pid_mismatch`、`redis_pool_pid_mismatch`、兼容保留的 `*_cas_abandoned`。
- 路由：`route_pc_items`、`route_pc_generation`、`route_pc_retired`。

`Memory::stats()`、`Pool::stats()`、`RedisPool::stats()`、`Monitor::stats()` 和 `Monitor::prometheus()` 已导出对应数据。

**指标语义**：

- 所有 `GENE_G` 计数器均为进程本地，多 worker 需要在扩展外聚合。
- `memory_cache_hit/miss` 只统计用户态 `Memory::get()`，不统计路由、DI 和配置内部读取。
- 计数器应使用同一 worker/PID 的区间增量和每请求比率分析，不使用跨进程累计值直接比较。

---

## 3. 已完成项目：中等复杂度

### 3.1 Pool CAS 漂移修复

**实现**：DB Pool 和 RedisPool 删除最多 64 轮的 `cmpset` 递减及放弃路径。连接槽预留、创建和销毁改为严格对称的 `Swoole\Atomic::add(+1)` / `sub(1)`；关闭、创建、回收和归还交错时不再重复递减，也不会在 Channel 关闭唤醒后误建 overflow 连接。

`db_pool_cas_abandoned` 和 `redis_pool_cas_abandoned` 作为兼容指标保留，新路径不再递增。

**已执行验收**：

- Windows：`DatabaseTest` **39/39**、`CacheTest` **63/63**、全量 **884/884**。
- Linux：MySQL/Redis Pool 各 200 协程 × 1000 次借还，0 failures。
- 结束状态：`total=2`、`idle=2`、`using=0`，计数无漂移。
- 未提交事务归还后自动 rollback，后续借出连接状态正常。

### 3.2 Pool 空闲队列固定等待消除

**实现**：DB Pool/RedisPool 的 `get()` 先调用 Channel `isEmpty()`。空队列直接进入 reserve/create 或饱和等待，不再先执行 `pop(0.001)`。判空到 pop 之间不 yield；队列关闭、创建失败、饱和等待和 `close()` 唤醒路径保持原总等待预算，并在关闭后再次检查状态。

DB Pool 直接构造与静态 `create()` 均只执行一次 min 预填。新增 DB/Redis idle miss 和 timeout 指标。

**已执行验收**：Pool min 预填结果为 `total=2/idle=2`；MySQL/Redis Pool 各 200 协程 × 1000 次借还，0 failures；idle miss 指标无异常增长。

### 3.3 Pool creator PID 生命周期约束

**实现**：DB Pool/RedisPool 构造时记录 `creatorPid`。`get/put/remove/recycleIdle/close/healthCheck/stats` 及静态清理路径拒绝 PID 不匹配的跨 fork 使用，并累计 `db_pool_pid_mismatch` / `redis_pool_pid_mismatch`。

两个 Pool 均为 final、私有 `__clone`，并设置 `ZEND_ACC_NOT_SERIALIZABLE`。计数仍使用 `Swoole\Atomic`，未改成不具备跨进程语义的普通 `zend_long`。

**已执行验收**：clone/serialize、min 预填、worker 内正常借还和 PID 一致路径通过；Linux MySQL/Redis Pool 各 200 协程 × 1000 次借还无 mismatch。

### 3.4 进程缓存观测与上下文管理

**实现**：

- `gene_request_context` 内联 `path_params`。
- `ctx_pool` 复用 context struct。
- `co_contexts` 使用冷却式 sweep。
- 同协程 `vm_stack` 快路径跳过 `getcid()`。
- CID 复用时校验表指针身份。
- `gene.swoole_getcid_capi=1` 默认启用，通过 `dlsym` 直调 Swoole C API，并保留回退路径。

**已执行验收**：Linux 上下文隔离 10 万请求 / 500 并发，manual + auto 两轮通过，`ctx_pool_hit` 约 99.5%，无上下文泄漏。

---

## 4. 已完成项目：高复杂度

### 4.1 `route_pc` generation 失效与延迟回收

**问题**：`Router::clear()/delTree()/delEvent()` 曾删除持久路由和 `fn_cache`，但不失效 `route_pc`。描述符借用路由树字符串和 closure zval，存在跨请求或协程交错时读取失效内存的风险。

**实现**：

1. 新增 `GENE_G(route_pc_generation)`。
2. `Router::clear()/delTree()/delEvent()` 调用 `gene_router_pc_invalidate()` 推进 generation。
3. 描述符记录生成号；执行前不匹配则摘除并回退 `get_router_info_slow()`。
4. 失效描述符挂入 `GENE_G(route_pc_retired)`，到 MSHUTDOWN 统一释放，避免中断在途借用。
5. 描述符不再持有 closure zval，只保存 `fn_cache` 键；执行时重新解析。
6. route、before、after、hook 四类 closure 在任何 hook 执行前一次性解析；缺失时整体回退，避免 hook 链只执行一部分。
7. 新增 `route_pc_generation`、`route_pc_retired` 指标。

**已执行验收**：

```text
php -d gene.route_precompile=1 audit/repro/route_pc_clear_invalidate.php
php -d gene.route_precompile=0 audit/repro/route_pc_clear_invalidate.php
```

两个模式输出逐行一致；generation 随 clear 推进，retired 同步增长，失效描述符不再执行；`RouterTest` 全部通过。

### 4.2 框架缓存与业务缓存拆表

**问题**：框架元数据与用户业务数据共用冻结 HashTable。业务写入会使框架读取永久退回加锁路径；删除产生的 tombstone 不降低 `nNumUsed`，高 churn 会耗尽预留 bucket 并静默拒绝新 key。

**实现**：

- 新增 `GENE_G(business_cache)`、`business_cache_expiry` 和 `business_cache_lock`。
- Router、Config、DI、Pool 配置继续使用框架表。
- PHP-facing `Gene\Memory` / `Gene\Cache` 使用独立业务表，不按 key 前缀猜测归属。
- 框架表在 `workerReady()` 后只读，保持 `arData` 稳定并持续使用跳锁路径。
- 业务表使用独立 rwlock、TTL、LRU，可正常 rehash。
- 业务读取通过 `gene_business_memory_get_copy()` 在锁内完成 owned deep copy，释放锁后不保留裸指针。
- `Memory::clean()` 只重建业务表，不触碰 Router、DI 和 Config。
- 新增框架表、业务表 items/num_used/table_size 指标。

**已执行验收**：

- Windows：5000 轮不同 key 的 set/get/del churn 通过，`cache_insert_refused` 无新增；`TestRunner` **884/884**。
- Linux：`TestRunner` **896/896**。
- 上下文隔离 10 万请求 / 500 并发，manual + auto 两轮通过。
- 结束时 `business_cache_items=0`，无 tombstone 耗尽或静默拒写。

**持续边界**：框架表裸指针仅可在启动后只读和稳定存储条件下借用；业务表必须在锁内完成 owned copy。Cache RCU 不采用。

---

## 5. 已结案但未修改代码

### 5.1 视图 output buffer 层数

源码复核确认：

- `render()` 每次调用只创建一层 `php_output_start_default`。
- `display()` / `displayExt()` 不创建 buffer，直接输出。
- include/contains 子视图写入当前 buffer，不增加层数。
- 只有显式嵌套调用 `render()` 才增加 buffer 层。

原“嵌套子视图共享一层 buffer”提案的前提不成立，因此结案且不修改代码。符号表继续使用 COW `ZVAL_COPY`，不共享可变 HashTable。

---

## 6. 当前配置基线

### 6.1 Gene Swoole 配置

| INI | 实际默认 | 生产建议 | 说明 |
|---|---:|---:|---|
| `gene.runtime_type` | 0 | `2` | Swoole 常驻模式 |
| `gene.run_environment` | 1 | `2` | 关闭 SQL 历史和 benchmark 采集 |
| `gene.view_compile` | 0 | 按发布方式 | 运行时编译缓存可设 `1` |
| `gene.view_compile_check_mtime` | 1 | `1` | 离线预编译可不进入编译分支 |
| `gene.route_precompile` | 0 | 由 V2 自动验收决定 | V1 已完成 generation 失效 |
| `gene.swoole_getcid_capi` | 1 | `1` | 失败时回退 PHP 调用 |
| `gene.swoole_auto_cleanup` | 0 | 按验收结果 | 每个新 context 注册一次 defer |
| `gene.co_contexts_max` | 1024 | 按 RSS 标定 | sweep 软阈值，不是并发上限 |
| `gene.ctx_pool_max` | 256 | 按 RSS 标定 | `workerReady()` 可预热 |
| `gene.cache_max_items` | 0 | 按业务标定 | 0 表示业务表不启用 LRU 上限 |
| `gene.cache_reserve` | 4096 | 自动矫正 | 生效 reserve 至少为 `max_items + max(64, max_items/4)` |
| Pool `waitTimeout` | 3.0 | 按 SLO | 超时后创建 overflow 连接 |

### 6.2 宿主配置记录

性能对比两侧必须使用完全相同的 PHP、OPcache、realpath cache、JIT、worker、数据库和编译配置，并将配置写入机器可读结果。V1 推荐基线：

```ini
opcache.enable                  = 1
opcache.enable_cli              = 1
opcache.memory_consumption      = 256
opcache.interned_strings_buffer = 32
opcache.max_accelerated_files   = 50000
opcache.validate_timestamps     = 0
opcache.save_comments           = 1
realpath_cache_size             = 4096k
realpath_cache_ttl              = 600
```

该配置用于减少测试噪声，不是启动其他优化的前置门槛。JIT、preload、opcache 关闭和 `file_cache_only=1` 均由对应自动化场景独立验证。

---

## 7. 验收记录

### 7.1 Windows NTS

- PHP 8.1.30 NTS x64 Release 构建成功。
- `CacheTest` **63/63**。
- `DatabaseTest` **39/39**。
- `RouterTest` **42/42**。
- `OrmTest` **177/177**。
- `BenchmarkTest` **42/42**。
- `TestRunner` **884/884**。

### 7.2 Linux Swoole

- PHP 8.1.34 NTS DEBUG + Swoole 6.1.9，Gene 6.2.1。
- 编译零警告，`TestRunner` **896/896**。
- Swoole getcid/route_precompile 四组合摘要一致，全部通过。
- Debug 构建约 10.5K req/s；该数据只记录当时场景，不作为通用收益结论。
- 上下文隔离 10 万请求 / 500 并发通过。
- MySQL/Redis Pool 各 200 协程 × 1000 次借还通过。
- 事务归还自动 rollback 验证通过。

### 7.3 已有验收入口

- `test/TestRunner.php`：全量功能回归。
- `audit/repro/route_pc_clear_invalidate.php`：route_pc generation 失效回归。
- `tools/acceptance/swoole_benchmark.php`：getcid/route_precompile 正确性矩阵。
- `tools/acceptance/linux_swoole_verify.sh`：构建、协程、Pool、HTTP 和 RSS 验收入口。
- `tools/acceptance/linux_swoole_profile.sh`：单 worker profiling、warm-up、压测、FlameGraph 和结果打包。
- `tools/acceptance/fpm_benchmark.php`：FPM 外部 benchmark 多轮调用入口。

V2 将这些入口统一为“功能回归 + 性能对比 + 自动判定 + 机器可读留档”的验收流程。V1 项目无需重新通过人工准入；补充验证只根据脚本结果确认现状或重新建立缺陷。

## 8. V1 关闭结论

V1 已完成以下核心落地：

1. 修复 route_pc 清理后的生命周期问题。
2. 修复 Pool CAS 计数漂移并减少借还调用成本。
3. 消除 Pool 空闲队列 miss 的固定 1 ms 等待。
4. 为 Pool 增加 creator PID、clone 和 serialize 生命周期约束。
5. 将框架缓存与业务缓存拆分，消除框架读锁性能悬崖和业务 tombstone 拒写问题。
6. 修复 NTS 范围内 static interned string 跨请求生命周期问题。
7. 完成模板、ORM、路由、Benchmark、上下文和观测等低至中复杂度优化。

以上项目已经落地，不再设置或回溯任何启动门槛。后续是否继续调整，只由 V2 自动化验收发现的功能错误、性能未达标或保护指标退化决定。
