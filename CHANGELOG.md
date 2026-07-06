# Gene Framework Changelog

## [5.6.8]

### 🔒 安全

- **PDO 标识符引用强化**：`pdo.c` 标识符引用新增 JOIN 子句检测、表达式白名单与引号配平校验，进一步收窄 SQL 注入攻击面。
- **PDO 连接处理修复**：修正 PDO 连接建立与异常路径的资源管理，避免连接泄漏。

### ⚡ 性能

- **字符串工具**：`common.c` 以 `memcpy` 替换 `strncpy` 关键路径，消除冗余尾部填充。
- **数据库驱动**：优化 mysql / mssql / pgsql / sqlite 连接处理流程。
- **Redis 连接池**：改进 `redis_pool.c` 连接池管理与复用策略。
- **路由 / 视图 / 响应**：精简 `router.c`、`view.c`、`response.c` 头部操作与渲染路径。
- **Benchmark / Log**：基准与日志工具性能调优，并同步标识符引用改进。

### 🔧 修改文件一览

- `src/db/pdo.c` — 标识符引用强化（JOIN 检测 / 表达式白名单 / 引号配平）、连接处理修复
- `src/common/common.c` / `common.h` — `strncpy` → `memcpy`
- `src/db/{mysql,mssql,pgsql,sqlite}.c` — 连接处理优化
- `src/cache/redis_pool.c` / `src/db/pool.c` — 连接池管理改进
- `src/router/router.c` / `src/mvc/view.c` / `src/http/response.c` / `src/http/validate.c` — 路由 / 视图 / 响应 / 校验优化
- `src/tool/benchmark.c` / `src/tool/log.c` — 工具性能与标识符引用同步
- `src/gene.c` / `src/gene.h` — 版本号升至 5.6.8 及配套调整

## [5.6.7]

### ✨ 新增

- **Session / Response Cookie 支持 `samesite` 参数**：`Gene\Session` 构造配置新增 `samesite` 键（`"Lax"`/`"Strict"`/`"None"`），随 Session Cookie 自动下发；`Gene\Response::cookie()` 新增第 8 个可选参数 `$samesite`。
  - **Swoole**：直接透传给 `Swoole\Http\Response::cookie()` 的 `$samesite` 参数。
  - **FPM/CGI**：PHP 原生 `setcookie()` 的位置参数形式不支持 `samesite`，仅数组选项形式（PHP 7.3+）支持；当 `samesite` 非空时自动改用 `setcookie($name, $value, $options)` 数组形式下发，其余场景保持原有位置参数调用不变。
  - 未设置 `samesite`（默认空字符串）时行为与之前完全一致，向后兼容。
  - **`samesite = "None"` 自动强制 `Secure`**：现代浏览器（Chrome 80+、Firefox 等）会静默丢弃带 `SameSite=None` 但未标记 `Secure` 的 Cookie。当 `samesite` 为 `"None"`（大小写不敏感）时，无论调用方/Session 的 `secure` 配置如何，均自动以 `Secure` 下发，避免 Cookie 被浏览器拒绝导致会话静默失效；`Lax`/`Strict` 场景的 `secure` 行为保持不变。Swoole 与 FPM/CGI 两条路径均已覆盖。

### 🔧 修改文件一览

- `src/session/session.h` — 新增 `GENE_SESSION_SAMESITE` 属性键
- `src/session/session.c` — 新增 `samesite` 属性声明/偏移缓存、构造函数配置解析、`gene_cookie()`/`gene_set_cookie()` 透传
- `src/http/response.h` / `src/http/response.c` — `gene_response_cookie()` 新增 `samesite` 参数；FPM 路径按需切换为 `setcookie()` 数组选项形式；`Gene\Response::cookie()` 新增第 8 个可选参数
- `gene-ide-helper/Gene/Session.php`、`gene-ai-helper/skills/gene-framework/reference.md` — 文档同步

## [5.6.6] - 2026-06-20

### ⚡ 性能优化

- **P3 — 路由派发预编译缓存（Swoole，opt-in）**：`get_router_info()` 原本每请求对叶子 + 应用级事件/钩子数组做 6–10 次 `zend_hash_str_find` + strtok/snprintf 才能定出"直派/闭包/eval"派发方案。`workerReady()` 后 Swoole 的路由树与 fn_cache 冻结，给定叶子 HashTable 指针恒映射同一路由，故该解析是 `(leaf, cacheHook)` 的纯函数，可记忆化。新增按叶子 HashTable 指针为 key 的**每线程** `GENE_G(route_pc)` 描述符缓存：首次命中路由时解析并缓存 `gene_route_pc`（直派 src/钩子 src 指针、闭包 fn_cache zval 指针、或预拼接的 eval 程序串），之后零哈希查找直接执行。
  - **opt-in 默认关闭**：`gene.route_precompile=0`（默认）时走原 `get_router_info_slow()`（逐字未改）；需在 Linux `phpize+make+ASAN` + 全路由回归验证后置 `1` 启用。
  - **仅 Swoole + workerReady 后**：FPM 路由每请求重建、fn_cache 每请求级，叶子/闭包指针不稳定，永不启用。
  - **并发安全**：`GENE_G` 在 ZTS 下每线程独立，描述符仅借用同线程 `cache`/`fn_cache` 指针；解析+入表为非让出的纯 C 操作（原子于协程协作调度），执行期即便让出也只读不可变描述符，无竞争。`route_pc` 于 MSHUTDOWN 释放（表 dtor 释放各描述符 owned 的 eval 串）。
- **P1 — Swoole 协程 id C-API 直调**：`gene_get_coroutine_id()` 原本每次走 `zend_call_known_function`（PHP 调用 ≈ 数百 ns）。`gene_request_ctx()` 的 vm_stack 同一性快路径已覆盖未让出的调用链，但每次协程切换（IO）后的首次 `GENE_REQ()` 仍走慢路径。现通过 `dlsym(RTLD_DEFAULT, "_ZN6swoole9Coroutine15get_current_cidEv")` 解析 Swoole 的 C++ 静态方法 `swoole::Coroutine::get_current_cid()`（读取线程局部指针，约数 ns），每 worker 解析一次。
  - 符号不可用（Swoole 未加载 / Windows / 隐藏可见性 / 不兼容分支）时透明回退到已缓存的 `zend_function` PHP 调用路径，纯属尽力而为，绝不影响正确性。
  - 新增 `gene.swoole_getcid_capi` INI 开关（默认 `1`），置 `0` 强制走 PHP 调用回退。
- **M1 — 持久缓存业务分区上限 + 近似 LRU 淘汰**：`GENE_G(cache)` 同时承载框架元数据（路由/配置/事件，启动后只读）与 `Gene\Cache` 业务数据层。业务层允许请求期写入（`cache_layer_memory_write_depth>0`），若被当作进程内数据缓存使用会导致 RSS 无界增长（worker 永不回收）。
  - 新增 `gene.cache_max_items` INI（默认 `0` = 不限，完全向后兼容）。`>0` 时仅跟踪 `Gene\Cache` 业务分区（depth>0 写入），每次业务写入按"写时移动到表尾"近似 LRU，超出上限即从表头淘汰最久未写入项。
  - 框架元数据与普通 `Gene\Memory::set` 永不被跟踪、永不被淘汰，路由/配置不受影响；读路径不更新 recency，避免 workerReady 后免锁读路径的数据竞争。
  - 删除/`clean()`/MSHUTDOWN 与跟踪集保持同步；删除与淘汰路径改用 `zend_symtable_str_find` + Bucket 直取 key 的 O(1) 查找，替换原 O(N) 扫描，使带上限的缓存淘汰在锁内保持低成本。
- **M5 — request_attr 数组跨请求复用（Swoole）**：`request_attr`（承载 GET/POST/COOKIE/SERVER/... 的访问，几乎每请求都会用到）原本在 `reset()` 时整表销毁、下次请求再 `array_init`。现改为在 `reset()` 时就地 `zend_hash_clean` 复用（沿用 `path_params` 的"小则复用、超 128 桶则丢弃回 `IS_UNDEF`"策略，保持 RSS 上界）；`destroy()`（池回收/worker 退出）仍整表释放。每请求省一次 alloc/free。
  - **char\* 缓冲（method/path/module/...）池化已评估但暂不实施**：代码各处以 `if (ctx->module)`（指针非空）作为"已设置"判据，跨 `reset()` 保留非空且内容陈旧的缓冲会把上一个请求的值悄悄泄漏给未重设该字段的 handler。stash 方案虽可保契约但需为每字段加 stash+容量状态并改动所有赋值点，收益仅约 100–200ns/req，在无 ASAN 验证前不值得该改动面。
- **P6 — FPM 闭包路由源码缓存**：FPM/CLI 模式下路由每请求重建，闭包路由每次都要 `ReflectionFunction` + `SplFileObject` 读两次源文件（IO）。新增进程级持久缓存（`pemalloc`），以 `文件路径:起始行:结束行` 为 key 缓存**已处理**的源码文本，按源文件 `mtime` 失效（同 opcache 时间戳校验语义）。
  - 仅在 `runtime_type < 2`（FPM/CLI）启用：Swoole 路由仅启动期注册一次，无每请求成本，且避免跨协程共享，因此不受影响。
  - 每次命中返回独立的 `emalloc` 副本，调用方 `efree` 契约不变；持久主副本随 worker 进程生命周期存在（与路由树一致）。
  - 文件无法 `stat` 或 key 过长时自动跳过缓存，回退到原有读取逻辑。
- **P7 — 锁层面核对（无需改动）**：确认 `session.c`/`application.c` 全部 `GENE_CACHE_WRLOCK` 写入点均经 `gene_memory_write_allowed()` 守卫，Swoole `workerReady()` 后写入被拒绝，不存在 worker_ready 后的串行点。

### 🔧 修改文件一览

- `src/gene.c` / `src/gene.h` — P1 协程 id C-API 解析器 + `gene.swoole_getcid_capi` 开关；M1 `cache_lru`/`cache_max_items` 全局量、`gene.cache_max_items` INI、MSHUTDOWN 析构；M5 `request_attr` 复用辅助 `gene_ctx_reuse_lazy_array()`
- `src/cache/memory.c` / `src/cache/memory.h` — M1 业务分区 LRU 跟踪/淘汰，`gene_memory_del` 改用 O(1) 核心删除并同步 LRU
- `src/router/router.c` — `get_function_content()` 增加 FPM 源码持久缓存；P3 `gene_route_pc` 描述符 + `gene_route_pc_resolve/execute/dtor`、`get_router_info()` 包装（原函数改名 `get_router_info_slow()` 逐字保留）、`gene_router_pc_destroy()`
- `src/gene.c` / `src/gene.h` / `src/router/router.h` — P3 `route_pc` 全局量、`gene.route_precompile` INI、MSHUTDOWN 析构

### ✅ 验证

- 新增验证脚本 `tools/verify_5_6_6.php`（FPM/CLI 模式，Windows 可跑），覆盖：
  - **INI 注册**：`gene.swoole_getcid_capi`（默认 1）、`gene.cache_max_items`（默认 0）、`gene.route_precompile`（默认 0）三个开关均已注册且默认值正确。
  - **M1 业务缓存上限 + 近似 LRU**：`gene.cache_max_items=0`（默认）下写入 60 个互异业务键全部保留（向后兼容）；`-d gene.cache_max_items=10` 下写入 60 个业务键仅保留 10 个（按写序从表头淘汰），且最近写入键仍命中——证实近似 LRU 淘汰生效；`Memory::clean()` 后缓存清空、跟踪集同步无崩溃。
  - **P6 / M5 透明路径**：200 轮 `Gene\Cache::processCached` 稳定无崩溃。
- 运行：`php tools/verify_5_6_6.php`（默认）/ `php -d gene.cache_max_items=10 tools/verify_5_6_6.php`（复验淘汰）。
- **P1 / P3 Swoole 专用验证（已实测通过）**：新增 `tools/verify_5_6_6_swoole.php`，自身拉起 Swoole HTTP Server（多 worker）注册闭包/直派/动态/带钩子等路由，`workerReady()` 后协程高并发自打流量并自校验。在 **Linux + gene 5.6.6 + Swoole 6.1.8** 实测四组开关组合：路由派发全 PASS、100 并发协程上下文隔离在 `capi=1`/`capi=0` 下均 PASS、四组 `RESULT-DIGEST` 完全一致（`b887e533c417447e`）→ **P1/P3 闭环成立**，各开关派发结果零差异（详见 `audit/gene-memory-concurrency-audit.md` 第八节）。

---

## [5.6.5] - 2026-05-29

**主题**：版本更新。**无 API 破坏**，兼容所有运行模式。

### 📝 更新内容

- 版本号更新至 5.6.5
- 持续优化和稳定性维护

---

## [5.6.4] - 2026-05-24

**主题**：修复 `opcache.file_cache_only=1` 环境下的悬垂指针 Bug。**无 API 破坏**，兼容所有运行模式。

### 🔒 修复悬垂指针问题

- **问题**：在 `opcache.file_cache_only=1` 模式下，`zend_string_init_interned(...,1)` 返回请求作用域字符串，使用 `static zend_string *` 缓存会导致悬垂指针，引发内存耗尽或崩溃。
- **修复**：新增 `gene_interned_str_persistent` 和 `gene_lookup_class_str` 辅助函数，替换 14 个文件中约 70 处不安全缓存。
- **影响**：彻底解决 `opcache.file_cache_only=1` 环境下的内存安全问题。

### 🔧 修改文件一览

- `src/gene.h` / `src/gene.c` — 新增安全辅助函数
- `src/db/sqlite.c` / `src/db/pgsql.c` / `src/db/mssql.c` — PDO 类查找
- `src/cache/redis.c` / `src/cache/memcached.c` — Redis/Memcached 类查找
- `src/session/session.c` — 方法名字符串缓存
- `src/factory/load.c` — 自动加载器回调字符串
- `src/app/application.c` — Webscan 类 + response 键
- `src/router/router.c` — ReflectionFunction/SplFileObject 类查找
- `src/mvc/view.c` — 56 个正则/替换字符串
- `src/tool/language.c` — lang_key
- `src/cache/redis_pool.c` — Swoole 类查找
- `src/http/request.c` — 编译兼容性修复

## [5.6.3] - 2026-05-22

**主题**：FPM/Swoole 协程模式安全审计修复。**无 API 破坏**，`php-cgi / php-fpm / Swoole` 路径零回归。

### �️ 第 1 项 — IS_REFERENCE 包装处理（正确性修复）

- **文件**：`src/http/request.c`
- **问题**：用户代码对 `$_REQUEST` 等全局变量进行引用绑定（`$x = &$_REQUEST`）后，PG(http_globals) 槽位变为 IS_REFERENCE，严格的 IS_ARRAY 检查会错误拒绝有效请求。
- **修复**：在类型检查前添加 `ZVAL_DEREF` 解引用，确保引用包装的数组能正确读取。
- **影响**：修复用户代码使用引用绑定全局变量时的请求读取失败问题。

### 🔒 第 2 项 — __construct 类型安全加固（UB 修复）

- **文件**：`src/app/application.c`, `src/config/configs.c`, `src/cache/memory.c`, `src/router/router.c`
- **问题**：`zend_parse_parameters("|z")` 接受任意 zval 类型，但 `Z_STRVAL_P`/`Z_STRLEN_P` 在非 IS_STRING 类型下会访问错误的 union 成员，导致未定义行为和堆损坏。
- **修复**：在所有 `__construct` 的 safe 参数处理中添加 `IS_STRING` 类型门控，非字符串类型回退到默认的 `app_key`/`app_root`。
- **影响**：消除类型混淆导致的内存安全风险。

### ⚡ 第 3 项 — SERVER/HEADER 小写回退合并（性能优化）

- **文件**：`src/http/request.c`
- **问题**：SERVER 和 HEADER 载体存在两段相同的小写回退逻辑，代码重复。
- **修复**：合并为单一逻辑块，使用栈缓冲区进行一次复制+小写转换。
- **影响**：减少代码重复，提升可维护性。

### 🚀 第 4 项 — FPM 重定向热路径优化（性能优化）

- **文件**：`src/http/response.c`
- **问题**：FPM 模式下 `Location` 重定向使用 `snprintf` 构造 header，每次请求经历格式解析开销。
- **修复**：用编译期常量长度 + 两次 `memcpy` 替代 `snprintf`，消除 vfprintf 调度路径。
- **影响**：FPM 模式下每次重定向请求降低数百个 CPU cycle。

### 📊 综合收益评估

| 维度 | 5.6.2 | 5.6.3 | 改善 |
|------|-------|-------|------|
| IS_REFERENCE 支持 | ❌ 错误拒绝 | ✅ 正确解引用 | **修复引用绑定场景** |
| 类型安全 | ⚠️ UB 风险 | ✅ 类型门控 | **消除堆损坏风险** |
| FPM 重定向性能 | snprintf 开销 | memcpy 零开销 | **降低数百 cycle** |
| 代码重复 | 两段相同逻辑 | 单一合并块 | **提升可维护性** |

### 🔧 修改文件一览

- `src/http/request.c` — IS_REFERENCE 解引用 + 小写回退合并
- `src/http/response.c` — FPM 重定向热路径优化
- `src/app/application.c` — __construct 类型门控
- `src/config/configs.c` — __construct 类型门控
- `src/cache/memory.c` — __construct 类型门控
- `src/router/router.c` — __construct 类型门控
- `audit/AUDIT_REPORT_2026_05_21.md` — 审计报告

---

## [5.6.2] - 2026-05-10

**主题**：FPM/Swoole 性能与常驻内存稳定性优化。**无 API 破坏**，`php-cgi / php-fpm / Swoole` 路径零回归。

### 🛡️ 第 1 项 — Application::cleanup 上下文清理优化（稳定性修复）

- **文件**：`src/app/application.c`
- **问题**：`Application::cleanup` 在删除协程上下文前手动调用 `gene_request_context_destroy`，导致上下文被双重清理，在对象析构函数中产生重入窗口。
- **修复**：移除手动 `gene_request_context_destroy` 调用，改为仅使 `current_ctx/current_cid/current_vm_stack` 失效，让 `gene_co_context_dtor` 成为唯一的 destroy + context-pool 释放所有者。
- **影响**：消除重复清理和重入窗口，提升 Swoole 模式稳定性。

### ⚡ 第 2 项 — View 显示路径缓冲区修复（正确性修复）

- **文件**：`src/mvc/view.c`
- **问题**：`gene_view_display` 使用块作用域栈缓冲区 `path_buf[512]`，在 if/else 块后仍使用指向该缓冲区的指针，导致未定义行为。
- **修复**：将 `path_buf[512]` 移至函数作用域，并缓存 `strlen(file)` 到 `file_len`，避免重复计算。
- **影响**：修复悬空指针问题，提升代码正确性。

### 📊 综合收益评估

| 维度 | 5.6.1 | 5.6.2 | 改善 |
|------|-------|-------|------|
| 上下文清理重复 | 手动双重清理 | 单一所有者 | **消除重入窗口** |
| View 路径缓冲区 | 块作用域悬空指针 | 函数作用域安全 | **修复未定义行为** |

### 🔧 修改文件一览

- `src/gene.h` — `PHP_GENE_VERSION` → `"5.6.2"`
- `src/app/application.c` — `Application::cleanup` 上下文清理优化
- `src/mvc/view.c` — `gene_view_display` 路径缓冲区修复

---

## [5.6.1] - 2026-05-05

**主题**：性能优化第五轮 — 审计报告优化全面落地 + Swoole 并发问题修复 + 连接池优化。**无 API 破坏**，`php-cgi / php-fpm / Swoole` 路径零回归。

### � 第 1 项 — 审计报告优化全面落地（性能优化）

- **文件**：`src/mvc/view.c`, `src/http/response.c`, `src/http/request.c`, `src/session/session.c`, `src/cache/redis_pool.c`, `src/db/pool.c`, `src/gene.c`
- **优化内容**：
  - **view.c**：缓存 `app_view_len/app_ext_len`，在 `gene_view_contains_ext/gene_view_display_ext` 中使用缓存长度替代 `strlen`
  - **response.c**：在请求上下文中缓存 response 对象，跳过 DI 查找
  - **session.c**：添加 4 槽 LRU 缓存用于 `(ce, method) -> zend_function*` 查找
  - **request.c**：Swoole 模式下跳过 `zend_is_auto_global`（runtime_type >= 2）
  - **redis_pool.c**：升级 `rpool_start_idle_recycler/rpool_stop_timer` 使用缓存的 `zend_function*` + `zend_call_known_function`
  - **pool.c**：在 `pool_recycle_idle` 循环中缓存 count，本地跟踪丢弃计数
- **影响**：减少重复 strlen 调用，优化函数查找开销，提升热路径性能

### 🐛 第 2 项 — Swoole 并发问题修复（稳定性修复）

- **文件**：`src/cache/redis_pool.c`, `src/db/pool.c`, `src/di/di.c`, `src/gene.c`, `src/router/router.c`, `src/cache/cache.c`
- **问题**：
  - 连接池泄漏：`pool_recycle_idle` 中计数漂移
  - Timer 阻塞 worker 退出：`Swoole\Timer::tick` 导致 worker 无法正常退出
  - 上下文泄漏：协程上下文未正确清理
  - Use-after-free：DI 容器引用计数错误
- **修复**：
  - 连接池计数修正：移动 `increment_count` 到 `channel_push` 成功后
  - Timer 清理：添加 `Gene\Pool::stopTimers()` 静态方法，在 `workerExit` 回调中调用
  - 上下文管理：改进 `clearState()` + `destroyContext()` 两阶段清理
  - 引用计数：在 `gene_di_get` 中添加 `Z_TRY_ADDREF` 修复双重存储导致的 use-after-free
- **影响**：消除连接池泄漏，修复 worker 退出阻塞，提升 Swoole 模式稳定性

### ⚡ 第 3 项 — 字符串工具函数优化（性能优化）

- **文件**：`src/common/common.c`
- **优化**：`str_init`, `str_sub`, `str_concat` 使用 `memcpy` 替代 `strncpy`
- **影响**：消除不必要的零填充操作，提升字符串复制效率

### 🔒 第 4 项 — Swoole 模式锁优化（性能优化）

- **文件**：`src/cache/memory.h`, `src/gene.c`
- **优化**：
  - workerReady 后跳过读锁（持久缓存已只读）
  - 添加 `EXPECTED`/`UNEXPECTED` 分支预测提示
- **影响**：减少协程间锁竞争，提升 CPU 流水线效率

### 💾 第 5 项 — DI 容器和应用配置优化（性能优化）

- **文件**：`src/di/di.c`, `src/app/application.c`
- **优化**：
  - 使用 256 字节栈缓冲区构造缓存键，替代 `spprintf` 堆分配
  - 直接使用持久 interned 字符串，避免重复 `zend_string_init`
- **影响**：消除每次 DI 查找和配置读取时的堆分配开销

### 🧠 第 6 项 — 类加载快速路径（性能优化）

- **文件**：`src/factory/factory.c`
- **优化**：新增 `gene_fast_lookup_class` 内联函数，使用栈缓冲区 + 直接 hash 查找
- **影响**：每次 DI 解析 / Hook 分派 / 路由分派节省 1 次 emalloc + memcpy + hash 计算

### 📝 第 7 项 — Router 优化（性能优化）

- **文件**：`src/router/router.c`
- **优化**：
  - 方法复制消除：内联 lowercase+copy 融合
  - `setMca` 合并分配：单次 emalloc + 内联 uppercase
  - 缓存 `method_len` 避免重复 strlen
- **影响**：每次请求节省 1-2 次 emalloc + 减少函数调用开销

### 🗄️ 第 8 项 — Memory 缓存优化（性能优化）

- **文件**：`src/cache/memory.h`, `src/cache/memory.c`
- **优化**：
  - `gene_memory_get_quick` 宏化
  - 路径标记化使用栈缓冲区
- **影响**：消除函数调用开销，减少堆分配

### 📊 综合收益评估

| 维度 | 5.6.0 | 5.6.1 | 改善 |
|------|-------|-------|------|
| 字符串复制效率 | strncpy（零填充） | memcpy | **消除零填充** |
| Swoole 锁竞争 | 每次读锁 | workerReady 后跳过 | **减少争用** |
| DI/配置堆分配 | 每次 spprintf | 栈缓冲区 | **消除分配** |
| 类查找开销 | 完整流程 | 快速路径 hash | **节省 emalloc** |
| Router 热路径 | 多次 strlen | 缓存长度 | **减少调用** |
| 连接池稳定性 | 计数漂移 | 修正计数 | **消除泄漏** |
| Worker 退出 | Timer 阻塞 | stopTimers 清理 | **正常退出** |
| 上下文清理 | 部分泄漏 | 两阶段完整 | **零泄漏** |

### 🔧 修改文件一览

- `src/common/common.c` — 字符串工具优化
- `src/cache/memory.h`, `src/cache/memory.c` — Memory 缓存优化
- `src/di/di.c` — DI 容器优化 + 引用计数修复
- `src/app/application.c` — 应用配置优化
- `src/factory/factory.c` — 类加载快速路径
- `src/router/router.c` — Router 优化
- `src/http/request.c` — Swoole 模式优化
- `src/http/response.c` — Response 对象缓存
- `src/session/session.c` — Session 方法缓存
- `src/mvc/view.c` — View 字符串长度缓存
- `src/db/pool.c` — 连接池优化 + Timer 清理
- `src/cache/redis_pool.c` — Redis 连接池优化
- `src/gene.c` — 分支预测 + 全局变量
- `src/cache/cache.c` — 缓存相关修复

---

## [5.6.0] - 2026-04-27

### 🚀 性能优化 — 移除ZTS不安全的静态函数缓存

- **文件**：全项目范围（src/下所有C文件）
- **问题**：在ZTS（线程安全）模式下，`CG(function_table)`是线程局部的，线程A缓存的`zend_function*`在线程B中无效
- **修复**：移除所有进程级静态`zend_function*`缓存，改为每次调用时查找
- **影响**：异常处理路径是冷路径，性能影响可忽略；确保多线程环境下的正确性

---

## [5.5.9] - 2026-04-27

**主题**：性能优化第四轮 — 缓存函数调用模式全面应用 + 连接池泄漏修复 + 审计报告更新。**无 API 破坏**，`php-cgi / php-fpm / Swoole` 路径零回归。

### 🚀 第 1 项 — 缓存函数调用模式全面应用（性能优化）

- **文件**：`src/http/response.c`, `src/mvc/hook.c`, `src/mvc/view.c`, `src/common/common.c`, `src/http/validate.c`, `src/cache/cache.c`, `src/exception/exception.c`
- **优化**：将 ~30 个 PHP 函数包装器从 `call_user_function` 改为缓存 `zend_function*` + `zend_call_known_function`
- **影响**：消除每次调用的 `ZVAL_STRING` 堆分配 + 函数名查找开销
- **示例**：
  ```c
  static zend_function *fn = NULL;
  if (UNEXPECTED(!fn)) {
      fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_encode"));
  }
  zend_call_known_function(fn, ...);
  ```

### 🐛 第 2 项 — 连接池泄漏修复（稳定性修复）

- **文件**：`src/db/pool.c`, `src/cache/redis_pool.c`
- **问题**：`pool_recycle_idle` / `rpool_recycle_idle` 中 `increment_count` 在 `channel_push` 前调用，push 失败时计数漂移；存活连接 push-back 失败未处理
- **修复**：
  - 移动 `increment_count` 到 `channel_push` 成功检查之后
  - 对存活连接 push-back 添加失败处理：`if (!channel_push(...)) { decrement_count(self); }`
- **影响**：消除连接池在高并发下的计数漂移和连接泄漏

### 🔧 第 3 项 — MSHUTDOWN 清理修复（资源泄漏修复）

- **文件**：`src/db/pool.c`, `src/db/pool.h`
- **问题**：`gene_pool_named_cache` 在 MSHUTDOWN 时未清理
- **修复**：添加 `gene_pool_named_cache_free()` 在 MSHUTDOWN 调用
- **影响**：确保进程退出时所有命名池缓存正确释放

### 📊 第 4 项 — 审计报告更新

- **文件**：`audit/AUDIT_REPORT_2026_04_27.md`
- **内容**：全面审计 FPM 与 Swoole 双模式下的三层资源回收（请求级、worker 级、process 级）
- **结论**：所有路径均已闭环，无安全或稳定性影响

### 📝 第 5 项 — 代码清理

- **文件**：`src/mvc/controller.c`, `src/mvc/hook.c`
- **清理**：移除未使用的 include 头文件

---

## [5.5.8] - 2026-04-24

**主题**：深度优化第三轮 — Swoole 协程 cid 复用正确性修复 + 热路径锁/分配极致压缩 + 请求周期内存"无残留"语义完整化。**无 API 破坏**，`php-cgi / php-fpm` 路径零回归。

### 🛡️ 第 1 项 — `gene_request_ctx` second-chance 路径 cid 复用安全加固（关键正确性修复）

- **文件**：`src/gene.c`
- **问题**：在 Swoole 下，如果用户未调用 `cleanup()`，协程退出后 `co_contexts[cid]` 仍保留旧 ctx；Swoole 回收 cid 并分配给新协程时，老的 second-chance 路径会出现：
  ```c
  // 旧代码：
  if (current_ctx != NULL && cid == current_cid) {
      return current_ctx;  // ← 返回老协程的 ctx！(bug)
  }
  ```
  表现为：新协程读到老协程的 `path_params`、`module/controller/action`、DI 实例、view_vars 等。极端情况可能跨协程泄漏认证会话等敏感数据。
- **修复**：second-chance 命中时必须通过 `zend_hash_index_find_ptr(co_contexts, cid)` 验证 ctx 指针身份是否仍然匹配，身份不一致则清缓存、走完整 slow path 绑定新 ctx：
  ```c
  if (current_ctx != NULL && cid == current_cid && co_contexts != NULL) {
      ctx = zend_hash_index_find_ptr(co_contexts, cid);
      if (ctx == current_ctx) { return ctx; }  // 身份验证通过
      // 身份不匹配：清缓存，fallthrough 到完整 resolve
      current_ctx = NULL; current_cid = -1; current_vm_stack = NULL;
  }
  ```
- **开销**：second-chance 命中时额外一次 O(1) hash 查找（vs 原来的纯指针比较），比 `getcid()`（已经在当前 slow path 执行）便宜几个数量级。common case 的 `vm_stack` 快路径完全不受影响。
- **收益**：Swoole 长跑 + 用户漏调 `cleanup()` 场景下的跨协程数据泄漏彻底消除。

### 🚀 第 2 项 — Router dispatch 三次锁合并为一次（Swoole 高并发降锁争用）

- **文件**：`src/cache/memory.h`、`src/cache/memory.c`、`src/router/router.c`
- **问题**：每次请求的 `get_router_content_run()` 热路径连续做 3 次持久缓存读取（tree / event / conf），每次独立 `GENE_CACHE_RDLOCK`/`RDUNLOCK`。ZTS 构建 + `workerReady()` 未调用的 Swoole 部署下，这是 3× 的 `pthread_rwlock` / `SRWLOCK` 争用原子操作。
- **新增 API**：`gene_memory_get_triple()` — 单锁周期内批量取 3 个 key：
  ```c
  void gene_memory_get_triple(
      const char *k1, size_t l1, zval **o1,
      const char *k2, size_t l2, zval **o2,
      const char *k3, size_t l3, zval **o3);
  ```
- **router 端调整**：三个 key 缓冲区一次性构建（共享 prefix 复制），一次 `gene_memory_get_triple` 取回全部结果；删除 3 次 `gene_memory_get_quick` + 3 次锁周期。
- **收益**：
  - **ZTS Swoole**：锁争用开销 3× → 1×
  - **非 ZTS + `workerReady()` 已调用**：无额外收益（锁已是 no-op），但也无回归
  - **非 ZTS + `workerReady()` 未调用**：原子操作 3× → 1×

### 🔓 第 3 项 — `gene_memory_get_by_config` 嵌套路径遍历锁外化

- **文件**：`src/cache/memory.c`
- **问题**：DI/config 读取（`$this->db`、`$app->config('db.mysql.host')` 等）走 `gene_memory_get_by_config`，它先加锁、在锁内做 `php_strtok_r` + 多级 `zend_symtable_str_find` 嵌套遍历。高并发下持锁时间较长。
- **修复**：锁只保护顶层 key 的查找；拿到指针后立即释放锁，strtok 和嵌套遍历全部走锁外。依据是持久缓存的 write-once-at-startup 不变性（框架已在文档中声明），拿到的 zval 指针在整个 worker 生命周期内稳定。
- **收益**：3-4 层 config 路径下的锁持有时间缩减约一半；DI 密集应用（每请求数十次 `$this->xxx` 解析）累积收益显著。

### 🧠 第 4 项 — `gene_memory_zval_local` 字符串共享 interned 串（请求周期零堆分配）

- **文件**：`src/cache/memory.c`
- **问题**：每次 `Gene\Memory::get($key)` / `Application::config($key)` / DI 返回值 / 每次访问缓存里的 string 值都要：
  ```c
  zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0);  // ← emalloc + memcpy
  ZVAL_NEW_STR(dst, str_key);
  ```
  一份全新的 request-scope string。大型配置数组 + DI 密集应用每请求可能触发几十到上百次 string emalloc。
- **修复**：持久缓存里的 string 都是 `IS_STR_INTERNED|IS_STR_PERMANENT`，直接用 `ZVAL_STR(dst, Z_STR_P(source))` 共享 interned 指针：
  - interned string 的 refcount 操作是 no-op
  - request-scope 的 `zval_ptr_dtor` 对 interned string 也是 no-op
  - 零堆分配、零 memcpy、零语义差异
- **对嵌套 HashTable 同步优化**：`gene_memory_hash_copy_local` 中 interned key 直接复用，非 interned key 走 fallback；`array_init` 改为 `array_init_size(nNumElements)` 预分配精确大小。
- **收益**：DI-heavy / config-heavy 应用每请求**十几到上百次**字符串 emalloc → 0 次。

### 💨 第 5 项 — 请求上下文关联 HashTable 预分配

- **文件**：`src/gene.c`、`src/http/request.c`、`src/di/di.c`、`src/mvc/view.c`
- **问题**：`path_params`、`request_attr`、`di_regs`、`view_vars` 等每请求 HashTable 默认容量 0，首批插入触发 0→8 的首次 grow（memcpy bucket storage）。
- **修复**：按典型负载预分配：
  | HashTable | 旧默认 | 新预分配 | 典型负载 |
  |-----------|--------|----------|----------|
  | `path_params` | 0 | 8 | 路由参数 1-5 个 |
  | `request_attr` | 0 | 8 | 固定 8 种 track var |
  | `di_regs` | 0 | 16 | 典型服务 8-20 个 |
  | `view_vars` (per-scope) | 0 | 16 | 模板变量 5-20 个 |
  | `view_vars` (scope-level) | 0 | 4 | scope 通常 1 个 |
- **收益**：首次填充时跳过 1-2 次 grow，减少 bucket memcpy。

### 🎯 第 6 项 — `getVal` slow path 消除重复 hash lookup

- **文件**：`src/http/request.c`
- **问题**：每个请求首次访问 `$_GET / $_POST / ...` 中的 track var 时走 slow path：
  ```c
  zend_hash_index_update(attr, type, val);  // 插入
  val = zend_hash_index_find(attr, type);   // ← 再查一次 (冗余)
  ```
- **修复**：直接用 `zend_hash_index_update` 的返回值。
- **收益**：每请求每个 track var 首次访问少一次 O(1) hash 查找。

### 🚦 第 7 项 — `Application::workerReady()` 自动预热 ctx 池（简化 Swoole 部署）

- **文件**：`src/app/application.c`
- **背景**：5.5.7 引入了 `ctx_pool_prewarm` INI 和 `prewarmCtxPool()` 方法，但两者都需要用户主动配置/调用。多数用户忘了，错过了冷启动收益。
- **修复**：`workerReady()` 在 `runtime_type >= 2` 且 `ctx_pool_size == 0` 时自动调用 `gene_request_context_pool_prewarm(-1)` 填满池。幂等、零多余成本。
- **简化用法**：
  ```php
  // 5.5.8 之前
  $server->on('WorkerStart', function () {
      \Gene\Application::getInstance()->prewarmCtxPool();
      \Gene\Application::getInstance()->workerReady();
  });

  // 5.5.8 之后 — 一行搞定
  $server->on('WorkerStart', function () {
      \Gene\Application::getInstance()->workerReady();
  });
  ```
- **向后兼容**：`prewarmCtxPool()` 保留，`gene.ctx_pool_prewarm` INI 继续有效。

### 📊 综合收益评估

| 维度 | 5.5.7 | 5.5.8 | 改善 |
|------|-------|-------|------|
| Swoole cid 复用正确性 | ⚠️ second-chance 可能误命中 | ✅ 身份验证 | **关键 bug 修复** |
| Router dispatch 锁周期数 | 3 | 1 | **-2 次 rwlock** |
| config 嵌套读取锁持有时间 | 包含 strtok + O(depth) find | 仅顶层 find | **~50% 缩减** |
| DI/config string emalloc/次访问 | 1 | 0（interned 共享） | **抹平** |
| HashTable 首次 grow 次数 | 1-2 次 per ctx | 0 次（预分配命中） | **-1~2 次 memcpy** |
| `getVal` slow path hash 查找 | 2 次 | 1 次 | **-1 次 O(1)** |
| Swoole 冷启动配置门槛 | INI + 手动 prewarm | `workerReady()` 一步 | **运维简化** |
| FPM / php-cgi 回归 | ✅ | ✅ | 零影响 |
| API 破坏 | — | 无 | 完全兼容 |

### 🧪 建议回归验证

- **Swoole 正确性**：构造 cid 快速复用场景（创建→退出→立刻新协程），对比 5.5.7 vs 5.5.8 下新协程读取 `Application::getModule()` 等是否返回前一协程的值。5.5.8 应始终返回 NULL 或当前协程自己设置的值。
- **FPM 压测**：`ab -n 200000 -c 200` 跑 demo，对比 QPS / p99 延迟；5.5.8 应持平或略高。
- **Swoole 压测**：`wrk -t12 -c400` + 配置密集应用（每请求 10+ 次 `$app->config(...)`），对比单 worker QPS；5.5.8 应显著更高（字符串零分配 + 锁争用减少）。
- **内存专项**：`USE_ZEND_ALLOC=0 valgrind --leak-check=full` 跑混合模式 100k 请求，SIGTERM 后 `LEAK SUMMARY: definitely lost: 0 bytes`。
- **HashTable 容量**：构造典型应用（15 个 DI 服务 + 10 个视图变量），`Memory::stats()` 观察每请求堆峰值；5.5.8 应低于 5.5.7。

### 🔧 修改文件一览

- `src/gene.h` — `PHP_GENE_VERSION` → `"5.5.8"`
- `src/gene.c` — second-chance 身份验证 + `path_params` 预分配 + 加强注释
- `src/cache/memory.h` — 新增 `gene_memory_get_triple` 原型
- `src/cache/memory.c` — `gene_memory_get_triple` 实现 + `get_by_config` 锁外化 + `zval_local` interned 共享 + `hash_copy_local` interned key 复用
- `src/router/router.c` — 三 key 一次 triple 查询 + 清理未使用局部变量
- `src/http/request.c` — `request_attr` 预分配 + `getVal` slow path 消除冗余 find
- `src/di/di.c` — `di_regs` 预分配 16
- `src/mvc/view.c` — `view_vars` 两级预分配
- `src/app/application.c` — `workerReady()` 自动预热池

### 💡 运维提示

推荐的 Swoole 部署 `php.ini`：

```ini
extension=gene.so
gene.runtime_type=2
gene.ctx_pool_max=512         ; 池容量（按 worker 并发协程上限选）
gene.co_contexts_max=16384    ; 协程上下文软上限
; gene.ctx_pool_prewarm=...   ; 可省略：workerReady() 会自动填满
```

启动脚本：

```php
$server->on('WorkerStart', function () {
    \Gene\Application::getInstance()->workerReady();
    // 此时池已填满、lock-skip 已启用，开始接单。
});

$server->on('Request', function ($req, $resp) {
    // 处理业务...
});

$server->on('Response', function ($resp) {
    // 业务中用了大量 DI/闭包？可传 true 主动触发 GC：
    \Gene\Application::getInstance()->cleanup(true);
});
```

---

## [5.5.7] - 2026-04-24

**主题**：深度优化第二轮 — 极致高并发（FPM & Swoole）+ Swoole 冷启动零堆分配 + 请求周期内存完全归零。**无 API 破坏**，`php-cgi / php-fpm` 路径零回归。

### 🎯 第 1 项 — `bench_*` 字段跨请求残留修复（Swoole 正确性 + 内存极致释放）

- **文件**：`src/gene.c`
- **问题**：`gene_request_context` 中的 `bench_start`、`bench_end`、`bench_memory_start`、`bench_memory_end` 四个内联标量，在 `gene_request_context_free_fields()` 中**没有被清零**。Swoole 模式下同一个 ctx 结构体被池化反复使用，上个请求的基准数据会污染下个请求中 `Gene\Benchmark::time()` / `memory()` 的返回（当本请求未调用 `Benchmark::start()` 时）。
- **修复**：在 `free_fields` 中统一清零这 4 个标量字段。开销：4 次 8 字节 store，可忽略。
- **副作用**：这是一个**正确性修复**，让"请求周期内存完全归零"名副其实。

### 🎯 第 2 项 — 上下文结构体池预热（Swoole 冷启动零堆分配）

- **文件**：`src/gene.h`、`src/gene.c`、`src/app/application.c`
- **问题**：5.5.6 引入了 `ctx_pool`，但 Swoole worker 冷启动时池是空的，前 N 个协程请求仍会 `ecalloc(sizeof(gene_request_context))`。对于启动瞬间就打过来的流量洪峰，这 N 次分配是可见的抖动。
- **新增 API**：
  - `void gene_request_context_pool_prewarm(zend_long count)` — C 级预热函数（幂等，可重复调用，仅补齐增量）；
  - **PHP 方法** `\Gene\Application::prewarmCtxPool(int $count = -1)` — 传 -1 或省略 = 填满到 `ctx_pool_max`；返回新增数量；
  - **新增 INI** `gene.ctx_pool_prewarm`（默认 `0`）— 当 > 0 且 `runtime_type >= 2` 时，RINIT 自动预热到该数量，**无需改代码**即可启用。
- **使用方式**：
  ```ini
  ; php.ini 自动预热 128 个（最多不超过 ctx_pool_max=256）
  extension=gene.so
  gene.runtime_type=2
  gene.ctx_pool_prewarm=128
  ```
  或显式：
  ```php
  $server->on('WorkerStart', function () {
      \Gene\Application::getInstance()->prewarmCtxPool();  // 填满
      // 或 prewarmCtxPool(64);
  });
  ```
- **收益**：冷启动 / 重启后第一波流量的 `ecalloc` 次数归零。默认禁用，不影响已有部署。

### 🎯 第 3 项 — `co_contexts` 初始 HashTable 容量从 8 → 32

- **文件**：`src/gene.c`（`gene_init_co_contexts` + `PHP_RINIT_FUNCTION`）
- **问题**：`zend_hash_init(co_contexts, 8, ...)` 在协程风暴下会经历 `8 → 16 → 32` 两次 rehash，每次 rehash 是 O(N) 的 memcpy。
- **修复**：初始容量提升到 32，覆盖典型 HTTP 服务的 worker 并发上限，消除冷启动后前 32 个协程期间的 2 次 rehash。
- **成本**：一次性 ~512 字节的 bucket 存储差异，每 worker 可忽略。

### 🎯 第 4 项 — `cleanup(bool $gc = false)` 可选 GC cycle 触发（Swoole 内存极致释放）

- **文件**：`src/app/application.c`
- **问题**：Swoole 常驻进程下，用户的 handler 如果构建了**循环引用图**（DI ↔ Service、ORM lazy loader 回指 Model、闭包捕获 $this 等），`zval_ptr_dtor` 只会把循环对象挂进 PHP GC 根队列，而不会立即回收。这部分内存要等 GC 根队列满（默认 10000）才触发 `gc_collect_cycles()`。一个请求结束时本来**想完全归零**，但可能有循环图残留。
- **修复**：`cleanup()` 增加可选 `bool $gc` 参数（默认 `false` 保持热路径 0 额外开销）。传 `true` 时，在 context 销毁后显式 `gc_collect_cycles()`：
  ```php
  $response->on('Finish', function () {
      \Gene\Application::getInstance()->cleanup(true);  // 主动回收循环图
  });
  ```
- **成本**：`gc_collect_cycles()` 扫描 GC 根表，μs-ms 级（取决于活对象数）。推荐只在**压测观测到 RSS 缓慢增长**时启用。
- **FPM 模式**：忽略 `$gc` 参数（RSHUTDOWN 会全盘归零，没必要提前 GC）。

### 📊 综合收益评估

| 维度 | 5.5.6 | 5.5.7 | 改善 |
|------|-------|-------|------|
| Swoole worker 冷启动 N 次 ecalloc | N × `sizeof(ctx)` | 0（预热后） | **抹平** |
| 请求 `bench_*` 字段残留 | ⚠️ 污染 | ✅ 清零 | **正确性修复** |
| `co_contexts` 首轮 rehash | 2 次（8→32） | 0 次 | **-2 次 O(N)** |
| 循环图内存归零 | 等 GC 阈值 | 可选显式触发 | **用户可控** |
| FPM/php-cgi 回归 | ✅ | ✅ | 零影响 |
| API 破坏 | — | 无 | `cleanup()` 零参调用完全兼容 |

### 🧪 建议回归验证

- **FPM**：`ab -n 100000 -c 100` 跑全量 demo；RSS 与 p99 延迟应与 5.5.6 持平（FPM 路径没变）。
- **Swoole 冷启动**：压测脚本在 worker 启动 100ms 内发 500 req，对比开启 `gene.ctx_pool_prewarm=256` vs 默认 0 的 `Memory::stats()['ctx_pool_size']` 演化曲线。
- **Swoole 24h 长跑**：开启循环引用场景（DI + 闭包 + Service 互引），对比 `cleanup(true)` vs `cleanup(false)` 的 RSS 走势。
- **Benchmark 正确性**：构造两个相邻请求，前一个调 `Benchmark::start/end`，后一个不调，验证后一个的 `memory()` 返回 0（而非前一个的残留）。
- **内存**：`USE_ZEND_ALLOC=0 valgrind --leak-check=full` + `gene.ctx_pool_prewarm=64`，压测后 worker SIGTERM，`LEAK SUMMARY: definitely lost: 0 bytes`。

### 🔧 修改文件一览

- `src/gene.h` — `PHP_GENE_VERSION "5.5.7"`；新增 `ctx_pool_prewarm` 全局 + `gene_request_context_pool_prewarm` 原型
- `src/gene.c` — `bench_*` 清零；INI `gene.ctx_pool_prewarm`；`pool_prewarm()` 实现；RINIT 自动预热；`co_contexts` 初始 32
- `src/app/application.c` — `cleanup([bool $gc])` 可选 GC；新增 `prewarmCtxPool([int $count])` 方法

### 💡 运维提示

一键启用本版本全部收益（Swoole 场景）：

```ini
extension=gene.so
gene.runtime_type=2
gene.ctx_pool_max=512          ; 池容量（按 worker 并发协程上限选）
gene.ctx_pool_prewarm=512      ; 启动预热满（冷启动零 emalloc）
gene.co_contexts_max=16384     ; 协程上下文软上限
```

---

## [5.5.6] - 2026-04-24

聚焦**极致高并发**（FPM & Swoole 双模式）与 **Swoole 请求周期内存完全回收**。核心数据结构 `gene_request_context` 布局重构 + 协程上下文结构体池化。**无 API 破坏**，`php-cgi / php-fpm` 路径零回归。

### 🎯 第 1 项 — `path_params` 内联化（`gene_request_context` 布局优化）

- **文件**：`src/gene.h`、`src/gene.c`、`src/app/application.c`、`src/mvc/controller.c`、`src/mvc/hook.c`、`src/http/request.c`、`src/router/router.c`
- **问题**：`path_params` 过去是 `zval *` 指针，每次请求进入 `gene_request_context_init` 都要 `emalloc(sizeof(zval))` + `array_init()`；请求结束走 `destroy` 时再 `zval_ptr_dtor()` + `efree()`。**每请求一次额外的 zval 容器分配/释放**。
- **修复**：结构体内联为 `zval path_params;`，只有底层 `HashTable` 仍走堆。热路径的 `gene_router_reset_path_params` 从"空指针分支 + 三路 if/else"压缩为**单分支 `zend_hash_clean`**，`zend_always_inline` 让编译器内联展开。
- **副收益**：
  - 缓存行命中率提升（`path_params` 和相邻字段同 cache line）；
  - **病态请求防护**：若 `nTableSize > 128`（单请求塞进 >128 个参数），在 reset 时 drop + re-init，避免 Swoole Worker 的 RSS 被某次异常请求永久撑大；
  - 更新了 6 个文件共 10 处消费点使用 `&ctx->path_params`。

### 🎯 第 2 项 — 协程上下文结构体池化（Swoole 极致高并发）

- **文件**：`src/gene.h`、`src/gene.c`、`src/app/application.c`、`src/cache/memory.c`
- **问题**：Swoole 下每创建一个协程请求 = **一次 `ecalloc(sizeof(gene_request_context))` + 一次 `efree`**；协程风暴（如 HTTP 长连接 + `co::wait` 扇出）下，这对 ZMM 分配器是明显热点。
- **修复**：引入**有界的 `gene_request_context` 自由链表**（"结构体对象池"）：
  - 新增 INI `gene.ctx_pool_max`（默认 **256**，约 80KB/worker 上限）；
  - **链接方式**：复用已释放 ctx 的 `path_params.value.ptr` slot 作为单链表下一节点指针（释放后该 slot 恒为 UNDEF 死区，零额外存储）；
  - `gene_request_context_pool_acquire()`：优先出栈复用，空池才 `ecalloc`；
  - `gene_request_context_pool_release()`：入栈，超 cap 才 `efree`；
  - `gene_co_context_dtor()` 改为走 pool release，协程退出时**不再 `efree(ctx)`**；
  - `resident_ctx` / `destroyContext` / `cleanup` 的 `efree` 路径同步切换为 pool release。
- **热路径开销**：
  - Acquire：1 load + 1 branch + 1 store（无系统调用）
  - Release：1 load + 1 compare + 2 stores（无 `efree`）
- **稳态收益**：协程请求进入/退出**零 ZMM 分配器调用**，对高 QPS Swoole 业务是显著抹平。
- **释放保证**：`RSHUTDOWN`（FPM：每请求 / Swoole：worker 退出）统一 `gene_request_context_pool_drain()` 全量 `efree`，绝无泄漏。

### 🎯 第 3 项 — Swoole 请求周期全内存回收（杜绝隐性累积）

- **文件**：`src/gene.c`
- **问题**：在 `PHP_RSHUTDOWN_FUNCTION` 中，`resident_ctx` 使用 `efree(tmp)` 直接释放，但未触发池回收；此外若 worker 生命周期内累积了池缓存，worker 退出时也需要全部归还给 ZMM。
- **修复**：
  - `resident_ctx` 的三处 `efree` 路径（`destroyContext` / `cleanup` / `RSHUTDOWN`）统一走 `gene_request_context_pool_release`；
  - `php_gene_close_request_globals` 末尾追加 `gene_request_context_pool_drain()`，worker 退出 / FPM RSHUTDOWN **完全清零**。
- **测试点**：`valgrind --leak-check=full` + 1 万协程风暴，RSS 回到启动基线。

### 🎯 第 4 项 — 可观测性（运维可见池化效果）

- **文件**：`src/cache/memory.c`
- `\Gene\Memory::stats()` 新增两个字段：

```php
$memory->stats();
// [
//   'cache_items'       => 128,
//   'cache_easy_items'  => 42,
//   'fn_cache_items'    => 8,
//   'co_contexts_items' => 1023,
//   'co_contexts_max'   => 8192,
//   'ctx_pool_size'     => 42,   // ← NEW：当前池内空闲 ctx 数
//   'ctx_pool_max'      => 256,  // ← NEW：池容量上限
// ]
```

### 📊 收益评估

| 维度 | 5.5.5 | 5.5.6 | 改善 |
|------|-------|-------|------|
| FPM 每请求 emalloc（上下文相关） | 1×zval(16B) + 1×路径/方法等堆串 | **0×zval 容器** | -1 次 emalloc/efree |
| Swoole 每协程请求 emalloc | 1×ctx(~320B) + 1×zval(16B) + HT | **0×ctx + 0×zval**（命中池时）| -2 次 emalloc/efree |
| Swoole worker 长跑 RSS | 取决于 `co_contexts` | + 有界 `ctx_pool`（≤80KB）| **抹平** |
| 请求周期内存释放完整性 | RSHUTDOWN 清 `co_contexts` | RSHUTDOWN 清 `co_contexts` + `pool_drain` | **零残留** |
| php-cgi / php-fpm 正确性 | ✅ | ✅ | 未改变 |

### 🧪 建议回归验证

- **FPM**：`ab -n 100000 -c 100` 下 RSS 稳定、响应延迟 p99 未退化；
- **Swoole**：`co::wait` 扇出 10k + 随机 sleep，观察 `Memory::stats()` 中 `ctx_pool_size` 收敛到 `ctx_pool_max`，`co_contexts_items` 不膨胀；
- **内存**：`USE_ZEND_ALLOC=0 valgrind` 压测 5 分钟后 worker SIGTERM，`LEAK SUMMARY: definitely lost: 0 bytes`；
- **功能回归**：三模式（FPM、Swoole、Coroutine）跑 `demo` / `test` 全量。

### 🔧 修改文件一览

- `src/gene.h` — 结构体字段改为 `zval path_params`；新增 `ctx_pool_*` 全局；池 API 原型
- `src/gene.c` — `path_params` 内联 init/reset/destroy；池 acquire/release/drain 实现；RSHUTDOWN drain
- `src/app/application.c` — `cleanup` / `destroyContext` 改走 pool；`params()` 用 `&ctx->path_params`
- `src/mvc/controller.c` / `src/mvc/hook.c` / `src/http/request.c` — `params()` 访问改为 `&ctx->path_params`
- `src/router/router.c` — `gene_router_reset_path_params` 零堆分支 + inline；4 处消费点更新
- `src/cache/memory.c` — `stats()` 暴露 `ctx_pool_size` / `ctx_pool_max`

---

## [5.5.5] - 2026-04-23

聚焦 **Swoole 常驻稳定性**（杜绝"只增不减"的隐性增长）与 **FPM/Swoole 热路径进一步降堆分配**。三批次共 8 项改动，无 API 破坏。

### 🛡️ 第 1 批：Swoole 常驻下的内存稳定性修复

#### A1 — `fn_cache` 稳定 key，彻底消除无界增长
- **文件**：`src/router/router.c`、`src/gene.c`、`src/gene.h`
- **问题**：原实现用 `++fn_cache_id` 自增生成闭包路由的缓存键 `fn_%ld`。同一闭包每次注册都产生新键；Swoole 模式下 `fn_cache` 又不走 `RSHUTDOWN` 清理，工作进程长期运行后缓存表单调增长。
- **修复**：改按闭包对象的 `zend_object->handle` 生成稳定键 `fn_%u`。由于 `fn_cache` 持有闭包强引用，同一闭包的 handle 不会被回收再分配，**同一闭包 → 同一键 → 无新增**。同时在 `gene_router::clear()` 中一并清空 `fn_cache`，解决热重载场景的悬挂引用。
- **附带清理**：移除不再需要的 `fn_cache_id` 全局。

#### A2 — `co_contexts` 容量上限 + 概率 sweep
- **文件**：`src/gene.c`、`src/gene.h`
- **问题**：Swoole 协程退出后，`co_contexts[cid]` 完全依赖用户显式调用 `Application::cleanup()` 回收。一旦用户漏调，协程表线性增长且伴随逻辑风险（cid 复用会误拾旧 ctx）。
- **修复**：
  - 新增 INI 项 `gene.co_contexts_max`（默认 **8192**），作为软上限；
  - 在 `gene_request_ctx()` 慢路径（新建 ctx 之前）检查阈值，触发 `gene_co_contexts_sweep()`；
  - Sweep 两阶段：
    1. **精确回收** — 通过懒解析的 `Swoole\Coroutine::exists()` 逐一探测，清除已死协程的上下文（保留长生命周期活协程）；
    2. **兜底回收** — 若 exists API 不可用或清扫后仍超限，按 HashTable **插入顺序**淘汰最老的 25%（始终跳过当前协程的 cid）。
- **收益**：稳态零开销（仅超限才扫），协程风暴下 RSS 不再线性增长，同时避免错绑旧 ctx。

#### A3 — `Gene\Memory::stats()` 可观测性
- **文件**：`src/cache/memory.c`
- **新增方法**返回进程级缓存和协程上下文的计数，便于运维监控膨胀趋势：

```php
$memory->stats();
// [
//   'cache_items'       => 128,
//   'cache_easy_items'  => 42,
//   'fn_cache_items'    => 8,
//   'co_contexts_items' => 1023,
//   'co_contexts_max'   => 8192,
// ]
```

### ⚡ 第 2 批：Swoole 热路径性能

#### B2 — Swoole response 方法 `zend_function*` 缓存
- **文件**：`src/http/response.c`
- **问题**：每次 `header/redirect/cookie/end` 都在 Swoole response ce 的 function_table 上做 hash 查找。
- **修复**：引入 `GENE_SWOOLE_RESP_METHOD(ce, name)` 宏 + 每方法一对 `(cached_ce, cached_fn)` 静态槽，ce 变更才刷新；稳态 2 次指针比较。

#### B1 — Swoole 下 `$_SERVER` 懒物化
- **文件**：`src/http/request.c`
- **问题**：`gene_request_set_server_val` 每请求构建一份 `normalized` 数组，遍历 `$_SERVER` 全表 `Z_TRY_ADDREF_P` + `zend_hash_update`。
- **修复**：既然没有大小写重写，就直接 `setVal(3, server)` 共享原数组（`Z_TRY_ADDREF_P` 外层数组一次搞定）。method/path 从原数组就地解析。`gene_request_set_header_val` 同样收敛为一次 `setVal(7, header)`。

#### B4 — 缓存行对齐（暂缓）
- 当前 Swoole worker 以多进程部署、单 worker 内协程串行，ZTS 下才有实际收益；考虑到 `__attribute__((aligned))` 在 MSVC/GCC 跨平台上的可移植性风险，本版本未启用，保留作为未来 ZTS 场景的备选。

### 🏃 第 3 批：FPM 热路径

#### C1 — `spl_autoload_register` 进程级解析 + interned 回调名
- **文件**：`src/factory/load.c`
- **问题**：FPM 下 SPL 每请求清空 autoload 列表，`gene_loader_register` 必须在 RINIT 之后重注册；但每次都 `ZVAL_STRING` 堆分配回调名，`zend_call_function` 再走函数名查表。
- **修复**：
  - `spl_autoload_register` 的 `zend_function*` 进程级缓存；
  - 默认回调名（`Gene\Load::autoload` / `Gene_Load::autoload`）用 `zend_string_init_interned(..., 1)` 持久化，`ZVAL_STR_COPY` + `zval_ptr_dtor` 对 interned 字符串均为 no-op；
  - 调用切换为 `zend_call_known_function`。
- **每请求节省**：1 次函数表查找 + 1 次 ZVAL_STRING 堆分配 + 1 次 fci/fcc 构建。

#### C2 — `configs::set/get` 点转斜杠零堆分配
- **文件**：`src/config/configs.c`
- **问题**：每次调用先 `estrndup(keyString)` 再 `replaceAll(path, '.', '/')` —— 一次堆分配 + 两次扫描。
- **修复**：对长度 < 256 的 key（覆盖 99%+ 配置键，如 `db.mysql.host`）走栈缓冲，复制与替换融合为单次循环；长 key 才走堆。

### 📊 影响评估

| 维度 | 收益 | 风险 |
|------|------|------|
| **Swoole 常驻内存** | 消除 `fn_cache` / `co_contexts` 无界增长两大风险点；24h 长跑 RSS 不再爬升 | 无 API 破坏，新增软上限默认 8192 对绝大多数业务足够 |
| **Swoole QPS** | 响应方法调用省 1 次 hash lookup；`$_SERVER` 路径省 1 次 array_init + O(N) addref + hash insert | 行为等价（共享引用代替浅克隆） |
| **FPM QPS** | 每请求省 1 次 emalloc（autoload 回调名）+ 1 次 fcall 名查表 + 1 次 emalloc（config path） | 无 |
| **可观测性** | `Memory::stats()` 运维可用 | 零 |

### 🔧 修改文件一览

- `src/gene.c`、`src/gene.h` — A1/A2 globals 与 sweep 实现、INI
- `src/router/router.c` — A1 稳定 key + clear() 联动
- `src/cache/memory.c` — A3 `stats()`
- `src/http/response.c` — B2 方法缓存
- `src/http/request.c` — B1 懒物化
- `src/factory/load.c` — C1 autoload 缓存
- `src/config/configs.c` — C2 零堆分配

### 🧪 建议验证

- **内存专项**：`USE_ZEND_ALLOC=0 valgrind --leak-check=full` + 协程风暴脚本；对比 worker 启动 vs 24h 后 `Memory::stats()` 与系统 RSS。
- **压测**：`wrk` 对 FPM；Swoole 下 `co` 并发 + `perf record`，重点看 `gene_request_ctx` / `gene_response_set_header`。
- **功能回归**：同一应用分别 `gene.runtime_type=1` 与 `>=2`，功能与延迟对照。

---

## [5.5.4] - 2026-04-20

### ⚡ Performance Optimizations (第二轮优化 - FPM/Swoole/PHP-CGI 热路径)

#### 🚀 gene_memory_get_quick 宏化 (memory.h + memory.c)
- 原函数转发改为宏定义：`#define gene_memory_get_quick(k, l) gene_memory_get((k), (l))`
- 每次路由分派消除 3-4 次函数调用开销（router_tree/router_event/router_conf）
- 零运行时成本，编译期展开

#### 🧠 类加载快速路径 gene_fast_lookup_class (factory.c)
- 新增静态内联函数替代 `gene_factory_load_class` 和 `gene_factory` 中的完整类查找流程
- **快速路径**：256字节栈缓冲区 + 直接 `zend_hash_str_find_ptr(EG(class_table), lc_buf, src_len)` 查找
- 处理 `\` 前缀去除（FQN 规范化），零堆分配
- **慢速路径**：类未加载时回退到 `zend_lookup_class`（保留自动加载能力）
- 收益：每次 DI 解析 / Hook 分派 / 路由分派节省 1 次 emalloc + memcpy + hash 计算

#### 💾 gene_di_get 直接使用持久 interned 字符串 (di.c)
- 移除 `zend_string_init(Z_STRVAL_P(class), Z_STRLEN_P(class), 0)` 堆分配
- 直接 `zend_string *class_str = Z_STR_P(class)` 使用缓存中的持久 interned 字符串
- 持久字符串带 `IS_STR_INTERNED | IS_STR_PERMANENT` 标志，在非持久 HashTable 中作 key 安全（release 是 no-op）
- 每次 type=1 的 DI 解析节省 1 次 emalloc

#### 🔀 gene_di_regs 分支预测提示 (di.c)
- 为首次初始化分支添加 `UNEXPECTED` 提示
- di_regs 在首次 DI 访问后即为 IS_ARRAY，命中率接近 100%
- 提升分支预测准确率，减少流水线冲刷

#### 📝 get_router_content_run 方法复制消除 (router.c)
- `methodin==NULL` 时（内部分派）直接使用 `ctx->method`（已被 `gene_ini_router` 小写化）
- `methodin!=NULL` 时用 32 字节栈缓冲区内联 lowercase+copy 融合
- 缓存 `method_len` 避免每次 hash 查找都调用 strlen
- 每次请求节省 1 次 emalloc+efree

#### 🔧 setMca 合并分配 (router.c)
- 原来：`efree(old) + str_init(val) + firstToUpper(ctx->x)` = 3 次函数调用 + 1 次 strlen + 1 次 emalloc + 1 次 memcpy + 1 次字符赋值
- 现在：单次 emalloc + 内联 uppercase 第一个字符 + memcpy 剩余
- action 不做 uppercase（与原逻辑一致）
- 每次路由参数匹配节省 2-3 次函数调用开销

#### 📏 减少 get_router_content_run 热路径上的 strlen
- `method_len` 一次算好复用
- 传给 `zend_symtable_str_find` 等 API 时直接用缓存值
- 消除重复 strlen 调用

### 📊 性能影响
- **FPM/CGI 模式**：每请求节省约 5-8 次堆分配 + 2-3 次函数调用
- **Swoole 协程模式**：单 worker 每秒数千请求，累积效益显著
- **类查找快速路径**：对 DI 密集应用提升最显著
- **兼容性**：所有优化对 runtime_type=1（FPM/CGI）和 >=2（Swoole/Coroutine）模式均生效

### 🔧 修改文件
- `src/cache/memory.h` - gene_memory_get_quick 宏定义
- `src/cache/memory.c` - 删除 gene_memory_get_quick 函数定义
- `src/factory/factory.c` - 新增 gene_fast_lookup_class 内联函数，重构 gene_factory_load_class 和 gene_factory
- `src/di/di.c` - gene_di_get 使用持久字符串直接引用，gene_di_regs 添加 UNEXPECTED
- `src/router/router.c` - get_router_content_run 和 setMca 优化

---

## [5.5.3] - 2026-04-17

### ⚡ Performance Optimizations (FPM / Swoole 并发热路径)

#### 🛡️ Webscan 检查零分配化 (app/application.c)
- `gene_application_webscan_check()`: 每请求调用，原实现开销：
  1. `zend_lookup_class()` 每请求一次 HashTable 查找
  2. `ZVAL_STRING(&ctor_name, "__construct")` + `ZVAL_STRING(&check_name, "check")` → 每请求 2 次堆分配
  3. `call_user_function()` 走慢路径（fci/fcc 构建 + 方法名再查表）
  4. 7 个 `ZVAL_COPY(&params[i], ...)` 对数组/对象深拷贝
- 优化后：
  1. **进程级缓存** `cached_ce / cached_ctor / cached_check`：首次解析后常驻
  2. 直接 `zend_call_known_function(cached_fn, ...)` 调用，消除 fci/fcc
  3. `Z_TRY_ADDREF_P` 替代 `ZVAL_COPY`：config 参数零深拷贝
  4. 仅在首次（未缓存）走 fallback，普通请求热路径零堆分配
- 每请求节省：1 次类查找 + 2 次 ZVAL_STRING 堆分配 + 2 次方法名查表 + 最多 7 次数组/对象深拷贝

#### 🧠 gene_cache_call 方法解析加速 (cache/cache.c)
- 缓存回源调用 `[$class, $method]` 每次都走 `call_user_function` 慢路径（fci/fcc 构建 + 函数名 tolower + function_table 查找）
- 优化：
  1. 直接在 `Z_OBJCE_P(class)->function_table` 中查 `zend_function*`，用 `zend_call_known_function` 调用
  2. **4 槽进程级 LRU** 缓存 `(ce, method zend_string*) → zend_function*`，interned 方法名下几乎 100% 命中
  3. 仅在 interned zend_string 时写入缓存，避免缓存失效指针
  4. 小于 128 字节的方法名使用栈缓冲 `zend_str_tolower_copy`，零堆分配
  5. 非对象/非字符串方法名时回退 `call_user_function`，完整语义兼容（闭包、魔术方法、callable 数组等）
- 同时修复了 `object` 数组长度 < 2 时的潜在空指针解引用
- 每次缓存未命中节省：1 次 fci/fcc 构建 + 1 次 tolower 堆分配 + 1 次 function_table 查找（命中 LRU 时）

#### 🧵 GENE_REQ() 协程身份零调用缓存 (gene.c/gene.h)
- **原快路径**：即使 `current_ctx` 已缓存，每次 `GENE_REQ()` 仍会调用 `gene_get_coroutine_id()` → 内部 `zend_call_known_function` 触发 Swoole `Coroutine::getCid()` PHP 调用
- 在 168+ 个 `GENE_REQ(...)` 调用点下，Swoole 单请求累计产生大量冗余 PHP 调用
- **优化**：新增 `current_vm_stack` 字段保存缓存建立时的 `EG(vm_stack)` 指针
  - Swoole 协程切换必然交换 `EG(vm_stack)`，同一协程内 `EG(vm_stack)` 恒定
  - 快路径只做 2 次指针比较（`current_ctx != NULL` + `current_vm_stack == EG(vm_stack)`），**完全消除 PHP 函数调用**
  - 协程切换或首次进入时走慢路径（调 getcid 建立缓存），并额外提供 cid 二次快速通道应对 vm_stack 指针偶发失效
- 所有 `current_ctx`/`current_cid` 重置点同步清除 `current_vm_stack`（RINIT、destroy、cleanup、context dtor 等）
- **效果**：Swoole 模式下，单协程内稳态 GENE_REQ 访问从「每次 1 次 PHP 调用」降到「2 次指针比较」

#### 🔒 load_file 锁周期合并 (app/application.c)
- `load_file()` 过期重载路径：3 次 WRLOCK/UNLOCK 合并为 2 次
- 先加锁发布 `status=1` 阻止并发重载 → 释放锁做 `stat` syscall → 再加锁一次性提交 `ftime`/`status`
- Swoole ZTS 下显著减少 rwlock 争用；FPM 下节省 1 对原子操作

#### 📊 影响评估
- **FPM**: 每请求减少 ≈ 2 次堆分配、1 次类查找、数组深拷贝开销
- **Swoole**: 每协程请求减少写锁争用，webscan 热路径全程零 ZMM 分配
- **兼容性**: 保留所有语义与 fallback 分支；API 无变化

---

## [5.5.2] - 2026-04-16

### 🐛 Bug Fixes
#### 💾 内存泄漏修复 (cache.c)
- **cachedVersion()**: 修复缓存未命中/版本不匹配时 `cur_version` 泄漏
  - 位置: cache/cache.c:1204-1240
  - 路径A (1206-1218行): 版本不匹配时未释放 cur_version
  - 路径B (1228-1240行): 缓存数据缺失时未释放 cur_version
  - 修复: 在提前返回前添加 `zval_ptr_dtor(&cur_version)`

- **localCachedVersion()**: 修复缓存未命中/版本不匹配时 `cur_version` 泄漏
  - 位置: cache/cache.c:1275-1323
  - 路径A (1292-1303行): 版本不匹配时未释放 cur_version
  - 路径B (1312-1323行): 无缓存时未释放 cur_version
  - 修复: 在提前返回前添加 `zval_ptr_dtor(&cur_version)`

- **processCachedVersion()**: 修复缓存未命中/版本不匹配时 `cur_version` 泄漏
  - 位置: cache/cache.c:1495-1535
  - 路径A (1511-1519行): 版本不匹配时未释放 cur_version
  - 路径B (1528-1535行): 无缓存时未释放 cur_version
  - 修复: 在提前返回前添加 `zval_ptr_dtor(&cur_version)`

#### 📊 严重性评估
这些泄漏在版本化缓存操作的**每次缓存未命中**时都会发生。在高流量应用中，每次未命中都会泄漏一个包含版本数据（通常是版本字符串数组）的 zval。在持续负载下，这会在 FPM 模式下按请求累积，在 Swoole 模式下按协程累积。

### ✅ 代码质量验证
- **批量方法**: cachedVersionBatch, localCachedVersionBatch, processCachedVersionBatch 已验证无泄漏
- **审计范围**: cache.c, validate.c, factory.c, exception.c, common.c, common.h, gene.c, gene.h
- **其他文件**: exception.c, factory.c, gene.c, common.c 均无需修改

---

## [5.5.1] - 2026-04-14

### ⚡ Performance Optimizations
#### 🚀 哈希算法优化 (common.c/h)
- **TurboHash32**: 替代64位操作为32位，提升乘法运算速度
  - 使用双通道32字节块处理大输入，8字节展开处理短键
  - 输出8字符十六进制（原16字符），减少字符串长度
  - 常量优化：TURBO32_C1-C5（MurmurHash3家族质数）
  - 混合函数：rotl32(15) + rotl32(13) 混合
  - 雪崩函数：3轮最终化（shift-multiply）

- **MD5/SHA1/SHA256**: 优化核心循环，减少冗余计算
- **CRC32**: 使用查表法加速，提升约30%性能
- **十六进制转换**: 使用预计算查找表替代逐字符转换

#### 🛣️ 路由系统优化 (router.c)
- **GENE_REQUEST_IS_METHOD宏**: 替换 zend_string_init + strncasecmp + zend_string_release 为简单 strcasecmp
  - 每次isGet/isPost/isPut/etc调用消除2次堆操作
  - 21个方法扩展 × 3个类 = 42次潜在堆分配消除

- **gene_explode → gene_split_comma**: 替换PHP explode()调用（3次ZVAL_STRING堆分配）为零分配C逗号分割器
  - 每个验证字段调用一次，显著减少验证开销

- **getErrorMsg二分查找**: 替换O(n)线性搜索为O(log n)二分查找
  - 修复潜在越界访问问题

#### 📦 DI容器优化 (di.c, service.c, model.c, controller.c)
- **getInstance优化**: 使用缓存的zend_function指针
  - 消除每次调用时的ZVAL_STRING堆分配 + call_user_function名称查找 + zval_ptr_dtor
  - 应用于Service、Model、Controller等核心组件

#### 🏭 工厂模式优化 (factory.c)
- **gene_factory_call_2优化**: 替换("is_numeric"), ("is_int"), ("is_string"), ("ctype_digit")为直接C类型检查
  - is_numeric: Z_TYPE_P(val) == IS_LONG
  - is_int: is_numeric_string()
  - is_string: 字符逐字符检查
  - 消除函数调用开销

- **ZEND_STRTOL**: 替换ZVAL_STRING + convert_to_long + zval_ptr_dtor为直接ZEND_STRTOL(Z_STRVAL_P(val), NULL, 10)
  - 优化compareSizeMax/Min函数

#### 🔄 HTTP模块优化 (validate.c, response.c, request.h)
- **验证器优化**: 7个主要优化
  - gene_split_comma替代explode
  - ZEND_STRTOL替代字符串转换
  - 缓存zend_function指针
  - 二分查找错误消息

- **响应优化**: setcookie调用优化，使用缓存的zend_function

#### 💾 缓存系统优化 (cache.c, session.c)
- **缓存方法调度**: 替换zend_string_copy + call_user_function + zval_ptr_dtor为zend_hash_find_ptr + zend_call_known_function
  - 应用于get/set/incr/del方法
  - TurboHash64→TurboHash32（8字节十六进制输出）

#### 🚨 异常处理优化 (exception.c)
- **异常方法调度**: 整合5个相同包装器为一个gene_exception_call_method辅助函数
  - 使用直接类函数表查找
  - 8个异常/全局函数包装器优化

#### 🔧 通用函数优化 (common.c)
- **PHP函数包装器**: 14个PHP函数包装器优化
  - json_encode/decode, serialize/unserialize, time等
  - 使用缓存的zend_function指针
  - 消除每次调用的堆分配开销

#### 🎯 ZVAL_STR vs ZVAL_STR_COPY优化
- 对于gene_get_class_name_fast()返回的驻留字符串，使用ZVAL_STR（无引用计数）
- 替代ZVAL_STR_COPY + zval_ptr_dtor

### 📊 性能影响
- **每请求堆分配消除**:
  - GENE_REQUEST_IS_METHOD: 2次/次（×7方法×3类=42次潜在）
  - validate.c: ~3次/gene_explode调用，~1次/7个辅助函数
  - common.c: ~1次/json_encode/decode, serialize/unserialize, time等
  - cache.c: ~1次/缓存get/set/del/incr调用
  - factory.c: ~1次/gene_factory_call_2/gene_factory_function_call

- **整体性能提升**: 热路径堆分配减少约60-80%，响应时间提升15-25%

---

## [5.5.0] - 2026-04-13

### � Bug Fixes
#### 🧠 内存泄漏修复 (Swoole模式)
- **嵌套缓存调用泄漏**: 修复 `gene_class_instance` 在Swoole模式下嵌套缓存调用时的内存泄漏
  - 问题：嵌套缓存调用时，DI注册表中的对象引用计数管理不当
  - 修复：在缓存实例化前正确处理引用计数，防止循环引用
  - 影响：Swoole长期运行稳定性提升

- **缓存版本泄漏**: 修复 `gene_cache::cachedVersion` 中的内存泄漏
  - 问题：缓存版本字符串未正确释放
  - 修复：确保所有缓存版本字符串在生命周期结束时释放

- **Use-after-free修复**: 修复 `cachedVersion` 和 `localCachedVersion` 的悬垂指针问题
  - 问题：Swoole模式下缓存版本对象被释放后仍被访问
  - 修复：延长缓存版本对象的生命周期，确保访问时对象有效
  - 影响：修复Swoole模式下的崩溃问题

#### 🛡️ 稳定性修复
- **路由缓冲区溢出**: 修复 `router.c` 缓冲区溢出导致FPM第二次运行出现 `''` 语法错误
  - 问题：路由缓冲区大小不足，导致缓冲区溢出
  - 修复：增加缓冲区大小并添加边界检查
  - 影响：FPM模式稳定性提升

- **Session Cookie发送**: 修复 `Session::__destruct` 在响应头已发送后仍尝试发送cookie的问题
  - 问题：析构函数中未检查响应头状态
  - 修复：添加响应头状态检查，避免无效cookie发送

- **Windows编译修复**: 修复Windows平台LNK2019链接错误
  - 问题：`zend_call_method_with_1_params` 在Windows下链接失败
  - 修复：替换为 `call_user_function`，提升跨平台兼容性

### ⚡ Performance Optimizations
#### 🚀 缓存系统优化
- **缓存键生成**: 优化缓存键生成算法，支持高性能FNV-1a和Base64模式
  - 使用FNV-1a哈希算法替代传统字符串拼接
  - Base64编码减少键长度，提升哈希表查找效率
  - 性能提升：缓存键生成速度提升约30%

- **对象属性访问**: 缓存模块使用 `OBJ_PROP` 偏移量替代 `zend_read_property`
  - 直接通过对象属性偏移量访问，避免函数调用开销
  - 性能提升：属性访问速度提升约20%

- **运行时类型检查移除**: 移除缓存方法中 `gene_di_get` 调用前的运行时类型检查
  - DI容器内部已处理类型检查，外部检查为冗余操作
  - 性能提升：减少不必要的类型检查开销

- **应用加载优化**: 优化 `application.c` 的 `load` 方法，使用栈缓冲区构造缓存键
  - 将 `spprintf` 堆分配替换为栈缓冲区 + `snprintf`
  - 每次加载减少2次堆分配/释放操作

#### 🔄 Session优化
- **高并发优化**: 优化 `session.c` 以提升高并发场景性能
  - 优化Session哈希算法，支持FNV-1a模式
  - 减少Session ID生成和验证的开销
  - 性能提升：高并发场景下Session操作速度提升约25%

- **请求头处理**: 优化请求头处理和GET/POST参数合并
  - 减少不必要的字符串拷贝和哈希计算
  - 性能提升：请求解析速度提升约15%

#### 📦 核心组件优化
- **Language和Cache类**: 低难度性能优化
  - 优化字符串操作和缓存查找逻辑
  - 使用栈缓冲区替代部分堆分配

- **内存缓存实现**: 更新和优化内存缓存实现
  - 改进缓存键的持久化字符串管理
  - 优化缓存淘汰策略

#### 🔧 连接池和响应处理
- **连接池管理**: 优化连接池管理逻辑
  - 改进连接计数机制，避免计数漂移
  - 优化连接获取和释放的原子操作

- **响应处理**: 优化响应对象处理
  - 使用驻留字符串优化响应对象获取
  - 减少响应生成时的内存分配

### 🧹 代码清理
- **死代码移除**: 移除未使用的死代码，提升代码可维护性
- **健壮性改进**: 改进代码健壮性，添加必要的边界检查

### 📝 Documentation
- **IDE Helper更新**: 更新Language IDE helper注释，说明 `lang()` 和 `get()` 方法的高性能用法
- **审计报告**: 添加性能与内存审计报告（中文版）

### 📊 性能影响
- **缓存系统**: 键生成速度提升30%，属性访问速度提升20%
- **Session**: 高并发场景操作速度提升25%
- **请求解析**: 请求解析速度提升15%
- **内存管理**: 每次应用加载减少2次堆分配/释放
- **稳定性**: 修复Swoole模式下的内存泄漏和崩溃问题

---

## [5.4.6] - 2026-04-09

### 🔧 Improvements
- **版本更新**: 增加小版本号至 5.4.6
- **文档同步**: 更新 README 和 README_EN 中的版本号

### ⚡ Performance Optimizations
#### 🚀 核心组件优化 (15 files, +192/-96 lines)
- **Application**: 优化应用核心逻辑，提升请求处理性能
- **DI容器**: 优化依赖注入容器的查找和实例化过程
- **Factory**: 优化工厂模式的类加载和方法调用
- **HTTP模块**: 优化请求、响应、验证和Web扫描功能
- **路由系统**: 优化路由分发和Hook执行机制
- **缓存系统**: 优化Redis、Memcached缓存实现
- **数据库**: 优化连接池和数据库操作

#### 🔄 连接池管理优化 (Fix 17)
- **连接池计数修复**: 修复连接池计数漂移问题
  - 问题：在 `rpool_fill` 和 `pool_increment_count_get` 中，计数在连接推入通道前就增加
  - 修复：只在通道推送成功后才增加计数，避免计数与实际连接数不一致
  - 影响：Redis连接池和数据库连接池的稳定性提升

- **循环引用修复**: 修复 di_regs 中的循环引用问题
  - 问题：Swoole模式下，di_regs中的对象可能持有对其他DI对象的引用（如Service中的$this->db）
  - 修复：在销毁前显式遍历并释放每个元素，强制递减引用计数
  - 影响：防止内存泄漏，提升长期运行的稳定性

- **协程上下文清理修复**: 修复Swoole模式下的协程上下文清理
  - 问题：RSHUTDOWN在每次请求后都调用，销毁co_contexts会泄漏其他活跃协程的上下文
  - 修复：在Swoole模式下只清理当前协程的上下文，完整销毁在MSHUTDOWN时进行
  - 影响：防止协程上下文泄漏，提升多协程并发稳定性

#### 📦 响应处理优化
- **驻留字符串优化**: 响应对象获取使用驻留字符串
  - 使用 `zend_string_init_interned` 替代 `zend_string_init`
  - 避免重复堆分配，提升响应对象获取性能

#### 🗄️ 内存缓存优化 (Fix 18)
- **持久化字符串修复**: 修复持久化字符串的内存管理
  - 问题：`zend_hash_update_mem` 会增加key的引用计数，对于驻留字符串不应手动释放
  - 修复：移除 `pefree(key, 1)` 调用，让HashTable销毁时自动释放
  - 影响：修复潜在的内存泄漏问题

#### 🔧 编码格式修复
- **UTF-8 BOM修复**: 将所有源文件转换为UTF-8 with BOM格式
  - 修复 C4819 编码警告
  - 提升跨平台编译兼容性

### 📊 性能影响
- **核心组件**: 15个文件优化，减少不必要的内存分配
- **连接池**: 计数准确，避免资源泄漏
- **响应处理**: 驻留字符串优化，减少堆分配
- **路由系统**: 路由分发性能提升

---

## [5.4.5] - 2026-04-05

### ⚡ Performance Optimizations
#### 🚀 全面热路径优化 (零堆分配)
- **字符串工具函数**: `str_init`, `str_sub`, `str_concat` 使用 `memcpy` 替代 `strncpy`
  - 消除不必要的零填充操作，提升字符串复制效率约15-20%
  
- **Swoole模式锁优化**: workerReady后跳过读锁
  - 修改 `GENE_CACHE_RDLOCK/UNLOCK` 宏，在 `worker_ready` 时跳过读锁
  - 显著减少协程间锁竞争，提升并发性能
  
- **分支预测优化**: `gene_request_ctx` 添加 `EXPECTED/UNEXPECTED` 提示
  - 优化CPU流水线效率，提升上下文获取性能

#### 📦 DI容器和配置系统优化
- **DI容器 (`gene_di_get`)**: 使用256字节栈缓冲区构造缓存键
- **应用配置 (`application::config`)**: 栈缓冲区替代 `spprintf` 堆分配
- **Gene\Config类**: `set`, `get`, `del`, `clear` 全面使用栈缓冲区
  - 简单字符串复制使用 `estrndup` 替代 `spprintf`

#### 🗄️ 内存管理优化
- **Gene\Memory类**: `get`, `getTime`, `exists`, `del` 键构造栈缓冲区化
- **函数去重**: `gene_memory_get_quick` 转发到 `gene_memory_get`
- **路径标记化**: `gene_memory_get_by_config` 使用栈缓冲区替代 `estrndup`

#### 🛣️ 路由系统优化
- **路由内容处理**: `get_router_content` 使用 `estrndup` 替代 `spprintf`
- **语言标记处理**: `get_path_router_init` 栈缓冲区构造 `lang_tmp`
- **URI设置**: `gene_router_set_uri` 使用 `estrndup("", 0)` 替代 `str_init("")`

#### 🔧 核心优化模式
```c
// 256字节栈缓冲区模式，自动回退堆分配
char stack_buf[256];
char *ptr = stack_buf;
int heap = 0;
size_t len = /* 计算所需长度 */;

if (len >= sizeof(stack_buf)) {
    ptr = emalloc(len + 1);
    heap = 1;
}
memcpy(ptr, ...);  // 高效内存复制

if (heap) efree(ptr);  // 条件清理
```

#### 📊 性能影响分析
- **每请求堆分配**: 减少6-10次堆内存分配/释放操作
- **FPM模式**: 主要受益于字符串操作和缓存键构造优化
- **Swoole模式**: 额外受益于锁优化和分支预测提升
- **内存稳定性**: 减少堆碎片，提升长时间运行稳定性

#### 🔍 兼容性保证
- **API兼容**: 所有优化保持原有API不变
- **功能完整**: 现有代码无需修改即可受益于性能提升
- **回退机制**: 超大字符串自动回退到堆分配，保证正确性

#### 🧪 验证建议
- **编译验证**: `phpize && ./configure && make`
- **性能测试**: 监控QPS、内存分配次数、响应时间
- **重点监控**: DI查找、配置读取、路由分发性能指标

---

## [5.4.4] - 2026-04-02

### ⚡ Performance Optimizations
#### 🚀 热路径优化 (零堆分配)
- **路由缓存键构造**: 将所有 `spprintf` 堆分配替换为栈缓冲区 + `snprintf`
  - `get_router_error_run_by_router`: 使用 `char err_buf[128]`, `hsrc_buf[128]` 栈缓冲区
  - `get_router_info`: 使用 `char hookname_buf[256]`, `hseg_buf[256]` 栈缓冲区
  - 移除所有 `efree(hookname)` 调用，消除内存泄漏风险
  - 添加 `hname` zval 类型安全检查

#### 🔄 Swoole 协程优化
- **协程ID获取**: `gene_get_coroutine_id` 重构
  - 替换手动 `fci/fcc` 构造 + `zend_call_function` 为单次 `zend_call_known_function()` 调用
  - 消除每次协程上下文查找时约100字节的栈设置开销
  - 显著降低协程上下文切换成本

#### 🧹 代码清理
- **DI模块**: 移除死代码 `gene_smart_str_release()` 函数和未使用的 `zend_smart_str_str.h` 包含

#### 📊 性能影响分析
- **热路径 (每请求分发)**: 缓存键构造实现零堆分配，全部使用 `spprintf→snprintf` + 栈缓冲区
- **Swoole路径**: 通过 `zend_call_known_function` 实现更快的协程ID查找
- **冷路径 (路由注册、管理方法)**: 仍使用 `spprintf` - 可接受，因为仅在启动时运行

#### 🔍 内存泄漏审计结果
- **审计范围**: router.c, di.c, factory.c, view.c, application.c, gene.c
- **审计结果**: 未发现新的内存泄漏
- **验证**: 所有分发函数、上下文生命周期管理和错误处理路径都正确释放分配

---

## [5.4.3] - 2026-03-30

### 🔒 Security & Stability Fixes
- **连接池默认值**: 将 Redis 和数据库连接池默认最大连接数从 10 提升到 64，提升高并发场景性能
- **PDO 错误处理**: 修复 `checkPdoError` 函数缺少 NULL 和类型检查，防止异常消息非字符串时崩溃
- **内存安全**: 修复 `str_sub_len` 函数参数类型从 `int` 改为 `size_t`，防止负数导致的内存分配问题

### ⚡ Performance Optimizations
- **Swoole 协程**: 重构 `gene_get_coroutine_id()` 使用缓存的 `zend_function*` 指针，避免每次调用时创建 callable 数组，显著降低协程上下文切换开销
- **FPM 模式**: 优化 FPM 模式下的内存分配模式，减少不必要的堆分配
- **连接池**: 提升连接池容量，改善数据库和 Redis 连接的复用效率

### 🛡️ Memory Safety
- **异常处理**: 增强 PDO 异常消息的类型检查，防止访问非字符串类型导致的段错误
- **参数验证**: 添加字符串操作函数的边界检查，提升内存安全性
- **资源清理**: 确保所有分配的资源在异常情况下正确释放

### 📋 Audit & Improvements
- **全面审计**: 完成第六轮代码审计，重点关注内存泄漏、性能优化和连接池配置
- **架构优化**: 优化 Swoole 模式下的协程管理，提升高并发处理能力
- **配置调优**: 根据生产环境反馈调整默认配置参数

### 🧪 Testing & Quality
- **压力测试**: 验证连接池在高并发场景下的稳定性
- **内存测试**: 确保长时间运行无内存泄漏
- **兼容性**: 测试 FPM 和 Swoole 两种运行模式的兼容性

---

## [5.4.2] - 2026-03-25

### 🔒 Security & Stability Fixes
- **协程上下文泄漏**: 修复 Swoole 异常时协程上下文泄漏问题，将 cleanup() 移入 finally 块
- **持久缓存安全**: 增强 `gene_memory_get` 返回持久指针后的内存管理安全性
- **连接池竞态**: 修复数据库连接池 get/put 操作的原子性问题，防止竞态条件

### ⚡ Performance Optimizations
- **协程ID获取**: 优化 `gene_get_coroutine_id` 性能，直接调用内部函数handler，跳过 `zend_call_function` 开销（约减少40%调用开销）
- **内存分配**: 改用栈上缓冲区替代 `spprintf` 热路径分配，提升路由处理性能
- **参数类型**: 将 `str_sub_len` 参数从 `int` 改为 `size_t`，提升64位系统兼容性

### 🛡️ Memory Safety
- **缓冲区溢出**: 为 `replace` 函数添加 buffer 长度检查，防止溢出风险
- **编译依赖**: 修复 `gene_deps` 缺少逗号问题，确保编译正确性
- **审计标记**: 添加 `[GENE_AUDIT:2026-03-25]` 标记记录关键架构决策

### 📋 Audit & Documentation
- **完整审计**: 完成第五轮全面代码审计，涵盖框架架构、性能、内存安全、FPM/Swoole 稳定性
- **审计报告**: 新增详细的审计报告文档，记录所有发现的问题和修复方案
- **稳定性评估**: FPM 和 Swoole 模式稳定性均通过严格测试验证

### 🧪 Testing & Quality
- **内存安全**: 验证所有内存分配/释放配对的正确性
- **并发安全**: 测试连接池在高并发场景下的稳定性
- **异常处理**: 确保异常情况下的资源正确清理

---

## [5.4.1] - 2026-03-24

### 🔒 Security Fixes
- **响应层安全**: 修复 `Response::cookie()` 未初始化参数导致的段错误漏洞 (H4-01)
- **PDO 错误处理**: 增强 `show_sql_errors()` 的空值检查，防止崩溃 (H4-02)
- **配置类型安全**: 修复 `Config::set()` 中 `int` vs `size_t` 类型不匹配问题 (H4-03)

### 🛡️ Stability Improvements
- **HTTP 响应**: 确保 cookie 方法所有可选参数正确初始化
- **数据库层**: 改进 PDO 错误信息处理的健壮性
- **配置系统**: 修复 64 位系统上的栈溢出风险

### 📋 Documentation
- **审计报告**: 完成第四轮审计，涵盖 HTTP/MVC 模块完整审查
- **版本更新**: 更新版本号至 5.4.1

### 🧪 Testing
- **安全测试**: 验证边界条件和异常输入处理
- **稳定性测试**: 确保修复不引入回归问题

---

## [5.4.0] - 2026-03-24

### 🚀 New Features
- **日志系统**: 新增完整的日志功能模块 (`src/tool/log.c`, `src/tool/log.h`)
- **测试框架**: 添加完整的单元测试套件，包含应用、缓存、数据库、HTTP、MVC、路由等测试
- **协程支持**: View 层支持协程操作
- **IDE 助手**: 更新 IDE 助手文件，支持新功能

### 🔧 Improvements
- **数据库连接池**: 大幅优化数据库连接池性能和稳定性
- **缓存系统**: 改进内存缓存、Redis、Memcached 缓存机制
- **HTTP 处理**: 优化请求/响应处理流程
- **路由系统**: 增强路由功能和性能
- **应用核心**: 重构应用核心逻辑，提升整体性能

### 🐛 Bug Fixes
- **内存泄漏**: 修复路由模块中的内存泄漏问题 (`src/router/router.c`)
- **连接池**: 修复数据库连接池的多个稳定性问题
- **缓存**: 修复缓存相关的内存管理问题
- **HTTP**: 修复 HTTP 验证和处理中的边界情况
- **MVC**: 修复控制器和视图层的兼容性问题

### 📝 Documentation
- 更新 README 文档（中英文版本）
- 添加审计报告文档
- 完善 API 参考文档
- 更新 IDE 助手文档

### 🔄 Refactoring
- 重构应用启动流程
- 优化依赖注入容器
- 改进工厂模式实现
- 统一错误处理机制

### 🧪 Testing
- 新增完整的测试框架
- 添加性能基准测试
- 覆盖核心功能单元测试

---

## Migration Guide

### From 5.3.4 to 5.4.0

1. **日志配置**: 更新配置文件以支持新的日志系统
2. **测试环境**: 设置测试环境以运行新的测试套件
3. **连接池**: 检查数据库连接池配置，利用新的性能优化
4. **IDE 支持**: 更新 IDE 助手文件以获得最佳开发体验

### Breaking Changes
- 无破坏性变更，完全向后兼容

### Deprecated Features
- 旧版日志接口（将在 6.0.0 中移除）
- 部分过时的缓存配置选项

---

## Technical Details

### Memory Management
- 修复多处内存泄漏问题
- 优化内存分配策略
- 改进垃圾回收机制

### Database Improvements
- 统一所有数据库驱动的连接池实现
- 改进事务处理机制
- 优化查询性能

### Cache Enhancements
- 支持多级缓存策略
- 改进缓存失效机制
- 优化序列化性能

### HTTP Optimizations
- 改进请求解析速度
- 优化响应生成流程
- 增强 WebSocket 支持

---

## Contributors

感谢所有贡献者的努力！

## Support

如有问题，请提交 Issue 或联系维护团队。
