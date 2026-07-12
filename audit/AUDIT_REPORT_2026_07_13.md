# Gene 扩展框架静态审计报告

**日期**: 2026-07-13 | **方式**: 纯静态代码审查 | **范围**: `src/` 全部 34 个 .c 文件

## 一、确认的问题

### 高风险

| # | 位置 | 问题 | 说明 |
|---|------|------|------|
| H1 | <ref_snippet file="F:\github_code\gene\src\common\common.c" lines="731-810" /> 及 1410-1470 共14处 | **ZTS 线程安全隐患**：`static zend_function *fn` 缓存 `CG(function_table)` 查找结果 | 项目在 `gene.h:70-79` 已定义 ZTS 安全的 `GENE_CG_FN_LOOKUP` 宏，且 `log.c`/`benchmark.c` 已使用，但 `common.c` 和 `validate.c`(174-273) 中的同类函数（`gene_json_encode`、`gene_serialize`、`gene_vsprintf`、`gene_preg_match` 等）仍用裸 static 缓存。ZTS 构建下跨线程共享 per-thread 指针，可能崩溃。非 ZTS 构建无影响 |

### 中风险

| # | 位置 | 问题 | 说明 |
|---|------|------|------|
| M1 | <ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="302-324" /> 及 452-474 | **模板流打开失败后缺少 return** | `stream == NULL` 时仅发 E_WARNING 并释放 path，随后仍执行 `ZVAL_STRING(ret, compile_path)` 返回（可能不存在或过期的）编译文件路径，导致后续 include 失败或渲染旧模板。非内存错误（用的是 compile_path 而非已释放的 path），但属逻辑缺陷 |
| M2 | mysql.c:198-206、mssql.c:196-206、pgsql.c:207-215 InitPdo | **配置项缺失时 E_ERROR 后无 return** | dsn/username/password 缺失时 `php_error_docref(E_ERROR)` 依赖 bailout 终止；若被错误处理器/`zend_try` 拦截语义变化，将 NULL 解引用。建议显式 `return` |
| M3 | <ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="128-134" /> 及 487-492 | **pemalloc 返回值未检查** | 持久化分配（malloc 系）OOM 时返回 NULL，后续写 `keys[key_count++]` 崩溃。emalloc 会 bailout 但 pemalloc 不会 |
| M4 | <ref_snippet file="F:\github_code\gene\src\common\common.c" lines="31-75" /> | **字符串工具函数无 NULL/越界防护** | `str_init`/`str_sub_len`/`str_append` 不检查 NULL 入参；`str_sub_len` 不校验 `start` 是否超出源串长度，依赖调用方保证，属脆弱 API |

### 低风险

| # | 位置 | 问题 |
|---|------|------|
| L1 | <ref_snippet file="F:\github_code\gene\src\db\pool.c" lines="55-76" /> | `gene_pool_named_cache` 全局 HashTable 无 MSHUTDOWN 清理，依赖 `closeAll()`；worker 异常退出时由 PHP MM 兜底（代码注释已承认） |
| L2 | pdo.c:997 `checkPdoError` | `zend_read_property` 的 `rv` 输出槽未 dtor；message 通常为已存在的字符串属性不产生临时值，仅在魔术 getter 场景泄漏 |
| L3 | memcached.c:243、292 | `servers` 非数组时 E_ERROR 前 `obj_object` 未释放（bailout 后由请求关停回收，实际影响小） |
| L4 | router.c:305 `get_path_router_inner` | 原地临时改写输入字符串（`*next_slash='\0'` 后恢复），当前调用安全但对常量串会崩溃 |
| L5 | common.c:224 `insertAll` / common.c:365 `replace_string` | 前者覆盖传入 dest 指针（当前调用方传 NULL，惯例脆弱）；后者 `php_strtok_r` 破坏输入串后失败路径无法恢复 |
| L6 | router.c:219-281 `get_path_router_init` | 返回值可能是新分配内存或原 `path` 指针，靠调用方 `path_new != path` 判断释放（router.c:2061 处理正确），所有权约定脆弱，易在新调用点引入双重释放/泄漏 |

## 二、复核后排除的误报（子代理初报但经源码核实无问题）

- **configs.c del/clear "双重释放"** — 两个 `efree` 分别处于 `RETURN_TRUE` 前和 fallthrough 路径，互斥执行，无双重释放
- **service.c data() Z_TRY_ADDREF "多加引用"** — `add_assoc_zval_ex` 不增加引用计数，调用前 addref 是标准做法，正确
- **validate.c 各辅助函数 "retval 未释放"** — retval 为输出参数，所有权归调用方，调用点（如 1197-1227）均正确 dtor
- **mysql/mssql/pgsql/sqlite execute "异常分支 retval 泄漏"** — 所有路径最终都汇聚到统一的 `zval_ptr_dtor(&retval)`（mysql.c:307），无泄漏
- **di.c "错误分支 local_params 泄漏"** — 两个 E_ERROR return 发生在 `local_params` 初始化（di.c:175）之前，无泄漏
- **cache.c:1344 "cur_version 泄漏"** — 该分支时 `cur_version` 尚未初始化（1350 行才初始化），正确地未 dtor
- **redis_pool.c:196 "redis_obj 泄漏"** — host/port/timeout 检查（164行）在 `redis_obj` 创建（171行）之前
- **view.c "use-after-free"** — 释放的是 `path`，后续使用的是 `compile_path`，无 UAF（但存在 M1 逻辑缺陷）

## 三、总体评价

代码库整体内存管理质量较好：大量 `[GENE_AUDIT]`/`[GENE_PERF]` 注释表明经过多轮审计修复，`zend_try` 保护 `op_array` 执行、连接池错误路径释放、RSHUTDOWN 上下文清理等均已到位。**未发现会在常规 FPM 请求路径上持续泄漏的高危内存问题**。

建议修复优先级：
1. **H1** — 将 `common.c`/`validate.c` 的 static fn 缓存统一替换为已有的 `GENE_CG_FN_LOOKUP` 宏（机械替换，风险低）
2. **M1** — view.c 两处 stream 失败分支补 return / 错误传播
3. **M2** — 三个 InitPdo 的 E_ERROR 后补显式 return
4. **M3** — memory.c 两处 pemalloc 补 NULL 检查
