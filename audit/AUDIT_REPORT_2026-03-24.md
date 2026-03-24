# Gene 框架代码审计报告

审计时间：2026-03-24  
审计范围：`src/` 核心 C 扩展实现（Gene v5.4.0）  
审计目标：
1. 性能优化
2. 内存泄漏
3. FPM 模式稳定性
4. Swoole 模式稳定性
5. 其它可改进项

---

## 一、性能优化（7项）

### P-01 [高] 路由调度每次请求都通过 `zend_eval_stringl` 执行

`src/router/router.c` 中 `get_router_info()` 将 hook + run 拼接为字符串再 eval，导致每次请求都重复解析/编译执行，无法充分利用 OPcache。

建议：改为 `call_user_function` / `zend_call_method` 直接调用，或至少做预编译缓存。

### P-02 [高] 路由拼接使用 O(n²) 模式

`get_router_info()` 中多次 `erealloc + strcat` 拼接 `run`，复杂度高且碎片化明显。

建议：使用 `smart_str` 统一构建。

### P-03 [高] Swoole 下 `GENE_REQ()` 高频触发 `getcid`

`GENE_REQ(v)` 每次都走 `gene_request_ctx()`，而 `gene_request_ctx()` 在协程模式下会调用 `gene_get_coroutine_id()`。

建议：利用 `current_cid/current_ctx` 做短路缓存，仅在上下文失效时重新取 CID。

### P-04 [中] 路由路径递归匹配存在重复字符串分配

`get_path_router()` 递归时每层 `spprintf` 复制路径，长路径下分配次数明显增加。

建议：改为基于偏移量或一次 tokenize。

### P-05 [中] DI 查找重复构建配置缓存键

`gene_di_get()` 每次都 `spprintf` 生成相同 config key。

建议：在请求上下文缓存 key。

### P-06 [中] 闭包路由源码反射提取成本高

`get_function_content()` 用 Reflection + SplFileObject 读取闭包源码，过程重且多次 PHP 层调用。

建议：避免源码提取路径，改为直接可调用对象存储与调度。

### P-07 [低] `common.c` 中大量 `call_user_function` 桥接

`json_encode`/`serialize` 等频繁以函数名查表调用，存在额外开销。

建议：可替换为对应内部 C API（按可维护性权衡）。

---

## 二、内存泄漏/内存风险（4项）

### M-01 [高] `gene_di_regs()` 中存在不可达释放分支

`if (IS_UNDEF || IS_NULL)` 内再判断 `!=IS_UNDEF && !=IS_NULL`，内层永远不成立，逻辑错误。

建议：重构判断，避免误导并消除潜在清理遗漏。

### M-02 [中] `insertAll` 采用固定额外 50 字节策略

`src/common/common.c` 中 `insertAll()` 可能在高替换次数场景溢出。

建议：先统计替换次数后精确分配。

### M-03 [中] `gene_memory_get_by_config` 返回持久化缓存内部指针

释放读锁后返回内部 zval 指针，在并发写场景下有悬挂风险。

建议：锁内拷贝到请求内存，或加强读写一致性策略。

### M-04 [低] `reset` 与 `destroy` 清理逻辑重复

`gene_request_context_reset/destroy` 大量重复释放代码，维护成本高且易产生遗漏。

建议：抽公共释放函数。

---

## 三、FPM 模式稳定性（3项）

### F-01 [高] `load_file` 状态字段更新缺少写锁保护

`file_cache_get_easy()` 后直接修改 `filenode` 字段，存在并发可见性与竞态隐患。

建议：状态更新过程加写锁。

### F-02 [中] 路由初始化失败路径处理较弱

`gene_ini_router()` 在 method/path 缺失时主要依赖 `E_WARNING`，缺少更强错误闭环。

建议：补充统一错误返回机制（尤其入口层）。

### F-03 [低] `gene_memory_del` 删除键为 O(n) 遍历

为释放持久化键指针而遍历哈希表，条目多时会影响性能稳定性。

建议：优化键生命周期管理策略，减少全表扫描删除。

---

## 四、Swoole 模式稳定性（5项）

### S-01 [严重] 连接池计数器非原子读改写

`pool_increment_count/pool_decrement_count` 是普通属性读写，在高并发协程下可能出现计数失真，进而影响池上限控制。

建议：采用原子计数（如 `Swoole\Atomic`）或以通道状态为核心计数源。

### S-02 [高] `gene_get_coroutine_id` 使用静态缓存函数指针

static 缓存策略在极端重载/生命周期边界下存在失效风险。

建议：改为模块全局并在合适生命周期刷新。

### S-03 [高] `co_contexts` 并发访问缺少保护

`gene_request_ctx()` 与 `cleanup/destroyContext` 对同一哈希并发读写时存在一致性风险。

建议：确保关键段不可重入，必要时增加轻量锁或严格调用时序约束。

### S-04 [中] `pool_in_coroutine` 每次反射式函数调用

反复构建 callable 并 `call_user_function`，额外成本较高。

建议：复用 `gene_get_coroutine_id()` 结果判断。

### S-05 [中] `cleanup` 两阶段处理与析构交叉复杂

`reset -> del` 之间若触发对象析构并回访上下文，可能出现时序复杂度与状态一致性问题。

建议：明确 cleanup 状态机，先失效指针再执行析构路径。

---

## 五、其它待改进（6项）

### O-01 键名拼写历史包袱：`chird`（应为 `child`）

不影响功能，但降低可读性。

### O-02 `gene_memory_set` 的 `validity` 参数未使用

接口语义与实现不一致，容易误解。

### O-03 DB 内部属性可见性过宽（大量 `PUBLIC`）

外部可直接篡改 SQL/连接对象，建议收敛到 `PROTECTED/PRIVATE`。

### O-04 `readfilecontent` 使用原生 `fopen`

建议改为 PHP 虚拟文件系统接口，提升一致性与安全性。

### O-05 Router `__call` 方法/事件识别采用线性扫描

可改用查表结构提高可维护性。

### O-06 SQL 历史上限清理可进一步优化

达到上限后删除最旧记录的方式仍有优化空间。

---

## 优先级建议（先做这三项）

1. **S-01**：连接池计数原子化（Swoole 稳定性核心）
2. **P-01**：去除路由主链 `zend_eval_stringl`
3. **P-03**：优化协程上下文获取路径（减少 `getcid` 高频开销）

---

## 审计结论

框架核心能力完整，且已具备不少稳定性增强代码（上下文清理、连接池回收、请求隔离等），但在**高并发 Swoole 场景**与**路由执行性能模型**上仍有明显优化空间。  
建议以“先稳定性、后性能微调”为实施顺序，逐步推进。

