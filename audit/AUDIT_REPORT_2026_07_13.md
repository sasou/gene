# Gene 扩展框架静态审计报告

**日期**: 2026-07-13 | **方式**: 纯静态代码审查 | **范围**: `src/` 全部 34 个 .c 文件

> 注：本文中的代码位置已做脱敏处理，仅保留模块、功能和相对范围，不包含本地绝对路径、可点击源码路径或过于精确的行号信息。

## 一、确认的问题

### 高风险

| # | 位置（已脱敏） | 问题 | 说明 |
|---|------|------|------|
| H1 | 公共函数与请求校验模块，共 22 处 | **ZTS 线程安全隐患**：`static zend_function *fn` 缓存 `CG(function_table)` 查找结果 | 项目已定义 ZTS 安全的 `GENE_CG_FN_LOOKUP` 宏，且其他工具模块已使用，但公共函数、请求校验及数据库辅助函数中的同类函数仍用裸 static 缓存。ZTS 构建下跨线程共享 per-thread 指针，可能崩溃。非 ZTS 构建无影响 |

### 中风险

| # | 位置（已脱敏） | 问题 | 说明 |
|---|------|------|------|
| M1 | 视图模块的模板检查与渲染流程，各 1 处 | **模板流打开失败后缺少 return** | `stream == NULL` 时仅发出 E_WARNING 并释放路径，随后仍继续返回或加载可能不存在、过期的编译文件路径，导致后续 include 失败或渲染旧模板。非内存错误，但属逻辑缺陷 |
| M2 | MySQL、MSSQL、PostgreSQL 及 SQLite 的 InitPdo 配置校验流程 | **配置项缺失时 E_ERROR 后无 return** | dsn/username/password 缺失时 `php_error_docref(E_ERROR)` 依赖 bailout 终止；若被错误处理器或 `zend_try` 拦截，执行可能继续并解引用 NULL |
| M3 | 内存缓存销毁与 LRU 跟踪销毁流程，各 1 处 | **pemalloc 返回值未检查** | 持久化分配（malloc 系）OOM 时返回 NULL，后续写入 key 数组可能崩溃。emalloc 会 bailout，但 pemalloc 不会 |
| M4 | 公共字符串工具函数 | **字符串工具函数无 NULL/越界防护** | `str_init`/`str_sub_len`/`str_append` 不检查 NULL 入参；`str_sub_len` 不校验 `start` 是否超出源串长度，依赖调用方保证，属脆弱 API |

### 低风险

| # | 位置（已脱敏） | 问题 |
|---|------|------|
| L1 | 数据库连接池命名缓存初始化与销毁流程 | `gene_pool_named_cache` 全局 HashTable 无 MSHUTDOWN 清理，依赖 `closeAll()`；worker 异常退出时由 PHP MM 兜底（代码注释已承认） |
| L2 | PDO 错误检查辅助函数 | `zend_read_property` 的 `rv` 输出槽未 dtor；message 通常为已存在的字符串属性，不产生临时值，仅在魔术 getter 场景可能泄漏 |
| L3 | Memcached 服务器配置校验流程 | `servers` 非数组时 E_ERROR 前对象未释放；bailout 后由请求关停回收，实际影响小 |
| L4 | 路由路径解析辅助函数 | 原地临时改写输入字符串后恢复，当前调用安全，但对常量字符串可能崩溃 |
| L5 | 公共批量插入与字符串替换辅助函数 | 前者覆盖传入 dest 指针（当前调用方传 NULL，惯例脆弱）；后者破坏输入串后失败路径无法恢复 |
| L6 | 路由初始化与路径所有权处理流程 | 返回值可能是新分配内存或原 path 指针，靠调用方判断是否释放；当前调用正确，但所有权约定脆弱，易在新调用点引入双重释放或泄漏 |

## 二、复核后排除的误报（子代理初报但经源码核实无问题）

- **配置模块 del/clear“双重释放”** — 两个 `efree` 分别处于 `RETURN_TRUE` 前和 fallthrough 路径，互斥执行，无双重释放
- **服务模块 data() `Z_TRY_ADDREF`“多加引用”** — `add_assoc_zval_ex` 不增加引用计数，调用前 addref 是标准做法，正确
- **请求校验辅助函数“retval 未释放”** — retval 为输出参数，所有权归调用方，调用点均正确 dtor
- **各数据库 execute“异常分支 retval 泄漏”** — 所有路径最终都汇聚到统一的 `zval_ptr_dtor(&retval)`，无泄漏
- **依赖注入错误分支 local_params 泄漏** — 两个 E_ERROR return 发生在 local_params 初始化之前，无泄漏
- **缓存流程 cur_version 泄漏** — 相关分支执行时 cur_version 尚未初始化，正确地未 dtor
- **Redis 池 redis_obj 泄漏** — host/port/timeout 检查发生在 redis_obj 创建之前
- **视图模块 use-after-free** — 释放的是 path，后续使用的是 compile_path，无 UAF；但存在 M1 逻辑缺陷

## 三、修复结果（2026-07-13）

### 已修复

1. **H1：ZTS 线程安全**
   - 新增 `GENE_CG_FN_DECL` 声明辅助宏：非 ZTS 使用函数内 static 缓存，ZTS 使用普通局部变量。
   - 将公共函数、请求校验和数据库辅助函数中的裸 static 函数指针缓存统一替换为 `GENE_CG_FN_DECL` + `GENE_CG_FN_LOOKUP`。
   - 覆盖公共函数模块 14 处、请求校验模块 7 处、PDO 辅助函数 1 处，共 22 处。

2. **M1：模板流打开失败**
   - 模板检查流程在 stream 打开失败时释放相关路径、将返回值设为 FALSE 并立即返回。
   - 模板渲染流程在 stream 打开失败时释放相关路径并返回失败状态，不再加载可能过期或不存在的编译文件。

3. **M2：InitPdo 配置校验**
   - MySQL、MSSQL、PostgreSQL 的 dsn/username/password 缺失分支补充对象释放和显式失败返回。
   - SQLite 存在相同 dsn 校验模式，已同步补充对象释放和显式失败返回。

4. **M3：持久化内存分配**
   - 内存缓存销毁和 LRU 销毁流程均增加 `pemalloc` 返回值检查。
   - OOM 时跳过 key 收集，避免 NULL 指针写入导致崩溃；后续仍正常销毁 HashTable。

### 尚未修复

- **M4、L1-L6** 仍属于本轮审计记录的待后续处理项，当前未对其行为进行改动。

## 四、总体评价

代码库整体内存管理质量较好：大量 `[GENE_AUDIT]`/`[GENE_PERF]` 注释表明经过多轮审计修复，`zend_try` 保护 `op_array` 执行、连接池错误路径释放、RSHUTDOWN 上下文清理等均已到位。**未发现会在常规 FPM 请求路径上持续泄漏的高危内存问题**。

本轮已完成 H1、M1、M2、M3 修复。当前 Windows 环境未提供 PHP 扩展编译工具链，尚未执行实际编译验证；建议在 Linux 环境使用 `phpize && ./configure && make`，并至少分别验证非 ZTS 与 ZTS 构建。
