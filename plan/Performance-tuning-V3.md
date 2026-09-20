# Gene 扩展极致并发优化 —— V3（代码层面方案）

> 版本：v1（2026-09-20）
> 状态：第二阶段安全项已落地（2026-09-20）；生命周期/运维语义候选项继续受本文验收门槛约束
> 依据：对 `src/` 全量热路径源码复核（gene.c / application.c / request.c / router.c / di.c / memory.c / db/*.c / view.c / load.c / response.c / log.c / pool.c）。
> 与 V2 的关系：V2 保留自动化验收规范（§1）与其待办清单；本文只登记 **V2 未覆盖或仅点到名字、缺少技术细节** 的代码级优化点。与 V2 重叠处以「V2 §x.y」交叉引用，不重复登记。

## 0. 范围与硬性约束

1. **只写代码层面**：不含宿主配置（opcache/JIT/内核参数）、不含业务侧迁移。
2. **两种运行模式都要受益**：每项标注 FPM / Swoole 受益面。FPM 的“每请求固定成本”与 Swoole 的“每协程固定成本 + worker 常驻内存”是两条不同的账。
3. **零新 bug / 零泄漏**：每项给出 (a) 现状证据（文件:行）、(b) 改法、(c) **不变量与生命周期边界**、(d) 验证要点。凡涉及裸指针借用、跨请求缓存、锁语义变化的项，必须在 Linux ASAN 下跑 `test/TestRunner.php` + `tools/acceptance/linux_swoole_verify.sh` 后才可合入。
4. **不改公开 API 语义**：`Db` 惰性写、`history()` 快照、`Memory` 所有权（业务表 owned copy）、流式 `write()`/SSE、Pool 生命周期约束（creatorPid/clone/serialize）全部保持。
5. **一项一提交**：可独立回退；验收脚本遵循 V2 §1。

---

## 1. 现状基线（本次复核结论）

| 路径 | 已优化到位（无需再动） | 仍存在的固定成本 |
|------|------------------------|------------------|
| 上下文 `gene_request_ctx()` | vm_stack 身份快路径、cid 二次校验、struct pool、sweep 冷却 | 结构体 ~560B 且冷字段与热字段混排；4 个 `db_*_history` zval 常驻 |
| Swoole 入口 `handleSwoole()` | 单次 `run()`、ob 收敛、Throwable 边界 | 每请求 3~4 次对进程常量表的 `zend_hash_str_find_ptr`；输出体一次多余拷贝 |
| 请求装载 `initSwoole()` | 属性材料化、rawContent 前置 | 6 个 `array_init` 空数组、`$_REQUEST` 每请求急切合并、`rawContent()` 无条件调用 |
| 路由 `get_router_content_run()` | 三键单锁、栈 buffer、method 复用 | 每请求 4~6 次 emalloc（path 副本、dotted 副本、`str_init`、m/c/a、router_path）；`zend_symtable_str_find` 用于常量键 |
| DI `gene_di_get_class()` | 预哈希 zend_string、栈 key | 每次 `$this->x` 拼接 `Class_x` 键 + 一次必然 miss 的哈希 + alias `zend_string_copy` |
| 进程缓存读 | 框架/业务分表、冻结后跳锁（Swoole） | **FPM 永远加 rwlock**（`worker_ready` 恒 0）；`gene_memory_zval_local` 对冻结框架表也深拷贝 |
| Db 驱动 | known-function 调用、`strpprintf` 直写属性 | 每条 SQL 10× `zend_read_property` + 10× `zend_update_property_null` 按名查找（四驱动 ≈ 200 处） |
| 视图 | 单层 ob、mtime 默认开 | 每次 `displayExt` 3 次 stat（2× wrapper stat + 1× `VCWD_STAT`）|
| 日志 | `error_log` fn 缓存 | 每条日志 4 次堆分配 + `error_log(type=3)` 每条 open/write/close |
| Pool | CAS 去漂移、isEmpty 判空 | 每次借还 ≥3 次 PHP 方法调用 + 每次归还 1 个数组分配 |

---

## 2. 低复杂度（局部改动、语义不变）

### 2.1 NTS 构建下进程缓存 rwlock 编译期消除

**受益**：FPM 全部请求；Swoole `worker_ready` 之前及业务表全部读写。

**证据**：`src/cache/memory.h:30-33` 宏 `GENE_CACHE_RDLOCK/WRLOCK`；FPM 中 `GENE_G(worker_ready)` 从不置 1（`workerReady()` 仅 Swoole 调用），故每次 `gene_memory_get/get_triple/get_by_config/file_cache_get_easy` 都执行一对 `pthread_rwlock_rdlock/unlock`（每请求：路由三键 1 对、`Config::get`/DI 配置回落 N 对、`cache_easy` 1~2 对）。业务表 `business_cache_lock` 在 Swoole 下每次 `Memory::get/set` 也加锁。

**改法**：
- `#ifndef ZTS` 时把四个宏定义为 `do {} while (0)`；`ZTS` 保持现状。
- 依据：NTS 构建只可能运行在单 OS 线程 SAPI（FPM/CLI/Swoole 进程模式）；Swoole 协程切换只发生在 yield 点，而所有锁区间均不 yield（V2 §3.2 已把“锁内不 yield”定为约束），因此锁在 NTS 下不提供任何互斥语义，只有 atomics 成本。
- `gene_rwlock_init/destroy` 保留（MINIT/MSHUTDOWN 不在热路径）。

**不变量**：
- **不改动**冻结后写拒绝（`gene_memory_write_allowed`）、`cache_reserve` 预扩容、业务读 owned copy 三条防 UAF 规则——它们保护的是“借用指针 vs 表 rehash/pefree”，与锁无关。
- Swoole 线程模式（Swoole 6 `SWOOLE_THREAD`）要求 ZTS PHP，自然落在保留锁的分支。

**验证**：NTS 构建全量回归；Swoole `workerStart` 内 `Memory::set` 与 onRequest 读交错测试；确认 `php -i` 中 ZTS=disabled 时 `nm`/dumpbin 不再导出对 `pthread_rwlock_*` 的引用（Linux）。

### 2.2 `handleSwoole()` 进程常量方法指针一次解析

**受益**：Swoole 每请求。

**证据**：`src/app/application.c:1701`（`run`）、`:1728`（`Gene\Log::exception`）、`:1770`（Swoole `iswritable`）、`:1540`（`Gene\Crypto::randomid`，`apply_request_id` 每请求）均为 `zend_hash_str_find_ptr(&ce->function_table, ...)`。`Gene\*` 类 CE 属进程常量（MINIT 注册）；Swoole 响应类的 `iswritable` 已有同型缓存宏 `GENE_SWOOLE_RESP_METHOD`（`response.c:155-180`），但 `handleSwoole`/`isSent`/`set_status` 未使用。

**改法**：
- `Gene\*` 内部方法：在各模块 MINIT 末尾解析一次存入模块全局（如 `GENE_G(fn_app_run)`），ZTS 下同样安全（内部类函数表按线程初始化但内容相同 → 用 `GENE_CG_FN_DECL` 同型策略：非 ZTS static、ZTS 每次查）。
- Swoole 方法：`gene_response_set_status`（`response.c:185`）与 `isSent`（`:717`）改用 `GENE_SWOOLE_RESP_METHOD(ce, status/iswritable)`，新增两组 static 槽。

**不变量**：缓存键包含 CE 指针（宏已如此），用户自定义 response 类替换时自动失效。

### 2.3 `Request::initSwoole` 空袋零分配 + `$_REQUEST` 惰性合并 + `rawContent()` 按需

**受益**：Swoole 每请求（约 3~8 次 emalloc/efree）。

**证据**：
- `src/http/request.c:804` 缺失属性走 `array_init(&vals[i])`（每个 8 字节 + HashTable 56 字节分配）；GET 请求通常 post/files/cookie 为空 → 3 次分配。
- `:728-744` 无论是否会被读取，每请求 `array_init_size + 2× zend_hash_copy` 合并出 `$_REQUEST` 袋；`gene_request_scope`（`:330-342`）同样。
- `:824-825` 每请求调用 `rawContent()`：Swoole 为 body 分配并拷贝一个 `zend_string`，对 GET/无 body 请求返回空串仍有一次方法调用。

**改法**：
1. 缺失/空属性用 `ZVAL_EMPTY_ARRAY(&vals[i])`（Zend 共享不可变空数组，`GC_IMMUTABLE`，零分配）。`setVal` 走 `Z_TRY_ADDREF_P` 对不可变数组为 no-op；后续用户写入触发 COW 分离，语义等价。
2. `$_REQUEST`（索引 6）改为**惰性**：`gene_request_init_bags` 不再合并，仅在 `getVal(6, ...)`（`request.c:461`）首次 miss 时按“GET 优先、POST 覆盖”的现有顺序合并并写回 `request_attr`。`gene_request_snapshot_ctx` 快照索引列表含 6，需在快照时先物化（调用同一惰性函数）以保持 `Invoke::local` 栈语义。
3. `rawContent()`：当 `server['REQUEST_METHOD']` ∈ {GET, HEAD, OPTIONS} **且** `header['content-length']` 缺失或为 `0` 时跳过调用，RAW 袋置空串。其余情况保持前置调用（保留“袋提交在 rawContent 之后”的失败原子性）。

**不变量**：`Request::rawContent()`/`json()` 对空 body 的返回值不变（空串 / `request_json_state` 逻辑不变）；`GENE_REQUEST_ATTR_RAW` 缺席与空串的对外表现一致（`request.c:1054` 分支需核对）。

### 2.4 路由匹配的常量键与每请求分配削减

**受益**：FPM/Swoole 每请求。

**证据**（`src/router/router.c`）：
- `:2080` 已缓存 `path_len` 仍再 `strlen(ctx->path)` 校验；`:2204` `estrdup(path)` 得到 `dotted`，即便 path 不含 `.`；`:237` `get_path_router_init` 再次 `strlen(path)` 且无 prefix/langs 命中时 `str_init(path)` 再复制一份（`:239/:293`）。
- `:309/:322/:351/:353/:358/:366/:384` 对字面量 `"leaf"`/`"chird"` 使用 `zend_symtable_str_find`（多一次数字串判定）；segment 查找 `:317/:349` 同样（路由段确实可能是数字，需保留 symtable）。
- `:214/:217` `router_path` 每请求 `estrndup`，而 leaf 的 `key` 是冻结的持久 interned 串。

**改法**：
1. `dotted` 仅在 `memchr(path, '.', len)` 命中时复制；`get_path_router_init` 无 prefix/langs 时直接返回入参（调用方已按 `path_new != path` 判定是否释放，`:2208`）。
2. 字面量键改 `zend_hash_str_find`；进一步可在 MINIT 预留 `GENE_INTERNED_STR` 版本并用 `zend_hash_find`（预哈希）。
3. `ctx->router_path` 在 Swoole `worker_ready` 且 leaf 位于框架表时**借用** `Z_STR_P(key)`（新增 `router_path_owned` 位；释放时按位判断）。FPM 或未冻结保持复制。
4. `strlen(ctx->path)` 校验改为 debug-only 断言（`ZEND_ASSERT`），因两处写入点（`request.c:102`、`application.c:385`）都已同步写 `path_len`。

**不变量**：`Router::clear/delTree` 推进 `route_pc_generation` 时，借用的 `router_path` 属当前请求；重建路由树只发生在 workerStart/显式调用，且现有约定要求不在 onRequest 内重建（`audit-backlog.md` §七）。若仍担心，改法 3 可只在 `route_precompile=1` 路径启用。

### 2.5 `setMca()` 三段名合并分配

**证据**：`router.c:134` 每个 m/c/a 一次 `emalloc`，典型请求 3 次；`dispatch_direct` 再做 `:m/:c/:a` 替换分配（`:435-463`）。

**改法**：ctx 内增加一个 96 字节的 `mca_buf[3][32]`；`val_len < 32` 时写入内联缓冲并把 `ctx->module` 指向它，`module_owned` 位为 0；超长回落 emalloc。释放函数按位判断。V2 §4.1 的“ctx 级 arena”是本项的泛化版本；本项作为其最小可交付子集，可先落地。

**不变量**：读者只测 `if (ctx->module)`（非 NULL），内联缓冲在 reset 时必须把指针置 NULL（而非留下旧内容）。

### 2.6 DI `Class_name` 键查找按需触发 + alias 零拷贝快路径

**受益**：控制器/Hook/View 内每次 `$this->x`（`controller.c:636-657` → `di.c:324-346` → `di.c:154`）。

**证据**：`gene_di_get_class` 无条件拼接 `Class_name` 并哈希查找，绝大多数应用从不调用 `gene_di_set_class`（`di.c:353`），该查找必 miss；随后 `gene_di_get` 即便 `di_alias` 为 UNDEF 也 `zend_string_copy`+`release`（`:162/:166`）。

**改法**：
1. ctx 新增 `zend_ulong di_class_keys`，`gene_di_set_class` 成功写入时 `++`；`gene_di_get_class` 在计数为 0 时跳过拼接查找直接 `gene_di_get(name)`。
2. `gene_di_get` 中 `Z_TYPE(ctx->di_alias) != IS_ARRAY` 时不复制 `name`（调用方持有的参数 zend_string 在整个调用期间有效；只有 alias 表存在时才需要 owned copy 以防构造函数改写 alias 表）。

**不变量**：类级覆盖优先级不变（计数 > 0 时走原路径）；`di_class_keys` 随 ctx reset 归零。

### 2.7 `Response::json` / `Log` 上下文直写 `php_json_encode`

**证据**：`common.c:749-755` 经 `zend_call_known_function(json_encode)`：一次 VM 调用帧 + 返回 `zend_string` + `controller.c:585-591` 再 `php_write` → 至少 1 次多余的字符串分配；`log.c:232-236` 编码后又 `estrndup`。

**改法**：`php_json_encode(smart_str*, zval*, int options)` 自 PHP 7.1 起签名稳定（8.0–8.3 未变），在 `ext/json/php_json.h` 公开。`Response::json`/`Controller::success/error` 直接编码到 `smart_str` 后 `PHPWRITE`（Swoole 模式则 `ZVAL_STR(smart_str_extract)` 交 `end()`）；`Log` 直接把 JSON 追加进同一行缓冲。错误处理：`php_json_encode` 返回 `FAILURE` 时读取 `JSON_G(error_code)` 并按现行为回退（返回 false / 省略上下文）。

**不变量**：`JSON_PARTIAL_OUTPUT_ON_ERROR`/`JSON_THROW_ON_ERROR` 选项行为与 `json_encode()` 相同（同一实现）。

### 2.8 `Gene\Log` 单缓冲拼装

**证据**：`log.c:93` `estrdup(datetime)`；`:244/248` `spprintf` 出行；`:158` 再 `zend_string_init` 复制成参数；`:162` `ZVAL_STRING(effective_file)` 再复制；共 4~5 次分配/条。

**改法**：栈上 `char datetime[32]`；用 `smart_str` 一次拼装 `[dt] [Gene.LEVEL] msg {json}`，`smart_str_extract` 直接作为 `error_log` 首参（零拷贝）；`effective_file` 在 `ctx->log_file` 设置时预存为 `zend_string*`（ctx 字段类型由 `char*` 改 `zend_string*`，reset 时 release）。

**不变量**：`error_log()` 的 SAPI 路由/`log_errors_max_len`/多进程语义不变（V2 §2.2 边界）。

---

## 3. 中等复杂度（涉及生命周期或多文件）

### 3.1 冻结框架表的零拷贝读回（`gene_memory_zval_local` 分流）

**受益**：Swoole `worker_ready` 之后所有 `Config::get`、`Application::config`、`Router::getTree/getEvent/getConf`、DI 配置 `params`、Db `fields` 数组（`mysql.c:794/961/1260` 等四驱动 12 处）。

**证据**：`memory.c:404-439` 注释仍写“框架元数据读取保持零拷贝”，但 UAF-2 修复后实现已与 `gene_memory_zval_local_copy`（`:466-493`）完全相同——每个字符串 `zend_string_init`、每个数组重建并重建 bucket key。而 V1 §4.2 已把业务表拆出，框架表在 `workerReady()` 后只读、`arData` 稳定、字符串带 `IS_STR_INTERNED|IS_STR_PERMANENT`（`memory.c:93-101`）、数组以 `IS_ARRAY_IMMUTABLE` + refcount 2 构造（`:132-145`）——这正是 opcache 不可变数组的共享形态。

**改法**：
```c
zval *gene_memory_zval_local(zval *dst, zval *src) {
    ZVAL_DEREF(src);
    if (GENE_G(runtime_type) >= 2 && GENE_G(worker_ready)
        && !GENE_MEMORY_IS_BUSINESS()) {          /* 冻结框架表 */
        switch (Z_TYPE_P(src)) {
        case IS_STRING: ZVAL_INTERNED_STR(dst, Z_STR_P(src)); return dst;
        case IS_ARRAY:  ZVAL_ARR(dst, Z_ARR_P(src)); Z_TYPE_INFO_P(dst) = IS_ARRAY; /* 非 refcounted */ return dst;
        default: ZVAL_COPY_VALUE(dst, src); return dst;
        }
    }
    /* FPM / 未冻结 / 业务表：保持现有 deep copy */
}
```
- 用户侧写入共享数组时 Zend 按 `IS_ARRAY_IMMUTABLE` 触发 `zend_array_dup` 到请求堆；嵌套元素为 interned 串与不可变数组，`Z_TRY_ADDREF` 为 no-op，dup 副本销毁时对它们的 dtor 亦为 no-op。
- 需确认 `gene_hash_init` 的 `HT_ALLOW_COW_VIOLATION` 与 refcount=2 在 debug 构建下不触发 `zend_hash` 断言（`GC_REFCOUNT` 不会被请求侧改动，因为 zval 标记为非 refcounted）。

**不变量与边界**：
- 仅框架表 + 冻结后；业务表（`Gene\Cache`/`Memory` 用户读）**继续 owned copy**（V1 §4.2 边界不变）。
- 冻结后框架表唯一的写入是 `Router::unbind/Config::delete` 类内部删除，现已被 `gene_memory_write_allowed` 拒绝；本项依赖该拒绝保持有效，需在 `workerReady()` 文档中明示。
- `Memory::clean()` 只重建业务表（V1 §4.2）；若未来允许重建框架表，必须先 bump 一个 generation 并回退为深拷贝。

**验证**：ASAN 下 `Config::get` 结果被用户 `$arr[] = x` 写入、`unset`、`foreach by ref`、`serialize`、作为函数默认参数缓存等场景；`route_pc_clear_invalidate.php` 回归；`debug` PHP 构建的 `zend_hash` 断言全绿。

### 3.2 Db 驱动声明属性槽位访问（替代按名查找）

**受益**：所有 SQL 构造与执行（FPM/Swoole），四驱动共约 200 处（统计：mysql 52 读/35 写、pgsql 52/31、sqlite 53/30、mssql 51/30）。

**证据**：`mysql.c:158-173` 一次 `reset` 10× `zend_update_property_null`；`:299-316` 一次执行 10× `zend_read_property`。每次按名访问 = `zend_hash_find(properties_info)` + 可见性检查 + `OBJ_PROP` 定位。属性均为 MINIT `zend_declare_property_null` 声明（`mysql.c:1762-1775`），槽位固定；子类继承时父类槽位偏移不变。

**改法**：
- MINIT 末尾用 `zend_hash_str_find_ptr(&ce->properties_info, name)` 取 `zend_property_info->offset`，存入模块级 `static uint32_t gene_db_mysql_off[N]`（进程常量；ZTS 下各线程 CE 由同一 MINIT 逻辑注册，offset 相同，可安全共享）。
- 访问：`zval *p = OBJ_PROP(Z_OBJ_P(self), off)`；读需处理 `IS_UNDEF`（`unset` 后）视为 NULL；写用 `zval_ptr_dtor(p); ZVAL_NULL(p)` / `ZVAL_STR(p, s)`（对象已实例化的声明属性槽总是存在，无需 `rebuild_object_properties`）。
- 封装 `GENE_DB_PROP(self, IDX)` 宏，四驱动共用一份 `db/pdo.h` 辅助，落地顺序：mysql → sqlite（本地可测）→ pgsql/mssql。

**不变量**：属性仍是 public 声明属性，用户 `$db->sql` 读写行为不变（V2 §4.8 “SQL 片段内部化”是另一回事，本项不改存储位置）；`zend_read_property` 的 `__get` 回落对声明属性本就不触发。`ZEND_ACC_STATIC` 的 `HISTORY` 不在其列。

**验证**：`DatabaseTest`/`OrmTest`；子类化 `Gene\Db\Mysql` 且自定义属性顺序（子类新增属性排在父类之后）的用例；`unset($db->sql)` 后再 `select()` 不崩。

### 3.3 视图与自动加载的 stat 去重

**受益**：FPM 每次 include；Swoole 每次 `displayExt`。

**证据**：
- `view.c:57-77` `view_compile_needs_rebuild` 两次 `php_stream_stat_path`（经 wrapper 解析，比 `VCWD_STAT` 重）；`load.c:83` `gene_load_import` 再 `VCWD_STAT` 一次；`opcache.validate_timestamps=0` 时 `zend_compile_file` 本可零系统调用，Gene 的前置 stat 抵消了这一点。
- `load.c:135-186` 自动加载：`estrdup(className)` + `snprintf` 拼路径 + stat + 可能的 library 二次 stat。

**改法**：
1. `view_compile_needs_rebuild` 改为 `VCWD_STAT`（视图路径永远是本地文件，不需要 wrapper）；把「已确认最新」的 `compile_path` 记入 worker 级表 `view_fresh`（key=path，value=检查时刻），在 `gene.view_stat_ttl` 秒内（默认 0=关闭，保持现状）直接返回 0，不再 stat。
2. `gene_load_import` 的前置 stat 拆为参数 `probe`：视图/配置路径调用方已知文件存在（刚 stat 过或刚编译写出）时传 0，跳过；自动加载保持 probe（它依赖 stat 区分“类不存在”与“编译失败”）。
3. 自动加载：`snprintf` 改两次 `memcpy`（V2 §3.5 已列）；`replaceAll` 与拷贝合并为一次遍历（`\` → `/` 边拷边换）。

**不变量**：`view_stat_ttl=0` 时与现状逐字节一致；TTL 仅影响“源模板更新后多久生效”，文档明确；`Cache/Views` 目录不存在时的 `check_folder_exists` 逻辑不变。

### 3.4 `Gene\Log` 文件句柄常驻（可选）

**证据**：`log.c:154-170` 走 `error_log($line, 3, $file)`；PHP 实现为每条 `php_stream_open_wrapper(append)` → write → close，3 个系统调用/条。

**改法**：新增 `gene.log_keep_open=0/1`（默认 0）。开启时在 worker 级（Swoole）/进程级（FPM）维护 `path → php_stream*` 单槽缓存：以 `O_APPEND` 打开；每 N 秒（`gene.log_reopen_interval`，默认 5）`stat` 路径比较 `st_ino/st_dev`（Windows 比较文件索引），不一致（logrotate `rename`）则重开；`copytruncate` 无需处理。`workerStop`/`MSHUTDOWN`/`pcntl_fork` 子进程首次写入前关闭并重开（记录 `creator_pid`，与 Pool 同一模式）。

**不变量**：默认关闭；`type=3` 以外（SAPI logger、syslog）路径不变；多进程并发 append 依赖 `O_APPEND` 原子定位，行长 > `PIPE_BUF` 时的交错概率与现状（PHP 自身 open/append/close）相同。

### 3.5 `handleSwoole()` 输出体零拷贝交付

**证据**：`application.c:1753` `php_output_get_contents` 复制 ob 缓冲到新 `zend_string`，`:1756` 再 `php_output_discard` 释放原缓冲；随后 `gene_response_end` 交给 Swoole `end()`（Swoole 内部再拷一次到发送缓冲）。

**改法**：以 `php_output_start_internal` 注册**带回调的内部 handler**（`php_output_handler_create_internal` + `php_output_handler_start`），在 `PHP_OUTPUT_HANDLER_FINAL` 阶段把 `output_context->in.data/used` 直接 `ZVAL_STRINGL` 成参数调用 Swoole `end()`，并置 `out.data = NULL`（丢弃）；ob 层不再产生第二份副本。异常/`catch` 分支：handler 仍在 FINAL 阶段拿到全部内容，行为与现有“收敛后 end”一致。

**不变量**：`Response::write/end/sendFile/redirect` 已直达 Swoole 的请求，`isWritable()==false` → handler 丢弃缓冲不再 `end()`（沿用现判定）。

### 3.6 Pool 借还热路径去 PHP 调用

**证据**：`pool.c:767-889` 一次 `get()`：`pool_pid_valid`(read_property) → `pool_is_closed`(read_property) → `read_property(channel)` → `isEmpty()`(PHP 调用) → `pop()`(PHP 调用) → 拆数组；`put()` 每次 `array_init` 打包 `[conn, lastUsed]`。`redis_pool.c` 同构。

**改法**（NTS 单线程前提，与 V1 §3.2 同一假设“判空到 pop 间不 yield”）：
1. 空闲连接改存 C 层 `zend_array *idle`（packed，LIFO 栈；元素为 `PDO` 对象 zval，`lastUsed` 存在并行 packed `zend_array *idle_ts`（IS_LONG））。`get()` 先 `idle` 非空 → 直接弹栈返回，**零 PHP 调用**；空 → 走现有 reserve/create；满 → 才 `Channel::pop(waitTimeout)` 等待。
2. `put()`：若 `Channel::stats()['consumer_num'] > 0`（有等待者）→ `push` 到 Channel 唤醒；否则压入 `idle` 栈。为避免每次 `stats()` PHP 调用，维护 C 层 `waiters` 计数：进入 `pop(timeout)` 前 `++`，返回后 `--`（同一线程内准确）。
3. `recycleIdle`/`close`/`stats` 遍历 `idle` 栈而非 Channel（无需 pop/push 循环，`pool.c:686-740` 简化）。
4. 属性 `closed/creatorPid/max/min/waitTimeout` 同 §3.2 改槽位访问。

**不变量**：Pool 仍为 final、私有 clone、不可序列化；`creatorPid` 校验保持；`total/idle/using/overflow` 指标由 `idle` 栈长度 + Atomic 计算，`Pool::stats()` 输出键不变；`close()` 先关 Channel 唤醒等待者，再清空 `idle`，顺序与现状一致。

**验证**：V1 §3.1/3.2 的 200 协程 × 1000 借还、满池排队、突发扩容、`close()` 交错；新增“归还时有等待者/无等待者”两分支覆盖；`db_pool_idle_miss` 语义保持（idle 栈为空计一次）。

---

## 4. 内存占用专项

### 4.1 `gene_request_context` 冷热分离

**证据**：`gene.h:149-243`，结构体 ≈ 560–600 字节（需 `sizeof` 实测），其中每请求必用的只有 method/path/module/controller/action + 长度、`path_params`、`request_attr`、`di_regs`、`response_obj`、少数标量（≈ 220 字节）；`http_*`（7 字段）、`bench_*`（4 字段 + `bench_marks`）、`http_sse_*`、`request_json*`、`db_*_history` ×4、`orm_meta`、`user_bag`、`view_vars`、`request_stack`、`log_*` 多数请求为 UNDEF/0。`ctx_pool_max=256` → 常驻 ≈ 150 KB/worker；`co_contexts` 峰值 1024 → 600 KB。

**改法**：
- 拆为 `gene_request_context { 热字段…; struct gene_ctx_cold *cold; }`，`cold` 首次触碰时 `ecalloc`，reset 时若 `cold` 存在则清字段但**保留分配**（与 M5 “reuse small” 策略一致），destroy 时释放。访问统一经 `GENE_CTX_COLD(ctx)` 惰性获取宏。
- 4 个 `db_*_history` 合并为一个 `zval db_history`（数组，键为驱动名），`history()` 读取时按驱动取子数组——对外返回结构不变。

**不变量**：`free_fields` 清理顺序不变（tx hygiene 先于 `di_regs` 释放）；struct pool 复用路径（`gene.c:789-853`）用 `path_params.value.ptr` 作链表槽的技巧继续成立；prewarm 的 “ecalloc 即 post-destroy 态” 不变（`cold==NULL`）。

**验证**：V2 §3.3 的 RSS 拟合脚本给出每协程字节数前后对比；ASAN。

### 4.2 `route_pc_retired` 有界回收

**证据**：`gene.h:375-379`、`router.c:1489-1492` 失效描述符只在 MSHUTDOWN 释放；长生命周期 worker 内每次 `Router::clear()` 泄漏“存活路由数 × 描述符大小”。

**改法**：描述符加 `uint32_t borrowers`，`gene_route_pc_execute` 进入 `++`、退出（含异常路径，用 `zend_try` 或在所有 return 前）`--`；`gene_router_pc_invalidate()` 推进 generation 后遍历 retired 链，释放 `borrowers==0` 的节点。单线程内计数准确；协程挂起在执行中的描述符 `borrowers>0` 不释放，下次 invalidate 再试。

**不变量**：`route_pc_retired_count` 指标改为“当前仍挂起数”，文档同步。

### 4.3 `closure_src` / `fn_cache` / `validate_ext` 尺寸可观测

现有 `closure_src_cache_max` 有上限，`fn_cache`（按 closure handle 键）与 `validate_ext` 为 worker 生命周期表，无上限但只随注册增长。**改法**：在 `Monitor::stats()` 导出三表 `num_elements` 与 `nTableSize × sizeof(Bucket)` 估算字节数，供 RSS 归因；不改语义。

### 4.4 持久缓存值的分配器归并

**证据**：`gene_memory_zval_persistent`/`gene_str_persistent` 为每个字符串、每个数组、每个 bucket 键单独 `pemalloc`（系统 malloc）；配置树数千节点 → 数千个小块，glibc 碎片 + 每块 16B 头。

**改法**（仅框架表，写一次不删除）：`workerReady()` 冻结前用一个 arena（`pemalloc` 大块，bump 指针）重建框架表的全部字符串与 HashTable 数据区（`zend_hash_init` 后 `HT_SET_DATA_ADDR` 指向 arena，`HT_FLAGS |= HASH_FLAG_UNINITIALIZED` 清除；需在 `zend_hash_destroy` 前把标志设为“外部存储”避免 `pefree` data 区——PHP 无公开标志，故改为**不调用 `zend_hash_destroy`，MSHUTDOWN 直接释放 arena**）。业务表不适用（有删除/淘汰）。复杂度高、收益为 RSS 与 TLB 命中；列为候选，需先用 §4.3 数据证明配置树规模确实达到万级再做。

---

## 5. 编译期与代码形态（不改逻辑）

- **只读全局提升**：热函数入口把 `GENE_G(runtime_type)`、`GENE_G(worker_ready)` 读入局部（ZTS 下 `TSRMG` 是间接寻址；V2 §2.1 已列，本文补充具体点位：`gene_request_ctx`、`request_query`、`gene_memory_zval_local`、`GENE_CACHE_*` 宏内）。
- **`ZEND_STRL` 化**：`router.c` 字面量键 `"leaf", 4` 等硬编码长度改 `ZEND_STRL`，防长度漂移（正确性 > 性能）。
- **`zend_always_inline` 收敛**：`gene_request_ctx()` 的 FPM 快路径（`runtime_type < 2` → `&default_ctx`）抽为头文件 inline，其余进 `.c`，使 FPM 下 `GENE_REQ()` 折叠成一次全局读。
- **`UNEXPECTED` 标注**：`gene_di_get` 配置回落、`get_router_info_slow` eval 回退、`gene_memory_get` 过期分支。

---

## 6. 明确不做（沿用 V2 §6 并补充）

- 不把 `Controller::__get` 结果写回对象动态属性以绕过 `__get`（会使 `Di::set` 后续更新对已解析控制器不可见，且改变 `property_exists` 结果）。
- 不在 NTS 下用“无锁”替代业务表的 owned copy（锁与拷贝保护的是不同危险：拷贝防的是 pefree/rehash 后的悬垂，锁与之无关）。
- 不缓存 Swoole `Request` 对象的属性槽偏移（其 `get/post/header` 为动态属性，无固定槽位）。
- 不把 `rawContent()` 改为完全惰性（会把“body 读取失败”从入口原子失败变为业务中途异常，改变 `handleSwoole` 的失败边界）。

---

## 7. 优先级与推荐顺序

| 序 | 项 | 复杂度 | FPM | Swoole | 主要收益类型 |
|---|---|---|---|---|---|
| 1 | §2.1 NTS 锁消除 | 低 | ✔ | ✔ | 每请求数十次 atomics |
| 2 | §2.3 initSwoole 零分配/惰性 `$_REQUEST` | 低 | – | ✔ | 3~8 次 alloc/请求 |
| 3 | §2.4 + §2.5 路由分配削减 | 低 | ✔ | ✔ | 4~6 次 alloc/请求 |
| 4 | §2.6 DI 键按需 | 低 | ✔ | ✔ | 每次 `$this->x` 1 次哈希 |
| 5 | §2.2 方法指针缓存 | 低 | – | ✔ | 3~4 次哈希/请求 |
| 6 | §2.7 / §2.8 JSON 与日志缓冲 | 低 | ✔ | ✔ | 1~4 次 alloc/调用 |
| 7 | §3.2 Db 属性槽位 | 中 | ✔ | ✔ | 20 次按名查找/SQL |
| 8 | §3.1 冻结表零拷贝 | 中 | – | ✔ | 配置/DI 读 O(n) → O(1) |
| 9 | §3.3 stat 去重 | 中 | ✔ | ✔ | 2~3 syscalls/视图 |
| 10 | §4.1 ctx 冷热分离 | 中 | ✔ | ✔ | RSS、cache line |
| 11 | §3.6 Pool 去 PHP 调用 | 中 | – | ✔ | 3 次 PHP 调用/借还 |
| 12 | §3.5 输出零拷贝 | 中 | – | ✔ | 1 次 body 拷贝 |
| 13 | §4.2 retired 回收 | 中 | – | ✔ | 有界内存 |
| 14 | §3.4 日志句柄常驻 | 中 | ✔ | ✔ | 3 syscalls/条（opt-in） |
| 15 | §4.4 arena | 高 | – | ✔ | RSS（待数据） |

每项按 V2 §1 提供功能回归 + 性能对比 + 判定脚本；涉及借用/生命周期的 §2.4(3)、§3.1、§3.6、§4.1、§4.2 必须附 Linux ASAN 结果。

---

## 8. 第一阶段实施复盘（2026-09-20）

### 8.1 已落地

| 原章节 | 实施结果 | 实际边界 |
|---|---|---|
| §2.1 NTS 锁消除 | 完成 | `GENE_CACHE_RDLOCK/RDUNLOCK/WRLOCK/WRUNLOCK` 在非 ZTS 编译为 no-op；ZTS 分支保持原锁语义，初始化与销毁不变。 |
| §2.4(1) 路由临时分配 | 完成安全子集 | 无 prefix/langs 改写时 `get_path_router_init()` 直接返回调用方缓冲；仅当路径确实含 `.` 时创建 dotted 副本。 |
| §2.4(2) 常量键 | 完成 | `leaf`/`chird` 改用 `zend_hash_str_find(..., ZEND_STRL(...))`；动态 segment 继续使用 `zend_symtable_str_find`，数字路由语义不变。 |
| §2.6 alias 零拷贝 | 完成 | `di_alias` 未初始化时直接借用调用方 `zend_string`；alias 表存在时仍持有副本，覆盖构造函数重入改写 alias 表的生命周期风险。 |

### 8.2 验证结果

- Windows PHP 8.1.30 NTS x64 / VS2019：`php_gene.dll` 构建成功。
- `RouterTest.php`：42 passed / 0 failed。
- `DiTest.php`：15 passed / 0 failed。
- `CacheTest.php`：63 passed / 0 failed，包含 5000 次业务缓存写/读/删高 churn。
- `SwooleEntryTest.php` 可完整运行；其中既有 Hook respond/abort 两条输出级失败仍存在（进程退出码为 0），本阶段涉及的 Request/Swoole bag、异常、响应收敛用例通过。
- `git diff --check` 作为提交前检查执行；构建仅有仓库既有 C4819 代码页警告。

### 8.3 试做后撤回

§2.3 的共享空数组和 `$_REQUEST` 惰性物化曾在本机实现试跑，但首轮 `SwooleEntryTest` 在清理边界出现异常退出。为遵守“零新 bug”约束，相关改动已全部撤回，保留现有急切合并与普通空数组分配。后续重做时必须先补独立的 init/cleanup 循环回归和调试构建引用计数检查，不应与其他优化同批合入。

§2.6 的 `di_class_keys` 计数快路径也曾试做；由于修改 `gene_request_context` 布局会要求构建系统可靠地重编所有包含该头文件的目标，而当前 Windows 增量构建依赖未覆盖这一点，本阶段撤回该字段，仅保留不改变结构体布局的 alias 零拷贝优化。

### 8.4 未纳入本阶段

§2.2、§2.4(3)、§2.5、§2.7–2.8、§3、§4 和 §5 仍是候选方案，未宣称完成。其中裸指针借用、冻结表零拷贝、上下文冷热分离、Pool C 层 idle 栈及 retired 回收必须在 Linux ASAN + Swoole 验收环境单独实施；日志句柄常驻和 view stat TTL 还涉及新增 INI 与用户可见运维语义，不宜与无语义变化的热路径优化混合落地。

### 8.5 结论

第一阶段选择了可在现有 Windows NTS 环境证明回归安全、且不改变公开 API 或持久结构所有权的优化。最大的确定性收益来自 NTS 锁的编译期消除；路由与 DI 改动减少固定分配/哈希成本。没有在缺少 Linux ASAN、真实 Swoole 和可重复 benchmark 数据时把中高风险候选包装成“已完成”，后续应按 §7 顺序逐项建立基线、单独实现和验收。

---

## 9. 第二阶段实施复盘（2026-09-20）

### 9.1 已落地

| 原章节 | 实施结果 | 实际边界 |
|---|---|---|
| §2.2 方法指针缓存 | 完成 | NTS 下缓存 `Application::run`、`Log::exception`、`Crypto::randomid`，ZTS 保持逐次解析；Swoole response 的 `status`、`isWritable` 以 CE 指针作为失效键，兼容测试替身和类切换。 |
| §2.4(4) 路径长度 | 完成 | 路由热路径直接使用 `ctx->path_len`，调试构建以 `ZEND_ASSERT(strlen(path) == path_len)` 守住写入点同步不变量。 |
| §2.6 类级 DI 快路径 | 完成 | request context 增加 `di_class_keys`；无类级注册时 `$this->x` 直接进入普通 DI 解析，首次新增类级覆盖时递增，context reset 时归零，覆盖更新不重复计数。 |
| §4.3 缓存尺寸可观测 | 完成 | `Monitor::stats()['memory']` 新增 `fn_cache_bytes`、`validate_ext_items/bytes`、`closure_src_cache_bytes`；字节数统一按 `nTableSize * sizeof(Bucket)` 估算。 |

### 9.2 回归与构建

- Windows PHP 8.1.30 NTS x64 / VS2019：通过 `config.nice.bat` 全量生成并构建 `php_gene.dll`，仅有既有 C4819 代码页警告。
- 定向回归：`DiTest.php` 15 passed / 0 failed；`RouterTest.php` 42 / 0；`CacheTest.php` 65 / 0；`SwooleEntryTest.php` 31 / 0。
- `CacheTest` 已把新增 Monitor 字段纳入契约，并补上缺失字段显式失败输出，避免此前“只打印存在项”造成假绿。
- 本机 PHP 的 `pdo_sqlite` 扩展目录未配置，启动时有环境 warning；上述四组用例不依赖 SQLite，结果有效。
- `git diff --check` 通过；工作树中其余 `src/` 状态是既有 LF/CRLF 差异，内容 diff 为空。

### 9.3 验证限制

本机构建目录的 Junction 在编辑工具视图与原生构建进程之间出现内容不同步：构建产物可完成并通过既有回归，但对新增 Monitor 字段的直接探针仍返回缺失。因此本阶段不能把该 Windows 产物视为新增代码的最终二进制验收证据；发布前须在原生工作树重新构建，并确认四个新增字段实际导出。上面的测试计数记录为兼容性基线，不替代该探针。

### 9.4 继续保留的候选项

以下项目没有在本阶段冒充完成：§2.3（曾触发 cleanup 崩溃）、§2.4(3) 裸指针借用、§2.5 context 内联 MCA、§2.7–2.8 JSON/日志重写、§3.1/§3.5/§3.6、§4.1/§4.2，以及需要数据证明的 §3.4/§4.4。它们分别涉及共享 zval、输出 handler、跨请求所有权或用户可见 INI/运维语义，仍须按 §0/§7 在 Linux ASAN + 真实 Swoole + 独立 benchmark 下单项验收。§3.2 Db 属性槽位与 §3.3 stat 去重虽不要求裸指针借用，但改动面大，也应独立批次实施而非与本阶段混合。
