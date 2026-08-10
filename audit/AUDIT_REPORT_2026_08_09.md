# Gene 扩展框架审计报告：近两周优化 + 新增 ORM（合理性 / 内存安全）

> 审计版本：6.0.0（HEAD `daa9512`）
> 审计日期：2026-08-09
> 审计范围：`2026-07-30 ~ 2026-08-08` 全部提交（`git diff f198821..HEAD`，共 114 文件、+9667/-8588），重点为
> **新增模块 `src/orm/`（meta.c + model.c + query.c，约 1700 行）** 与 08-06/08-07/08-08 三批优化
> 审计方式：**静态代码审查 + 运行时实测**（与前几轮纯静态审计不同，本轮跑通了扩展）
> 运行环境：Windows / PHP 8.1.34 NTS x64 / `php_gene.dll`（2026-08-08 12:19 构建，含 ORM v1 + 08-08 加固）
> 复现脚本：`audit/repro/*.php`（每条运行时结论均可一键复现）
> **修复状态（2026-08-09 当日闭环）**：H1/H2/H3、M1/M2/M3/M4/M5/M7、L1、DatabaseTest 同步均已修复、
> 重新编译并实测回归通过，M6 保留在 `audit/plan/PLAN.md` §7.2.3 —— 详见 §九「修复落地与复盘」。

---

## 一、总体结论

1. **ORM 的内存管理是干净的。** 14 条 ORM 路径（find / findAll / paginate / create / updateBy / destroy /
   destroyAll / save / delete / fill / \_\_get / \_\_set / query 链 / 弃用未终结的 query）在预热后实测
   **0 B/call 增长**，异常路径（表不存在、唯一键冲突）同样 0 B/call。
   `ctx->orm_meta` 的 init / reset / destroy / pool-acquire 四处生命周期齐备，meta 结构体的
   `zend_string_copy` ↔ `release`、`smart_str` → `ZVAL_STR` → `dtor` 所有权转移全部配对正确。
   **未发现 ORM 泄漏、重复释放或悬垂指针。**
2. **但本轮发现 3 个必须修的实锤问题，其中 2 个是运行时实测确认的：**
   - **H1（崩溃，最高优先级）**：`Gene\Db\*` 构造函数在 config 缺 `username`/`password` 时**直接段错误**。
     框架自己新加的 `test/OrmTest.php` 就命中了它 —— 跑测试套件的结果不是「失败」，而是 **PHP 进程崩溃
     （0xC0000005）**。这不是 ORM 引入的缺陷，但**被 ORM 的新测试首次暴露**。
   - **H2（新引入的真实泄漏）**：08-07 给 `Router::match()` 加的「上下文隔离」修复，在**未命中分支漏了一次
     `zval_ptr_dtor`**，每次失配泄漏一个 `zend_array`。实测 **56 B/call**（2 万次 → 1.12 MB）。
   - **H3（ORM 数据正确性）**：`timestamps` 用 `sapi_get_request_time()` 取时间，**全仓仅此一处**（其余 11 处
     均为 `time(NULL)`）。在 Swoole/常驻进程下 `created_at`/`updated_at` 会**冻结在 worker 起始时刻**。
     CLI 实测已可见 3 秒漂移。
3. **ORM 的功能设计存在一处结构性缺陷（M1）**：它自称 ActiveRecord，但 `find()`/`findAll()`/`query()->row()`
   **一律返回数组、从不返回模型实例**，而 `exists` 标记**只能由一次成功的 insert 置位**。于是最自然的写法
   `(new U)->fill(U::find($id))->save()` 会走 **INSERT 分支并带上主键** → 唯一键冲突 / 数据重复。
   实测已复现 `PDOException: UNIQUE constraint failed`。这是 v1 必须先补齐的语义缺口，而非锦上添花。
4. **驳回 4 条候选发现**（含并行复核提出的 3 条），见第七节。其中「ORM find() 泄漏 44 B/call」是
   **测量假象** —— 真实原因是默认开启的调试 SQL history 填到 200 条上限；这条差点被写成误报，值得记录。
5. 总评：**近两周的优化批次质量依然高**（错误路径释放、persistent 堆 key 复制、CAS 提取、per-ctx 隔离等
   都落地正确），风险已不在「老代码的内存安全」，而集中在 **①新加固代码自身的收尾遗漏（H2）**、
   **②新 ORM 与既有 Db 层的契约衔接（H1/H3/M1/M4/M5）**。

---

## 二、问题清单（按优先级）

| # | 等级 | 位置 | 问题 | 证据 |
|---|------|------|------|------|
| **H1** | 崩溃 | `src/db/pdo.c:513,516` | 缺 `username`/`password` 时解引用 NULL → 段错误，4 个驱动全中 | **运行时实测** |
| **H2** | 泄漏 | `src/router/router.c:2241,2299` | `match()` 未命中分支泄漏 `zend_array`，56 B/call | **运行时实测** |
| **H3** | 正确性 | `src/orm/meta.c:312` | `timestamps` 时间戳在常驻进程下冻结 | **运行时实测** |
| **M1** | 功能缺陷 | `src/orm/model.c` 全局 | 查询不返回模型 + `exists` 不可 hydrate → `fill(find())->save()` 变重复 INSERT | **运行时实测** |
| **M2** | 一致性 | `model.c:366,651` | `create()`/`save()` 返回**字符串** id，且把字符串主键写回 attributes | **运行时实测** |
| **M3** | 健壮性 | `model.c` / `query.c` 各 db 调用点 | 异常挂起后继续下发后续 SQL 调用，无 `EG(exception)` 门禁 | 静态 |
| **M4** | 正确性 | `src/orm/meta.c:337-340` | `gene_orm_db_limit()` 用**类名子串** `strstr(cname,"Mysql")` 判方言，与 `gene_orm_db_reset()` 的 `ce ==` 判法不一致 | 静态 |
| **M5** | UAF 风险 | `src/orm/meta.c:198-217` | `gene_orm_get_db()` 返回**借用指针**，静态方法跨多次 `call_user_function` 持有 | 静态 |
| **M6** | 功能缺口 | `src/orm/query.c` | 无 join/group/having/union/offset/first/update/delete、无事务/关联/软删除；`$fields` 不做写白名单 | 静态 |
| **M7** | 功能缺口 | `model.c:845-861` | 未实现 `__isset`/`__unset` → `isset($m->name)` 恒 false、`unset()` 静默无效 | 静态 |
| **L1** | 安全一致性 | `src/http/response.c:786-820` | `sendFile()` FPM 分支拒绝非 plain-file wrapper，**Swoole 分支完全不校验** | 静态 |
| **L2** | 并发 | `src/orm/*` | ORM 静态方法在共享 Db 对象上改状态；跨协程共享同一 DI `db` 不安全 | 静态 |
| **L3** | 观察 | `src/db/*.c`（history） | `gene.run_environment=0`（默认）下每条 SQL 都入 history，单驱动单请求上限 200 条 ≈ 180 KB | **运行时实测** |
| **L4** | 观察 | `model.c:361-368` | `create()` = insert + lastId 两次独立调用，无事务包裹 | 静态 |

---

## 三、H 级问题详情

### H1 — `gene_pdo_construct()` NULL 解引用导致段错误（崩溃）

`src/db/pdo.c:505-523`：

<ref_snippet file="F:\github_code\gene\src\db\pdo.c" lines="505-523" />

```c
zval params[4] = { *dsn, *user, *pass, *options };   // user/pass 可能为 NULL
```

4 个驱动的 `*InitPdo()` 都用 `zend_hash_str_find()` 取 `username`/`password`，**未命中返回 NULL 且不做
兜底**就直接传入：

- `src/db/sqlite.c:245-246,262`
- `src/db/mysql.c:254`
- `src/db/pgsql.c:258`
- `src/db/mssql.c:246`

SQLite DSN 本来就不需要用户名密码，因此这是 SQLite 用户的**必然踩点**。

**实测**（`audit/repro/pdo_construct_null_crash.php`）：

```
STEP A: about to construct with dsn only
EXIT=-1073741819        # 0xC0000005 访问违例
```

框架自带的新测试 `test/OrmTest.php:96-98` 正是这样构造连接，因此：

```
> php test/OrmTest.php
=== Gene ORM Test Suite ===
... 27 项 class surface 全部 ✓
Testing ORM CRUD (SQLite):
<进程崩溃，exit 0xC0000005>
```

**对比**：连接池路径**是正确的** —— `src/db/pool.c:239-259` 的 `pool_normalize_config()` 会把缺失的
`username`/`password` 补成空串。修复应把同一约定下沉到 `gene_pdo_construct()`：

```c
zval z_empty_user, z_empty_pass;
if (!user) { ZVAL_EMPTY_STRING(&z_empty_user); user = &z_empty_user; }
if (!pass) { ZVAL_NULL(&z_empty_pass);         pass = &z_empty_pass; }
```

（`PDO::__construct` 的 `$username`/`$password` 均为 nullable，传 NULL zval 即可。）
建议同时把 `test/OrmTest.php`、`test/DatabaseTest.php` 的 dsn-only 用例保留为回归用例 ——
它是这个 bug 的天然哨兵。

> 附带发现：`test/DatabaseTest.php:191` 调用了不存在的 `Sqlite::connect()`，测试套件在此 fatal；
> `Sqlite::getHistory()` 同样不存在。测试文件与 6.0.0 的实际 API 已脱节。

### H2 — `Router::match()` 未命中分支泄漏（新引入）

08-07 的「match 上下文隔离」修复把 `ctx->path_params` 挪走后调用了
`gene_router_reset_path_params()`，该函数在 `IS_UNDEF` 时会 **`array_init()` 新建一个数组**
（`router.c:174-191`）。命中分支正确释放了它（`router.c:2344-2346`），**未命中分支没有**：

<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="2293-2301" />

```c
/* No match — nothing was captured; restore the request context untouched. */
...
ctx->path_params = saved_path_params;   // 覆盖掉 2241 行新建的数组 → 泄漏
RETURN_FALSE;
```

**实测**（`audit/repro/router_match_leak.php`，2 万次失配）：

```
Router::match() miss    +1120000 B (56.00 B/call)
```

56 B 正好是一个空 `zend_array`。FPM 下由请求末尾的 ZMM 兜底（valgrind/ASAN 可见）；
**Swoole 常驻协程内循环调用 `match()` 做预检则是真实增长**。

**修复**：未命中分支补一次释放，与命中分支对称：

```c
if (Z_TYPE(ctx->path_params) == IS_ARRAY) {
    zval_ptr_dtor(&ctx->path_params);
}
ctx->path_params = saved_path_params;
```

顺带建议：命中/未命中两段恢复代码完全重复，抽成一个 `restore:` 标签，避免下次再漏一处。

### H3 — ORM 时间戳在常驻进程下冻结

`src/orm/meta.c:304-323`：

<ref_snippet file="F:\github_code\gene\src\orm\meta.c" lines="304-323" />

```c
t = (time_t)sapi_get_request_time();
```

全仓 12 处取时间，**只有这一处**用 SAPI 请求时间，其余（`memory.c`、`pool.c`、`redis_pool.c`、
`session.c`、`router.c`、`application.c`）全部是 `time(NULL)`。

SAPI 请求时间在一次 SAPI 请求内是常量。CLI / Swoole Server 的一次 SAPI「请求」覆盖整个进程或整个
worker 生命周期，因此写入的 `created_at`/`updated_at` 会停在起点。

**实测**（`audit/repro/orm_timestamps_stale.php`）：

```
request_time    : 2026-08-09 15:24:13
first                created_at=2026-08-09 15:24:13 updated_at=2026-08-09 15:24:13
second (3s later)    created_at=2026-08-09 15:24:13 updated_at=2026-08-09 15:24:13   <-- 应为 15:24:16
wall clock now  : 2026-08-09 15:24:16
```

**修复**：改用 `time(NULL)`，与框架其余部分一致。

---

## 四、ORM 功能合理性评估

### 已验证可用（`audit/repro/orm_crud_smoke.php`，全绿）

`create` → `find` → `updateBy` → `find` → `paginate` → `query()->where()->order()->all()` →
`query()->count()`（正确返回 `int`）→ `query()->limit(1)->row()` → 实例 `fill/save/toArray` →
`__set/__get` → `save()` 更新分支 → `delete()`（正确清掉 pk 与 `exists`）→ `destroy` → `destroyAll`。

链式状态隔离经专项验证（`audit/repro/orm_state_isolation.php`）：静态方法之间、异常抛出之后、
弃用未终结的 `query()` 之后、以及与裸驱动混用之后，后续查询**均不受污染**。
`create()` 不篡改调用方数组（`SEPARATE_ARRAY` 生效）。`Query::__destruct` 的 dirty 门闩有效。
**这部分设计是站得住的。**

### M1 — 结构性缺陷：查不出模型，`exists` 无法 hydrate

`find()` / `findAll()` / `paginate()` / `Query::row()`/`all()` **全部返回数组**
（`model.c:228`、`264`、`319`，`query.c:349`），而 `exists` 属性**只在 insert 成功后**被置 1
（`model.c:664-665`）。ORM 也没有提供 `hydrate()` / `newFromRow()` / `find($id, /*asModel*/ true)`。

后果：把查询结果装回模型再保存 —— 最自然的 CRUD 写法 —— 会走 INSERT 分支，并且因为
`fill()` 把主键也写进了 attributes，INSERT 会带上显式主键：

```php
$m = new U();
$m->fill(U::find($id));   // exists 仍为 false
$m->name = 'renamed';
$m->save();               // → INSERT ... (id, name, ...) → 唯一键冲突
```

**实测**（`audit/repro/orm_edge_cases.php`）：

```
PHP Fatal error: Uncaught PDOException: SQLSTATE[23000]: Integrity constraint violation:
19 UNIQUE constraint failed: orm_users.id ... Gene\Orm\Model->save()
```

在无唯一约束的表上，同一段代码会**静默写入重复行**，这比抛异常更危险。

**建议（任选其一，推荐前两条同时做）**：
1. `fill()`（或新增 `hydrate()`）在数据里含非空主键时把 `exists` 置 1；
2. 提供 `find($id, true)` / `Query::first()` 返回模型实例（内部置 `exists=1`）；
3. `save()` 的 INSERT 分支在 payload 含主键时改走 UPDATE，或至少 `E_WARNING` 提示。

同时建议明确 `$fields` 的语义：目前它**只作为 SELECT 列表**，`fill()`/`__set()` 完全不校验字段名
（连数字键都能写进 attributes，`model.c:550-552`），缺少 mass-assignment 防护。

### M2 — 返回类型不一致

`create()` 与 `save()` 直接把驱动 `lastId()` 的返回值透传，PDO 给的是**字符串**：

```
create() => string        // '1'
save() insert => string   // '2'
count() => integer        // Query::count() 已在 08-08 加固为 int
```

并且 `save()` 会把**字符串主键**写回 attributes（`model.c:655-660`），于是
`$m->toArray()['id'] === '2'`，而 `find()` 返回的 `id` 是 `int(1)` —— 同一字段两种类型。
建议 `create()`/`save()` 与 `Query::count()` 对齐，统一 `ZVAL_LONG`（或按 pk 列类型转换）。

### M3 — 缺少异常门禁

`gene_orm_db_call()` 包的是 `call_user_function`：被调方抛异常时它**仍返回 SUCCESS**。
`find()` 因此会在 `where()` 已抛异常的情况下继续下发 `limit()` 和 `row()`（`model.c:212-234`），
`paginate()`/`save()`/`destroy*()` 同理。实测未导致泄漏（清理路径都在函数尾部，都会走到），
但会产生无意义的二次错误、掩盖首因。建议每次 db 调用后加：

```c
if (UNEXPECTED(EG(exception))) goto cleanup;
```

### M4 — 方言判定手法不一致，会给非标准驱动生成错误 SQL

`gene_orm_db_reset()` 用类指针比较（`meta.c:227-234`，正确），但 `gene_orm_db_limit()` 改用类名子串：

<ref_snippet file="F:\github_code\gene\src\orm\meta.c" lines="328-350" />

```c
if (strstr(cname, "Mysql") || strstr(cname, "mysql")) mysql_style = 1;
```

MySQL 的 `limit($a,$b)` 是 `LIMIT offset,count`，其余驱动是 `LIMIT count OFFSET offset`，
两者**参数顺序相反**。因此任何「实际是 MySQL 但类名不含 Mysql」的句柄（自定义子类、
`Gene\Db\Pdo` 直连、连接池包装对象）都会拿到**颠倒的 offset/limit**，
`paginate()` 静默返回错误的页。反之，类名里恰好含 "mysql" 的非 MySQL 驱动同样中招。
建议与 `gene_orm_db_reset()` 统一为 `ce ==` 比较，未知驱动走文档化的默认语义并可选告警。

### M5 — `gene_orm_get_db()` 借用指针跨用户代码调用

`meta.c:198-217` 把 `gene_di_get()` 的返回值**原样返回**（DI 注册表里的借用 zval），
`model.c` 的静态方法随后跨 3~5 次 `call_user_function` 持有它。若期间的用户代码
（getter、PDO 错误处理、`__destruct`、hook）调用了 `Di::del('db')` 或 `Di::set('db', $other)`，
对象即被释放 → **UAF**。

`Query` 路径是安全的（`query_init` 把 db 存进属性，持强引用，`query.c:180-183` 的注释也点明了这一点），
`gene_di_get()` 自身也已在 08-07 用 `zend_string_copy` 处理了同类的别名悬垂问题（`di.c:159-162`）——
说明这个风险模型团队已经认识到了，只是没覆盖到 ORM 的 db 句柄。
建议静态方法入口 `Z_TRY_ADDREF_P(db)`、出口 `zval_ptr_dtor`。

### M6/M7 — 能力与语义缺口（v1 可接受，但应记入 PLAN）

- `Query` 仅有 `where/in/order/limit/all/row/cell/count`；缺 `join/group/having/union/offset/
  first/exists/pluck/update/delete`，也没有事务、关联（hasMany/belongsTo）、软删除、属性转换。
  底层 Db 层其实已具备 `join/union/group/having`（08-06/08-07 刚补齐），ORM 未透出，落差明显。
- API 对称性：`Model::where()` 返回 `Query`，`Model::find()` 返回数组；有 `Model::updateBy()`
  却没有 `Query::update()`；`Model::destroy($id)` 与实例 `delete()` 语义重叠。
- 未实现 `__isset`/`__unset`：`isset($m->name)` / `empty($m->name)` / `$m->name ?? $d`
  对 attributes 恒判为「不存在」，这是 PHP 侧最常见的踩点之一。
- `Query` 在 PHP ≥ 8.2 打开了 `ALLOW_DYNAMIC_PROPERTIES`（`query.c:440-442`），
  与 `final` + protected 内部状态的封闭设计相悖，建议去掉。

---

## 五、内存安全核查明细（近两周改动）

### 5.1 ORM 运行时泄漏实测

`audit/repro/orm_leak_probe.php`（先预热 1000 次填满 history 上限，再各 5000 次）：

```
NA::find(1)                                                 +0 B (0.00 B/call)
NA::query()->where(str,bind)->limit(1)->row()               +0 B (0.00 B/call)
NA::paginate()                                           +1520 B (0.30 B/call)
NA::create()+destroy()                                   -5280 B (-1.06 B/call)
save/delete instance                                        +0 B (0.00 B/call)
destroyAll                                                -776 B (-0.16 B/call)
Model::where()->count()                                  +4680 B (0.94 B/call)
```

异常路径（`audit/repro/orm_exception_paths.php`，3000 次）：

```
failing find() on missing table                       +0 B (0.00 B/call)
failing create() on missing table                     +0 B (0.00 B/call)
```

亚字节级的正负抖动是分配器 bin 复用噪声，非单调增长。**结论：ORM 无泄漏。**

### 5.2 静态核查（逐条通过）

| 核查项 | 位置 | 结论 |
|--------|------|------|
| `ctx->orm_meta` 生命周期 | `gene.c:305`(init) / `472-475`(free_fields) / `565`(pool acquire) | ✅ init/reset/destroy/池化四处齐备，与 `di_regs` 同策略 |
| meta 结构体引用计数 | `meta.c:45-96,177-196` | ✅ `from_array`/`to_array`/`release` 三者 `zend_string_copy` ↔ `release`、`ZVAL_COPY` ↔ `zval_ptr_dtor` 全配对 |
| meta 加载失败早退 | `meta.c:137-145` | ✅ 抛异常处尚未分配任何资源，无泄漏 |
| `smart_str` → `ZVAL_STR` 所有权转移 | `model.c:217,458,507,629,724`、`meta.c:112` | ✅ 每处恰好一次释放；`apply_where` 的 `cond`+`args[0]` 双释放对应 refcount 2，正确 |
| `create/updateBy/save` 入参不可变性 | `model.c:353-354,397-398,607-608` | ✅ `ZVAL_COPY`+`SEPARATE_ARRAY`，运行时已验证调用方数组不被篡改 |
| `save()` 更新分支先拷贝 pk 再 `hash_del` | `model.c:612-615` | ✅ 规避共享 HT 上的 UAF，注释也说明了动机 |
| `Query` dirty 门闩 + `__destruct` 复位 | `query.c:167,171-175,219-234` | ✅ 错误路径同样 `finish()`，运行时验证弃用 query 不污染后续查询 |
| `Query::count()` 返回类型加固 | `query.c:398-413` | ✅ 08-08 的 int 归一化生效（实测 `is_int` 为真） |
| pool / redis_pool named_cache persistent key | `pool.c:42-85`、`redis_pool.c:42-76` | ✅ `pemalloc` + key 复制到 persistent 堆 + `runtime_type>=2` 门禁，MSHUTDOWN 有清理 |
| `memory.c` TTL sweep | `memory.c:462-487` | ✅ 固定 64 槽批处理有 break 边界；key `copy`/`release` 配对 |
| `db/pool.c` CAS 递减 | `pool.c:524-556` | ✅ 与 `redis_pool.c` 对称，上一轮 C1 已闭环 |
| `union()` 的 `SEPARATE_ARRAY` | `mysql.c:1092`、`pgsql.c:1077`、`sqlite.c:1080`、`mssql.c:1072` | ✅ 4 驱动一致 |
| `sqlite` attach/detach 新方法 | `sqlite.c:1434-1541` | ✅ 标识符校验 + NUL 字节拒绝 + `smart_str` 错误路径无泄漏 |
| `controller.c` forward 深度门禁 | `controller.c:268-296`、`gene.c:1141-1143` | ✅ RSHUTDOWN 复位，bailout 不会卡死计数 |
| `session.c` `regenerateId()` | `session.c:1208-1258` | ✅ `old_id` copy/release 配对 |

### 5.3 L 级观察项

- **L1 `sendFile()` 两条分支门禁不对称**：FPM 分支（`response.c:816-820`）拒绝一切非
  `php_plain_files_wrapper`，**Swoole 分支（`response.c:786-805`）把 `$file` 原样交给
  `Swoole\Http\Response::sendfile()`，没有任何校验**。同一份应用代码在两种运行模式下安全边界不同。
  两条分支都不做路径规范化 —— 这可以是「调用方责任」，但**必须在文档里写明**，
  且建议把 wrapper 校验提到分支之前统一执行。
- **L2 协程共享 Db 句柄**：ORM 静态方法在共享的 Db 对象上顺序改写 select/where/limit 属性。
  `di_regs` 是 per-ctx 的，正常用法下每协程各持一份；但一旦把 Db 实例注册到跨协程可见的位置
  （或 PDO 被 Swoole runtime hook 后在 SQL 中让出），状态就会交叉。建议文档明确要求 ORM 走
  `pool` 配置或 per-coroutine 句柄。
- **L3 默认开启的 SQL history**：`gene.run_environment=0`（默认）下每条 SQL 都写入
  `ctx->db_*_history`，上限 `GENE_DB_HISTORY_MAX=200`。实测填满约 **177 KB**。
  上限有效、不是泄漏，但**它在 profiling 中长得像 44 B/call 的泄漏**（见第七节 R4），
  建议文档提示生产环境置 `run_environment=1`。
- **L4 `create()` 无事务**：`insert` 与 `lastId` 是两次独立驱动调用，同一句柄上的并发写入
  理论上可拿到他人的 id。建议文档说明或提供 `createInTransaction()`。

---

## 六、修复建议排序

| 顺序 | 项 | 改动量 | 说明 |
|------|-----|--------|------|
| 1 | **H1** `gene_pdo_construct()` NULL 兜底 | ~6 行 | 修崩溃；顺带让 `test/OrmTest.php` 能跑完 |
| 2 | **H2** `Router::match()` 未命中分支补 dtor | ~3 行 | 修实锤泄漏；建议同时抽 `restore:` 标签 |
| 3 | **H3** `sapi_get_request_time()` → `time(NULL)` | 1 行 | 修 Swoole 下的错误时间戳 |
| 4 | **M2** `create()`/`save()` 返回 int | ~4 行 | 消除同字段双类型 |
| 5 | **M1** `fill()`/`hydrate()` 置 `exists` + `find(..., asModel)` | 中 | 消除重复 INSERT 陷阱，v1 语义补齐 |
| 6 | **M4** limit 方言判定改 `ce ==` | ~10 行 | 修非标准驱动的错误分页 |
| 7 | **M5** ORM db 句柄加引用 | ~10 行 | 关闭 UAF 窗口 |
| 8 | **M3** 各 db 调用点加 `EG(exception)` 门禁 | 中 | 首因不被掩盖 |
| 9 | **M7** `__isset`/`__unset` | 小 | 常见踩点 |
| 10 | **L1** sendFile 门禁前置到分支之前 | 小 | 两模式安全边界一致 |
| 11 | **M6** Query 能力补齐（join/group/first/事务…） | 大 | 记入 `audit/plan/PLAN.md` 分批推进 |
| 12 | 修 `test/DatabaseTest.php` 与 6.0.0 API 脱节（`connect()`/`getHistory()` 不存在） | 小 | 测试套件当前在此 fatal |

---

## 七、驳回的候选发现

| # | 候选结论 | 复核结果 |
|---|----------|----------|
| R1 | `router.c:2194/2299/2351` 保存/恢复 `path_params` 未处理引用计数 → UAF | **驳回**。这是所有权**移动**（保存后立即 `ZVAL_UNDEF` 原位），引用计数守恒，加 `ADDREF` 反而会造成泄漏。真正的缺陷是同一段代码**未命中分支少了一次 dtor**（已记为 H2） |
| R2 | `di.c:481` `has()` 使用 `gene_di_resolve_alias()` 的借用指针 → UAF | **驳回**。`resolve_alias` 与 `zend_hash_exists` 之间不执行任何用户代码，别名表不可能在此期间 rehash。`get()`/`instance()` 需要拷贝是因为它们中间会调构造函数 —— 二者情形不同。（真实的同类风险在 ORM 的 db 句柄上，已记为 M5） |
| R3 | `validate.c` `sometimes()` 存回调时引用计数处理可疑 | **驳回**。`ZVAL_COPY` + `zend_hash_update` 组合本身收支平衡，未见失衡路径 |
| R4 | ORM `find()` 泄漏 44 B/call（首轮实测数据） | **驳回：测量假象**。任意放在探针序列**首位**的操作都会呈现 ~44 B/call、总量恒为 ~177 KB —— 真实原因是默认开启的调试 SQL history 正在填向 200 条上限（见 L3）。预热后所有 ORM 路径归零。**这条差点被写成误报，记录在此以备后续审计参考** |

---

## 八、复现脚本清单（`audit/repro/`）

| 脚本 | 用途 | 对应发现 |
|------|------|----------|
| `pdo_construct_null_crash.php` | dsn-only 构造 → 0xC0000005 | H1 |
| `router_match_leak.php` | 2 万次失配 `match()` → 56 B/call | H2 |
| `orm_timestamps_stale.php` | 时间戳冻结（3 秒漂移） | H3 |
| `orm_crud_smoke.php` | ORM 全 CRUD 冒烟（全绿） | 第四节 |
| `orm_edge_cases.php` | 标量守卫 / 私有构造 / `fill(find())->save()` 陷阱 / 返回类型 | M1、M2 |
| `orm_leak_probe.php` | 预热后 7 路径泄漏探针（全 0） | 5.1 |
| `orm_exception_paths.php` | 异常路径泄漏探针（全 0） | 5.1 |
| `orm_state_isolation.php` | 静态调用间 / 异常后 / 弃用 query 后 / 混用裸驱动的状态隔离 | 第四节 |

> 运行前提：已安装含 ORM 的 `gene` ≥ 6.0.0，且已加载 `pdo_sqlite`。
> 除 `pdo_construct_null_crash.php`（预期崩溃）外，其余脚本均应以 exit 0 结束。

---

## 九、修复落地与复盘（2026-08-09 当日）

> 修复构建：`php_gene.dll`（2026-08-09 23:58 构建，`F:\php_src\php-8.1.30-src` x64 Release NTS，
> 已部署至运行环境）。验证环境与本报告审计环境一致（PHP 8.1.34 NTS x64 CLI）。
> 改动文件：`src/db/pdo.c`、`src/router/router.c`、`src/orm/{meta,model,query}.c`、`src/orm/orm.h`、
> `src/http/response.c`、`test/{OrmTest,DatabaseTest}.php`。

### 9.1 逐项落地状态

| # | 状态 | 修复内容 | 实测证据（修复后复跑） |
|---|------|----------|------------------------|
| H1 | ✅ 已修复 | `gene_pdo_construct()` 对 NULL `user`/`pass` 兜底（user→空串，pass→NULL zval），与 `pool_normalize_config()` 同约定；4 驱动一处生效 | `pdo_construct_null_crash.php`：STEP A→C 全部打印，exit 0（原为 0xC0000005） |
| H2 | ✅ 已修复 | `match()` 命中/未命中恢复代码抽为共享 `restore:` 标签，未命中分支同样 dtor 新建的 `path_params` 数组 | `router_match_leak.php`：`+0 B (0.00 B/call)`（原 56 B/call × 2 万次 = 1.12 MB） |
| H3 | ✅ 已修复 | `gene_orm_apply_timestamps()` 改 `time(NULL)`，与全仓其余 11 处一致 | `orm_timestamps_stale.php`：3 秒后 `created_at` 推进 3 秒，与墙钟一致（原冻结） |
| M1 | ✅ 已修复 | ①`fill()` 在合并后 attributes 含非空主键时置 `exists=1`；②`find($id, $asModel=false)` 第二参为 true 时返回 hydrated 模型实例（未命中返回 null） | `orm_edge_cases.php`：`fill(find())->save()` 行数保持 1（UPDATE，原为唯一键冲突/重复行）；`OrmTest` 新增 5 项断言全绿 |
| M2 | ✅ 已修复 | 新增 `gene_orm_normalize_id()`：`create()`/`save()` 的 `lastId` 返回值数字串归一为 long，写回 attributes 的主键同为 int | `create() => integer`、`save() insert => integer`（原均为 string） |
| M3 | ✅ 已修复 | `orm.h` 新增 `gene_orm_has_exception()` 内联门禁；`model.c` 全部 9 个 db 调用序列与 `query.c::gene_orm_query_apply()` 每步后 `goto cleanup`/`return FAILURE`；`gene_orm_db_reset()` 未知驱动回退分支在异常挂起时跳过用户态 `reset()` | `orm_exception_paths.php`：异常路径 0 B/call，异常后健康查询不受污染 |
| M4 | ✅ 已修复 | `gene_orm_db_limit()` 改 `ce == gene_db_mysql_ce` 判定（与 `gene_orm_db_reset()` 一致），未知驱动走文档化默认 `LIMIT count OFFSET offset` | `orm_crud_smoke.php` paginate 断言通过；静态审查确认子串判定已移除 |
| M5 | ✅ 已修复 | `gene_orm_get_db()` 出口 `Z_TRY_ADDREF_P` 返回持有引用；`model.c` 全部 10 处调用点（含 `gene_orm_new_query`）在用毕 `zval_ptr_dtor(db)` | 全部泄漏探针（leak_probe / edge_cases / exception_paths）保持 0 B/call，无新增泄漏 |
| M7 | ✅ 已修复 | 新增 `__isset`/`__unset`（attributes 语义，`exists`/`attributes` 保留名只读），注册进方法表 | `OrmTest`：`isset($m->name)` 为 true、`unset()` 后变 false |
| L1 | ✅ 已修复 | `sendFile()` 的 plain-file wrapper 校验前置到 Swoole/FPM 分支之前，两模式安全边界一致 | 静态审查；运行时冒烟（CLI/FPM 分支）通过 |
| M6 | 📌 保留 PLAN | Query 能力补齐（join/group/having/union/first/update/delete、事务、关联、软删除、mass-assignment 白名单）属大改动，记入 `PLAN.md` §7.2.3 分批推进 | — |
| 测试同步 | ✅ 已修复 | `DatabaseTest.php`：SQLite 段按 6.0.0 真实 API 重写（`sql()->execute()`/`select()->where()->row()`/`history()` 等），dsn-only 构造保留为 H1 回归哨兵；全文件 `catch (Exception)` → `catch (Throwable)`，失效 API 降级为报告项不再中断套件；attach 测试的字符串 config 构造改为数组形式 | 套件跑至 `=== Complete ===` 退出 0（原在 `Sqlite::connect()` 处 fatal） |
| 附带项 | ✅ 已修复 | 去掉 `Query` 的 `ALLOW_DYNAMIC_PROPERTIES`（原 §四 M6/M7 小节建议），与 `final`+protected 封闭设计对齐 | `OrmTest` class surface 全绿 |

### 9.2 回归验证汇总（修复构建上全量复跑）

```
pdo_construct_null_crash.php   exit 0，STEP C 到达（原崩溃 0xC0000005）
router_match_leak.php          0.00 B/call（原 56 B/call）
orm_timestamps_stale.php       时间戳随墙钟推进（原冻结）
orm_crud_smoke.php             exit 0，全链路输出与审计基线一致
orm_edge_cases.php             hydrate trap 变 UPDATE；create/save/count 全 integer
orm_leak_probe.php             7 条路径全部 0 B/call（与审计基线一致，无回归）
orm_exception_paths.php        异常路径 0 B/call，异常后查询隔离正常
orm_state_isolation.php        exit 0，9 步隔离检查全部符合预期
test/OrmTest.php               55 passed / 0 failed（原：CRUD 段进程崩溃）
test/DatabaseTest.php          exit 0，SQLite 段全绿（原：fatal 中断）
test/RouterTest.php            exit 0，全部通过
```

### 9.3 复盘（本轮修复批的教训与确认）

1. **「对称收尾」必须机械化**。H2 的根因是 08-07 的隔离修复在命中/未命中两条分支各写了一遍恢复代码，
   未命中分支漏了一行 dtor。本轮按审计建议抽成单一 `restore:` 标签，两条分支共享同一段收尾 ——
   今后同类「保存现场 → 探查 → 恢复现场」的代码一律禁止双份拷贝。
2. **新代码的契约要下沉到最底层入口**。H1 的兜底本属于 `gene_pdo_construct()` 这个唯一汇聚点，
   却只在 pool 路径做了；ORM 的新测试一进来就踩爆。教训：底层公共入口不接受「调用方保证非空」的隐式契约。
3. **SAPI 时间语义是常驻进程的经典坑**（H3）。`sapi_get_request_time()` 在 CLI/Swoole 下覆盖整个进程
   生命周期，全仓一致性检查（`time(NULL)` 11:1 的对比）是发现它的关键手段，值得保留为审计惯例。
4. **借用指针跨 `call_user_function` 持有 = UAF 窗口**（M5）。`di.c` 08-07 已处理过同类别名悬垂，
   但风险模型没有随新模块（ORM）同步铺开。本轮在 `gene_orm_get_db()` 单点加引用，比逐调用点加固更不易漏。
5. **验证方式确认有效**：8 个 repro 脚本 + 3 个测试套件在修复构建上全部复跑，H1/H2/H3 三条运行时结论
   全部翻转，泄漏探针维持 0 B/call 基线无回归。`audit/repro/` 的一键复现机制继续作为修复验收标准。
6. **遗留共识**：M6（Query 能力补齐）与 7.2.4 的文档项不属缺陷修复，已按约定留在 `PLAN.md`；
   L2（跨协程共享 Db 句柄）/L3（默认 SQL history ≈177 KB）/L4（create 无事务）为设计观察项，
   以文档约束解决，不改代码。

---

## 十、修复复检（2026-08-10）：落地确认 + 4 条新发现

> 复检方式：对 §9.1 声称的 11 项修复逐条比对源码，并在当前构建上跑通测试套件与新增复现脚本。
> 环境：`php -n` + `pdo_sqlite` + `F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll`（PHP 8.1 NTS x64）。

### 10.1 落地确认（11/11 在源码中存在，运行时可验证）

| # | 源码位置 | 结论 |
|---|----------|------|
| H1 | `src/db/pdo.c:508-522` | ✅ `user`→`ZVAL_EMPTY_STRING`、`pass`→`ZVAL_NULL` 兜底，唯一汇聚点生效 |
| H2 | `src/router/router.c:2293-2352` | ✅ 已抽 `restore:` 标签，命中/未命中共享同一段 dtor + 恢复 |
| H3 | `src/orm/meta.c:341` | ✅ 改 `time(NULL)` |
| M1 | `model.c:242-257`（find asModel）、`model.c:627-639`（fill hydrate） | ✅ 已实现（但引入 N2，见下） |
| M2 | `meta.c:312-330` `gene_orm_normalize_id()` | ✅ create/save 两处调用 |
| M3 | `orm/orm.h:38-44` + `model.c`/`query.c` 各调用点 `if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;` | ✅ 覆盖 find/findAll/paginate/create/updateBy/destroy/destroyAll/save/delete 全部序列 |
| M4 | `meta.c:366-…` | ✅ 子串判定已移除，改 `ce ==` |
| M5 | `meta.c:216-222` + 10 处 `zval_ptr_dtor(db)` | ⚠️ **仅部分修复，残留 UAF —— 见 N1** |
| M7 | `model.c:951-1005` + 方法表 1024-1025 | ✅ `__isset`/`__unset` 已注册 |
| L1 | `response.c:785-799` | ✅ wrapper 校验已前置到 Swoole 分支之前 |
| 附带 | `query.c:445` | ✅ `ALLOW_DYNAMIC_PROPERTIES` 已去掉 |

`test/OrmTest.php` 在当前构建上 **55 passed / 0 failed**（含 M1/M7 的 5 项新断言），
崩溃、泄漏、时间戳三条 H 级结论确认翻转。

### 10.2 新发现清单

| # | 等级 | 位置 | 问题 | 证据 |
|---|------|------|------|------|
| **N1** | **UAF（高）** | `src/orm/meta.c:198-223` + `model.c` 全部调用点 | M5 的修复只 ADDREF 了**对象**，却仍返回 DI 哈希表的**借用槽位指针**；槽位内容被换掉后，`gene_orm_db_reset(db)`/`zval_ptr_dtor(db)` 作用在**新对象**上 → 新对象在仍被 DI 注册的情况下被提前析构 | **运行时实测** |
| **N2** | 数据丢失（中） | `model.c:627-639`、方法表 | M1 的 hydrate 规则使**客户端生成主键**（UUID/自然键/数据导入）无法再 `fill()+save()` 插入：恒走 UPDATE、影响 0 行、返回 0、无任何报错；且**没有任何逃生口**（`__set('exists')`/`__unset('exists')` 被硬拒，无 `setExists()`/`forceInsert()`/`isNew()`） | **运行时实测** |
| **N3** | 一致性（低） | `model.c:246-257`、`meta.c:316-330` | ①`find($id,true)` 用 `object_init_ex()` 建对象，**跳过子类 `__construct`**，与 `new U()` 行为不一致；②非整型主键表上 `create()`/`save()` 返回的是 `lastInsertId()`（SQLite 的 rowid），与真实主键无关；③`normalize_id()` 会把形如 `'007'` 的**字符串主键**转成 `int 7` 并写回 attributes | **运行时实测** |
| **N4** | 注释过期（信息） | `query.c:185-186` | 注释仍写 "db from gene_di_get is a borrowed pointer"，与 M5 之后的「调用方持有引用」契约矛盾，易误导下一次改动 | 静态 |

### 10.3 N1 — M5 的 UAF 只是往后挪了一格（必须修）

`gene_di_get()` 命中注册表时 `return pzval;`（`di.c:165-167`），返回的是 **`zend_array` 桶内的 zval 槽位指针**。
M5 的修复在 `gene_orm_get_db()` 出口加了 `Z_TRY_ADDREF_P(db)`，保住了**当时那个对象**，
但调用方一路持有的仍是**槽位指针**。于是当 ORM 序列中间执行的用户代码改动了注册表：

- `Di::set('db', $other)` → 槽位内容被替换。后续 `gene_orm_db_call(db, ...)` 打到**错误的对象**上；
  `cleanup` 里的 `zval_ptr_dtor(db)` 释放的是 **`$other` 的引用**（那一份属于注册表）→
  `$other` 在仍被 DI 注册的情况下被析构；我们 ADDREF 的原对象则被泄漏到请求末尾。
- `Di::del('db')` + 若干 `Di::set()` → 桶被删除、`arData` 可能 realloc → 槽位指针悬垂，
  `zval_ptr_dtor(db)` 读的是已释放内存。

**实测**（`audit/repro/orm_di_swap_uaf2.php`，`select()` 内 `Di::set('db', new FakeDb('B'))`）：

```
STEP A
  [dtor B]                <-- B 在 ORM 调用尚未返回时就被析构，而它仍在 DI 注册表里
STEP B result={"id":1}
slot tag = B              <-- Di::get('db') 返回已释放对象（读到旧内存，靠运气没崩）
STEP C: touch slot again
slot tag = B
STEP D done
  [dtor A]                <-- 我们 ADDREF 的 A 被泄漏到脚本结束
```

`audit/repro/orm_di_swap_uaf.php`（`Di::del` + 64 次 `Di::set` 强制 rehash）则表现为
**后续 db 调用全部静默失败**（`find()` 返回 null、`first->calls` 只停在 `select`），
即指针已指向别的桶内容 —— 同样是越界/失效读，只是这次没有可观察的崩溃。

**修复**：不要把注册表槽位指针交给调用方，改为**返回值语义**（调用方持有独立 zval）：

```c
/* meta.c —— 签名改为输出参数 */
int gene_orm_get_db(zend_string *connection, zval *out)
{
    zval *db = gene_di_get(name);
    if (!db || Z_TYPE_P(db) != IS_OBJECT) { /* throw */ return FAILURE; }
    ZVAL_COPY(out, db);   /* 拷贝 zval 本身，而非借用槽位 */
    return SUCCESS;
}
```

调用点把 `zval *db` 改成局部 `zval db_holder`，`cleanup` 中 `zval_ptr_dtor(&db_holder)` 保持不变。
这样即便注册表被删/被换，句柄仍指向我们自己那份对象，`reset()`/`dtor` 都作用在正确对象上。
（`gene_orm_new_query()` 同理，`query_init` 内的 `zend_update_property` 会再拷一份。）

> 教训补充（承 §9.3 第 4 条）：**「加引用」和「拿到独立 zval」是两件事**。
> 对哈希表返回的槽位指针，ADDREF 只保住内容不保住容器；跨用户代码持有时必须拷贝 zval。

### 10.4 N2 — M1 的 hydrate 规则让自然主键无法插入（语义回归）

`fill()` 现在的规则是「payload 含非空主键 ⇒ `exists=1`」。对自增主键这是对的，
但对**客户端生成主键**就把唯一的插入路径堵死了，且失败是**静默**的。

**实测**（`audit/repro/orm_natural_pk_insert.php`，`doc(uuid TEXT PRIMARY KEY, title TEXT)`）：

```
exists after fill : true
save() returned   : 0
rows in table     : 0 -> []          <-- 数据静默丢失，无异常、无 warning
__set('exists',false) => false       <-- 保留名被硬拒，改不动
exists still        : true
after unset(exists) : true           <-- __unset 同样拒绝
has setExists()     : false
has forceInsert()   : false          <-- 无任何逃生口
create() rows       : 1              <-- 只有静态 create() 还能插
```

审计原始建议（§四 M1）本是三条**可选**方案，其中第 1 条（fill 置 exists）单独落地时正是这个副作用；
原建议的第 3 条「INSERT 分支 payload 含主键时改走 UPDATE，或至少 `E_WARNING`」也未实现，
因此现在既不报错也不回退。

**修复建议（按优先级）**：
1. `save()` 的 UPDATE 分支在 `affectedRows()==0` 时不要直接返回 0 —— 至少发 `E_NOTICE`/
   或按配置回退 INSERT（upsert 语义需显式开关，勿默认）；
2. 提供显式逃生口：`fill(array $data, bool $hydrate = true)` 第二参可关闭自动置位，
   或加 `markNew()` / `setExists(bool)`（public 方法即可，不必放宽 `__set` 保留名）；
3. 文档写明「`fill()` 含非空主键即视为已持久化」这一约定（`AGENTS.md` 已记，README/PLAN 也应同步），
   并注明自然主键表应使用 `Model::create()`。

### 10.5 N3/N4 — 一致性与注释

- `find($id, true)` 走 `object_init_ex()`，**不调用子类构造函数**。基类 `Gene\Model::__construct` 是空的，
  所以当前无害；但用户子类若在构造函数里设默认值/注入依赖，hydrate 出来的实例与 `new U()` 行为不同。
  建议：要么在 `ce->constructor` 无必填参数时调用它，要么在文档明确「hydrate 不走构造函数」（Laravel 同此约定）。
- 非整型主键下 `create()`/`save()` 返回 `lastInsertId()`（实测 uuid 表返回 `1`，即 SQLite rowid），
  语义上不是主键。建议：主键不在 `insert` payload 中时才取 `lastId()`，否则回填 payload 里的主键值。
- `normalize_id()` 无条件把数字串转 long。若主键列是 `CHAR(6)` 之类的零填充编号（`'007'`），
  写回 attributes 的会是 `int 7`，下一次 `save()` 的 WHERE 就对不上。建议按 pk 列类型或仅在
  「payload 未提供主键」的自增场景下归一化。
- `query.c:185-186` 的注释请随 N1 的修复一并更新。

### 10.6 新增复现脚本

| 脚本 | 用途 | 对应发现 |
|------|------|----------|
| `orm_di_swap_uaf.php` | ORM 调用中 `Di::del('db')`+rehash → 后续 db 调用静默失效 | N1 |
| `orm_di_swap_uaf2.php` | ORM 调用中 `Di::set('db', $other)` → `$other` 被提前析构（UAF），原句柄泄漏 | N1 |
| `orm_natural_pk_insert.php` | UUID 主键表 `fill()+save()` 静默丢数据 + 无逃生口 | N2、N3 |

### 10.7 修复优先级

| 顺序 | 项 | 改动量 | 说明 |
|------|-----|--------|------|
| 1 | **N1** `gene_orm_get_db()` 改 `ZVAL_COPY` 输出参数 | 中（1 处函数 + 10 处调用点签名） | 实锤 UAF，优先级高于其余全部 |
| 2 | **N2** `save()` 零影响行告警 + `fill()` hydrate 开关 / `setExists()` | 小~中 | 消除静默数据丢失 |
| 3 | **N3** 非整型主键的 `lastId` 回填与 `normalize_id` 条件化 | 小 | 一致性 |
| 4 | **N4** 更新 `query.c` 过期注释 | 1 行 | 防止下次改动被误导 |
