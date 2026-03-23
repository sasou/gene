# Gene PHP 扩展 — 全面源代码审计报告

**版本**: 5.3.6  
**日期**: 2025年  
**范围**: `src/` 目录下的所有 `.c` 和 `.h` 文件

---

## 目录

1. [架构概述](#1-架构概述)
2. [已修复的问题（已验证）](#2-已修复的问题已验证)
3. [新发现 — 错误与潜在崩溃](#3-新发现--错误与潜在崩溃)
4. [新发现 — 内存管理](#4-新发现--内存管理)
5. [新发现 — 安全问题](#5-新发现--安全问题)
6. [新发现 — 代码质量与可维护性](#6-新发现--代码质量与可维护性)
7. [新发现 — 协程 / Swoole 安全性](#7-新发现--协程--swoole-安全性)
8. [模块逐项总结](#8-模块逐项总结)
9. [建议](#9-建议)

---

## 1. 架构概述

Gene 扩展是一个功能完整的 PHP MVC 框架，以 C 扩展形式实现。主要组件：

- **核心** (`gene.h`, `gene.c`): 模块全局变量、请求上下文生命周期、通过 `gene_request_context` 结构和 `GENE_REQ()` 宏进行协程上下文管理。
- **应用** (`app/application.c`): 启动、路由分发、两阶段清理（`clearState` + `destroyContext`）。
- **DI 容器** (`di/di.c`): 服务注册/检索，在每请求的 `di_regs` 哈希表中缓存实例。
- **路由** (`router/router.c`): URL 路由与钩子系统、基于反射的源码缓存、`zend_eval_stringl` 执行。
- **数据库** (`db/mysql.c`, `db/mssql.c`, `db/pgsql.c`, `db/sqlite.c`, `db/pdo.c`, `db/pool.c`): MySQL、MSSQL、PostgreSQL、SQLite 的 PDO 包装查询构建器，支持连接池。
- **缓存** (`cache/cache.c`, `cache/memory.c`, `cache/redis.c`, `cache/memcached.c`): 内存（HashTable）、Redis、Memcached 缓存驱动，使用 RWLock 保护。
- **HTTP** (`http/request.c`, `http/response.c`, `http/validate.c`, `http/webscan.c`): 请求/响应包装器、验证、Web 安全扫描器。
- **MVC** (`mvc/controller.c`, `mvc/model.c`, `mvc/view.c`): 控制器/模型基类、使用 PHP include 的视图渲染。
- **会话** (`session/session.c`): 自定义会话管理，支持 DI 驱动的存储驱动、Cookie 处理、基于路径的 get/set。
- **工具** (`tool/benchmark.c`, `tool/execute.c`, `tool/language.c`, `tool/log.c`): 基准测试、动态代码执行、i18n 语言文件、结构化日志。
- **其他** (`factory/factory.c`, `factory/load.c`, `config/configs.c`, `common/common.c`, `exception/exception.c`, `service/service.c`): 工厂模式、自动加载、INI 配置解析、实用函数、异常处理、服务层。

### 请求上下文模型

```
gene_request_context {
    method, path, router_path, module, controller, action, child_views, lang  (char*)
    path_params       (zval* → array)
    request_attr      (zval → array)
    di_regs           (zval → array)
    view_vars         (zval → array)
    db_*_history      (zval → array)
    bench_start/end   (struct timeval)
    bench_memory_*    (zend_long)
}
```

上下文按模式管理：
- **FPM/CLI**: `default_ctx`（模块全局变量）
- **Swoole 常驻**: `resident_ctx`（堆分配，跨请求持久化）
- **Swoole 协程**: `co_contexts` 哈希表，以协程 ID 为键

---

## 2. 已修复的问题（已验证）

这些修复已在代码库中应用并验证：

| # | 文件 | 问题 | 修复 |
|---|------|-------|-----|
| 5 | `router/router.c` | `get_function_content` 缺少错误检查 → SIGSEGV | 在 Reflection 调用后添加异常/类型检查 |
| 6 | `router/router.c` | `get_router_content_F` NULL src → 未定义行为 | 添加 NULL 检查 |
| 7 | `router/router.c` | `get_router_info` NULL cacheHook 解引用 → SIGSEGV | 在 `Z_ARRVAL_P` 前添加 NULL/类型检查 |
| 8 | `router/router.c` | `zend_eval_stringl` 无保护 → worker 崩溃 | 用 `zend_try`/`zend_catch` 包装 |
| 11 | `app/application.c` | `clearState` 过早销毁上下文（协程） | 改为 `gene_request_context_reset()` |
| 12 | `app/application.c` | `destroyContext` 旧指针窗口 | 在哈希删除前使 `current_ctx` 无效 |
| 13 | `gene.c` | `RSHUTDOWN` 旧指针 | 在销毁前移动 NULL 赋值 |
| 14 | `db/mysql.c` | `ATTR_PERSISTENT` 在协程中导致套接字冲突 | 为 `runtime_type >= 2` 移除持久化标志 |
| 15 | `di/di.c` | 引用计数错误 — 双哈希条目，单引用计数 → UAF | 在第一个 `zend_hash_update` 前添加 `Z_TRY_ADDREF` |

---

## 3. 新发现 — 错误与潜在崩溃

### 发现 3.1: `in()` 方法中的 `spprintf` 格式不匹配（所有数据库驱动）

**文件**: `db/mssql.c:657`, `db/pgsql.c:657`, `db/sqlite.c:656`  
**严重程度**: 中等（潜在截断/未定义行为）

```c
spprintf(&in_tmp, 0, "%s", in, in_len);
```

带 `%s` 格式的 `spprintf` 忽略额外的 `in_len` 参数。当 `in` 是以 NUL 结尾的（确实如此，来自 `zend_parse_parameters "s"`）时这是无害的，但代码似乎意图进行长度限制的复制。额外的参数是死代码。

**建议**: 从 `spprintf` 中移除未使用的 `in_len` 参数，或者如果意图进行长度限制，使用 `spprintf(&in_tmp, 0, "%.*s", (int)in_len, in)`。

---

### 发现 3.2: `getBenchMemory` 整数除法截断

**文件**: `tool/benchmark.c:114-123`  
**严重程度**: 低（错误结果）

```c
void getBenchMemory(zend_long *memory_start, zend_long *memory_end, char **ret, bool type) {
    zend_long memory;
    memory = *memory_end - *memory_start;
    if (type) {
        spprintf(ret, 0, "%.3f", memory/1024);
    } else {
        spprintf(ret, 0, "%.3f", memory/1048576);
    }
}
```

`memory` 是 `zend_long`（整数）。`memory/1024` 执行**整数除法**，然后整数结果被隐式转换为 `double` 用于 `%f`。这会丢失小数精度。例如，1500 字节 → `1500/1024 = 1`（整数）→ 打印 `1.000` 而不是 `1.465`。

**修复**: 在除法前转换为 double: `(double)memory / 1024.0`。

---

### 发现 3.3: `sqliteInitPdo` 需要用户名/密码但 SQLite 不使用它们

**文件**: `db/sqlite.c:187-191`  
**严重程度**: 低（可用性问题）

```c
if ((user = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("username"))) == NULL) {
     php_error_docref(NULL, E_ERROR, "PDO need a valid username.");
}
if ((pass = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"))) == NULL) {
     php_error_docref(NULL, E_ERROR, "PDO need a valid password.");
}
```

SQLite 数据库不需要用户名/密码。这强制用户提供空/虚拟凭据。应该为 SQLite 驱动设为可选。

---

### 发现 3.4: `pgsqlInitPdo` 缺少 `ATTR_EMULATE_PREPARES` 选项

**文件**: `db/pgsql.c:199-201`  
**严重程度**: 低（不一致性）

```c
add_index_long(&option, 3, 2);   // ATTR_ERRMODE = EXCEPTION
add_index_long(&option, 19, 2);  // ATTR_DEFAULT_FETCH_MODE = ASSOC
```

MySQL 驱动（`mysql.c`）设置 4 个 PDO 属性（ERRMODE、DEFAULT_FETCH_MODE、EMULATE_PREPARES、STRINGIFY_FETCHES）。PostgreSQL 和 SQLite 只设置 2 个。MSSQL 设置 4 个。这种不一致性可能导致跨驱动程序的行为差异。

---

### 发现 3.5: 错误消息中的拼写错误 — "dns" 而不是 "dsn"

**文件**: `db/mssql.c:173`, `db/pgsql.c:185`, `db/sqlite.c:185`  
**严重程度**: 微不足道

```c
php_error_docref(NULL, E_ERROR, "PDO need a valid dns.");
```

应该是 "dsn"（数据源名称），不是 "dns"（域名系统）。

---

### 发现 3.6: 多个数据库驱动中的未使用变量

**文件**: 所有数据库驱动的 `select()`、`insert()`、`update()` 方法  
**严重程度**: 微不足道（编译器警告）

像 `insert()` 和 `update()` 方法中的 `*select` 等变量被声明但从未使用。例如在 `mssql.c:410`: `char *table = NULL, *select = NULL, *sql = NULL;` — `select` 在 `insert` 中未使用。

---

## 4. 新发现 — 内存管理

### 发现 4.1: `gene_execite_opcodes_run` 缺少 `ret` 的 `zval_ptr_dtor`

**文件**: `tool/execute.c:47-58`  
**严重程度**: 中等（内存泄漏）

```c
void *gene_execite_opcodes_run(zend_op_array *op_array) {
    zend_op_array *orig_op_array = CG(active_op_array);
    zval ret;
    CG(active_op_array) = op_array;
    if (CG(active_op_array)) {
        zend_execute(CG(active_op_array), &ret);
        destroy_op_array(CG(active_op_array));
        efree(CG(active_op_array));
    }
    CG(active_op_array) = orig_op_array;
    return NULL;
}
```

`zend_execute` 向 `ret` 写入返回值，但 `ret` 从未用 `zval_ptr_dtor(&ret)` 清理。如果执行的代码返回复杂值（字符串、数组、对象），这会泄漏。

**修复**: 在 `efree(CG(active_op_array));` 后添加 `zval_ptr_dtor(&ret);`。

---

### 发现 4.2: `GetOpcodes` 在成功路径上泄漏 `opcodes_array`

**文件**: `tool/execute.c:110`  
**严重程度**: 低（轻微泄漏）

```c
RETURN_ZVAL(&opcodes_array, 1, 0);
```

`RETURN_ZVAL` 带 `dtor=0` 意味着 `opcodes_array` 在复制后不被销毁。由于 `RETURN_ZVAL` 复制值，原始 `opcodes_array` zval 的数组应该被释放。应该是 `RETURN_ZVAL(&opcodes_array, 1, 1)` 以在复制后销毁。

---

### 发现 4.3: `SaveHistory` 函数潜在的 NULL `param` 解引用

**文件**: `db/mssql.c:127`, `db/pgsql.c:127`, `db/sqlite.c:127`, 以及 `mysql.c` 中的等效代码  
**严重程度**: 低

```c
jsonEncode(&z_data, param);
```

`param` 来自 `zend_read_property(...GENE_DB_*_DATA...)`。如果 DATA 属性为 NULL（例如，对于没有参数的原始 SQL 查询），`jsonEncode` 接收 NULL 类型的 zval。行为取决于 `jsonEncode` 实现。值得添加 NULL/类型检查。

---

### 发现 4.4: `smart_str_appends(&field_str, "")` 是无操作分配

**文件**: 所有数据库驱动的 `insert()`、`batchInsert()` 方法  
**严重程度**: 微不足道

```c
smart_str_appends(&field_str, "");
smart_str_appends(&value_str, "");
```

这些分配 `smart_str` 缓冲区只是为了写入空字符串。这样做是为了确保在 `smart_str_0()` 之前 `field_str.s` 不为 NULL，但 `smart_str_0()` 已经处理 NULL。这些行可以移除。

---

## 5. 新发现 — 安全问题

### 发现 5.1: `Gene_Execute::StringRun` 执行任意 PHP 代码

**文件**: `tool/execute.c:117-130`  
**严重程度**: 高（安全设计问题）

```c
PHP_METHOD(gene_execute, StringRun) {
    zend_string *php_script;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &php_script) == FAILURE) {
        return;
    }
    zend_try {
        zend_eval_stringl(ZSTR_VAL(php_script), ZSTR_LEN(php_script), NULL, "");
    } zend_catch {
        zend_bailout();
    } zend_end_try();
    RETURN_TRUE;
}
```

这提供了直接的 PHP 级别 `eval()` 等价物。虽然 PHP 本身存在 `eval()`，但将其作为类方法暴露可能绕过 `disable_functions` 限制。**没有访问控制或验证**被执行。

**建议**: 考虑移除此类或添加运行时检查以仅限于开发模式，通过检查 `GENE_G(run_environment)`。至少，显著记录安全影响。

---

### 发现 5.2: `group()`、`having()`、`order()` 方法接受原始 SQL 字符串

**文件**: 所有数据库驱动（`mssql.c`、`pgsql.c`、`sqlite.c`、`mysql.c`）  
**严重程度**: 中等（如果用户输入到达这些方法则存在 SQL 注入）

```c
PHP_METHOD(gene_db_mssql, group) {
    // ...
    spprintf(&group_tmp, 0, " GROUP BY %s", group);
```

`group()`、`having()` 方法直接将用户提供的字符串插入 SQL 而不进行参数化。虽然 `where()` 和 `in()` 使用参数化查询（`?` 占位符），但这些子句构建器不使用。如果用户输入流入 `->group($userInput)`，这是 SQL 注入向量。

**建议**: 明确记录这些方法绝不能接收未经清理的用户输入，或添加标识符引用。

---

### 发现 5.3: `webscan` 模式是编译时静态的

**文件**: `http/webscan.c`  
**严重程度**: 低（灵活性有限）

XSS/SQLi 检测模式在 C 中硬编码。它们无法在不重新编译扩展的情况下更新。新的攻击向量需要代码更改和重新编译。

**建议**: 考虑允许通过 INI 或配置文件进行模式配置，以实现生产灵活性。

---

## 6. 新发现 — 代码质量与可维护性

### 发现 6.1: 数据库驱动间大量代码重复

**文件**: `db/mssql.c`（1200 行）、`db/pgsql.c`（1201 行）、`db/sqlite.c`（1200 行）、`db/mysql.c`  
**严重程度**: 高（可维护性）

四个数据库驱动文件**95%+ 相同**。唯一的差异是：
- 引号字符: MySQL 使用 `` ` ``, MSSQL 使用 `[`/`]`, PostgreSQL 使用 `"`, SQLite 使用 `` ` ``
- MSSQL `limit()` 使用 `OFFSET...FETCH NEXT` 语法 vs `LIMIT...OFFSET`
- 类条目名称和属性宏名称
- MSSQL 有额外的 `add_index_long(&option, 11, 2)` 和 `add_index_long(&option, 8, 2)` PDO 选项

每个错误修复必须以相同方式应用于所有四个文件。这已经导致不一致性（发现 3.4）。

**建议**: 提取通用基础实现，使用驱动特定的回调/配置处理引号字符和方言差异。单个 `gene_db_base.c` 配合驱动结构可以将约 4800 行近重复代码减少到约 1500 行。

---

### 发现 6.2: 函数名拼写错误: `gene_execite_opcodes_run`

**文件**: `tool/execute.c:47`, `tool/execute.h:23`  
**严重程度**: 微不足道

"execite" 应该是 "execute" → `gene_execute_opcodes_run`。这是一个公共 API 名称。

---

### 发现 6.3: `Gene_Log` 使用静态属性 — 非协程安全

**文件**: `tool/log.c:107,121`  
**严重程度**: 中等

```c
prop_level = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
prop_file = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
```

`Gene\Log::$file` 和 `Gene\Log::$level` 是静态类属性。在 Swoole 协程模式下，如果一个协程调用 `Log::setFile()`，它会影响所有协程。这是一个共享可变状态问题。

**建议**: 要么将此记录为有意的全局配置（对于日志配置可接受），要么如果需要每请求日志路由，则移至请求上下文。

---

### 发现 6.4: `Gene_Language` 配置缓存是每对象，不是每请求

**文件**: `tool/language.c:170-179`  
**严重程度**: 低

语言文件缓存（`config` 属性）存储为实例属性。这意味着每个新的 `Gene\Language` 对象实例都会重新加载语言文件。如果框架每请求创建新的 Language 对象（在 DI 中很常见），缓存无效。

**建议**: 考虑在请求上下文或静态属性中存储缓存，以在请求内有效重用。

---

## 7. 新发现 — 协程 / Swoole 安全性

### 发现 7.1: 数据库历史存储在请求上下文中 — 无界增长

**文件**: `gene.h:100-103`, 所有数据库驱动  
**严重程度**: 低（长时间运行进程中的内存压力）

```c
zval db_mysql_history;
zval db_pgsql_history;
zval db_sqlite_history;
zval db_mssql_history;
```

每次查询执行都会追加到历史数组（非生产模式下）。在处理许多请求后才 `clearState` 的 Swoole 服务器中，此数组无限增长。

**建议**: 添加可配置的历史限制，或在非生产模式下也在请求边界清除历史。

---

### 发现 7.2: 历史仅在构造函数中清除，不跨请求

**文件**: 所有数据库驱动的 `__construct` 方法（例如 `mssql.c:300-303`）  
**严重程度**: 低

```c
if (Z_TYPE(GENE_REQ(db_mssql_history)) != IS_UNDEF) {
    zval_ptr_dtor(&GENE_REQ(db_mssql_history));
}
ZVAL_UNDEF(&GENE_REQ(db_mssql_history));
```

历史仅在新数据库实例构造时重置。如果 Swoole worker 重用池化连接（无新构造函数调用），历史从上一个请求累积。

---

### 发现 7.3: 基准状态正确移至请求上下文 ✓

**文件**: `tool/benchmark.c:38-39`

注释确认基准变量已从文件作用域移至 `gene_request_context`。这对于协程安全性是正确的。数据库驱动 `SaveHistory` 函数也正确使用局部变量进行计时。

---

## 8. 模块逐项总结

| 模块 | 文件 | 行数 | 状态 | 关键问题 |
|--------|-------|-------|--------|------------|
| **核心** | `gene.h`, `gene.c` | ~300 | ✅ 良好 | 协程上下文模型健全 |
| **应用** | `app/application.c` | ~800 | ✅ 良好 | 两阶段清理正确实现 |
| **DI** | `di/di.c` | ~400 | ✅ 已修复 | 引用计数错误已修复（修复 15） |
| **路由** | `router/router.c` | ~1200 | ✅ 已修复 | 多个崩溃修复已应用（修复 5-8） |
| **DB MySQL** | `db/mysql.c` | ~1200 | ✅ 已修复 | 持久连接修复已应用（修复 14） |
| **DB MSSQL** | `db/mssql.c` | 1200 | ⚠️ | spprintf 格式不匹配（3.1），代码重复（6.1） |
| **DB PgSQL** | `db/pgsql.c` | 1201 | ⚠️ | 与 MSSQL 相同问题，缺少 PDO 选项（3.4） |
| **DB SQLite** | `db/sqlite.c` | 1200 | ⚠️ | 不必要的用户名/密码要求（3.3） |
| **DB PDO** | `db/pdo.c` | ~630 | ✅ 良好 | 清洁的 PDO 包装器实现 |
| **DB Pool** | `db/pool.c`, `pool.h` | ~200 | ✅ 良好 | 池获取/返回/通知模式 |
| **缓存** | `cache/*.c` | ~600 | ✅ 良好 | RWLock 保护的缓存操作 |
| **HTTP** | `http/*.c` | ~1200 | ✅ 良好 | 请求/响应/验证/Webscan |
| **MVC** | `mvc/*.c` | ~800 | ✅ 良好 | 控制器/模型/视图 |
| **会话** | `session/session.c` | ~750 | ✅ 良好 | DI 驱动的会话与 Cookie 处理 |
| **基准** | `tool/benchmark.c` | 226 | ⚠️ | 整数除法错误（3.2） |
| **执行** | `tool/execute.c` | 169 | ⚠️ | 内存泄漏（4.1），安全问题（5.1） |
| **语言** | `tool/language.c` | 283 | ✅ 良好 | 清洁的 i18n 实现 |
| **日志** | `tool/log.c` | 407 | ⚠️ | 静态属性非协程隔离（6.3） |
| **配置** | `config/configs.c` | ~400 | ✅ 良好 | INI/PHP 配置解析 |
| **通用** | `common/common.c` | ~500 | ✅ 良好 | 实用函数 |
| **工厂** | `factory/factory.c`, `load.c` | ~400 | ✅ 良好 | 自动加载，工厂模式 |
| **异常** | `exception/exception.c` | ~200 | ✅ 良好 | 异常类注册 |
| **服务** | `service/service.c` | ~300 | ✅ 良好 | 服务层 |
| **Webscan** | `http/webscan.c` | ~320 | ✅ 良好 | 安全过滤器（静态模式） |

---

## 9. 建议

### 优先级 1 — 错误修复（应修复）

1. **修复 `getBenchMemory` 整数除法**（`benchmark.c:118-121`）：除法前转换为 `(double)`。
2. **修复 `gene_execite_opcodes_run` 内存泄漏**（`execute.c`）：添加 `zval_ptr_dtor(&ret)`。
3. **修复 `GetOpcodes` 返回泄漏**（`execute.c:110`）：改为 `RETURN_ZVAL(&opcodes_array, 1, 1)`。
4. **修复 MSSQL/PgSQL/SQLite 驱动中错误消息的 "dns" 拼写错误**。

### 优先级 2 — 安全（应审查）

5. **限制 `Gene_Execute::StringRun`** 为开发模式或完全移除。
6. **记录 `group()`/`having()`/`order()` 方法中的 SQL 注入风险**。

### 优先级 3 — 可用性

7. **使 SQLite 用户名/密码可选**在 `sqliteInitPdo` 中。
8. **统一所有四个数据库驱动的 PDO 选项**以保持一致性。

### 优先级 4 — 可维护性（长期）

9. **重构数据库驱动**以消除约 3600 行重复代码。
10. **重命名 `gene_execite_opcodes_run`** 为 `gene_execute_opcodes_run`。

### 优先级 5 — 协程安全性（低风险，建议修复）

11. **添加历史大小限制**或数据库查询历史的每请求清除。
12. **记录 `Gene\Log` 静态属性在协程模式下的行为**。

---

## 总结

Gene 扩展是一个结构良好的 PHP 框架，具有成熟的协程感知架构。先前应用的修复（5-8、11-15）解决了最关键的崩溃和内存损坏问题。剩余发现主要是：

- **2 个真正的错误**（基准中的整数除法，执行中的内存泄漏）
- **1 个安全问题**（无限制的 eval）
- **1 个主要可维护性问题**（数据库驱动代码重复）
- **几个小问题**（拼写错误、未使用变量、不一致性）

代码库展示了对 PHP 内部 API 和协程安全性的仔细关注，正确使用了 `zval` 生命周期管理、`zend_hash` 操作和请求上下文隔离。
