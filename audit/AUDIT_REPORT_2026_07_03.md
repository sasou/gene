# Gene 扩展全量代码审计报告（2026-07-03）

> 审计版本：5.6.7（develop 分支）
> 审计范围：src/ 全部模块（core / router / mvc / http / session / cache / db / tool / factory / di / config）
> 审计方法：源码级静态审计 + 与历史审计报告（AUDIT_REPORT_2026_06_21.md 等）交叉比对；关键发现均经人工二次核实源码确认，排除误报
> 审计重点：bug 漏洞、逻辑错误、内存泄漏、性能提升路径

---

## 一、确认的 Bug / 漏洞（含修复方法）

### 1.1 [P1] pool.c：`pool_in_coroutine()` 使用 `"getCid"` 大小写错误 —— 协程检测恒失效

- 位置：`src/db/pool.c:97-106`
- 状态：**历史审计（2026-06-21 发现 1）已指出，当前代码仍未修复**（已核实）

```c
fn_getcid = zend_hash_str_find_ptr(&co_ce_cached->function_table, ZEND_STRL("getCid"));
if (!fn_getcid) return 0;
```

**问题**：PHP 类的 `function_table` 键以**全小写**存储（`"getcid"`），`zend_hash_str_find_ptr` 做精确字节比较，`"getCid"` 查找必然失败 → `pool_in_coroutine()` 在 Swoole 模式下**恒返回 0**。后果：DB 连接池无法识别协程上下文，协程模式下连接借还路径退化/行为异常。

**修复**（与 `redis_pool.c:690` 的 `rpool_in_coroutine()` 对齐，同时获得 P1 dlsym C-API 直调收益 ~300ns→~5ns）：

```c
static bool pool_in_coroutine(void)
{
    if (GENE_G(runtime_type) < 2) {
        return 0;
    }
    return gene_get_coroutine_id() >= 0;
}
```

### 1.2 [P1] validate.c：`key` 为 NULL 时仅告警不返回 —— 后续解引用未初始化 zval，可致段错误

- 位置：`src/http/validate.c:436-450`（skipOnEmpty）、`551-553`（__call）、`605-607`（msg）、`983-985`
- 对照正确实现：`filter()`（477-480）、`required()`（279-282）告警后立即 return

```c
key = zend_read_property(..., GENE_VALIDATE_KEY, ...);
if (key && Z_TYPE_P(key) == IS_NULL) {
    php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
}                                      /* ← 缺少 return */
...
keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key));  /* IS_NULL zval 的 value 未初始化 → 读到垃圾指针 */
```

**问题**：用户未先调用 `name()` 时，`key` 为 IS_NULL，其 `value` union 未初始化；`Z_STRVAL_P(key)` / `Z_STRLEN_P(key)` 读取垃圾指针/长度，轻则读越界，重则 SIGSEGV。属**用户可稳定触发的崩溃路径**。

**修复**：四处统一改为与 `filter()` 一致的守卫：

```c
if (!key || Z_TYPE_P(key) != IS_STRING) {
    php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
    RETURN_ZVAL(self, 1, 0);
}
```

### 1.3 [P1→安全加固] pdo.c：identifier quote 函数为空实现 —— 表名/列名零转义

- 位置：`src/db/pdo.c:31-49`（已核实为空实现）

```c
void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq) {
    smart_str_appendl(dest, name, len);   /* 未加引号、未转义 */
}
char *gene_quote_table(const char *name, char oq, char cq)  { return str_init(name); }
char *gene_quote_columns(const char *name, char oq, char cq){ return str_init(name); }
char *gene_quote_order(const char *name, char oq, char cq)  { return str_init(name); }
```

**问题**：mysql/pgsql/sqlite/mssql 四驱动的 `select/insert/update/order` 均调用这些函数，但它们不做任何引号包裹与转义。表名/列名/order 字段若来自用户输入（如前端传排序字段），即构成 **SQL 注入面**。数据值走 `?` 预处理是安全的，但标识符不是。

**修复**：实现真实的 quote（各驱动传入正确的 oq/cq，如 MySQL 为 `` ` ``）：

```c
void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq) {
    size_t i;
    smart_str_appendc(dest, oq);
    for (i = 0; i < len; i++) {
        if (name[i] == cq) smart_str_appendc(dest, cq);  /* 双写转义 */
        smart_str_appendc(dest, name[i]);
    }
    smart_str_appendc(dest, cq);
}
```

注意需处理 `db.table`、`col AS alias`、`func(col)` 等复合形式（可按 `.` 分段 quote，含 `(`/空格的白名单校验后原样通过）。若担心兼容性破坏，至少先加**标识符字符白名单校验**（`[A-Za-z0-9_.,`() ]`），非法字符直接报错。

### 1.4 [P2] redis_pool.c：`rpool_decrement_count()` get→sub 之间存在 TOCTOU 竞态

- 位置：`src/cache/redis_pool.c:407-421`

```c
RPOOL_ATOMIC_CALL(atomic, "get", 0, &ret);
zend_long val = ...;
if (val > 0) {
    RPOOL_ATOMIC_CALL(atomic, "sub", 1, NULL);   /* get 与 sub 之间可被并发插入 */
}
```

**问题**：多协程/多 worker 并发进入时，可能同时读到 `val==1` 然后都执行 `sub` → 计数器下溢为负，导致连接池后续容量判断失真（`count > max` 收缩逻辑、`put()` 的溢出丢弃同理，见 1243-1246）。

**修复**：用 `Swoole\Atomic::cmpset` 做 CAS 循环递减：

```c
while (1) {
    /* get val */
    if (val <= 0) break;
    /* cmpset(val, val-1) 成功则 break，失败重试 */
}
```

`put()` 的 auto-shrink 分支（`rpool_get_count > max` 后 `rpool_decrement_count`）同样改用该 CAS 递减。收益：消除高并发下计数器漂移，防止池容量被慢性侵蚀。

### 1.5 [P2] router.c：`get_router_error_run()` 早退分支下 `run` 未初始化路径

- 位置：`src/router/router.c:1539-1555`

**问题**：`run` 仅在 if 分支被赋值；else 分支虽然 return 了，但控制流结构脆弱，后续维护插入代码极易引入未初始化使用。建议 `char *run = NULL;` 初始化并在使用/释放处判空。属防御性修复。

### 1.6 [P3] common.c：`ReplaceStr()` 原地写回，替换串变长时缓冲区溢出

- 位置：`src/common/common.c:401-430`；当前唯一调用 `src/db/mysql.c` 的 `ReplaceStr(in_tmp, "in(?)", "$")`

**问题**：函数把变长结果 `memcpy` 回 `sSrc` 原缓冲区，不知道目标容量。当前调用 `"in(?)"→"$"` 替换串更短，**暂无实际溢出**；但这是一颗地雷——任何新增调用若 `replace_len > match_len` 即溢出。

**修复**：改为返回新分配字符串（调用方释放），或加 `buf_size` 参数校验；并在函数头注释标明约束。

### 1.7 [P3] response.c：`gene_response_set_header_ex` 长 header 分支强制 `response_code = 200`

- 位置：`src/http/response.c:245`

**问题**：header 总长 ≥1024 时走堆分配分支，该分支额外设置了 `ctr.response_code = 200`，而 <1024 的栈分支不设置。设置任意 header 恰好较长时会把 HTTP 状态码重置为 200，覆盖此前 `setCode()` 的结果。行为不一致，属逻辑错误。

**修复**：删除 `ctr.response_code = 200;`（与栈分支保持一致）。

### 1.8 [P3] view.c：路径遍历检测仅拦截 `".."`

- 位置：`src/mvc/view.c:169-173`

**问题**：只做 `strstr(file, "..")`，绝对路径（`/etc/...`）、Windows 盘符/UNC（`C:\`、`\\server\`）未拦截。视图名通常来自代码而非用户输入，风险取决于业务是否把用户输入拼进视图名。

**修复**：追加 `file[0]=='/'`、`file[0]=='\\'`、`isalpha(file[0]) && file[1]==':'` 的拒绝分支；更彻底的方案是拼接完整路径后 `realpath` 校验前缀在 app_root 之下。

### 1.9 [P3] load.c / 各处 `static` 函数指针缓存的 ZTS 隐患

- 位置：`src/factory/load.c`（spl_register_fn 等）、`src/db/pool.c:97`、`src/db/pdo.c`（json_encode fn）等多处

**问题**：非 ZTS 构建下缓存内部函数指针是安全的（内部函数进程级不变）；但 ZTS 构建下 `CG(function_table)` 按线程隔离，`static` 缓存跨线程共享指针理论上不安全。项目已在 router 的闭包缓存用 `#ifdef ZTS` 守卫，但这些散落的 static 缓存未统一处理。

**修复**：统一封装 `GENE_STATIC_FN_CACHE(name, table, key)` 宏，ZTS 下退化为每次查找（或 TSRM 槽），非 ZTS 保持缓存。

---

## 二、内存泄漏分析

### 2.1 FPM 模式：基本无泄漏 ✅

复核 2026-06-21 报告结论仍成立：请求级资源在 RSHUTDOWN 释放、进程级在 MSHUTDOWN 释放，链路完整。唯一遗留隐患：

- **[P2] `gene_closure_src_cache` 无上限**（`src/router/router.c:1585-1634`）：FPM 下动态注册大量不同闭包路由（跨请求 key 不同）时持久 HashTable 无界增长。实际风险低（路由闭包为有限集合）。
  **修复**：写入前检查 `zend_hash_num_elements() >= N`（建议 1024），超限即整体清空或拒绝写入——闭包源码缓存重建成本低，清空策略即可，无需完整 LRU。

### 2.2 Swoole 常驻模式：两处已知隐患仍需运维配合 ⚠️

1. **`co_contexts` 协程上下文滞留**（`src/gene.c`）：协程未调用 `Application::cleanup()` 时条目滞留；`gene.co_contexts_max=0` 时 sweep 永不触发 → 无界增长。
   **建议**：将 `co_contexts_max=0` 语义从"不限"改为"使用默认 1024"，杜绝误配置导致 OOM；文档强调协程末尾 cleanup。
2. **`Gene\Cache` 持久缓存默认无界**（`gene.cache_max_items=0` 默认关闭 LRU）：用作进程内业务缓存时 RSS 只增不降。
   **建议**：Swoole 模式（runtime_type>=2）下若未显式配置，启动时输出一次 NOTICE 提示设置 `gene.cache_max_items`。

### 2.3 本次排查确认无泄漏的路径（澄清历史/子审计误报）

以下模式经核实**正确，非泄漏**，记录在案避免后续重复报告：

| 疑似点 | 结论 |
|---|---|
| `cache.c:1860` processCachedBatch 无 `Z_TRY_ADDREF` | 正确。该函数无 keys_batch 二次持有，`gene_cache_key` 产出 rc=1，1883 行单次 dtor 平衡；1672/1772 行的 ADDREF 是补偿 `add_next_index_zval` 的所有权转移，两处语义不同 |
| `validate.c` `array_init + Z_TRY_ADDREF + hash_update + dtor` 模式 | 平衡：update 转移 1 个 ref，dtor 消耗 1 个，ADDREF 恰好补偿，最终 rc=1，无泄漏 |
| factory/di/exception 中 `params[0] = *param` 浅拷贝传参 | 安全：`zend_call_known_function` 入帧时对参数 addref、调用结束自行 dtor，调用方保留所有权是 Zend 标准传参模式 |
| `response.c` set_header_ex 栈分支 `header_len < sizeof(buf)` | 无越界：`header_len ≤ 1023`，`buf[header_len]` 最大写 `buf[1023]`，在界内 |
| `language.c` `%.*s` 负精度 | 已被 243-247 行 `Z_STRLEN==0` 守卫拦截，不可达 |
| `router.c get_path_router_inner` 原地临时置 `\0` | 操作对象是 `get_path_router_init` 中 `estrdup` 的请求级私有副本，无跨线程共享；仅需注意递归中间不得有早退路径跳过恢复 `'/'`（当前无） |

---

## 三、性能提升路径（按"收益/实施成本"排序）

### T1 立即可做（高收益 / 低难度）

| # | 优化点 | 位置 | 收益 | 难度 | 说明 |
|---|---|---|---|---|---|
| 1 | `pool_in_coroutine()` 改用 `gene_get_coroutine_id()` | `src/db/pool.c:85-117` | **高** | 低 | 兼修 1.1 的 bug；每次借/还连接省一次 PHP 方法调用（~300ns→~5ns），DB 热路径直接受益 |
| 2 | `gene_co_contexts_sweep` 存活探测走 dlsym C-API | `src/gene.c`（gene_swoole_co_exists） | 中高 | 低 | 复用 P1 已有的 dlsym 基建，sweep 触发时 N 次 PHP 调用归零；co_contexts 大时收益显著 |
| 3 | 路由注册 `__call` 中 20+ 次 `snprintf` 改 memcpy 拼接 | `src/router/router.c:2258-2828` | 中 | 低 | 仅影响路由注册期（Swoole 下仅 worker 启动一次，FPM 每请求）；FPM 场景收益明显 |
| 4 | `benchmark.c` / 各处内部函数指针一次性缓存 | `src/tool/benchmark.c:59-94` 等 | 低中 | 低 | `memory_get_peak_usage`/`error_log`/`json_encode` 每次 hash 查找 → static 缓存（注意 1.9 的 ZTS 宏封装） |

### T2 中期（中高收益 / 中难度）

| # | 优化点 | 位置 | 收益 | 难度 | 说明 |
|---|---|---|---|---|---|
| 5 | 路由 `route_pc` 预编译在 `worker_ready` 时全树预热 | `src/router/router.c:1337-1366` | 中高 | 中 | 消除 Swoole 首批请求的编译毛刺；需实现路由树遍历 + 对每个 leaf 调 `gene_route_pc_resolve`；配合 P3 开关灰度 |
| 6 | `gene_memory_get_by_config` 配置路径解析缓存 | `src/cache/memory.c:612-657` | 中 | 中 | `worker_ready` 后配置冻结，"db/mysql/host" 这类路径可缓存最终 zval 指针；收益取决于热路径读配置频率 |
| 7 | 请求上下文 9 个 char* 字段 arena 化 | `src/gene.c:344-361` | 中 | 中 | method/path/module/... 单次 emalloc 批量分配，reset 单次 efree；每请求省 ~8 次分配/释放 |
| 8 | SQL 构造链 `smart_str` 预估容量 + 减少 property 读写 | `src/db/mysql.c` 等四驱动 | 中 | 中 | `where/field/data` 属性每步 `zend_read_property`+`zend_update_property_str`；可在对象内嵌 slot（OBJ_PROP offset）直取，链式调用越长收益越大 |

### T3 长期（高收益 / 高难度，或需验证）

| # | 优化点 | 收益 | 难度 | 说明 |
|---|---|---|---|---|
| 9 | 视图模板 `parser_templates` 28 次 pcre_replace 合并 | 中高 | 高 | 每次编译模板 28 次全文扫描+28 次重分配；可合并为单趟状态机或按标签前缀分组一次扫描。仅影响模板编译（有缓存则冷路径），优先级看模板变更频率 |
| 10 | redis_pool/pool 计数器迁移到扩展内原子变量 | 中 | 高 | 现走 `Swoole\Atomic` PHP 对象调用；进程内池可用 C 层 `int64 + __atomic_*` 直接操作，同时天然解决 1.4 的 TOCTOU。跨进程共享池则保留 Swoole\Atomic+CAS |
| 11 | `get_path_router_inner` 递归改迭代 | 低中 | 中 | 深路径下减少调用开销与栈风险；路由深度通常 <10，收益有限，排后 |

### 不建议做

- `session.c` get/set/del 的 `zend_string_init` 栈化：单次分配开销极小，且 zend_string 必须堆分配，收益近零。
- `cachedBatch` 小批量栈分配：`safe_emalloc` 已足够快，复杂度不值。

---

## 四、修复实施计划（建议顺序）

| 阶段 | 内容 | 风险 |
|---|---|---|
| 第一批（bugfix，建议随 5.6.8 发布） | 1.1 pool getCid、1.2 validate NULL key 崩溃、1.7 response_code=200、1.5 run 初始化 | 极低，均为局部修复，配套 .phpt 用例：未调 name() 直接 skipOnEmpty/msg；长 header 后校验状态码 |
| 第二批（安全加固） | 1.3 identifier quote 实现（先白名单校验，再完整 quote）、1.8 view 路径校验增强、1.6 ReplaceStr 签名加固 | 中——quote 变更可能影响存量 SQL（含反引号/别名的表名），需回归 demo/test 全量 SQL 用例 |
| 第三批（并发正确性） | 1.4 redis_pool CAS 递减（含 put 收缩分支）、co_contexts_max=0 语义修正 | 低，需 Swoole 压测验证计数器不漂移（可复用 tools/ 下已有 Swoole 验证脚本模式） |
| 第四批（性能 T1→T2） | 表中 #1-#4 先行（一天内可完成），#5-#8 按压测数据决定取舍 | #5 依赖 P3 开关 ASAN 验证通过后默认开启 |

---

## 五、总结

- **确认 bug：9 项**（P1×3、P2×2、P3×4），其中 `pool.c getCid`（上次审计已指出仍未修复）与 `validate.c NULL key 崩溃` 应最优先修复。
- **内存泄漏**：FPM 无实质泄漏；Swoole 下两处已知无界增长点依赖配置约束，建议改默认语义兜底；本次另澄清 6 处误报模式，供后续审计参考。
- **性能**：最高性价比是 #1（顺带修 bug）与 #2（复用现成 dlsym 基建）；中期最大空间在路由预热（#5）与 SQL 构造链 property 直取（#8）。
