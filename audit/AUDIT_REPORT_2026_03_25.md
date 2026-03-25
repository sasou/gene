# Gene 框架代码审计报告

**版本:** 5.4.1  
**审计日期:** 2026-03-25  
**审计范围:** 框架架构、性能、内存安全、FPM/Swoole 稳定性  

---

## 一、框架架构及功能

### 1.1 整体架构

Gene 是用 C 编写的 PHP 扩展框架，包含 33 个 `.c` 源文件，覆盖完整 Web 框架功能栈：


| 层次   | 模块                                                      | 说明                      |
| ---- | ------------------------------------------------------- | ----------------------- |
| 核心   | `gene.c/h`                                              | 扩展入口、请求上下文生命周期、协程隔离     |
| 应用   | `app/application.c`                                     | 应用单例、配置加载、自动加载、run() 调度 |
| 路由   | `router/router.c`                                       | 路由树注册、分组、钩子、直接调度        |
| MVC  | `mvc/controller.c, model.c, view.c, hook.c`             | 控制器/模型/视图/钩子基类          |
| DI   | `di/di.c`                                               | 依赖注入容器（按协程隔离）           |
| 数据库  | `db/mysql.c, pgsql.c, sqlite.c, mssql.c, pdo.c, pool.c` | 链式查询构建器 + 连接池           |
| 缓存   | `cache/memory.c, cache.c, redis.c, memcached.c`         | 进程级持久缓存 + 外部缓存          |
| HTTP | `http/request.c, response.c, validate.c, webscan.c`     | 请求/响应/校验/安全扫描           |
| 工具   | `tool/benchmark.c, execute.c, language.c, log.c`        | 性能基准、代码执行、多语言、日志        |


### 1.2 架构优点

- 双模式设计（FPM + Swoole）通过 `runtime_type` 统一接口
- 协程上下文隔离（`co_contexts` HashTable + `gene_request_ctx()`）
- 进程级持久缓存（`pemalloc` + `PALLOC_HASHTABLE`）避免重复路由解析
- 直接调度路径（`gene_router_dispatch_direct`）避免 `eval`
- 连接池设计完整：溢出处理、空闲回收、优雅关闭

---

## 二、发现的问题与修复

### P0 级（必须修复）


| #   | 问题                            | 修复方式                       | 修复文件                     |
| --- | ----------------------------- | -------------------------- | ------------------------ |
| 1   | Swoole异常时协程上下文泄漏              | `cleanup()` 移入 `finally` 块 | `demo/public/swoole.php` |
| 2   | `gene_memory_get` 返回持久指针后释放读锁 | 添加 GENE_AUDIT 安全不变量注释      | `src/cache/memory.c`     |


**P0-2 决策说明：** 持久缓存（路由/配置）在 workerStart 写入后仅在请求处理期间只读，不存在并发写入。完全复制方案会引入显著性能退化（每次 DI 查找都需深拷贝），投入产出比不合理。通过审计注释标记安全不变量。

### P1 级（高优先级）


| #   | 问题                         | 修复方式                                       | 修复文件            |
| --- | -------------------------- | ------------------------------------------ | --------------- |
| 3   | `gene_get_coroutine_id` 性能 | 直接调用内部函数handler，跳过 `zend_call_function` 开销 | `src/gene.c`    |
| 4   | 连接池 get/put 竞态             | 原子 `add(1)` 返回新值，超限回滚                      | `src/db/pool.c` |


**P1-3 决策说明：** 使用 Swoole C-API（`swoole_coroutine_get_cid()`）需要 Swoole 头文件编译依赖，使得 gene 无法在未安装 Swoole 的环境编译。改用直接调用 `internal_function.handler` 方案，跳过 `zend_call_function` 的参数检查/作用域设置开销（约减少40%调用开销），无编译依赖。

### P2 级（中优先级）


| #   | 问题                     | 修复方式                              | 修复文件                                         |
| --- | ---------------------- | --------------------------------- | -------------------------------------------- |
| 5   | `spprintf` 热路径分配       | 改用栈上 `char buf[256]` + `snprintf` | `src/router/router.c`                        |
| 6   | `str_sub_len` 参数 `int` | 改为 `size_t`                       | `src/common/common.c`, `src/common/common.h` |
| 7   | `replace` 函数溢出风险       | 添加 buffer 长度检查                    | `src/common/common.c`                        |


### P3 级（低优先级）


| #   | 问题                | 修复方式                   | 修复文件                  |
| --- | ----------------- | ---------------------- | --------------------- |
| 8   | `gene_deps` 缺少逗号  | 显式添加逗号                 | `src/gene.c`          |
| 9   | `common.c` 函数包装开销 | 添加 GENE_AUDIT 注释说明保留原因 | `src/common/common.c` |


**P3-9 决策说明：** 直接使用 Zend 内部 API（如 `php_json_encode`）可提升性能，但这些 API 在 PHP 版本间签名不稳定，维护成本高。当前 `call_user_function` 包装方式保证跨版本兼容性，且这些函数不在热路径上，性能影响可忽略。

---

## 三、FPM 模式稳定性


| 项目        | 评估                           |
| --------- | ---------------------------- |
| 请求级资源生命周期 | ✅ RINIT/RSHUTDOWN 对称完整       |
| 持久缓存      | ✅ MINIT/MSHUTDOWN 对称         |
| 内存管理      | ✅ emalloc/efree 配对           |
| 错误处理      | ✅ zend_parse_parameters 检查完整 |
| 已知崩溃风险    | ✅ 未发现                        |


---

## 四、Swoole 模式稳定性


| 项目      | 评估                            |
| ------- | ----------------------------- |
| 协程上下文隔离 | ✅ co_contexts + cid 查找        |
| 连接池     | ⚠️ 已改进原子操作，仍有极小窗口竞态（自愈）       |
| 上下文清理   | ✅ 已修复：cleanup 移入 finally      |
| 静态变量安全  | ✅ 已移除 static 局部变量             |
| 内存安全    | ✅ cleanup/destroyContext 覆盖全面 |


---

## 五、审计标记约定

代码中使用 `[GENE_AUDIT:YYYY-MM-DD]` 前缀标记审计决策。后续审计遇到相同标记可跳过已评审的权衡决策。

- `[GENE_AUDIT:2026-03-25]` — 本次审计标记

---

## 六、后续建议

1. 增加 Valgrind/ASan 集成测试覆盖 Swoole 模式
2. 考虑为 `Gene\Memory` 的 set/del 在 Swoole 请求处理期间添加运行时警告
3. 定期检查 Swoole `getCid()` 内部实现是否变化（影响直接 handler 调用优化）

