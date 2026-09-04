# Gene 扩展极致并发性能优化清单

> 基于 `src/` 源码静态分析（版本 6.2.0，2026-09-04）。所有结论均给出 `文件:行` 位置以便复核；
> 收益为静态估算，落地前须用 Swoole/FPM 双模式压测（`wrk`/`ab` + `Gene\Monitor::stats()`）验证。
> 涉及生命周期/裸指针的改动必须在 Linux `-fsanitize=address` 下跑全量 `test/*.php` 回归。

## 0. 已有优化基线（不重复建议）

代码中已有大量 `GENE_PERF` / `GENE_MEM` 标记的成果，后续优化应建立在这些基线之上：

| 领域 | 已完成 | 位置 |
|---|---|---|
| 请求上下文 | `gene_request_context` 内联 `path_params`、struct 池复用（`ctx_pool`）、`co_contexts` 冷却式 sweep、`vm_stack` 同协程快路径跳过 `getcid()` | `gene.c:1125-1250` |
| 协程 ID | `dlsym` 直接调用 Swoole C-API `get_current_cid()`（`gene.swoole_getcid_capi=1`） | `gene.c:175-213` |
| 路由 | 可选预编译 dispatch 描述符 `route_pc`（`gene.route_precompile`，默认关） | `router.c` |
| 类查找 | `gene_lookup_class_str` 栈上小写缓冲直查 `EG(class_table)` | `gene.c:259-289` |
| 内部函数 | `GENE_CG_FN_LOOKUP` 缓存 `json_encode` 等函数指针（非 ZTS） | `gene.h:73-90` |
| 进程缓存 | `workerReady()` 冻结 bucket 数组 + 无锁读快路径（`GENE_CACHE_RDLOCK` 条件跳锁） | `memory.h:27-30` |
| 字符串 | `gene_strreplace_fast`、ctx 内缓存 `*_len` 避免 `strlen` | `common.c`、`gene.h:163-170` |

---

## 1. 请求热路径（Router / Dispatch / Request / Response）

### 1.1 【高】`get_router_info_slow` 每请求解析 hook 串 —— 默认打开 `route_precompile`
- **位置**：`router/router.c:1081-1130`
- **现状**：`gene.route_precompile=0` 为默认值，每次 dispatch 都 `emalloc` + `php_strtok_r` 拆分 `hook` 字段、`strcmp("clearBefore")`、多次 `zend_hash_str_find("hook:before"/"hsrc:before"/...)`。
- **方案**：
  1. 在 Linux ASAN + 全量 RouterTest 验证后，将 `route_precompile` 默认置 1（Swoole 且 `workerReady()` 后自动生效）。
  2. 把 hook 解析结果（before/after/clear 位标）和解析出的 `zend_function *`（见 1.2）一并存进 `gene_route_pc`。
- **收益**：每请求省 6–10 次哈希查找 + 1 次 alloc。**风险**：低-中（P3 路径已存在，仅需回归）。

### 1.2 【中】action 双重哈希探测
- **位置**：`router/router.c:465` `zend_hash_str_exists(function_table)` → `factory/factory.c:276` 再 `zend_hash_str_find_ptr` 同一 key。
- **方案**：`gene_factory_call_1` 增加 `zend_function *fn` 入参（或返回未命中状态），dispatch 直接 `find_ptr` 一次并传入；`route_pc` 预编译时直接缓存 `fn`。
- **收益**：每请求省 1 次哈希探测。**风险**：低。

### 1.3 【高（可变路由多）】`chird` 占位子路由线性扫描
- **位置**：`router/router.c:320-351` `get_path_router_inner`
- **现状**：精确段未命中时 `ZEND_HASH_FOREACH` 遍历全部 `chird` 占位子路由并递归，同层 `/:id`、`/:name`、正则路由多时每段 O(children)。
- **方案**：注册期（`bind`）对 `chird` 预排序（静态 → 正则 → 泛型占位）并建立"首字符/类型"分桶索引，命中即 `break`；顺序语义须与现有一致。
- **收益**：深层 REST 路由树匹配从 O(n) 降到近 O(1)。**风险**：中。

### 1.4 【中】URI 解析与 m/c/a 多次堆分配
- **位置**：`router/router.c:233-290`（`str_sub_len`/`str_init`/`php_strtok_r`/`estrdup` ×2）、`router/router.c:124-159` `setMca`（每段 `emalloc`+`efree`）、`http/request.c:78-103`（method/path 各拷贝一次）、`gene.c:603-620` reset 时逐字段 `efree`。
- **现状**：一次请求约 8–10 次小块 emalloc/efree 仅用于持有 method/path/lang/module/controller/action 等短串。
- **方案**：
  1. `ctx` 内为这些字段提供**带容量的内联小缓冲**（如 64/256 B），reset 只置长度为 0 不释放，超长才回退 heap。
  2. URI 解析改为只读指针扫描，仅对最终保留的子串做一次拷贝；`lang` 可记录 `(ptr,len)` 而非 `estrndup`。
  3. 注意所有 `ctx->module != NULL` 的"是否已设置"判断需改为长度/标志位。
- **收益**：Swoole 稳态下请求周期分配器流量再降 ~50%。**风险**：中（生命周期，需 ASAN）。

### 1.5 【高（启用 webscan 时）】Webscan 对象每请求实例化
- **位置**：`app/application.c:418-447`
- **现状**：`object_init_ex` + 7 参数 `__construct` + `check()` + dtor，每请求一次；输入数组还要 `gene_webscan_flatten()` 全量 `smart_str` 拼接后 `preg_match` 两遍（`webscan.c:85-161`）。
- **方案**：`Webscan` 无请求级状态时在 `workerReady()` 预建 per-worker 实例，仅调 `check()`；flatten 增加递归深度/长度上限，短字符串（cookie/referer）直接匹配不拼接。
- **收益**：省一次对象构造与 7 个参数装拆。**风险**：中（需确认 `check()` 无副作用）。

### 1.6 【中】响应输出逐块进入 SAPI 层
- **位置**：`http/response.c:836-839`（`write()` 每次 `php_write`+`sapi_flush`）、`response.c:863-865`（循环 `php_output_discard`）
- **方案**：`write()` 累积到 ctx 级 `smart_str`，在 `end()`/析构时一次输出；Swoole 模式直接调用 `Swoole\Http\Response->end()`。
- **收益**：减少 output 层 handler 检查与 flush 系统调用。**风险**：中。

### 1.7 【低】零散 `strlen` / 未缓存函数指针
- `router/router.c:2147/2222/2478` `zend_read_property(..., strlen(GENE_ROUTER_SAFE))` → `ZEND_STRL`。
- `factory/factory.c:213-219` 已算 `action_len` 却又 `ZVAL_STRING` 重算 → `ZVAL_STRINGL`。
- `http/json.c:74/103` `Gene\Json::encode/decode` 每次 `zend_hash_str_find_ptr(CG(function_table))` → 复用 `GENE_CG_FN_LOOKUP` 或直接调 `gene_json_encode`。
- `di/di.c:143-165` alias 链最多 8 跳每跳一次 `zend_hash_find` → `Di::alias()` 注册时解析到最终目标。
- `gene.c:410-435` `gene_get_router_uri` 最多 4 次 alloc → 单次 `smart_str` 生成后 `ZVAL_STR`。

---

## 2. 视图 / 自动加载

### 2.1 【高（模板重）】模板编译 28 次 `php_pcre_replace`
- **位置**：`mvc/view.c:623-627`
- **现状**：`gene.view_compile=0` 时每请求完整走 28 轮正则替换。
- **方案**：生产强制 `gene.view_compile=1` + `gene.view_compile_check_mtime=0`；Swoole 下在 `workerReady()` 预编译热点模板到 worker 级缓存。文档需明确。
- **收益**：模板渲染 CPU 下降一个量级。**风险**：低。

### 2.2 【中】视图渲染用 `php_output_start_default` 捕获
- **位置**：`mvc/view.c:832-859`
- **方案**：嵌套子视图共享一层 buffer，或改为直接捕获到 `smart_str`。**风险**：低-中。

### 2.3 【中（FPM 启动阶段）】自动加载路径构造与重复 stat
- **位置**：`factory/load.c:131-165`（`estrdup`+`replaceAll`+`snprintf`）、`load.c:83-91`（`VCWD_STAT` + `zend_compile_file`）
- **方案**：`gene_load_import` 先查 `EG(included_files)` 短路；`workerReady()` 预 include 控制器/模型目录；路径拼接改 `memcpy`。**风险**：低。

---

## 3. 进程缓存（Gene\Memory / Gene\Cache）

### 3.1 【高】框架缓存与业务缓存共用一张表、一把锁
- **位置**：`memory.h:27-30`、`gene.h:296` `cache_business_dirty`
- **现状**：任何一次 `Gene\Cache` 业务写将 `cache_business_dirty=1`，此后**所有**读（含路由、配置、DI 等框架元数据）永久回到 `rwlock` 路径。
- **方案**：拆为两张 `HashTable` + 两把锁：框架表 `workerReady()` 后真正 write-once、永久无锁；业务表独立加锁/LRU/TTL。进阶可对业务表做 RCU（写者复制替换表指针，读者读快照）。
- **收益**：消除高并发读的串行点，路由/配置读取回到无锁。**风险**：中（RCU 旧表回收需防 UAF）。

### 3.2 【中】`Memory::get()` 无差别深拷贝
- **位置**：`memory.c:1299` → `memory.c:396-458` `gene_memory_zval_local_copy`
- **方案**：`IS_LONG/IS_DOUBLE/IS_NULL/IS_BOOL` 直接 `ZVAL_COPY_VALUE`；persistent/interned 字符串走 `ZVAL_STR` + 不可变标志；大数组可提供明确不逃逸约定的 `getBorrowed()`（内部使用）。**风险**：中（误用跨协程 UAF，需严格约定）。

### 3.3 【中】`mget()` 逐 key 加读锁
- **位置**：`memory.c:1788-1824`
- **方案**：一次 `GENE_CACHE_RDLOCK()` 内遍历全部 key 裸查 `zend_symtable_str_find`，结束后一次解锁；锁内禁止 yield。**收益**：N 次锁 → 1 次。**风险**：低。

### 3.4 【中】缓存键生成
- **位置**：`cache.c:564-619` `gene_cache_key`
- **现状**：每次拼接 `sign+class+method+args+ttl` 后 MD5/xxHash 再分配 `zend_string`。
- **方案**：构造期预生成 `(sign,class,method)` 前缀 `zend_string`，运行时仅追加 args；默认哈希改 `xxHash64`/`TurboHash32`（`common.c:973/1315`）避免 MD5 hex 转换。**风险**：低。

### 3.5 【低】TTL 采样清理与非原子计数
- `memory.c:560-585`：每 32 次写扫 64 项，写少读多时过期 key 堆积 → 暴露 `gene.cache_expiry_sweep_*` 配置；Swoole 可加 `Timer::tick` 后台 sweep。
- `memory.c:1298` 等 `GENE_G(x)++` 计数器：仅监控用途，可接受；如需精确改 `GENE_ATOMIC_INC` 宏。

### 3.6 【低】FPM 非 ZTS 下读锁无意义
- FPM worker 单线程，`worker_ready=0` 时每次读仍 `pthread_rwlock_rdlock`。`#ifndef ZTS && runtime_type<2` 可直接跳锁。**风险**：中（ZTS 必须保留）。

---

## 4. 数据库（Pool / PDO / 驱动）

### 4.1 【P0】池化连接未设置 `ATTR_DEFAULT_FETCH_MODE`
- **位置**：`db/pool.c:271-276` 只设了 ERRMODE / EMULATE_PREPARES / PERSISTENT；非池路径 `db/mysql.c:267-268` 设了 `19 => 2 (FETCH_ASSOC)`。
- **后果**：池连接落到 PDO 默认 `FETCH_BOTH`，每行结果双键（数字+字符串），内存与 `zend_string` 分配翻倍。
- **方案**：`pool_normalize_config()` 加 `add_index_long(&z, 19, 2)`。**收益**：高。**风险**：低（一行修复，行为与非池一致）。

### 4.2 【P0】FPM 模式完全不走连接池 / 持久连接
- **位置**：`db/pool.c:1562` `runtime_type < 2` 直接返回 0；`db/mysql.c:215-290` 每请求 `PDO::__construct`。
- **方案**：FPM 下默认 `PDO::ATTR_PERSISTENT=true`（index 12），借 `EG(persistent_list)` 复用连接；释放前调用现有 `gene_db_tx_hygiene` 回滚未提交事务。**收益**：每请求省 TCP 握手 + 认证。**风险**：中（会话变量残留）。

### 4.3 【P0】每次 PDO/PDOStatement 调用重新查 `function_table`
- **位置**：`db/pdo.c` 共 12 处 `zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object|statement)->function_table, ...)`（exec/prepare/execute/fetch/fetchAll/rowCount/lastInsertId 等）。
- **方案**：PDO 与 PDOStatement 是内部类，`zend_function *` 进程内稳定。MINIT 后按 `ce` 首次解析并缓存到静态结构体（ZTS 下按 `GENE_CG_FN_LOOKUP` 同样策略处理），之后直接 `zend_call_known_function`。**收益**：每条 SQL 省 3–5 次哈希查找。**风险**：低。

### 4.4 【P0】ORM 通过 `call_user_function` 调用 Db 方法
- **位置**：`orm/meta.c:341-351` `gene_orm_db_call`（`ZVAL_STRING(&fname)` + `call_user_function`），被 `orm/model.c`、`orm/query.c` 数十处调用。
- **方案**：按 `db->ce` 缓存常用方法（select/where/limit/row/all/cell/lastId/affectedRows/...）的 `zend_function *`，改用 `zend_call_known_function`。**收益**：简单查询省 4–6 次方法名分配与解析。**风险**：低-中。

### 4.5 【P1】预处理语句不复用
- **位置**：`db/pdo.c:856-866`、`db/mysql.c:349/365`
- **现状**：每条查询 `prepare` 新建 `PDOStatement` → `execute` → 释放。
- **方案**：每个连接维护 `sql → PDOStatement` 小型 LRU（如 64 条），执行前重绑参数；DDL/`SET` 语句触发失效；连接归还池时保留。**收益**：OLTP 场景省一次 DB 往返 + 对象创建。**风险**：高（失效与并发安全）。

### 4.6 【P1】SQL 片段全部经 PHP 对象属性中转
- **位置**：`db/mysql.c:158-173`（reset 11 个属性）、`mysql.c:301-343`（execute 读 9 个属性）；四个驱动同构。
- **方案**：链式构建期在 C 侧对象结构体中直接维护 `smart_str`/字段，只在 `execute` 时组装；对外 API 不变。**收益**：每条 SQL 省 10+ 次 `zend_read/update_property`。**风险**：中。

### 4.7 【P1】健康检查与回收
- `db/pool.c:368-404` 探活用 `getAttribute(ATTR_SERVER_INFO)`，借出时不探活，死连接靠业务 SQL 抛错后重连（`mysql.c:356-369`）。→ 提供 `ping_on_get` 可选配置；空闲超过阈值才探活。
- `db/pool.c:720-804` 每个 Pool 一个 `Timer::tick`，逐连接 `pop/getAttribute/push` → 单 worker 合并为一个全局 timer。

### 4.8 【P1（长期）】四驱动文件近乎逐函数复制
- `mysql.c/sqlite.c/pgsql.c/mssql.c` 各 ~60 KB，仅引号字符与少量方言方法不同（`pdo.c:1269-1285` `makeWhere` 甚至靠 `strstr(class_name,"Pgsql")` 判断引号）。
- **方案**：抽象 `gene_db_dialect { oq, cq, callbacks }`，四文件只注册方言。这是 4.3/4.5/4.6 能集中落地的前提。**风险**：中（重构回归）。

### 4.9 【高（开发模式）】SQL 历史 JSON 编码
- `db/pdo.c:1153-1171`、`mysql.c:175-212`：`run_environment=0` 时每条 SQL `json_encode` 参数入历史。生产必须 `gene.run_environment>=1`；文档强调。

### 4.10 其他
- `orm/meta.c:147-236` 请求级元数据命中后仍 `zend_string_copy` 5 个字段 → 出借指针 + `from_cache` 标志。
- `orm/model.c:763-797` `findMany(preserveOrder)` 多次 `zval_get_string` → 预归一化 ids。
- `mvc/model.c:82-122`、`service/service.c:81-122` `__get/__set` 每次类名 + DI 查找 → 缓存 `zend_string *` 类名。
- `orm/query.c:124-140/185+` ops 数组 push + 线性 apply → C 侧链表或直接生成 SQL。

---

## 5. Redis / Memcached / Session / Log / 工具

### 5.1 【中】RedisPool 计数走 `Swoole\Atomic` PHP 方法
- **位置**：`cache/redis_pool.c:453-491/580-594`；`db/pool.c:519-590` 同构。
- **现状**：每次 `get()/put()` 1–3 次 `zend_call_known_function` 操作 Atomic。
- **方案**：Pool 对象 per-worker，同 worker 协程单线程调度，计数可直接用结构体内 `zend_long`；仅跨 worker 共享统计才需 Atomic。**风险**：中（需禁用 clone/序列化）。

### 5.2 【中】RedisPool `get()` 1 ms 轮询忙等
- **位置**：`cache/redis_pool.c:1232-1310`（`pop(0.001)` × `max+2` 次重试）
- **方案**：首次非阻塞 pop 未命中 → 直接以 `waitTimeout` 阻塞 pop；`workerStart` 预建 `min` 个连接。**收益**：降 CPU 与 p99。

### 5.3 【中】序列化走 PHP `serialize()/unserialize()`
- **位置**：`common/common.c:779-793`、`cache/redis.c:496-515`、`memcached.c:369`（getMulti 逐元素 unserialize）
- **方案**：直接调 `php_var_serialize/php_var_unserialize`（按 `PHP_VERSION_ID` shim）；支持 igbinary 时直接链接其 C 函数。**风险**：中。

### 5.4 【中】Memcached 驱动每次查方法指针 + 无池
- `cache/memcached.c:115-173`：缓存 `zend_function *`，增加 `MemcachedPool` 复用连接。

### 5.5 【中】Session 每次 `set()/del()` 立即 dirty + 重发 cookie
- **位置**：`session/session.c:1049-1051`、`session.c:497`
- **方案**：写合并 —— 请求内只更新内存数组，`save()`/析构时一次写 handler、一次发 cookie；ID 生成（`session.c:334-375` `gettimeofday+snprintf+MD5`）改 `php_random_bytes` + `gene_u64_to_hex`。**风险**：中（崩溃丢写，需析构兜底）。

### 5.6 【高（日志密集）】Log 每条走 JSON + `error_log()` PHP 调用
- **位置**：`tool/log.c:177-257`
- **方案**：级别过滤前移到任何分配之前；`context` 为空跳过 JSON；本地文件路径用 C `write()` 追加（可选 `gene.log_buffer` 批量刷）；秒级时间字符串缓存。**收益**：日志吞吐 5–10×。**风险**：低-中（缓冲丢尾）。

### 5.7 【低】Benchmark / Monitor / Crypto
- `tool/benchmark.c:69-115`：`start()/end()` 调 PHP `memory_get_peak_usage` → 用 `zend_memory_peak_usage()` C API，计时改 `gene_hrtime()`。
- `tool/monitor.c:43-187`：`stats()` 逐池调 PHP `stats()` → 各池导出 C 结构体计数器。
- `tool/crypto.c:67-222`：`base64/hash_hmac/random_bytes/bin2hex` 走 PHP 函数 → 直接 OpenSSL EVP / `php_base64_encode` / `php_random_bytes`。

---

## 6. 通用 / 横切

### 6.1 【高】JSON 编解码走 PHP 调用帧
- **位置**：`common/common.c:749-762`
- **方案**：`php_json_encode()` / `php_json_decode_ex()` 直接调用（按 `PHP_VERSION_ID` 处理签名差异）。**收益**：JSON API 每次省约 0.5 µs 调用帧。**风险**：中（多版本兼容）。

### 6.2 【高】编译优化标志缺失
- **位置**：`config.m4`（仅探测 `clock_gettime`）、`config.w32`（仅 `/I` `/utf-8`）
- **方案**：
  ```m4
  PHP_ARG_ENABLE(gene-opt, whether to enable aggressive optimizations, [...], no)
  if test "$PHP_GENE_OPT" != "no"; then
    CFLAGS="$CFLAGS -O3 -fvisibility=hidden -fno-plt"
    # 可选：-flto，-march=native（仅同机部署）
  fi
  ```
  Windows：`ADD_FLAG("CFLAGS_GENE", "/O2 /GL"); ADD_FLAG("LDFLAGS_GENE", "/LTCG")`。
  配合 `PHP_GENE_API` 已有的 `visibility("default")`，内部 helper 加 `static`。
- **收益**：中-高。**风险**：中（LTO 构建时间、`-march=native` 不可移植）。

### 6.3 【中】`gene_preg_match` 与 Validate 未缓存编译正则
- **位置**：`common/common.c`（`gene_preg_match`）、`http/validate.c:1172`、`validate.c:781-970`（每条规则回调 PHP）
- **方案**：借 `pcre_get_compiled_regex_cache()`（PHP 自带缓存）确保走缓存路径；内置规则在 C 层实现避免 `gene_factory_call`。

### 6.4 【中】ZTS 下 `GENE_G()` 热路径访问
- 热函数内把 `GENE_G(runtime_type)` 等只读值读入局部变量一次；已有 `ZEND_ENABLE_STATIC_TSRMLS_CACHE=1`。

### 6.5 【中】HTTP 客户端 FPM 下每次 `curl_easy_init`
- **位置**：`http/http.c:1359-1407`；multi 模式（`http.c:1838-1950`）全部经 PHP `curl_*` 函数。
- **方案**：FPM 也按 `host:port:ssl` 复用 `CurlHandle`（ctx 级），开 `CURLOPT_TCP_KEEPALIVE`；有 libcurl 头时直接调 `curl_multi_*` C API。

---

## 7. 生产推荐 php.ini（Swoole 模式）

当前默认值偏保守（兼容优先），高并发环境建议：

```ini
gene.runtime_type          = 2
gene.run_environment       = 2        ; 关闭 SQL 历史/benchmark 采集
gene.use_namespace         = 1
gene.view_compile          = 1
gene.view_compile_check_mtime = 0
gene.route_precompile      = 1        ; 需先在 Linux ASAN + RouterTest 验证
gene.swoole_getcid_capi    = 1
gene.swoole_auto_cleanup   = 1        ; 防漏调 cleanup() 导致 co_contexts 堆积
gene.co_contexts_max       = 8192     ; ≈ 单 worker 峰值协程数
gene.ctx_pool_max          = 512
gene.ctx_pool_prewarm      = 512
gene.cache_max_items       = 10000
gene.cache_reserve         = 12560    ; ≥ max_items + max(64, max_items/4)
gene.slow_query_ms         = 200      ; 可选，用于定位慢 SQL
```

运行期观测：`Gene\Monitor::stats()` 中 `cache_insert_refused`、`db_pool_get_timeout`、
`redis_pool_cas_abandoned`、`co_contexts_sweep_count`、`ctx_pool_miss` 持续增长即为配置不足信号。

---

## 8. 落地顺序建议

| 阶段 | 项目 | 依据 |
|---|---|---|
| **第一批（低风险高收益，可立即做）** | 4.1 池连接 `FETCH_ASSOC`；4.3 PDO 函数指针缓存；4.4 ORM `zend_call_known_function`；3.3 `mget` 单次加锁；1.7 零散 `ZEND_STRL`/fn 缓存；5.6 Log 级别前置过滤；7 推荐配置写入文档 | 改动局部、无生命周期风险 |
| **第二批（中风险）** | 1.1 `route_precompile` 默认开 + hook 预编译；1.2 action 单次查找；4.2 FPM 持久连接；5.1 Pool 计数 C 化；5.5 Session 写合并；6.1 JSON 直调；6.2 编译标志 | 需回归测试与多版本兼容 |
| **第三批（架构级）** | 3.1 框架/业务缓存拆表；1.3 路由 `chird` 索引；1.4 ctx 内联缓冲；4.8 驱动抽象 → 4.5 语句缓存、4.6 C 侧 SQL 构建；1.6 响应批量输出 | 收益最大但需 ASAN + Swoole 全量验收 |

## 9. 验证方法

1. 微基准：`php test/BenchmarkTest.php`；对比修改前后 `Gene\Benchmark` 输出。
2. 压测：Swoole 模式 `wrk -t8 -c1024 -d60s`，关注 RPS 与 p99；FPM 模式同参数对照。
3. 内存/安全：Linux `CFLAGS="-fsanitize=address -O1" phpize && make`，跑全量 `test/*.php` 与 `tools/verify_5_6_6_swoole.php`。
4. 观测：压测期间轮询 `Gene\Monitor::stats()` / `Gene\Memory::stats()`，确认拒绝/超时/sweep 计数为 0。
