---
name: Audit landing verification
overview: 核查 07-30 / 08-06 两份审计报告的落地情况：绝大多数条目确已落地，但发现 5 个真实缺陷（含 1 个 FPM 悬垂指针、1 个未兑现的 SSRF 加固、1 处连接池计数回退缺失），以及 3 处「半程落地」造成的设计不一致。本计划按严重度分批修复。
todos:
  - id: named-cache
    content: 修复 pool.c / redis_pool.c 命名池 C 层缓存的 FPM 跨请求悬垂：改 pemalloc + persistent HashTable，订正误导性注释
    status: completed
  - id: sendfile-wrapper
    content: response.c sendFile 补 php_stream_locate_url_wrapper 校验，真正拒绝非 plain files wrapper
    status: completed
  - id: rpool-fallback
    content: redis_pool.c rpool_decrement_count 补 cmpset 不可用时的 get→sub 回退路径
    status: completed
  - id: union-separate
    content: 四个 DB 驱动 union() 补 SEPARATE_ARRAY 写时分离，并调换 addref 与 add_next_index_zval 顺序
    status: completed
  - id: di-release
    content: di.c 两个 E_ERROR 早返回路径补 resolved_name release
    status: completed
  - id: router-match
    content: 抽出 gene_router_resolve_leaf 供 match 与 dispatch 共用，补 safe 前缀回退，消除 ctx 副作用
    status: completed
  - id: triple-ttl
    content: gene_memory_get_triple 补 TTL 过期判定，收敛与 gene_memory_get 的语义分叉
    status: completed
  - id: pf1-rpool
    content: 把 PF1 跨界调用合并对称移植到 redis_pool.c 的 put()
    status: completed
  - id: perf-batch
    content: 性能批 P-1~P-5：已写入 audit/plan/PLAN.md §三，受 tools/acceptance profile 准入约束，未取证据前不进主线
    status: completed
  - id: named-cache-gate
    content: named_cache 补 runtime_type >= 2 门禁与持久 key 副本，消除持久表持有请求期对象导致的 UAF
    status: completed
isProject: false
---

# Gene 审计落地情况核查与修复计划

代码基线：`89907c3`（工作树干净）。两份报告第九节声称的落地项，我逐条对照源码复核，**绝大多数属实**。以下只列出「声称已落地但有缺陷」与「核查中新发现」的部分。

## 核查结论速览

已确认正确落地、无需改动的项（不再展开）：`session.c` 静态方法缓存删除（H1）、`gene.c` sweep cooldown + 256 栈分批 + 遥测（M1/L2）、Swoole `defer` 自动 cleanup（F1）、`log.c` rv 槽修正（L4）、`pool.c` CAS 原子递减（C1，实现甚至比 `redis_pool.c` 原版更完整）、`app_stopped` 迁入 per-ctx（8 处检查点齐全、reset 正确）、`slow_query_ms` 默认 0 时真正零开销、`monitor.c` 池分区所有权转移、`view.c` render 失败路径清理。

`cache_expiry` TTL 表这一项我特别核查了「是否破坏 workerReady 后免锁读」：**没有破坏**。惰性删除的守卫条件 `!(runtime_type >= 2 && worker_ready)`（[src/cache/memory.c](src/cache/memory.c) 第 674 行）与 `memory.h` 中 RDLOCK 跳过条件严格互补，两者互斥，不存在免锁状态下写共享表。TTL 表条目的生命周期也与主表同步（del / LRU evict / clean 三个入口全覆盖）。

---

## 第一批：正确性与内存安全（建议立即修）

### 1. 命名连接池 C 层缓存在 FPM 下跨请求悬垂（最高优先级）

[src/db/pool.c](src/db/pool.c) 第 55-76 行与 [src/cache/redis_pool.c](src/cache/redis_pool.c) 第 42-63 行：

```c
static HashTable *gene_pool_named_cache = NULL;
...
ALLOC_HASHTABLE(gene_pool_named_cache);          /* = emalloc，请求堆 */
zend_hash_init(gene_pool_named_cache, 8, NULL, NULL, 0);  /* persistent=0 */
```

`gene_pool_named_cache_clear()` 的调用点只有 `closeAll()`（第 1147 行）和 MSHUTDOWN（第 1628 行），**RSHUTDOWN 不清、指针不置 NULL**。源码注释第 43-45 行断言「In FPM/CLI the cache is freed at request end and rebuilt next request — fine」，但重建的前提是指针被置 NULL，而实际没有。请求 1 结束后 PHP MM 回收该 HashTable，请求 2 的 `gene_pool_named_cache_get()` 直接解引用已释放内存。

触发条件是「多请求 SAPI + swoole 扩展已加载但非 Server 模式」（池构造需要 `Swoole\Coroutine\Channel`，[src/db/pool.c](src/db/pool.c) 第 1256 行）。非 Server 模式下 `runtime_type < 2`，但构造路径本身不受 `runtime_type` 拦截，所以这条路径是可达的。因为 PHP MM 不把内存还给 OS，表现为静默读到陈旧数据而非段错误，这正是它至今未被发现的原因。

修法二选一：
- 改 `pemalloc` + `zend_hash_init(..., /*persistent*/ 1)`，与注释第 53-54 行自己给出的建议一致；
- 或在 RSHUTDOWN 中对 `runtime_type < 2` 调用 `..._clear()`。

推荐前者，两个文件对称改，并同步订正注释。

**08-08 追加**：只做 `pemalloc` 是不够的。表活下来之后，里面存的 `zend_object*`（`pool.c` 第 1418 行 `Z_OBJ_P(return_value)`）和 `zend_string` key 仍然是请求生命周期的，多请求 SAPI 下请求 2 会在 `getInstance()` 命中缓存并对已释放对象 `GC_ADDREF` —— 从「静默读脏数据」升级成确定的 use-after-free。最终落地为三件事：表在持久堆、key 用 `zend_string_init(..., 1)` 复制到持久堆、`..._put()` 入口加 `runtime_type >= 2` 门禁。非 Swoole Server 下表根本不会创建，`getInstance()` 回落到静态 `instances` 属性，行为不变。

### 2. `sendFile` 的 wrapper 拒绝从未实现（安全）

[src/http/response.c](src/http/response.c) 第 811-813 行，注释声称已关闭 SSRF：

```c
/* [GENE_FIX:2026-08-07-5] plain files only (EX_USE_URL / wrappers rejected) to
 * close the SSRF surface when $file is derived from user input. */
php_stream *stream = php_stream_open_wrapper_ex(ZSTR_VAL(file), "rb", REPORT_ERRORS, NULL, NULL);
```

`REPORT_ERRORS` 与 wrapper 无关，代码里没有任何 wrapper 校验。`php://filter`、`data://`，以及 `allow_url_fopen=On` 时的 `http://` 全部可用。08-06 报告 §9.1 把这条记为已落地，实际未落地。

修法：调用前用 `php_stream_locate_url_wrapper()` 判定，非 plain files wrapper 直接 `RETURN_FALSE`。其余部分（8KB 分块、`ssize_t got`、三条错误路径的 `php_stream_close`、offset 越过 EOF 返回 false）均已正确落地。

### 3. RedisPool 计数递减缺回退路径

[src/cache/redis_pool.c](src/cache/redis_pool.c) 第 439 行：

```c
if (!fn_get || !fn_cmpset) return;  /* Swoole\Atomic unavailable */
```

`cmpset` 不可用时**静默不递减**，计数只增不减，池最终永久判定为饱和。而 [src/db/pool.c](src/db/pool.c) 第 557-567 行有 legacy get→sub 兜底。这是与 C1 方向相反的对称性缺口：C1 把 CAS 从 rpool 补到 pool，却没把 pool 的回退补回 rpool。

修法：把 `pool.c` 的回退分支移植到 `rpool_decrement_count`。

### 4. `union()` 就地修改可能被共享的参数数组

[src/db/mysql.c](src/db/mysql.c) 第 1085-1091 行（`pgsql.c` / `mssql.c` / `sqlite.c` 同构）：

```c
zval *data = zend_read_property(..., GENE_DB_MYSQL_DATA, 1, NULL);
ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(sub_data), value) {
    add_next_index_zval(data, value);
    Z_TRY_ADDREF_P(value);
} ZEND_HASH_FOREACH_END();
```

两个问题：`data` 未做写时分离，若 refcount>1（userland 持有同一数组）则是就地改写共享数组；以及 `add_next_index_zval` 先转移所有权、`Z_TRY_ADDREF_P` 后补引用，顺序反了，插入失败即悬垂。引用计数净结果目前正确，但顺序应调换、并补 `SEPARATE_ARRAY`。四个驱动一起改。

### 5. `gene_di_get` 两个 E_ERROR 早返回漏 release

[src/di/di.c](src/di/di.c) 第 211、216 行：`resolved_name`（第 543 行同型逻辑的 `zend_string_copy` 副本）在这两条 `E_ERROR` 返回路径上未 release。E_ERROR 会 bailout、内存随请求回收，属形式缺陷，顺手补齐即可。

---

## 第二批：功能设计的合理性（半程落地导致的不一致）

### 6. `Router::match()` 与 `dispatch` 分叉

F1-3 立项的初衷是「解锁路由单元测试」，但 [src/router/router.c](src/router/router.c) 第 2159-2295 行把键构造逻辑整段复制自 `get_router_content_run`（第 2017-2060 行），而非抽公共函数，已产生两处实质分叉：

- **safe 前缀无回退**：`match` 只从 `GENE_ROUTER_SAFE` 属性读（第 2182 行），而 dispatch 路径在属性为空时会回退到 `app_key` / `app_root`。未显式传 safe 构造的 Router，`match` 与 `dispatch` 会查不同的缓存键，匹配结果不一致 —— 这直接抵消了它作为测试基础设施的价值。
- **有副作用**：`match` 调用 `gene_router_reset_path_params()`（第 2211 行）和 `gene_router_set_uri()`（第 2269 行），会覆盖当前 ctx 的 module / controller / action / path_params。请求处理中途调 `match()` 做预演会污染真实 dispatch 状态。

修法：抽出 `static zval *gene_router_resolve_leaf(method, path, safe_str, safe_len, zval **conf_out)` 供两边共用，safe 回退逻辑一并纳入；`match` 改为在临时状态上执行，或至少在文档中明示副作用。

### 7. `gene_memory_get_triple()` 漏检 TTL

[src/cache/memory.c](src/cache/memory.c) 第 700-718 行完全没有调用 `gene_memory_expired_nolock`，而 `gene_memory_get()`（第 669 行）有。这是 08-07 新增 TTL 功能的漏网点：同一份数据经两条读路径会有不同的 TTL 语义。`get_triple` 目前只服务路由的 `:rt` / `:cf` 键（正常无 TTL），所以现网无影响，但语义分叉应当收敛 —— 加同样的空表短路 + 过期判定即可，无 TTL 部署零额外成本。

### 8. PF1 未对称落地到 RedisPool

[src/cache/redis_pool.c](src/cache/redis_pool.c) 第 1270-1304 行的 `put()` 仍是未合并版本：稳态 2 次跨界、溢出分支 3 次。而 `pool.c` 第 904-933 行已合并到稳态 2 次、溢出 2 次。与 C1 情况相同的「孪生代码只改了一边」。

---

## 第三批：高并发性能（建议先取 profile 证据再动）

当前借还路径的跨界调用实测（静态计数）：DB 池「借 + 还」稳态 3 次（`Channel::pop` + `Atomic::get` + `Channel::push`），理论下限 2 次。

- **P-1** [src/db/pool.c](src/db/pool.c) 第 911 行：正常归还路径的 `Atomic::get` 仅用于 `cur > max` 溢出判断。溢出连接的唯一来源是第 847 行的超时补偿，可改为「先 push，仅当 `db_pool_get_timeout` 计数非零时才做溢出检查」，稳态压到 1 次跨界。这是剩余空间最大的一处。
- **P-2** [src/db/pool.c](src/db/pool.c) 第 852-854 行、[src/cache/redis_pool.c](src/cache/redis_pool.c) 第 1248-1251 行：`php_error_docref` 的实参在 C 里无条件求值，即使 E_NOTICE 被屏蔽也白付 1-2 次 `Atomic::get`。
- **P-3** [src/router/router.c](src/router/router.c) 第 286 行：`get_path_router_init` 在无 prefix / 无 langs 时返回 `str_init(path)`，复制了一份与入参完全相同的字符串，调用方随即 efree 原件。改为返回 `path` 本身、调用方用指针相等判断（该模式在第 2250 行已存在），每请求省 1 次 emalloc + memcpy。
- **P-4** [src/cache/memory.c](src/cache/memory.c) 第 200 行：TTL 表非空时，get 热路径上每次调 `time(NULL)`，可换 `sapi_get_request_time` 或缓存的秒级时间戳。
- **P-5** [src/tool/monitor.c](src/tool/monitor.c) 第 308 行：计数器是 per-worker module globals（非共享内存、非原子 `++`，这在 per-worker 语义下是正确且安全的），但 Prometheus 导出缺 `worker_id` label，多 worker 抓取会剧烈抖动。属可观测性缺陷。

## 关于 ZTS

`pool.c` / `redis_pool.c` 的 named_cache、[src/router/router.c](src/router/router.c) 第 1614 行的 `gene_closure_src_cache`（持久 HT 无锁并发写）、[src/cache/cache.c](src/cache/cache.c) 第 734-739 行的 4 槽 fn LRU（字段撕裂）在 ZTS 下均有真实竞争。因 Swoole 不支持 ZTS，两份报告将其列为观察项，我认同维持该定级 —— 但第 1 项修好后 named_cache 的 ZTS 问题会顺带消失。

## 环境限制

本机 Windows，无法编译 / ASAN / 压测，以上全部为静态确认。第 1、3、4 项的运行时验证需承接 `audit/plan/PLAN.md` 的 O6 清单。