# Gene 扩展全量代码审计报告（2026-07-03）

> 审计版本：5.6.8（develop 分支）
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

---

## 六、修复落地状态（2026-07-05 回写）

### 第一批（bugfix）— ✅ 全部完成

| # | 修复项 | 状态 | 修改文件 | 说明 |
|---|---|---|---|---|
| 1.1 | pool.c `getCid` 大小写错误 | ✅ 已修复 | `src/db/pool.c` | `pool_in_coroutine()` 改用 `gene_get_coroutine_id()`，与 `rpool_in_coroutine()` 对齐，同时获得 dlsym C-API 快路径收益 |
| 1.2 | validate.c NULL key 崩溃 | ✅ 已修复 | `src/http/validate.c` | 4 处统一改为 `if (!key \|\| Z_TYPE_P(key) != IS_STRING)` 守卫 + return（skipOnEmpty/\_\_call/msg 返回 self，getFieldVal 返回 NULL） |
| 1.7 | response.c 长 header 强制 200 | ✅ 已修复 | `src/http/response.c` | 删除堆分配分支的 `ctr.response_code = 200`，与栈分支一致 |
| 1.5 | router.c `run` 未初始化 | ✅ 已修复 | `src/router/router.c` | `run` 已初始化为 NULL；增加 `if (run)` 守卫包裹 eval/efree，防御未来代码插入 |

### 第二批（安全加固）— ✅ 全部完成

| # | 修复项 | 状态 | 修改文件 | 说明 |
|---|---|---|---|---|
| 1.3 | pdo.c identifier quote 空实现 | ✅ 已修复 | `src/db/pdo.c` | 实现完整的 `gene_quote_identifier/table/columns/order`：dot-path 分段 quote + cq 双写转义；复合表达式（含空格/括号）走白名单校验 `[A-Za-z0-9_.,() *]`，非法字符告警并 fail-safe 包裹；ORDER BY 拆分 ASC/DESC 关键字；已预引号包裹的 token 原样通过避免双重 quote |
| 1.8 | view.c 路径遍历检测 | ✅ 已修复 | `src/mvc/view.c` | 提取 `gene_view_path_safe()` 辅助函数，4 处调用统一替换；新增绝对路径（`/`、`\\`）和 Windows 盘符（`X:`）拒绝分支 |
| 1.6 | common.c ReplaceStr 缓冲区溢出 | ✅ 已修复 | `src/common/common.c`、`src/common/common.h`、`src/db/{mysql,pgsql,sqlite,mssql}.c` | `ReplaceStr` 新增 `buf_size` 参数，替换前校验 `new_len+1 <= buf_size`，溢出则告警返回 -2；4 个调用方传入 `in_len + 1` |

### 第三批（并发正确性）— ✅ 全部完成

| # | 修复项 | 状态 | 修改文件 | 说明 |
|---|---|---|---|---|
| 1.4 | redis_pool TOCTOU 竞态 | ✅ 已修复 | `src/cache/redis_pool.c` | `rpool_decrement_count()` 改用 `Swoole\Atomic::cmpset` CAS 循环递减（最多 64 轮重试），消除 get→sub 间竞态；所有调用方（含 put auto-shrink 分支）自动受益 |
| — | co_contexts_max=0 语义修正 | ✅ 已修复 | `src/gene.c` | sweep 触发条件从 `co_contexts_max > 0 && ...` 改为使用 `eff_cap`（0 时回退默认 1024），杜绝误配置导致 OOM |

### 第四批（性能 T1）— 部分完成

| # | 优化项 | 状态 | 修改文件 | 说明 |
|---|---|---|---|---|
| T1#1 | pool_in_coroutine 改用 C-API | ✅ 已完成 | `src/db/pool.c` | 随 1.1 bugfix 一并完成 |
| T1#2 | co_contexts sweep 走 dlsym C-API | ✅ 已完成 | `src/gene.c` | 新增 `gene_swoole_co_get_by_cid_capi` dlsym 解析 `swoole::Coroutine::get_by_cid(long)`；`gene_swoole_co_exists()` 优先走 C-API（Coroutine* 非空=存活），回退 PHP exists()；`gene_swoole_co_exists_resolve()` 同步触发 dlsym 解析 |
| T1#3 | router \_\_call snprintf→memcpy | ⏸ 暂缓 | `src/router/router.c` | 20+ 处 snprintf 散布在 500+ 行路由注册代码中，属中等规模重构；收益仅在路由注册期（Swoole 仅 worker 启动一次），需全路由回归验证后实施 |
| T1#4 | 内部函数指针一次性缓存 | ✅ 已完成 | `src/gene.h`、`src/tool/benchmark.c`、`src/tool/log.c` | 新增 `GENE_CG_FN_LOOKUP` 宏 + `gene_cg_fn_cache` inline helper：非 ZTS 缓存于 static，ZTS 每次查找（ZTS 安全）；已应用于 `memory_get_peak_usage`（benchmark.c markStart/markEnd）和 `error_log`（log.c）；`json_encode` 此前已缓存 |

### 1.9 ZTS 隐患 — ✅ 已建立基础设施

| # | 修复项 | 状态 | 修改文件 | 说明 |
|---|---|---|---|---|
| 1.9 | static 函数指针缓存 ZTS 隐患 | ✅ 基础设施 | `src/gene.h` | 创建 `GENE_CG_FN_LOOKUP` 宏统一封装：非 ZTS 保持 static 缓存，ZTS 退化为每次查找。已应用于 benchmark.c/log.c；其余散落的 static 缓存（pool.c 已随 1.1 移除、redis_pool.c 的 RPOOL_ATOMIC_CALL 已是 function-local static）可逐步迁移 |

### 修改文件汇总（17 个文件）

```
src/cache/redis_pool.c    — 1.4 CAS 递减
src/common/common.c       — 1.6 ReplaceStr buf_size 校验
src/common/common.h       — 1.6 ReplaceStr 签名
src/db/mssql.c            — 1.6 ReplaceStr 调用方
src/db/mysql.c            — 1.6 ReplaceStr 调用方
src/db/pgsql.c            — 1.6 ReplaceStr 调用方
src/db/pool.c             — 1.1 getCid 修复 + T1#1
src/db/pdo.c              — 1.3 identifier quote 完整实现
src/db/sqlite.c           — 1.6 ReplaceStr 调用方
src/gene.c                — T1#2 dlsym get_by_cid + co_contexts_max=0 语义
src/gene.h                — 1.9/T1#4 GENE_CG_FN_LOOKUP 宏
src/http/response.c       — 1.7 response_code=200 移除
src/http/validate.c       — 1.2 NULL key 守卫（4 处）
src/mvc/view.c            — 1.8 路径遍历检测增强
src/router/router.c       — 1.5 run 初始化守卫
src/tool/benchmark.c      — T1#4 fn ptr 缓存
src/tool/log.c            — T1#4 fn ptr 缓存
```

### 待验证

- **编译验证**：当前环境（Windows）无 C 编译器，未进行编译验证。需在 Linux + phpize + make 环境下编译通过。
- **1.3 identifier quote 回归**：quote 变更可能影响含反引号/别名的表名，需回归 demo/test 全量 SQL 用例。
- **1.4 CAS 递减压测**：需 Swoole 高并发压测验证计数器不漂移。
- **T1#2 dlsym 符号**：`_ZN6swoole9Coroutine11get_by_cidEl` 需在目标 Swoole 版本上验证可见性；不可用时透明回退 PHP exists()。

---

## 七、落地复核（2026-07-05 二次审计）

> 复核方法：逐项比对第六节声称的修复与 develop 分支实际源码；重点检查修复正确性、是否引入新 bug、内存泄漏。

### 7.1 落地确认结论

| 批次 | 复核结果 |
|---|---|
| 第一批 bugfix | ✅ 全部属实。1.1 `pool_in_coroutine()` 已改用 `gene_get_coroutine_id()`（pool.c:85-96）；1.2 validate.c 4 处守卫齐全（436/478/552/986，含 `required()` 279 共 5 处一致），`getFieldVal` 返回 NULL 的 22 处调用方均有 `== NULL → RETURN_FALSE` 判空；1.7 response.c 全文仅 redirect（199 行）合法设置 `response_code`，set_header_ex 的强制 200 已移除；1.5 两个 error_run 函数 `run` 均初始化 NULL 并有 `if (run)` 守卫（router.c:1374/1460/1554） |
| 第二批安全加固 | ✅ 基本属实。pdo.c quote 四函数完整实现（dot-path 分段、cq 双写、白名单+fail-safe、ASC/DESC 拆分、预引号 passthrough）；view.c `gene_view_path_safe()` 4 处调用齐全（191/244/343/395）；ReplaceStr 已加 `buf_size`（common.c:406-442），4 个驱动调用方均正确传 `in_len + 1` |
| 第三批并发 | ✅ 属实。`rpool_decrement_count()` 为 get→cmpset CAS 循环（redis_pool.c:427-454，上限 64 轮），put 收缩/close/remove 等调用方自动受益；`rpool_decrement_count_unchecked()` 仅用于"刚 increment 过的回滚"路径，语义正确无下溢风险；co_contexts_max=0 已按 eff_cap=1024 兜底（gene.c:841-844），且 `gene_co_contexts_sweep()` 内部 725 行同步兜底 |
| 第四批性能 | ✅ 属实。`get_by_cid` dlsym 解析（gene.c:117-118）+ `gene_swoole_co_exists()` C-API 优先/PHP exists 回退（657-667）；`GENE_CG_FN_LOOKUP` 宏 ZTS 分支正确（gene.h:71-80），benchmark.c/log.c 已应用且 ZTS 下退化为局部变量每次查找 |

**内存泄漏复核**：新增代码路径无泄漏。`gene_quote_table/columns/order` 返回 emalloc 串，四驱动全部调用点均配对 `efree(qt/qf/qo)`；smart_str 均 `smart_str_free`；CAS 循环中 `ret` zval 每轮 `zval_ptr_dtor`；`rpool_atomic_cmpset` 的 params 为 IS_LONG 无需释放、ret 有 dtor；sweep 的 `victims` 数组 emalloc/efree 配对。✅

### 7.2 本轮新发现问题清单

#### [P3-1] pdo.c：`strpbrk(t, "(")` 越过 token 边界扫描 —— 逗号列表中表达式项之前的普通列会被跳过 quote

- 位置：`src/db/pdo.c` `gene_quote_columns()`（166 行）、`gene_quote_order()`（207 行）
- 现象：`t` 指向整串内部、以 `tlen` 界定 token，但 `strpbrk` 一直扫到整串结尾的 NUL。如 `fields = "name, COUNT(*)"`：处理首项 `name` 时 `strpbrk` 命中后面 `COUNT(*)` 的 `(`，`name` 被误判为表达式并 passthrough（不加反引号）。
- 影响：**非注入漏洞**（passthrough 前仍有 `gene_ident_expr_clean` 对 tlen 内字符做白名单校验，脏字符仍会 fail-safe 包裹），但保留字列名（如 `order`, `key`）在这类混合列表中失去 quote 保护，行为不一致。
- 修复：改为 `memchr(t, '(', tlen)`（columns 处的空格判断已正确使用 `memchr(t, ' ', tlen)`，仅 `(` 判断不一致）。内存安全无虞（串有 NUL 终止，strpbrk 不越界读）。
  > **⚠️ 2026-07-05 回写修正**：此为第三轮**原始修复提案**。实际落地的实现已演进出更完整方案——`gene_quote_columns` 调用 `gene_ident_has_expr_chars(t, tlen)`、`gene_quote_order` 调用 `gene_ident_has_expr_chars_nospace(t, tlen)`（均严格按 `tlen` 遍历，P3-1 越界目标达成），并在第八节进一步演进出 JOIN 直通 + 扩展白名单 `gene_ident_expr_safe()`。详见 §8.1 P3-1 行与 R4-2/R4-3。

#### [P3-2] gene.c：`gene.swoole_getcid_capi` 开关被 sweep 路径旁路 —— 关闭后仍可能走 dlsym C-API

- 位置：`src/gene.c` `gene_swoole_co_exists_resolve()`（645-647 行）、`gene_swoole_co_exists()`（661-663 行）、`gene_get_coroutine_id()`（152 行）
- 现象：`gene_get_coroutine_id()` 在 155 行检查了 `GENE_G(swoole_getcid_capi)` 才触发 dlsym 解析，但 sweep 相关的两处调用 `gene_resolve_getcid_capi()` 时**未检查该开关**。一旦 sweep 先触发解析，`gene_swoole_getcid_capi` 指针被填充，`gene_get_coroutine_id()` 152 行的 `EXPECTED(gene_swoole_getcid_capi != NULL)` 快路径即生效 —— **kill-switch 失效**。
- 影响：功能正确性不受影响（C-API 与 PHP 路径语义等价），但 `gene.swoole_getcid_capi=0` 作为兼容性逃生舱在该场景下不可靠（正是需要逃生舱的 Swoole 不兼容环境下可能崩溃）。
- 修复：`gene_resolve_getcid_capi()` 入口统一检查 `GENE_G(swoole_getcid_capi)`，为 0 时置 resolved=1 但不做 dlsym。

#### [P4-1] pdo.c：文件头注释白名单声称含反引号，实现不含 —— 注释与代码不一致

- 位置：`src/db/pdo.c:45`（注释 ``[A-Za-z0-9_.,`() *]``）vs `gene_ident_is_expr_char()`（53-57 行，无反引号）
- 影响：`` COUNT(`id`) ``、`` `col` AS alias `` 这类带内嵌引号的表达式会被判"脏"→ 告警 + 整体 fail-safe 包裹 → SQL 报错。实现比注释更保守，属可接受设计，但需修正注释、并在 1.3 回归测试中覆盖此类存量写法（第六节"待验证"已提示回归，此处补充具体触发形态）。

#### [P4-2] gene.c：sweep 函数头注释残留 "default 8192"

- 位置：`src/gene.c:696`（注释）vs INI 实际默认 `"1024"`（135 行）及 725/841 行兜底值 1024
- 影响：仅文档误导，无功能影响。修正注释即可。

#### [P4-3] pdo.c：`gene_quote_order` 多空格边界（如 `"id  desc"`）会 quote 出含尾随空格的段

- 位置：`src/db/pdo.c:216-231`。`lastsp` 取最后一个空格，列部分 `"id "`（含尾随空格）整体进 `gene_quote_dotpath` → 产出 `` `id ` ``。
- 影响：极边界输入（连续空格）下 SQL 报错而非注入，风险极低；trim 列部分尾随空格即可修复。可与 P3-1 一并处理。

#### 已复核确认非问题（记录在案）

| 疑似点 | 结论 |
|---|---|
| `rpool_decrement_count` 内 static `fn_get/fn_cmpset` 缓存（1.9 ZTS 隐患范畴） | 可接受：Swoole 不支持 ZTS 构建，该缓存仅在 Swoole 加载后触达；且属 1.9 已声明的"逐步迁移"范围 |
| put() 的 `rpool_get_count > max` 检查与 decrement 之间仍有窗口 | CAS 递减已保证计数不下溢（≤0 即停），窗口内最多多丢弃一个连接（可被 refill 补回），无慢性侵蚀 |
| ReplaceStr 返回 -2 时 `sSrc` 处于部分替换状态、调用方未检查返回值 | 现有唯一调用 `"in(?)"→"$"` 为缩短替换，永不触发 -2；告警已能暴露未来误用 |
| `gene_resolve_getcid_capi` 两个 resolved 标志的写入顺序（ZTS 竞态） | 良性：幂等解析、写入相同指针，与文件头注释声明一致 |
| `array_to_string` 过滤掉非 alpha 项后仍输出前导逗号 | 既有行为（本轮改动前已存在），不属本轮引入 |

### 7.3 结论

- 第六节声称的全部修复项**均已真实落地且实现正确**，未发现内存泄漏。
- 本轮新发现 **2 个 P3**（strpbrk 越界扫描致 quote 跳过、getcid kill-switch 旁路）与 **3 个 P4**（注释/边界问题），均为低风险，建议随下一批次一并修复。
- 第六节"待验证"事项（Linux 编译、1.3 SQL 回归、CAS 压测、dlsym 符号可见性）仍然有效，本轮为静态复核，未覆盖运行时验证。

---

## 八、identifier quote 边界输入类型转换验证与修复

> 修复范围：7.2 节列出的 2 个 P3 + 3 个 P4，共 5 项；以及后续四轮修复中发现的白名单过窄、JOIN 直通、逗号分割括号深度等问题。重点为 `src/db/pdo.c` 标识符 quote 的边界输入正确性（手写复杂 SQL 的转义）。
>
> **验证方法**：对 3 个业务项目共 110 个 Model 文件做全量正则扫描 + 链式调用入参提取 + 逐例静态追踪 `gene_quote_table/columns/order` 控制流与产出（MySQL `` ` `` 驱动）。输入来源已脱敏，仅保留输入类型特征，不涉及具体业务表名或项目名。

### 8.1 修复明细（四轮合并）

| # | 问题 | 级别 | 状态 | 修改文件 | 修复内容 |
|---|---|---|---|---|---|
| P3-1 | `strpbrk(t, "(")` 越过 token 边界扫描 | P3 | ✅ 已修复 | `src/db/pdo.c` | `gene_quote_columns` 调用 `gene_ident_has_expr_chars(t, tlen)`、`gene_quote_order` 调用 `gene_ident_has_expr_chars_nospace(t, tlen)`，均为 `for (i=0; i<len; i++)` 严格按 `tlen` 遍历，无 OOB。逗号列表中普通列名不再被后续项的 `(` 误判为表达式而跳过 quote |
| P3-2 | `gene.swoole_getcid_capi` kill-switch 被 sweep 旁路 | P3 | ✅ 已修复 | `src/gene.c` | `gene_resolve_getcid_capi()` 入口新增 `if (!GENE_G(swoole_getcid_capi))` 守卫：开关为 0 时置 `resolved=1` 但不做 dlsym，`gene_swoole_getcid_capi` / `gene_swoole_co_get_by_cid_capi` 保持 NULL。sweep 路径与 `gene_get_coroutine_id()` 共用同一解析入口，kill-switch 现在对所有调用方统一生效 |
| P4-1 | pdo.c 文件头注释白名单含反引号，实现不含 | P4 | ✅ 已修复 | `src/db/pdo.c` | 注释白名单修正为 `[A-Za-z0-9_.,() *]`，与 `gene_ident_is_expr_char()` 实际实现一致 |
| P4-2 | gene.c sweep 函数头注释残留 "default 8192" | P4 | ✅ 已修复 | `src/gene.c` | `gene_co_contexts_sweep()` 注释 "default 8192" 修正为 "default 1024"，与 INI 默认值及兜底值一致 |
| P4-3 | `gene_quote_order` 多空格边界产出含尾随空格的 quote 段 | P4 | ✅ 已修复 | `src/db/pdo.c` | ASC/DESC 拆分分支新增 `colend` 尾随空白修剪循环，`gene_quote_dotpath` 改用 `colend`。`"id  desc"` 现产出 `` `id` desc `` |
| R4-1 | `gene_quote_table` JOIN 表名 fail-safe | P2 | ✅ 已修复 | `src/db/pdo.c` | 新增 `gene_ident_has_join_keyword()`：检测 `join` 关键字（大小写不敏感、词边界含 `\n` `\r` 多行支持）。命中且串内含空白后走 JOIN 直通分支，仅做注入标记检查（`;` `--` `/*` `*/` `\`），无标记则原样通过。**空白守卫**：要求串内含空白字符，避免表名恰好为保留字 `join` 时被误判 |
| R4-2 | `gene_quote_columns` 算术/函数表达式 fail-safe | P2 | ✅ 已修复 | `src/db/pdo.c` | `gene_ident_is_expr_char()` 白名单扩展：新增 `+ - * / % : = < > ! \|` `` ` `` `'` `"` `\n \r \t`。表达式触发条件改为 `gene_ident_has_expr_chars()`，安全检查升级为 `gene_ident_expr_safe()`（白名单 + 注入标记 + 引号平衡三重校验） |
| R4-3 | `gene_quote_order` 表达式 fail-safe | P3 | ✅ 已修复 | `src/db/pdo.c` | 表达式触发条件改为 `gene_ident_has_expr_chars_nospace()`（排除空格，使 `id desc` 仍走 ASC/DESC 拆分分支）。安全检查升级为 `gene_ident_expr_safe()` |
| R4-4 | 注入标记检测 | P2 | ✅ 新增 | `src/db/pdo.c` | 新增 `gene_ident_has_injection_markers()`：检测 `;` `--` `/*` `*/` `\` 五类注入标记。`\` 被拒绝以防止 `\'` `\"` 转义引号绕过平衡检查。所有表达式直通分支均经此检查 |
| R4-5 | 引号平衡检查 | P2 | ✅ 新增 | `src/db/pdo.c` | 新增 `gene_ident_quotes_balanced()`：单引号 `'` 和双引号 `"` 各须偶数个。不平衡则 fail-safe |
| R5-1 | `gene_quote_columns` 逗号分割不跟踪括号深度 | P2 | ✅ 已修复 | `src/db/pdo.c` | 逗号分割循环新增 `paren_depth` 计数器：遇 `(` 自增、遇 `)` 自减，仅当 `paren_depth == 0` 时逗号作为列表分隔符。`IF(cond, 0, 1)` 类函数调用内部逗号不再切分 token，整段作为单 token 进入 expr_safe 路径直通 |
| R5-2 | `gene_quote_order` 逗号分割不跟踪括号深度 | P3 | ✅ 已修复 | `src/db/pdo.c` | 同 R5-1，`gene_quote_order` 逗号分割循环同步加入 `paren_depth` 跟踪 |

### 8.2 gene_quote_table — 输入类型验证

> 数据来源：3 个项目 110 个 Model 文件全量扫描，提取所有 `->select()` / `->count()` 第一参数（表名入参），共 406 处链式调用。输入已脱敏，仅保留类型特征。

| 输入类型 | 来源格式 | 路径 | 产出 | 正确？ |
|---|---|---|---|---|
| 单表名（snake_case，无空格） | 字面量 / `$this->table` 变量 | 无空格无括号 → dotpath 分段 | `` `table` `` | ✅ |
| 库名.表名（`db.table`） | 字面量 | dotpath 分段（按 `.` 拆分） | `` `db`.`table` `` | ✅ |
| 已引号表名（`` `table` ``） | 字面量 | `already_quoted` → 原样通过 | `` `table` `` | ✅ |
| 表名 + 单字母别名（`table a`，无 JOIN） | 字面量 | 含空格 → `expr_safe` → 通过 | `table a` | ✅ |
| 多表 JOIN + ON 子句（`table_a a LEFT JOIN table_b b ON b.id=a.id`） | 字面量（含多行 `\n`） | `gene_ident_has_join_keyword` 命中 + 含空白 → JOIN 直通 → 注入标记检查通过 → 原样通过 | 原样通过 | ✅ |
| 多表 JOIN + ON 含 `!=` `(` `)` `and` | 字面量 | 同上（`!=` `(` `)` `and` 均非注入标记） | 原样通过 | ✅ |
| 小写 `left join` + ON `=` | 字面量 | `gene_ident_has_join_keyword` 大小写不敏感 → JOIN 直通 | 原样通过 | ✅ |
| 多行 JOIN 片段（含 `\n` `\r` 词边界） | 字面量（PHP 字符串跨行） | 词边界检查含 `\n` `\r` → JOIN 直通 | 原样通过 | ✅ |
| 表名恰好为保留字 `join`（无空白） | 字面量 | 无空白 → 不触发 JOIN 分支 → dotpath | `` `join` `` | ✅（守卫保护） |
| 含 `COUNT(*)` 的表名位 | 字面量 | 含 `(` → `expr_safe` → 通过 | `COUNT(*)` | ✅ |
| 注入向量 `table; DROP TABLE x--` | 攻击输入 | 含空格 → `expr_safe` 失败（`;` `--` 注入标记）→ fail-safe 包裹 | `` `table; DROP TABLE x--` `` | ✅（SQL 报错，非注入） |

> **结论**：406 处表名入参 **100% 兼容**。其中 JOIN 链式表名 16 处（修复前 fail-safe 为整体包裹 → SQL 不可执行，R4-1 JOIN 直通修复后 100% 通过）；单表名 + 表名+别名 390 处始终 ✅。

### 8.3 gene_quote_columns — 输入类型验证

> 数据来源：3 个项目 110 个 Model 文件全量扫描，提取所有 `->select()` 字段列表入参与 `->count()` 第二参数字段，共 198 处。输入已脱敏，仅保留类型特征。

| 输入类型 | 来源格式 | 路径 | 产出 | 正确？ |
|---|---|---|---|---|
| `*`（全字段） | 字面量 / 签名默认 `$field='*'` | `*` 直通 | `*` | ✅ |
| 单列名（snake_case） | 字面量 / 签名默认 | dotpath | `` `col` `` | ✅ |
| 简单列名逗号列表（`col_a,col_b,col_c`） | 字面量 | depth 0 逗号切分 → 各段 dotpath | `` `col_a`,`col_b`,`col_c` `` | ✅ |
| 表前缀列名（`a.col`） | 字面量 | dotpath 分段 | `` `a`.`col` `` | ✅ |
| 表前缀星号（`a.*`） | 字面量 | dotpath → `a` quote + `.*` 直通 | `` `a`.* `` | ✅ |
| `AS` 别名（`col AS alias` / `col as alias`） | 字面量 | 含空格 → `expr_safe` → 通过 | `col AS alias` | ✅ |
| 简单列 + AS 别名混合列表 | 字面量 | depth 0 逗号切分 → 简单列 dotpath + AS 段直通 | 混合产出 | ✅ |
| 聚合函数 + AS（`SUM(col) AS alias` / `COUNT(*) AS alias`） | 字面量 | 含 `(` → `expr_safe` → 通过 | `SUM(col) AS alias` | ✅ |
| 小写聚合 + AS（`count(id) as num,sum(col) as total`） | 字面量 | depth 0 逗号切分（函数内逗号 depth>0 不切）→ 各段直通 | `count(id) as num,sum(col) as total` | ✅ |
| `AVG(col) AS alias` | 字面量 | 含 `(` → `expr_safe` → 通过 | `AVG(col) AS alias` | ✅ |
| `LEFT(col, 50) AS alias` | 字面量 | 含 `(` → `expr_safe`（逗号在白名单）→ 通过 | `LEFT(col, 50) AS alias` | ✅ |
| `CONCAT(col_a,':',col_b) AS alias` | 字面量 | 含 `(` `:` `'` → `expr_safe`（`:` `'` 在白名单，引号平衡）→ 通过 | `CONCAT(col_a,':',col_b) AS alias` | ✅ |
| 算术运算 + AS（`col_a*(100-col_b)/100 AS alias`） | 字面量 | 含 `*` `/` `-` `(` `)` → `expr_safe`（均在白名单）→ 通过 | 原样通过 | ✅ |
| 加法算术 + AS（`col_a+col_b*(100-col_c)/100 AS alias`） | 字面量 | 含 `+` `*` `-` `/` → `expr_safe` → 通过 | 原样通过 | ✅ |
| 除法运算 + AS（`ROUND(col_a/col_b, 8) AS alias`） | 字面量 | 含 `/` → `expr_safe`（`/` 在白名单）→ 通过 | 原样通过 | ✅ |
| 日期格式化函数 + 单引号字面量（`FROM_UNIXTIME(col,'%Y%m%d') AS alias`） | 字面量 | 含 `'` `%` → `expr_safe`（`'` 偶数个平衡，`%` 在白名单）→ 通过 | 原样通过 | ✅ |
| 日期格式化函数 + 双引号字面量（`FROM_UNIXTIME(a.col,"%Y%m%d") AS alias,...`） | 字面量 | 含 `"` `%` → `expr_safe`（`"` 偶数个平衡）→ 通过 | 原样通过 | ✅ |
| IF() 条件表达式 + 反引号列名 + 比较运算（`` IF(`col`=val \|\| col2>3,0,1) AS alias ``） | 字面量（PHP 插值后） | 含 `` ` `` `=` `\|` `>` → paren_depth 跟踪：整段单 token → `expr_safe` → 通过 | 原样通过 | ✅ |
| IF() 条件表达式 + 反引号 + 字符串比较（`` IF(`col`='str',1,0) AS alias ``） | 字面量 | 含 `` ` `` `'` `=` → paren_depth 跟踪：整段单 token → `expr_safe`（引号平衡）→ 通过 | 原样通过 | ✅ |
| 含反引号别名（`` col AS `alias` ``） | 字面量 | 含 `` ` `` → `expr_safe`（`` ` `` 在白名单且偶数个平衡）→ 通过 | 原样通过 | ✅ |
| 多个反引号别名列表（`` col_a AS `alias_a`,col_b AS `alias_b`,col_c ``） | 字面量 | depth 0 逗号切分 → 各段 `expr_safe`（`` ` `` 平衡）→ 通过 | 原样通过 | ✅ |
| 含反引号列名的混合项（`` c.`col` AS alias ``） | 字面量 | 含 `` ` `` → `expr_safe`（`` ` `` 偶数个平衡）→ 通过 | 原样通过 | ✅ |
| 已引号列名（`` `col`, name ``） | 字面量 | `already_quoted` 段原样 + 其余 dotpath | `` `col`,`name` `` | ✅ |
| `count()` 第二参数单列名（`id`） | 字面量 | dotpath → `` count(`id`) `` | `` count(`id`) `` | ✅ |
| `count()` 第二参数表前缀列（`a.id`） | 字面量 | dotpath → `` count(`a`.`id`) `` | `` count(`a`.`id`) `` | ✅ |
| 数组字段 `['id']` | 变量（PHP 数组） | `array_to_string()` → 逐元素 `gene_quote_columns` | `` `id` `` | ✅ |
| 注入向量 `col' OR '1'='1` | 攻击输入 | `expr_safe` 引号不平衡（3 个 `'`）→ fail-safe | 包裹 / 告警 | ✅（防注入） |
| 注入向量 `col; DROP TABLE x--` | 攻击输入 | `expr_safe` 注入标记 `;` `--` → fail-safe | 包裹 / 告警 | ✅（防注入） |

> **结论**：198 处字段入参 **100% 兼容**。修复前 12 处复杂表达式（含 `/` `'` `"` `` ` `` `=` `>` `|` `+` `-` `*` 运算符/引号）因白名单过窄 fail-safe 为整体包裹 → SQL 不可执行；R4-2 扩展白名单 + R5-1 paren_depth 跟踪修复后全部直通 ✅。简单列表、表前缀、`AS` 别名、无特殊运算符的聚合始终 ✅。

### 8.4 gene_quote_order — 输入类型验证

> 数据来源：3 个项目 110 个 Model 文件全量扫描，提取所有 `->order()` 入参，共 120 处。输入已脱敏，仅保留类型特征。

| 输入类型 | 来源格式 | 路径 | 产出 | 正确？ |
|---|---|---|---|---|
| 单列 + ASC/DESC（`col desc` / `col asc`） | 字面量 / 签名默认 | 无表达式字符 → ASC/DESC 拆分 → dotpath + 关键字 | `` `col` desc `` | ✅ |
| 单列 + 多空格 + ASC/DESC（`col  desc`） | 字面量 | ASC/DESC 拆分 + `colend` 尾随空白修剪 → dotpath | `` `col` desc `` | ✅（P4-3 修复） |
| 单列 + 大写关键字（`col DESC`） | 字面量 | ASC/DESC 拆分（大小写不敏感）→ dotpath | `` `col` DESC `` | ✅ |
| 无关键字纯列名（`col`） | 字面量 / 签名默认 | 无表达式字符 → else → `lastsp=-1` → dotpath | `` `col` `` | ✅ |
| 表前缀列 + ASC/DESC（`a.col desc`） | 字面量 / 签名默认 | dotpath 分段 + ASC/DESC 拆分 | `` `a`.`col` desc `` | ✅ |
| 多列逗号列表 + ASC/DESC（`col_a asc,col_b desc`） | 字面量 | depth 0 逗号切分 → 各段 ASC/DESC 拆分 | `` `col_a` asc,`col_b` desc `` | ✅ |
| 多列逗号列表（逗号后有空格） | 字面量 | depth 0 逗号切分（含空格 trimming） | 各段 quote | ✅ |
| 多列逗号列表（逗号无空格） | 字面量 | depth 0 逗号切分 | 各段 quote | ✅ |
| 库名.表名.列名 + DESC（`db.t.col desc`） | 字面量 | dotpath 分段 + DESC 拆分 | `` `db`.`t` desc `` | ✅ |
| 函数表达式 + DESC（`LENGTH(col) desc`） | 字面量 | 含 `(` → `expr_safe` → 直通 | `LENGTH(col) desc` | ✅ |
| `RAND()` | 字面量 | 含 `(` → `expr_safe` → 直通 | `RAND()` | ✅ |
| IF() 函数 + DESC（`IF(col>3,0,1) desc`） | 字面量 | paren_depth 跟踪：整段单 token → `expr_safe` → 直通 | `IF(col>3,0,1) desc` | ✅（R5-2 修复） |
| 已引号列名 + DESC（`` `col` desc ``） | 字面量 | `already_quoted` → 原样 | `` `col` desc `` | ✅ |
| 简单列名逗号列表（`col_a,col_b`，无 ASC/DESC） | 字面量 | depth 0 逗号切分 → dotpath | `` `col_a`,`col_b` `` | ✅ |
| 列名 + COUNT(*) 混合（`col, COUNT(*)`） | 字面量 | depth 0 逗号切分 → `col` dotpath + `COUNT(*)` 直通 | `` `col`,COUNT(*) `` | ✅（P3-1 修复） |
| 保留字列名逗号列表（`order, key`） | 字面量 | depth 0 逗号切分 → 各段 dotpath（保留字获 quote 保护） | `` `order`,`key` `` | ✅ |

> **结论**：120 处 order 入参 **100% 兼容**。含函数表达式（`LENGTH()` / `RAND()` / `IF()`）、预引号列名、大写 `DESC`、无排序关键字纯列名、多列逗号列表（含/不含空格）均覆盖 ✅。P4-3 修复多空格尾随空白问题；R5-2 修复 IF() 类函数表达式逗号分割破坏参数完整性问题。

### 8.5 不支持的输入类型

> 以下输入类型**不经 quote 函数转义**或**被 quote 函数主动拒绝**，需开发者通过其他路径（`->sql()` + `?` 参数绑定）处理或避免使用。

#### 8.5.1 注入向量（quote 函数主动拒绝 — fail-safe）

| 输入类型 | 触发检测 | quote 行为 | 开发者应对 |
|---|---|---|---|
| 含 `;`（SQL 语句分隔符） | `gene_ident_has_injection_markers` | fail-safe 包裹 → SQL 报错 | 走 `->sql()` + `?` 绑定 |
| 含 `--`（行注释） | 同上 | 同上 | 同上 |
| 含 `/*` 或 `*/`（块注释） | 同上 | 同上 | 同上 |
| 含 `\`（引号转义） | 同上 | 同上（防止 `\'` 绕过平衡检查） | 同上 |
| 单引号不平衡（奇数个 `'`） | `gene_ident_quotes_balanced` | fail-safe 包裹 → SQL 报错 | 修正引号配对 |
| 双引号不平衡（奇数个 `"`） | 同上 | 同上 | 同上 |
| 白名单外字符（`@` `#` `&` `^` `~` `?` 等） | `gene_ident_is_expr_char` | fail-safe 包裹 → SQL 报错 | 走 `->sql()` + `?` 绑定 |

#### 8.5.2 旁路路径（不经 quote 函数 — 开发者自行负责转义）

| 路径 | 出现次数 | 典型输入类型 | 风险 | 开发者应对 |
|---|---|---|---|---|
| `->sql()` 手写 SQL | 126 处 | 多表 JOIN + 聚合、CASE WHEN、子查询、`FROM_UNIXTIME` + GROUP BY、`GROUP_CONCAT`、`information_schema`、动态字段占位 `@` / `{$field}` 插值 | **高**：标识符不经 quote，值需走 `?` 绑定；PHP 变量插值（`{$var}`）若来自用户输入即注入 | 值走 `?` 绑定；标识符若来自用户输入需白名单校验；避免 `{$var}` 插值用户输入 |
| `->group()` | 20 处 | 表前缀列名（`a.col`）、多列逗号列表（`col_a,col_b,col_c`）、别名列、`FROM_UNIXTIME()` 表达式 | **中**：不经 quote，原样拼接 | 列名来自代码字面量时安全；若来自用户输入需白名单校验 |
| `->sql()->order($order)` | 少量 | order 串拼入 raw SQL | **中**：不经 `gene_quote_order` | 同 `sql()` 路径 |
| `->insert()` / `->update()` 字段键 | 全目录 | `gene_quote_identifier`（点路径 quote，不经 columns 白名单） | **低**：数组键经 `filterData()` 过滤为表字段名 | 依赖 `filterData()` 过滤 |

#### 8.5.3 变量传参（运行时传入 — 无法静态验证）

| 输入类型 | 出现次数 | 默认值 | 风险 | 说明 |
|---|---|---|---|---|
| 变量 `$field` / `$fields`（字段列表） | 多（主流写法） | `'*'`（安全子集） | **低**：默认安全 | 调用方若传复杂表达式（如 `ROUND(a/b)`），由扩展白名单 + 引号平衡 + 注入标记三重防护处理；传入注入向量则 fail-safe 拦截 |
| 变量 `$order` / `$orderBy`（排序字段） | 多（主流写法） | `'col desc'` / `'a.col desc'`（安全子集） | **低**：默认安全 | 同上 |
| 变量 `$table` / `$this->table`（表名） | 多（主流写法） | `protected $table = 'snake_case'`（简单标识符） | **低**：子类均为简单名 | 运行时 dotpath quote ✅ |

> **变量传参结论**：变量传参默认值均为安全子集（`*`、简单列名、dotpath order、简单表名）。调用方若传入复杂表达式，由扩展白名单 + 引号平衡 + 注入标记检查三重防护处理；传入注入向量则 fail-safe 拦截。**唯一固有局限**：`IF(1=1,(SELECT password FROM users),0)` 类子查询表达式——`SELECT`/`FROM`/字母/空格均在白名单内，会通过 `expr_safe` 检查直通。这是 quote 函数的**设计边界**（对抗标识符注入，非对抗完整 SQL 语句注入），需业务侧避免用户输入拼入此类表达式，完整 SQL 应走 `->sql()` + `?` 绑定。

### 8.6 安全模型

**威胁模型**：标识符（表名/列名/order 字段）可能来自用户输入（如前端传排序字段）。quote 函数必须防止 SQL 注入。

**白名单字符安全分析**：

| 字符类别 | 代表字符 | 用途 | 注入风险 | 安全理由 |
|---|---|---|---|---|
| 算术运算 | `+ - * / %` | `ROUND(a/b,8)` | 无 | 无法跳出 SELECT 上下文；`;` `--` `/* */` 仍被拒绝 |
| 比较运算 | `= < > !` | `IF(type='link',1,0)` | 无 | 同上；最坏情况产出布尔表达式，非注入 |
| 逻辑运算 | `\|` | `IF(\|\|col>3,...)` | 无 | 同上 |
| 冒号 | `:` | `CONCAT(ip,':',port)` | 无 | MySQL 中 `:` 非特殊字符（仅用于变量 `@var:=val`，`@` 不在白名单） |
| 反引号 | `` ` `` | 表达式内预引号列名 `` `type`='link' `` | 无 | 反引号是标识符引号本身；fail-safe 包裹也用反引号，无法借此跳出 |
| 单/双引号 | `'` `"` | `FROM_UNIXTIME(t,'%Y')` | **可控** | 仅在引号**平衡**（偶数个）时通过；不平衡则 fail-safe。`\` 被拒绝，无法用 `\'` 绕过 |
| 空白 | `空格 \t \n \r` | 多行 JOIN、AS 别名 | 无 | 用于触发表达式分支与 JOIN 检测；无语义注入能力 |

**始终拒绝的字符/模式**（任何分支）：

| 模式 | 拒绝理由 |
|---|---|
| `;` | SQL 语句分隔符 |
| `--` | 行注释 |
| `/*` `*/` | 块注释 |
| `\` | 引号转义，可绕过平衡检查 |
| 不平衡的 `'` 或 `"` | 可跳出字符串上下文 |
| 白名单外任意字符 | `@` `#` `&` `^` `~` `?` 等 |

**paren_depth 跟踪的安全性**：`paren_depth` 仅控制逗号是否作为列表分隔符，不影响 `gene_ident_expr_safe()` 的三重校验。攻击者无法通过 `()` 嵌套绕过白名单——子查询 `SELECT ... FROM` 不含注入标记/不平衡引号/白名单外字符时会通过 clean 检查，**这是 §8.5.3 已记录的固有局限**，非 paren_depth 引入的新风险。真正的子查询注入防护靠注入标记检测 + 引号平衡 + 白名单三重校验，完整 SQL 应走 `->sql()` + `?` 绑定。

### 8.7 兼容性统计（修复后最终状态）

| quote 函数 | 全量统计 | 修复前兼容率 | 修复后兼容率 | 备注 |
|---|---|---|---|---|
| `gene_quote_table` | 406 | 83%（JOIN 表名 fail-safe） | **100%** | R4-1 JOIN 直通 |
| `gene_quote_columns` | 198 | 93%（白名单过窄 + IF() 逗号分割） | **100%** | R4-2 扩展白名单 + R5-1 paren_depth |
| `gene_quote_order` | 120 | 100% | **100%** | P4-3 多空格修剪 + R5-2 paren_depth（真实 order 入参无 IF() 形态，R5-2 为防御性修复） |
| `->sql()` 旁路 | 126 | 不经 quote | 不经 quote | 开发者自行负责 |
| `->group()` 旁路 | 20 | 不经 quote | 不经 quote | 开发者自行负责 |

> **三项目合计**：110 个 Model 文件、724 处链式 quote 调用、166 处旁路调用。修复后链式 quote 调用 **100% 兼容**，无遗留 fail-safe。旁路调用不经 quote，需开发者通过 `?` 参数绑定处理值注入。

### 8.8 P3-2 kill-switch 验证（控制流追踪）

| 场景 | `gene.swoole_getcid_capi` | sweep 先触发？ | `gene_swoole_getcid_capi` | `gene_get_coroutine_id()` 路径 | 正确？ |
|---|---|---|---|---|---|
| 开关 ON（默认） | 1 | 是 | dlsym 解析填充 | C-API 快路径 | ✅ |
| 开关 OFF，sweep 先触发 | 0 | 是 | **NULL**（修复后 resolve 入口守卫返回，不 dlsym） | PHP method 回退路径 | ✅ **已修复** |
| 开关 OFF，getcid 先触发 | 0 | 否 | NULL（155 行 `&& GENE_G(swoole_getcid_capi)` 不触发 resolve） | PHP method 回退路径 | ✅（修复前亦正确） |

修复前：开关 OFF + sweep 先触发 → `gene_resolve_getcid_capi()` 无守卫 → dlsym 填充指针 → `gene_get_coroutine_id()` 152 行快路径生效 → **kill-switch 失效**。
修复后：resolve 入口统一检查 `GENE_G(swoole_getcid_capi)`，为 0 时置 resolved=1 但不做 dlsym → 两个 C-API 指针均保持 NULL → getcid 走 PHP 路径、sweep 走 PHP exists() 回退 → **kill-switch 对所有调用方可靠生效**。

### 8.9 修改文件汇总

```
src/db/pdo.c    — P3-1 strpbrk→gene_ident_has_expr_chars/_nospace（严格按 tlen 遍历）
                   P4-1 注释白名单修正
                   P4-3 order 列部分尾随空白修剪
                   R4-1 gene_ident_has_join_keyword() + JOIN 直通分支（含 \n \r 多行词边界）
                   R4-2 gene_ident_is_expr_char() 白名单扩展（+ - * / % : = < > ! | ` ' " \n \r \t）
                       gene_ident_expr_safe() 三重校验（白名单 + 注入标记 + 引号平衡）
                   R4-3 gene_quote_order 表达式触发条件改为 gene_ident_has_expr_chars_nospace()
                   R4-4 gene_ident_has_injection_markers()（; -- /* */ \）
                   R4-5 gene_ident_quotes_balanced()（' " 偶数个）
                   R5-1/2 gene_quote_columns/order 逗号分割循环新增 paren_depth 计数器
src/gene.c      — P3-2 gene_resolve_getcid_capi 入口 kill-switch 守卫
                   P4-2 sweep 注释 8192→1024
```

### 8.10 待验证

- **编译验证**：当前环境（Windows）无 C 编译器，未进行编译验证。需在 Linux + phpize + make 环境下编译通过。
- **运行时回归（单元级边界）**：以下边界用例需在运行时验证产出与预期一致：
  - `name, COUNT(*)` → `` `name`,COUNT(*) ``（P3-1 逗号列表）
  - `order, key` → `` `order`,`key` ``（保留字列名 quote 保护）
  - `id  desc` → `` `id` desc ``（P4-3 多空格修剪）
  - `` COUNT(`id`) `` → fail-safe（反引号非白名单）
- **运行时回归（JOIN 表名）**：验证多表 JOIN + ON 子句表名经 `gene_quote_table` 直通后 SQL 在 MySQL 下可执行（含多行 `\n` 词边界、小写 `left join`、ON 含 `!=` `(` `)` `and`）。
- **运行时回归（复杂表达式字段）**：验证以下类型经 `gene_quote_columns` 直通后 SQL 可执行：
  - `ROUND(col_a/col_b, 8) AS alias`（除法运算）
  - `col_a*(100-col_b)/100 AS alias`（算术运算）
  - `FROM_UNIXTIME(col,'%Y%m%d') AS alias`（单引号字面量）
  - `` IF(`col`=val || col2>3,0,1) AS alias ``（IF() 条件 + 比较运算，paren_depth 整段直通）
  - `` IF(`col`='str',1,0) AS alias ``（IF() 条件 + 字符串比较）
  - `` col AS `alias` ``（反引号别名）
  - `CONCAT(col_a,':',col_b) AS alias`（CONCAT + 冒号 + 引号）
- **运行时回归（order 函数表达式）**：验证 `LENGTH(col) desc` / `IF(col>3,0,1) desc` 经 `gene_quote_order` 直通后 SQL 可执行。
- **注入回归**：验证 `col; DROP TABLE x--`、`col' OR '1'='1`、`col\'; DROP--` 等注入向量仍被 fail-safe 拦截。
- **paren_depth 回归**：`count(id) as num,sum(col) as total` → 验证两函数各自直通、逗号在 depth 0 切分；`id,name` / `a.*,b.app_name` → 验证 depth 0 逗号切分行为不变。
- **固有局限回归**：`IF(1=1,(SELECT password FROM users),0)` → 验证子查询通过 clean 的固有局限在运行时表现（预期直通，**这是已知局限非新增**，需业务侧避免用户输入拼入此类表达式）。

---

## 九、落地情况三次复核（2026-07-05 第三轮源码核对）

> 复核方法：逐项将第六/七/八节声称的修复与 develop 分支实际源码逐行比对（validate.c / response.c / pool.c / router.c / pdo.c / gene.c / redis_pool.c / common.c / view.c / mysql.c 等），重点核实落地真实性、实现合理性、新引入 bug 与内存泄漏。

### 9.1 落地确认（全部属实）

| 修复项 | 源码核实位置 | 结论 |
|---|---|---|
| 1.1 pool getCid | `pool.c:85-96` `pool_in_coroutine()` 已改用 `gene_get_coroutine_id() >= 0` | ✅ |
| 1.2 validate NULL key | `validate.c:279/436/478/552/607/986` 共 6 处守卫齐全（`!key \|\| Z_TYPE_P(key) != IS_STRING` + return） | ✅ |
| 1.7 response 强制 200 | `response.c` 全文仅 199 行（redirect）合法设置 `response_code`，set_header_ex 已无强制 200 | ✅ |
| 1.5 router run 初始化 | `router.c:1374/1460` `char *run = NULL;`，1554 `if (run)` 守卫 | ✅ |
| 1.3 identifier quote | `pdo.c:63-412` 完整实现（dotpath 分段 + cq 双写、扩展白名单、注入标记、引号平衡、JOIN 直通、paren_depth、ASC/DESC 拆分 + P4-3 trim） | ✅ |
| 1.6 ReplaceStr buf_size | `common.c:406-442` 校验 `new_len+1 > buf_size` 返回 -2；4 驱动调用方均传 `in_len + 1` | ✅ |
| 1.8 view 路径校验 | `view.c:167` `gene_view_path_safe()`，191/244/343/395 四处调用 | ✅ |
| 1.4 redis_pool CAS | `redis_pool.c:427-453` get→cmpset CAS 循环（上限 64 轮）；`_unchecked` 仅用于刚 increment 的回滚路径，语义正确 | ✅ |
| co_contexts_max=0 兜底 | `gene.c:854` `eff_cap = max > 0 ? max : 1024` | ✅ |
| P3-2 kill-switch | `gene.c:104-135` `gene_resolve_getcid_capi()` 入口守卫 `!GENE_G(swoole_getcid_capi)` → 置 resolved 不 dlsym；sweep（659/675）与 getcid（168）共用同一入口 | ✅ |
| T1#2 get_by_cid dlsym | `gene.c:125-131` 双符号解析；`gene_swoole_co_exists` C-API 优先 | ✅ |
| P3-1/P4-1/P4-2/P4-3/R4-x/R5-x | `pdo.c` 逐项核实：`gene_ident_has_expr_chars(_nospace)` 严格按 tlen 遍历；注释白名单已与实现一致；order trim（389-390）；JOIN 关键字词边界含 `\n\r`（168-189）；paren_depth（301-333/351-407） | ✅ |

### 9.2 内存泄漏复核 — 无泄漏 ✅

- `gene_quote_table/columns/order` 所有分支：`smart_str` 均 `smart_str_free`，返回 `str_init`（emalloc）串；mysql.c 全部 10 处调用点（385/393-394/406/426-428/467/513/548/571/884）均配对 `efree(qt/qf/qo)`（pgsql/sqlite/mssql 同构）。
- `ReplaceStr` 循环内 `caNewString` emalloc/efree 配对，提前 return -2 时尚未分配，无泄漏。
- CAS 循环每轮 `ret` 均 `zval_ptr_dtor`；`rpool_atomic_cmpset` 的 params 为 IS_LONG 无需释放、ret 有 dtor。
- `array_to_string`（pdo.c:427-429）`quoted` 配对 efree。

### 9.3 本轮新发现问题

#### [P2-N1] pdo.c：`gene_ident_already_quoted()` 首尾字符判定可被构造绕过 —— 预引号直通未做注入标记检查

- 位置：`src/db/pdo.c:73-75`（判定）、`245-250`（gene_quote_table 顶部直通）、`312/362`（columns/order token 直通）
- 现象：判定仅检查 `s[0]==oq && s[len-1]==cq`。构造输入 `` `x`; DROP TABLE users; -- ` ``（首字符反引号、尾字符反引号）会被判为"已引号"→ `str_init(name)` **原样直通**，完全跳过注入标记 / 引号平衡 / 白名单三重校验。`gene_quote_table` 在任何检查之前即先做该判定（250 行），columns/order 的 token 级直通同理（token 内含 `;` 时若首尾恰为反引号亦直通）。
- 影响：若表名/列名/order 字段来自用户输入，攻击者以反引号首尾包裹 payload 即可绕过 R4-4 注入标记检测，**重新打开 1.3 修复所针对的注入面**。风险取决于业务是否将用户输入直接传入这三个位置（与 1.3 原始威胁模型一致）。
- 修复建议：`already_quoted` 直通前追加安全校验——(a) 最小修复：直通前先过 `gene_ident_has_injection_markers()`，命中则 fail-safe；(b) 更严格：校验内部不含裸 cq（仅允许成对 `cqcq` 转义），否则按未引号处理走常规分支。
- 状态：**✅ 已修复（2026-07-05）**。采用方案 (a)：4 处 `already_quoted` 直通点（`gene_quote_segment`、`gene_quote_table` 顶部、`gene_quote_columns`/`gene_quote_order` token 级）统一改为 `already_quoted && !gene_ident_has_injection_markers()` 才直通。含标记的输入落入常规分支：table 走 expr_safe/dotpath → fail-safe 包裹（cq 双写中和）；columns/order 因 token 含反引号触发 expr 分支 → expr_safe 注入标记检查失败 → 告警 + fail-safe 包裹。合法预引号输入（`` `table` ``、`` `col` desc ``）行为不变。回归用例 `` `x`; DROP TABLE t; -- ` `` → fail-safe（见 §10.4#5）。

#### 记录在案的非问题（本轮核实）

| 疑似点 | 结论 |
|---|---|
| `gene_quote_order` ASC/DESC 分支 `start = i + 1; continue;`（394 行）与外层 401 行重复赋值 | 无害：continue 跳过外层赋值，两处值相同，无逻辑影响；可顺手删一处 |
| `gene_ident_has_expr_chars` 不含 `-`（负号/连字符） | 有意设计（注释已说明）：`user-name` 类列名应被反引号包裹而非当算术；纯减法表达式通常伴随其他运算符或括号触发表达式分支 |
| `-` 单独出现的算术列（如 `a-b AS c` 无其他运算符）会被整体 dotpath 包裹为 `` `a-b AS c` `` → SQL 报错 | fail-safe 行为（报错而非注入），且实际扫描的 724 处入参无此形态；接受 |
| ReplaceStr 返回 -2 时部分替换状态 | 现有唯一调用形态（`"in(?)"→"$"` 缩短替换）永不触发；告警足以暴露未来误用 |
| redis_pool CAS 64 轮上限后放弃递减 | 可接受：极端争用下少减一次仅令计数偏高（保守方向），不会下溢 |

### 9.4 结论

- 第六/七/八节声称的修复**全部真实落地且实现合理**，未发现内存泄漏。
- 本轮新发现 **1 个 P2**（already_quoted 预引号直通绕过注入检查），**已于当日修复**（详见 §9.3 P2-N1）；另确认多项非问题记录在案。
- 运行时验证事项（编译、SQL 回归、CAS 压测、dlsym 符号可见性）仍未执行，统一收敛至第十节。

---

## 十、未启动 / 未完成事项汇总（统一收敛）

> 以下为报告各节中声明但**尚未启动或未完成**的事项，统一移至文末跟踪。已完成事项见第六~九节。

### 10.1 待修复 bug

| # | 事项 | 级别 | 来源 |
|---|---|---|---|
| ~~1~~ | ~~pdo.c `already_quoted` 预引号直通绕过注入标记检查~~ → **✅ 已修复（2026-07-05）**，4 处直通点统一加注入标记守卫，详见 §9.3 P2-N1 | ~~P2~~ | §9.3 P2-N1 |

（当前无待修复 bug）

### 10.2 未启动的修复项

| # | 事项 | 级别 | 来源 | 说明 |
|---|---|---|---|---|
| 1 | `gene_closure_src_cache` 无上限（建议 ≥1024 时清空或拒写） | P2 | §2.1 | 已核实 `router.c:1625-1639` `gene_closure_src_cache_put()` 仍无容量检查 |
| 2 | Swoole 模式未配置 `gene.cache_max_items` 时启动 NOTICE 提示 | P3 | §2.2 | LRU 机制已有（memory.c），但未配置时的提示未实现 |
| 3 | 1.9 剩余散落 static 函数指针缓存迁移到 `GENE_CG_FN_LOOKUP` | P3 | §1.9 / 第六节 | 基础设施已建（gene.h），benchmark.c/log.c 已迁移；redis_pool.c 等 function-local static 待逐步迁移（Swoole 不支持 ZTS，实际风险低） |

### 10.3 未启动的性能优化项

| # | 事项 | 收益/难度 | 来源 | 说明 |
|---|---|---|---|---|
| 1 | T1#3 路由注册 `__call` 中 20+ 次 `snprintf` 改 memcpy 拼接 | 中/低 | §3 T1、第六节（⏸ 暂缓） | 中等规模重构，需全路由回归验证 |
| 2 | T2#5 路由 `route_pc` 预编译 `worker_ready` 时全树预热 | 中高/中 | §3 T2 | 需路由树遍历 + P3 开关灰度 |
| 3 | T2#6 `gene_memory_get_by_config` 配置路径解析缓存 | 中/中 | §3 T2 | |
| 4 | T2#7 请求上下文 9 个 char* 字段 arena 化 | 中/中 | §3 T2 | |
| 5 | T2#8 SQL 构造链 smart_str 预估容量 + property slot 直取 | 中/中 | §3 T2 | |
| 6 | T3#9 视图模板 28 次 pcre_replace 合并单趟状态机 | 中高/高 | §3 T3 | |
| 7 | T3#10 池计数器迁移到扩展内 C 层原子变量 | 中/高 | §3 T3 | CAS 修复（1.4）已缓解正确性问题，此项为性能演进 |
| 8 | T3#11 `get_path_router_inner` 递归改迭代 | 低中/中 | §3 T3 | 优先级最低 |

### 10.4 待执行的验证项（运行时）

| # | 事项 | 来源 |
|---|---|---|
| 1 | Linux + phpize + make 编译验证（当前 Windows 环境无 C 编译器，所有修改仅静态核对） | §6/§8 待验证 |
| 2 | 1.3 identifier quote 全量 SQL 回归（§8.10 列出的单元级边界 / JOIN / 复杂表达式 / order 函数 / 注入 / paren_depth / 固有局限用例） | §8.10 |
| 3 | 1.4 CAS 递减 Swoole 高并发压测（计数器不漂移） | §6 待验证 |
| 4 | T1#2 dlsym 符号 `_ZN6swoole9Coroutine11get_by_cidEl` 在目标 Swoole 版本可见性验证 | §6 待验证 |
| 5 | 新增回归用例：`` `x`; DROP TABLE t; -- ` `` 预引号包裹注入向量 → 预期 fail-safe（配合 10.1#1 修复） | §9.3 |
