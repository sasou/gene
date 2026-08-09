# Gene 扩展框架审计报告：近两周优化 + 新增 ORM（合理性 / 内存安全）

> 审计版本：6.0.0（HEAD `daa9512`）
> 审计日期：2026-08-09
> 审计范围：`2026-07-30 ~ 2026-08-08` 全部提交（`git diff f198821..HEAD`，共 114 文件、+9667/-8588），重点为
> **新增模块 `src/orm/`（meta.c + model.c + query.c，约 1700 行）** 与 08-06/08-07/08-08 三批优化
> 审计方式：**静态代码审查 + 运行时实测**（与前几轮纯静态审计不同，本轮跑通了扩展）
> 运行环境：Windows / PHP 8.1.34 NTS x64 / `php_gene.dll`（2026-08-08 12:19 构建，含 ORM v1 + 08-08 加固）
> 复现脚本：`audit/repro/*.php`（每条运行时结论均可一键复现）

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
