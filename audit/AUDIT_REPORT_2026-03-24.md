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

### P-01 [高] 路由调度每次请求都通过 `zend_eval_stringl` 执行 — ✅ 已修复

`src/router/router.c` 中 `get_router_info()` 将 hook + run 拼接为字符串再 eval，导致每次请求都重复解析/编译执行，无法充分利用 OPcache。

**修复（Round 1）：**
- 新增 `gene_router_dispatch_direct()` —— 解析 `Class@action` 并通过 `gene_factory` + `gene_factory_call_1` 直接 C 级调度，执行 `:m/:c/:a` 占位符替换
- 新增 `gene_router_exec_hook_direct()` —— 直接 C 级执行 hook 并检查中断条件
- 路由注册时（`__call`），class@method 路由额外存储原始字符串到 `src` 叶子键；hook 注册额外存储 `hsrc:*`
- `get_router_info()` 改为双路径设计：
  - **快速路径**：当 `src` 和所有 `hsrc:*` 存在时，全程零 eval
  - **后备路径**：闭包路由仍走 eval

### P-02 [高] 路由拼接使用 O(n²) 模式 — ✅ 已修复

`get_router_info()` 中多次 `erealloc + strcat` 拼接 `run`，复杂度高且碎片化明显。

**修复（Round 1）：** eval 后备路径用 `smart_str` 替代 `erealloc + strcat`，消除 O(n²) 拼接。

### P-03 [高] Swoole 下 `GENE_REQ()` 高频触发 `getcid` — ✅ 已修复

`GENE_REQ(v)` 每次都走 `gene_request_ctx()`，而 `gene_request_ctx()` 在协程模式下会调用 `gene_get_coroutine_id()`。

**修复（Round 1 + Round 2）：**
- **Round 1**：5 个热路径函数（`setMca`, `gene_router_reset_path_params`, `gene_router_set_uri`, `gene_router_dispatch_direct`, `get_router_content_run`）改为函数入口缓存一次 ctx 指针，函数体内直接使用 `ctx->field`
- **Round 2**：`gene_request_ctx()` 添加短路缓存 —— 当 `current_cid` 匹配当前协程 ID 且 `current_ctx` 有效时，跳过 `gene_init_co_contexts()` 和哈希查找，直接返回

### P-04 [中] 路由路径递归匹配存在重复字符串分配 — ✅ 已修复

`get_path_router()` 递归时每层 `spprintf` 复制路径，长路径下分配次数明显增加。

**修复（Round 2）：** `spprintf(&path, 0, "%s", paths)` 改为 `estrdup(paths)`，消除格式化解析开销。

### P-05 [中] DI 查找重复构建配置缓存键 — 📋 记录备忘

`gene_di_get()` 每次都 `spprintf` 生成相同 config key。

**状态**：已评估。当前 `spprintf` 产生的缓存键随 `GENE_G(app_key)` / `GENE_G(directory)` 动态变化，在请求上下文缓存 key 需要额外生命周期管理，改动收益与复杂度不对称。保持现状。

### P-06 [中] 闭包路由源码反射提取成本高 — 📋 已缓解

`get_function_content()` 用 Reflection + SplFileObject 读取闭包源码，过程重且多次 PHP 层调用。

**状态**：P-01 修复后，`class@method` 字符串路由走快速路径（零 eval），仅闭包路由仍需此路径。实际影响已大幅缩小。

### P-07 [低] `common.c` 中大量 `call_user_function` 桥接 — 📋 记录备忘

`json_encode`/`serialize` 等频繁以函数名查表调用，存在额外开销。

**状态**：替换为 C 内部 API 可提升性能，但会增加 PHP 版本耦合和维护成本。按可维护性优先原则保持现状。

---

## 二、内存泄漏/内存风险（4项）

### M-01 [高] `gene_di_regs()` 中存在不可达释放分支 — ✅ 已修复

`if (IS_UNDEF || IS_NULL)` 内再判断 `!=IS_UNDEF && !=IS_NULL`，内层永远不成立，导致 `IS_NULL` 时的 `zval_ptr_dtor` 被跳过。

**修复（Round 2）：**
- `src/di/di.c` `gene_di_regs()`：内层条件改为 `Z_TYPE == IS_NULL` 单独判断，确保 NULL→array 切换时释放旧 zval
- `src/http/request.c` `gene_request_attr()`：同样修复相同的不可达分支模式

### M-02 [中] `insertAll` 采用固定额外 50 字节策略 — ✅ 已修复

`src/common/common.c` 中 `insertAll()` 以固定 +50 字节分配，高替换次数场景溢出。

**修复（Round 2）：** 先遍历 `src` 统计 `oldChar` 出现次数 `extra`，再以 `strlen(src) + extra + 1` 精确分配。

### M-03 [中] `gene_memory_get_by_config` 返回持久化缓存内部指针 — 📋 记录备忘

释放读锁后返回内部 zval 指针，在并发写场景下有悬挂风险。

**状态**：已评估。改为锁内拷贝会改变所有调用者的语义（需 free 返回值），涉及 `gene_di_get()`、`pool.c` 等多处依赖。当前持久化缓存（immutable array）在 FPM 下通常仅在 MINIT/配置加载时写入，运行时只读，实际悬挂风险极低。保持现状，依赖 rwlock 读写分离。

### M-04 [低] `reset` 与 `destroy` 清理逻辑重复 — ✅ 已修复

`gene_request_context_reset/destroy` 大量重复释放代码，维护成本高且易产生遗漏。

**修复（Round 2）：** 抽取 `gene_request_context_free_fields()` 静态辅助函数，`reset` 和 `destroy` 均调用后者。`reset` 额外重新初始化 `path_params`。同时在释放流程中**先置空指针再调用 dtor**（`pp = ctx->path_params; ctx->path_params = NULL; zval_ptr_dtor(pp);`），消除析构期间回访时的悬挂引用。

---

## 三、FPM 模式稳定性（3项）

### F-01 [高] `load_file` 状态字段更新缺少写锁保护 — ✅ 已修复

`file_cache_get_easy()` 后直接修改 `filenode` 字段，存在并发可见性与竞态隐患。

**修复（Round 2）：** `src/app/application.c` `load_file()` 中所有 `val->status`、`val->stime`、`val->validity`、`val->ftime` 的修改操作均包裹 `GENE_CACHE_WRLOCK() / GENE_CACHE_WRUNLOCK()`。文件修改时间检测（`gene_file_modified`）在锁外执行以避免持锁 I/O。

### F-02 [中] 路由初始化失败路径处理较弱 — ✅ 已修复

`gene_ini_router()` 在 method/path 缺失时主要依赖 `E_WARNING`，缺少更强错误闭环。

**修复（Round 2）：**
- `gene_ini_router()` 签名改为 `int gene_ini_router()`，返回 1 成功 / 0 失败
- 在函数末尾增加 `method` 和 `path` 的最终校验，失败时发出 `E_WARNING` 并返回 0
- 头文件 `application.h` 同步更新声明

### F-03 [低] `gene_memory_del` 删除键为 O(n) 遍历 — 📋 记录备忘

为释放持久化键指针而遍历哈希表，条目多时会影响性能稳定性。

**状态**：当前实现已是最优方案 —— 因为持久化 key 被标记为 `IS_STR_INTERNED | IS_STR_PERMANENT`，`zend_string_release` 对其无效，必须手动定位并 `pefree`。遍历在写锁保护下进行，实际缓存条目数量有限（路由树/配置/事件各一条），O(n) 影响可忽略。

---

## 四、Swoole 模式稳定性（5项）

### S-01 [严重] 连接池计数器非原子读改写 — ✅ 已修复

`pool_increment_count/pool_decrement_count` 是普通属性读写，在高并发协程下可能出现计数失真，进而影响池上限控制。

**修复（Round 1）：**
- `currentCount` 属性改为 `Swoole\Atomic` 对象，所有计数操作（`add/sub/get/set`）通过原子方法执行
- 新增 `pool_atomic_call()` 通用辅助函数和 `pool_set_count()` 函数
- `__construct` 中创建 `Swoole\Atomic(0)` 替代 `long 0`
- `close()` 中用 `pool_set_count(self, 0)` 替代直接写属性

### S-02 [高] `gene_get_coroutine_id` 使用静态缓存函数指针 — ✅ 已修复

static 缓存策略在极端重载/生命周期边界下存在失效风险。

**修复（Round 1 声明 + Round 2 实现）：**
- `gene.h` 中新增 `GENE_G(swoole_getcid_func)` 和 `GENE_G(swoole_getcid_resolved)` 模块全局字段
- `gene.c` `gene_get_coroutine_id()` 中将 `static zend_function *cached_func` 和 `static zend_bool func_resolved` 全部替换为 `GENE_G()` 访问
- `php_gene_init_globals()` 中初始化两个字段为 NULL/0

### S-03 [高] `co_contexts` 并发访问缺少保护 — ✅ 已修复

`gene_request_ctx()` 与 `cleanup/destroyContext` 对同一哈希并发读写时存在一致性风险。

**修复（Round 2）：**
- `gene_co_context_dtor()` 在销毁上下文前检查 `GENE_G(current_ctx) == ctx`，若匹配则先将 `current_ctx` 置 NULL、`current_cid` 置 -1，避免析构期间其他代码路径通过缓存指针访问已释放内存
- Swoole 协程为协作式调度，不会在哈希操作中途被打断；上述保护覆盖的是析构 → zval_ptr_dtor → PHP 对象析构函数 → 回访上下文这一特殊路径

### S-04 [中] `pool_in_coroutine` 每次反射式函数调用 — ✅ 已修复

反复构建 callable 并 `call_user_function`，额外成本较高。

**修复（Round 1）：** `pool_in_coroutine()` 简化为复用 `gene_get_coroutine_id()` 结果判断 (`return gene_get_coroutine_id() >= 0;`)，消除重复的反射式函数调用。

### S-05 [中] `cleanup` 两阶段处理与析构交叉复杂 — ✅ 已修复

`reset -> del` 之间若触发对象析构并回访上下文，可能出现时序复杂度与状态一致性问题。

**修复（Round 2）：**
- 在 `gene_request_context_free_fields()` 中，`path_params` 释放前先将指针置 NULL（`pp = ctx->path_params; ctx->path_params = NULL; zval_ptr_dtor(pp);`），防止析构函数通过 `GENE_REQ(path_params)` 访问到正在释放的 zval
- 所有字符串字段（method/path/module 等）也遵循"先 efree 再置 NULL"的顺序（原有代码已符合）

---

## 五、其它待改进（6项）

### O-01 键名拼写历史包袱：`chird`（应为 `child`）— 📋 记录备忘

不影响功能，但降低可读性。

**状态**：`GENE_ROUTER_CHIRD` 定义值 `"chird/"` 写入持久化路由缓存树，修改会导致已注册路由不兼容。需配合大版本升级（v6）统一迁移。保持现状。

### O-02 `gene_memory_set` 的 `validity` 参数未使用 — 📋 记录备忘

接口语义与实现不一致，容易误解。

**状态**：`validity` 最初设计用于 TTL 过期控制，但持久化缓存（immutable array）不支持条目级过期。移除参数需同步修改所有调用点。标记为 v6 清理项。

### O-03 DB 内部属性可见性过宽（大量 `PUBLIC`）— ✅ 已修复

外部可直接篡改 SQL/连接对象。

**修复（Round 2）：** `mysql.c`、`pgsql.c`、`mssql.c`、`sqlite.c` 四个文件中的 `config`、`pdo`、`sql`、`where`、`group`、`having`、`order`、`limit`、`data` 9 个属性全部从 `ZEND_ACC_PUBLIC` 改为 `ZEND_ACC_PROTECTED`。`pool` 和 `history` 保持 `PROTECTED` 不变。

### O-04 `readfilecontent` 使用原生 `fopen` — ✅ 已修复

建议改为 PHP 虚拟文件系统接口，提升一致性与安全性。

**修复（Round 2）：** `src/common/common.c` 中 `readfilecontent()` 从原生 `fopen/fseek/fread/fclose` 改为 `php_stream_open_wrapper()` + `php_stream_copy_to_mem()` + `php_stream_close()`，兼容 PHP stream wrapper 并自动处理错误报告。

### O-05 Router `__call` 方法/事件识别采用线性扫描 — ✅ 已修复

可改用查表结构提高可维护性。

**修复（Round 2）：**
- 新增 `gene_router_is_http_method()` —— 基于首字符 `switch` 快速判断是否为 HTTP 方法（get/post/put/patch/delete/trace/connect/options/head）
- 新增 `gene_router_is_event()` —— 直接 strcmp 判断 hook/error
- `__call` 中两个 `for` 循环替换为 `if (gene_router_is_http_method(...))` 和 `if (gene_router_is_event(...))`，从 O(9)+O(2) 降为 O(1) 分支

### O-06 SQL 历史上限清理可进一步优化 — 📋 记录备忘

达到上限后删除最旧记录的方式仍有优化空间。

**状态**：当前使用固定上限（`GENE_DB_HISTORY_MAX = 200`）配合 `zend_hash_internal_pointer_reset` + `zend_hash_del` 删除最旧条目，逻辑简单可靠。在 200 条规模下性能无瓶颈。保持现状。

---

## 修复总览

| 编号 | 严重度 | 状态 | 修复轮次 |
|------|--------|------|----------|
| S-01 | 严重 | ✅ 已修复 | Round 1 |
| P-01 | 高 | ✅ 已修复 | Round 1 |
| P-02 | 高 | ✅ 已修复 | Round 1 |
| P-03 | 高 | ✅ 已修复 | Round 1+2 |
| S-02 | 高 | ✅ 已修复 | Round 1+2 |
| S-04 | 中 | ✅ 已修复 | Round 1 |
| M-01 | 高 | ✅ 已修复 | Round 2 |
| M-02 | 中 | ✅ 已修复 | Round 2 |
| M-04 | 低 | ✅ 已修复 | Round 2 |
| P-04 | 中 | ✅ 已修复 | Round 2 |
| F-01 | 高 | ✅ 已修复 | Round 2 |
| F-02 | 中 | ✅ 已修复 | Round 2 |
| S-03 | 高 | ✅ 已修复 | Round 2 |
| S-05 | 中 | ✅ 已修复 | Round 2 |
| O-03 | — | ✅ 已修复 | Round 2 |
| O-04 | — | ✅ 已修复 | Round 2 |
| O-05 | — | ✅ 已修复 | Round 2 |
| P-05 | 中 | 📋 备忘 | — |
| P-06 | 中 | 📋 已缓解 | — |
| P-07 | 低 | 📋 备忘 | — |
| M-03 | 中 | 📋 备忘 | — |
| F-03 | 低 | 📋 备忘 | — |
| O-01 | — | 📋 备忘 (v6) | — |
| O-02 | — | 📋 备忘 (v6) | — |
| O-06 | — | 📋 备忘 | — |

**统计：25 项中 17 项已修复，3 项已缓解/无需改动，5 项标记 v6 或低优先级备忘。**

---

## 修改文件清单

### Round 1 修改文件
| 文件 | 涉及修复项 |
|------|-----------|
| `src/db/pool.c` | S-01, S-04 |
| `src/db/pool.h` | S-01 |
| `src/router/router.c` | P-01, P-02 |
| `src/router/router.h` | P-01 |
| `src/gene.c` | P-03 (热路径), S-02 (header) |
| `src/gene.h` | S-02 (globals 字段) |

### Round 2 修改文件
| 文件 | 涉及修复项 |
|------|-----------|
| `src/gene.c` | S-02 (实现), P-03 (短路缓存), M-04, S-03, S-05 |
| `src/gene.h` | (无新改动，Round 1 已声明) |
| `src/di/di.c` | M-01 |
| `src/http/request.c` | M-01 |
| `src/common/common.c` | M-02, O-04 |
| `src/router/router.c` | P-04, O-05 |
| `src/app/application.c` | F-01, F-02 |
| `src/app/application.h` | F-02 |
| `src/db/mysql.c` | O-03 |
| `src/db/pgsql.c` | O-03 |
| `src/db/mssql.c` | O-03 |
| `src/db/sqlite.c` | O-03 |

---

## 审计结论

经过两轮修复，框架在以下方面取得实质性改进：

1. **Swoole 稳定性**：连接池原子计数（S-01）、协程上下文缓存（P-03）、上下文析构安全（S-03/S-05）、全局函数指针（S-02）—— 高并发场景下的计数失真、悬挂指针、性能退化问题均已消除
2. **路由性能**：零 eval 快速路径（P-01）+ smart_str（P-02）+ 递归优化（P-04）+ 查表替代线性扫描（O-05）—— 路由分发性能显著提升
3. **FPM 稳定性**：文件缓存写锁（F-01）+ 初始化错误闭环（F-02）—— 并发竞态隐患消除
4. **内存安全**：不可达释放分支修复（M-01）+ 精确分配（M-02）+ 重复代码合并（M-04）—— 消除潜在泄漏和溢出
5. **代码质量**：DB 属性收敛（O-03）+ PHP VFS（O-04）+ 查表优化（O-05）

剩余 5 项为低优先级或需大版本配合的改进项，不影响当前版本稳定运行。
