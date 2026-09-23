# Gene 6.2.5 审计：开发摩擦、常驻内存与能力缺口

> 审计版本：6.2.5（HEAD `0a5188e`，2026-09-22）
> 审计日期：2026-09-23
> 审计范围：`src/`、`gene-ide-helper/`、`gene-ai-helper/`、`demo/`
> 审计方式：源码对照 + 本机运行时探针。已关闭的演进计划（ORM v2、生命周期、REST、Hook、入口收口、性能 V2）不重复开题。
> 运行环境：Windows / PHP 8.1.34 NTS x64 / `php_gene.dll` 6.2.5
>   （`D:\wampServer-php8.1_x64_nts\bin\php.exe -n -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll`）
> 复现脚本：
> - `audit/repro/session_store_ttl.php`（P1，已跑，确认缺陷）
> - `audit/repro/controller_di_shadow.php`（P3，已跑：`$this->user =` 会盖住 DI；这是属性写入路径，不是 `assign()`）

---

## 一、总体结论

1. **请求级生命周期是干净的。** DI、视图变量、ORM meta、连接池事务卫生都挂在请求上下文上，`cleanup()` / RSHUTDOWN 会拆掉。本轮没有发现新的按请求 refcount 泄漏。
2. **常驻进程的增长点在「写了但不设过期」的进程内存，不在请求对象。** `Gene\Session` 落库不把 cookie 寿命传给存储。`Memory::set($k, $v)` 在省略 TTL 时是永不过期，而且 `gene.cache_max_items` 默认 0，不做淘汰。Swoole worker 上用 `Gene\Memory` / `Ext\LocalStore` 当 session 驱动时，每个新会话都留下一条活到进程退出的记录。Redis / Memcached 同样收不到 TTL，demo 的组件配置里也没有 `ttl`，记录同样不过期。
3. **模板数据不要写在控制器属性上。** `$this->user = ...` 进的是 DI 类键，会盖住 `Di::set('user')` 的登录用户，并且一直留到请求结束。视图赋值的现成入口是 `$this->view->assign()`，写入 `view_vars`，与 DI 无关，`display()` 结束就释放。后台 demo 还在用属性赋值，首页和 RedisDemo 已经在用 `assign()`。
4. **helper 对 Memory 冻结的描述与源码相反。** `swoole.md` 写「`workerReady()` 之后 `Memory::set` 会告警并拒绝」。源码从 2026-08-24 起，用户态 `set/del/incr/rateLimit/lock` 写入的是另一张业务表，请求期允许写。demo 的本地模式依赖这个行为。按文档写会误判；按 demo 抄会踩到第 2 条的永不过期。
5. **能明显缩短业务代码、且不增加常驻状态的能力还有三处：** 单语句状态翻转、模型声明版本键、按页码分页。demo 里 5 处 `abs(status-1)` 裸 SQL、每个 Service 手写的缓存失效、以及 `(page-1)*limit` 都是这三处的直接证据。

---

## 二、问题清单

| # | 等级 | 位置 | 问题 | 证据 |
|---|------|------|------|------|
| **P1** | 常驻增长 | `src/session/session.c` `gene_data_save_ex` | 落库 `set` 只传 2 个参数，cookie 寿命到不了存储 | **运行时** `session_store_ttl.php`：`argc=2 ttl=NULL` |
| **P2** | 文档与实现相反 | `gene-ai-helper/.../swoole.md`、`reference.md`、`gene-ide-helper/Gene/Memory.php` | 仍写「workerReady 后 Memory 只读」，源码已允许业务分区写入 | 静态对照 |
| **P3** | 用法 | 后台控制器 `$this->字段 =`，模板 `$this->字段` | 属性写入进 DI 类键，会盖住同名组件；模板数据应走 `$this->view->assign()` | **运行时** `controller_di_shadow.php`：属性写入 `shadowed=1` |
| **P4** | UAF（条件触发） | `src/cache/cache.c` `gene_cache_call` | 进程级 static 缓存用户类的 `zend_function*`，类条目非 immutable 时跨请求悬垂 | 静态 |
| **L1** | 观测失真 | `src/cache/memory.c` 注释 vs `cache_insert_refused` | 注释写了「插满就拒绝」，计数器从未增加 | 静态 |

---

## 三、问题与解决思路

### P1 — Session 落库不带 TTL，worker 内存只增不减

`cookie_lifetime` 属性默认 **86400**（`session.c` 声明处）。Cookie 按这个寿命写。落库却是：

```c
zval params[] = { *session_id, *data };
gene_session_call_method(hook, gene_session_method_set(), 2, params, &ret);
```

三个内置存储的第三个参数才是 TTL：

| 存储 | `set` 签名 | 不传第三个参数时 |
|------|------------|------------------|
| `Gene\Memory` | `Sz\|l`，缺省 `validity = 0` | `gene_memory_set_expiry_nolock` 把 0 当成永久，并删掉旧的过期记录 |
| `Gene\Cache\Redis` | `zz\|z` | 回落组件配置 `ttl`；demo 的 `redis` 没配，不过期 |
| `Gene\Cache\Memcached` | `zz\|zz` | 同上；demo 的 `memcache` 没配，Memcached 的 0 就是不过期 |
| `Ext\LocalStore` | `set($key, $value, $ttl = 0)` | 转到 `Memory::set`，同样永久 |

`Memory::set` 在进入业务写深度之后写入 `business_cache`，不进路由表，所以这条路径**不会**把路由表 `arData` 挪走。增长的是业务分区本身：

- `gene.cache_max_items` 默认 `"0"`（`gene.c`），LRU 不启用。
- 用户态 `Memory::set` 只有在 `cache_max_items > 0` 时才进入淘汰。
- TTL 清扫只处理 `validity > 0` 的键。

因此 FPM 使用 Memory 会话时，worker 回收前会话记录一直在；Swoole 下 `max_request` 往往是一万，本地模式（`GENE_DEMO_LOCAL=1` → `localStore`）每个新访客一条记录，直到 worker 退出。这不是请求内 refcount 泄漏，效果相同：进程 RSS 随独立会话数上涨，没有回收点。

ide-helper 还把 `Memory::delete` 写成「可直接作为 `session.driver`」。契约在方法名上成立，寿命上不成立。

**解决思路（兼容已有 2 参数句柄）：**

在 `gene_data_save_ex` 里读取 `cookie_lifetime`。值 `<= 0` 时用声明默认值 86400，避免把 0 传进去变成「立刻过期」或「永不过期」。然后看 `set` 的 `common.num_args`（变参同样视为可接受）：

- `num_args >= 3`：第三个参数传寿命秒数。Memory / Redis / Memcached / LocalStore 都已接受这个参数，每次保存刷新 TTL，和滑动过期的 cookie 一致。
- `num_args == 2`：维持现在的两参数调用。`demo/application/Ext/Session.php` 的 `set(string $key, $data)` 不会收到第三个参数，也不会 `ArgumentCountError`。这个类当前没有被路由或配置引用；它自己的 `$session_lifetime = 0` 若以后被接上，仍会把 0 传给底层，应改成与 cookie 寿命相同，或直接删除这个平行实现。

不新增进程级结构。TTL 记在现有的 expiry 表里，已有的抽样清扫会回收。FPM 与 Swoole 走同一条 `gene_data_save_ex`。

配套：

- demo 的 `memcache` / `redis` 会话若继续做外部存储，组件上仍建议有 `ttl`，作为驱动没收到第三个参数时的底线。修完 P1 之后以传入值为准。
- `gene-ide-helper/Gene/Memory.php` 的 `delete` 注释补一句：作 session 驱动时寿命来自 `Gene\Session` 的 `ttl`，不要依赖 `set` 的默认 0。
- 回归：把 `session_store_ttl.php` 的期望收成「`argc=3` 且 `ttl=86400`」，并加一条 2 参数句柄仍被调用的用例。

**不要**把 `gene.cache_max_items` 当成会话回收的主手段。打开上限后，会话键和缓存键挤在同一个业务分区里，热点缓存会把登录态淘汰掉。上限只适合作为「TTL 失效时的最后一道内存天花板」，且要大于同时在线会话数。

### P2 — helper 仍写「workerReady 之后 Memory 只读」

源码（`src/cache/memory.h`）把表拆开了：

- `cache_layer_memory_write_depth == 0` → 路由/配置表 `GENE_G(cache)`。Swoole 下 `workerReady()` 之后这条表拒绝写入，锁自由读者拿裸指针才安全。
- 深度 `> 0` → 业务表 `GENE_G(business_cache)`。`Memory::set/del/incr/decr/rateLimit/lock/mset` 和 `Gene\Cache` 的进程缓存都先 `GENE_CACHE_LAYER_MEMORY_WRITE_ENTER()`。`gene_memory_write_allowed()` 在深度 `> 0` 时直接放行。

与文档的冲突：

| 文档 | 写了什么 | 源码 |
|------|----------|------|
| `swoole.md` §4.4、§6 | 请求期 `Memory::set/del` 告警并拒绝；填充必须在 `workerReady()` 之前 | 用户态写入进业务表，允许 |
| `reference.md` `Memory::lock` | 「`workerReady()` 后冻结写入」 | `lock()` 有 ENTER，允许 |
| `Memory.php` `incr` | 「与 set 一样受 workerReady 冻结约束」 | `incr` 走 `gene_memory_adjust`，允许 |
| `Memory.php` `rateLimit` | 冻结前后都可用 | 与源码一致 |
| `SKILL.md` | 「`workerReady()` 后勿在请求中写 Memory」 | demo `LocalStore` 在请求里写 |

demo `swoole.php` 在 `workerStart` 末尾调用 `workerReady()`，本地模式的 session 和 `cachedVersion` 都在随后的请求里经 `LocalStore` 写 Memory。按当前源码这是合法的；按 `swoole.md` 这是禁止的。两份说明不能同时当规范。

**解决思路（只改文档，不改冻结语义）：**

1. `swoole.md`、`SKILL.md`、`reference.md`、`Memory.php` 改成同一句话：路由和配置在 `workerReady()` 时冻结；用户态 `Memory::*` 与 `Gene\Cache` 的进程缓存写入业务分区，请求期可用。
2. 同一段写明业务分区的边界：`ttl=0` 活到进程退出；默认没有条数上限；值不能是对象或资源；多 worker 不共享。会话和版本号要传 TTL（依赖 P1）。
3. 路由/配置的预热仍然必须在 `workerReady()` 之前。这句话保留，不要和业务分区混写。

NTS 构建里 `GENE_CACHE_WRLOCK` 是空宏，协程安全靠「业务写不碰路由表」加「C 函数内部不让出」。文档不要再写成「有读写锁所以可以随便持有 `get()` 返回的内部指针跨过 PHP 调用」。

### P3 — 模板数据走 `$this->view->assign()`，不改 `__set`

`Controller::__set` 和 `View::__set` 都调用 `gene_di_set_class`，键是 `类名 + '_' + 属性名`，值留在 `ctx->di_regs` 里直到请求结束。`gene_di_get_class` 先查这个类键，再回落 `gene_di_get(name)`。所以 `$this->user = $行` 会盖住 `BeforeHook` 里 `Di::set('user', ...)` 放进的登录用户。`Controllers\Admin\User::save()` 用 `$this->user['user_id']` 识别当前用户，这个名字不能再当模板字段。

探针确认的是这条属性路径（CLI，扩展 6.2.5）：

```text
before_user_id=1
after_user_id=9
shadowed=1
```

`View::assign()` / `Controller::assign()` 不走这条路径，两者都调用 `gene_view_set_vars()`，数据在请求上下文的 `view_vars` 里。`Controller::display()` 和 `View::display()` 把它放进模板符号表，渲染结束调用 `gene_view_clear_vars()`。大列表不会进 `di_regs`，事务卫生扫描也扫不到它。FPM 与 Swoole 已经是这一套，按请求/协程隔离，没有新增进程状态。

demo 分裂成两种写法：`Controllers\Index`、`Controllers\RedisDemo` 用 `$this->assign()`，模板读裸变量（`index/doc.php` 的 `$help`、`$id`）。后台控制器用 `$this->userlist = ...`，模板读 `$this->userlist`（`admin/user/run.php`、`parent.php` 的 `$this->title`）。后一种才会和 DI 撞名。

**采用的解决思路：约定用 `$this->view->assign()`，C 层 `__set` 保持为 DI。**

```php
$this->view->assign('title', '用户管理');
$this->view->assign('userlist', UserService::getInstance()->lists($page, $limit, $search));
$this->view->assign('search', $search);
$this->display('admin/user/run', 'parent');
```

模板里这些字段改成裸变量 `$title`、`$userlist`、`$search`。`$this->display()` 可以保留：执行视图时 `$this` 仍是控制器，`$this->user`、`$this->session`、`$this->contains()` 继续走 DI / 控制器方法。登录用户不要 `assign('user', ...)`，布局 `parent.php` 里的 `$this->user` 就还是会话里的那份。

`$this->assign('title', ...)` 与 `$this->view->assign('title', ...)` 是同一个 `gene_view_set_vars()`。首页已经在用前一种，可以保留。推荐对外只讲 `$this->view->assign()`，避免再出现第三种写法。

不要写成 `$this->view->title = ...`。那是 `View::__set`，仍然写入 DI 类键（类名换成 `Gene\View`），模板里的 `$this->title` 也读不到 `view_vars`。

不改 `Controller::__set`。若把它改成「有 DI 条目就拒绝，否则写入 `view_vars`」，现有 `$this->title =` 加模板 `$this->title` 会一起失效，等于强迫所有旧页面迁移，又让属性赋值和 `assign()` 变成两个入口写同一张表。冲突用推荐入口避开即可。

`controller_di_shadow.php` 继续说明属性写入会遮蔽 DI，不作为要改成 `after_user_id=1` 的回归。Context 仍显式 `Context::get/set`，不从 `__get` 回落。

### P4 — `gene_cache_call` 跨请求缓存用户方法指针

`cache.c` 里 4 槽 static：

```c
static struct gene_fn_slot slots[4] = {{0}};
```

命中条件是类条目指针和 interned 方法名都相等，然后 `zend_call_known_function`。只在方法名 interned 时写入槽位。没有在 RSHUTDOWN 清空，也没有走 `gene.h` 里 `GENE_CG_FN_LOOKUP` 的 ZTS 分支。

同文件其它 `static zend_function*`（连接池、Redis、Swoole Channel）指向的是扩展方法，进程寿命，不是这一条。

风险范围：

- opcache 把用户类标成 `ZEND_ACC_IMMUTABLE` 时，指针在进程内稳定，槽位有效，这也是它想优化的路径。
- FPM **未开 opcache** 时，用户类在请求结束销毁。下一请求堆复用可能让新的 `zend_class_entry` 落在同一地址，方法名 `"row"` 仍是 interned 字符串，槽位命中后调用已释放的 `zend_function`。
- ZTS 下用户类条目按线程存在，进程级槽会跨线程复用。

Swoole worker 在 `workerStart` 加载类之后不再卸载，生产配置通常开着 opcache，所以这不是当前 demo 的必现崩溃。它和仓库里已经修过的「interned 字符串跨请求悬垂」是同一类错误，应同样收口。

**解决思路：**

只缓存 `fn->type == ZEND_INTERNAL_FUNCTION` 或带 `ZEND_ACC_IMMUTABLE` 的用户方法。其余每次 `function_table` 查找。回调后面是 SQL 或网络，少一次 hash 查找没有可测量的收益，不值得留悬垂指针。ZTS 下连 immutable 槽也不做进程级 static，直接查找。不要把 `zend_function*` 放进请求上下文再增加一条清理路径。

无安全的用户态复现（要 ASAN，且 `opcache.enable=0`）。不提供会故意崩溃的探针。

### L1 — `cache_insert_refused` 恒为 0

`memory.c` 在 `gene_memory_set`、`rateLimit`、`lock`、`gene_memory_adjust` 的注释里写「新键若会扩容冻结的 bucket 就拒绝，并计入 `cache_insert_refused`」。这四处都是直接 `gene_symtable_update` → `zend_hash_update`。全仓库没有 `cache_insert_refused++`。`Monitor` 仍输出这个字段。

路由表安全来自 P2 的分表，不是这条不存在的拒绝。业务表可以扩容；在 NTS 上扩容发生在不让出的 C 调用里，不会挪动路由表。

**解决思路：** 改注释，说明拒绝插入没有实现，路由表靠分表保护。`Monitor` 去掉该字段，或改成业务表的 `nNumOfElements` / `nTableSize`，避免把 0 读成「没有发生过饱和」。不要在没有淘汰策略时补上「插不进去就失败」：会话键会在预留槽用尽后静默丢写。

---

## 四、优化项

O2–O4 是新增 C API，约束如下。O1 只改 demo 与文档，不进扩展。

- 状态只放请求上下文或当次调用的栈上。禁止模型 static 上挂 Db、Cache、PDO。
- FPM 与 Swoole 同一条代码。Swoole 不在协程间共享查询对象。
- 标识符走已有的 `gene_orm_valid_ident`。不把用户字符串拼进 SQL 片段。
- 异常路径沿用 ORM 现有的 `gene_orm_has_exception()` 门禁，先释放再返回。
- 落地时同步 `gene-ide-helper` 与 `gene-ai-helper/skills/gene-framework/reference.md`，并给 `test/OrmTest.php` 或 `test/MvcTest.php` 加断言。

### O1 — 后台页面改为 `$this->view->assign()`

这是 P3 的落地，不新增 API。`gene-ai-helper` 里视图示例目前写的是 `$this->assign()`，与 `$this->view->assign()` 存储相同；技能与 `reference.md` 把推荐写法收成后者，并写明模板用裸变量，`$this->组件名` 只表示 DI。

demo 后台控制器（`Admin\User`、`Group`、`Module`、`Log`、`Index` 登录页、`Doc\Mark`）把 `$this->字段 =` 换成 `$this->view->assign()`。对应模板（含 `parent.php`、`dialog.php`、`login.php`）里的 `$this->title`、`$this->userlist`、`$this->page` 等改为裸变量。`$this->user` 保持不变，它是 `BeforeHook` 注入的登录态。

渲染峰值随之下降：列表在 `display()` 返回时从 `view_vars` 释放，不再留在 `di_regs`。Swoole 上仍是当前协程的请求上下文，没有跨请求残留。

### O2 — `Model::flip()`：一条 UPDATE 翻转状态

demo 有 5 处相同的逃生舱：

- `Models\Admin\User::status`
- `Models\Admin\Group::status`
- `Models\Admin\Module` 的 status
- `Models\Admin\Log` 的 status
- `Models\Doc\Mark` 的 status

形式都是 `UPDATE ... SET status=abs(status-1) WHERE pk=?`。一条语句，行锁串行，两次并发点击结果是翻转两次。

已有的 `Model::toggle()` 不是它的替代：先 `SELECT` 再 `UPDATE ... WHERE pk=? AND field=?`。并发第二次影响行数为 0，点击会「没反应」，还多一次往返，并且时间戳要靠第二次 UPDATE。注释写明这是有意的 CAS。保留 `toggle()`，不要改它的语义。

**新增** `Model::flip($id, string $field, array $values = [0, 1])`：

```sql
UPDATE t
   SET field = CASE WHEN field = ? THEN ? ELSE ? END
       [, updated_at = ?]
 WHERE pk = ?
```

- `field` 必须通过 `gene_orm_valid_ident`，并且在 `$fields` 白名单里。
- `$values` 两个元素，绑定进 CASE，不拼进 SQL。MySQL / SQLite / PostgreSQL / SQL Server 都支持这个 CASE。
- `timestamps` 打开时，`updated_at` 写在同一条 UPDATE 里，用 `time(NULL)`，不要用 `sapi_get_request_time()`。
- 返回 `affectedRows`。不先 SELECT。不持有跨调用的行缓存。
- 空标识符、字段不在白名单、values 不是 2 个：抛异常并 `db->reset()`，与 `toggle()` 的清理方式一致。

demo 的五个 `status()` 改为 `return static::flip($id, 'status');`。Service 层的缓存失效先保持现状，等 O3 再收。

### O3 — 模型声明版本键，写成功后自动 `updateVersion`

`Services\Admin\User` 用大约 70 行维护两套键：`db.sys_user.user_id` 和 `db.sys_user.user_name`。改名要先读出旧登录名，删除要在 `DELETE` 之前取出登录名，漏一次就留下最长 3600 秒的旧缓存。`row` / `getField` / `getUserInfoByName` 三处 `cachedVersion` 的键必须和这里手动对齐。

**模型上声明映射，不在 C 里写业务键名：**

```php
protected static array $versionKeys = [
    'db.sys_user.user_id'   => 'user_id',
    'db.sys_user.user_name' => 'user_name',
];
```

左键是 `updateVersion` 的版本字段，右值是列名。

执行规则：

1. 仅当 `gene_di_get` 能找到 `cache` 且它有 `updateVersion` 时才工作。没有缓存组件的应用零额外 SQL。
2. `create` / `updateBy` / `destroy` / `save` / `delete` / `flip` 在**语句成功之后**收集受影响的键。主键来自参数或属性。若写入数组改到了非主键的映射列，先按主键 `SELECT` 出旧值（demo 现在就是这么一次 `userNameById`），新值用 payload，两个都 bump。删除前同样只为映射列做这一次读取。
3. 已处于事务中时，把待 bump 的键列表挂在**请求上下文**的冷区，不挂 static。`transaction()` 提交成功后 flush；回滚则丢掉列表。自动提交的单条写在 `affectedRows` 之后立刻 flush。这样不会在回滚前把版本号抬上去，让别的请求把未提交的行缓存下来。
4. 上下文在 `cleanup()` 里释放。协程各自一份。进程里不留「上次 bump 的用户名」。
5. `cachedVersion` 的读法不变。Service 删除 `bumpUserCacheForUser` / `bumpUserCacheDeleted`，写操作只留业务本身。

不要在模型 static 上缓存 `Gene\Cache` 实例。每次 flush 时按名字取 DI，取到的是当前协程的请求级对象。

### O4 — `Model::page($where, $page, $perPage, $order = null)`

`paginate($where, $offset, $limit, $order)` 要调用方自己算偏移。demo 的用户列表、以及 `Group::lists` 里手写的 `count()` + `limit()` 都是这一步。`Group::lists` 还绕开了已有的 `paginate()`，同一条件打两次查询 API。

`page()` 只做：`$page < 1` 视为 1，`$perPage < 1` 抛异常，`offset = ($page - 1) * $perPage`，然后调用现有 `paginate`。返回值仍是 `{count, list}`，可额外带 `page` 与 `limit` 两个键，不改变 `count` / `list` 的类型。不增加 SQL，不复制结果集。

`Group::lists` 改为 `static::page(['group_pid' => 0], $page, $pagesize, 'group_id asc')`。调用方若仍持有偏移量，继续用 `paginate`。

---

## 五、demo 上现在就能改的部分

这些不依赖新的 C API，但它们让 demo 和 helper 教的是旧写法。

| 位置 | 现状 | 改法 |
|------|------|------|
| `Models\Admin\User::getUserInfoByName` | 多表 `sql()` | `query()->fields([...])->join('sys_group b', 'b.group_id=a.group_id', 'left')->where('user_name', '=', $name)->row()`。`fields()` 会盖过 `$fields` 投影，`join()` 已支持 ON 字符串 |
| `Services\Admin\User::generatePasswordHash` | `md5` + `sha1` 截断 | `password_hash` / `password_verify`。这是 PHP 标准库，不必进扩展 |
| 各后台 `addPost` / `editPost` | 不校验 | 已有 `Validate`。skill 里的控制器模板就是这个顺序 |
| `Hooks\RequestId` | 路由未引用；不限制长度、不区分是否信任入站头 | 删除。入口已经是 `Application::requestId()` |
| `Ext\Session` | 无引用，`$session_lifetime = 0` | 删除，避免和 `Gene\Session` 并列 |
| `config/router.ini.php` | 每条路由写 `adminAuth@clearAfter`，`hook()` 注册在引用之后 | 组上 `through(['adminAuth'])`。类钩子在派发时按名查找，注册顺序不影响正确性，但逐条后缀难审 |
| `config.ini.php` | 示例口令写在仓库里 | 保持本地 demo 可用即可，文档标明不要抄进生产配置 |

`toggle()` 不要拿去替换那 5 处 `abs(status-1)`，原因见 O2。在 `flip()` 落地前，这 5 处可以留着，并在注释里指向 O2，避免有人改成 CAS 后改变点击语义。

---

## 六、本轮不立项

| 候选 | 原因 |
|------|------|
| `Controller::__get` 回落 `Context` | 在 P3 的双存储上再加一条隐式查找，名字冲突更难查。证据仍只有单应用的双写 |
| `Http::request(max_bytes)` 的伪降级 | Swoole 协程客户端仍是 `execute()` 之后才有完整 body，做了会让两个运行时的内存承诺不一致 |
| `cachedHotVersion` | 省的是用户态一个 `if`，不减少版本查询 |
| 把 `cache_insert_refused` 补成「拒绝写入」 | 没有淘汰时，拒绝的是新会话和新缓存键，见 L1 |
| 软删除、关联预加载 | demo 的剩余裸 SQL 用 `join` + `fields` + `flip` 就能去掉。关联会引入身份映射和跨协程共享模型，本轮没有内存预算 |
| 改 `Model::toggle()` 为单条 UPDATE | 现有调用方依赖「并发第二次返回 0」。新语义用 `flip()` |
| 改 `Controller::__set` 把模板字段写入 `view_vars` | `$this->view->assign()` 已经写入 `view_vars` 且不碰 DI。改 `__set` 会打断仍用 `$this->title` 的模板，并让属性赋值和 `assign()` 双写同一张表 |

---

## 七、建议顺序

1. **P1 + P2**：先止住 worker 上会话记录只增不减，并让 helper 与分表语义一致。改动面是 `gene_data_save_ex` 和文档，不碰路由热路径。
2. **P3 / O1**：后台控制器和模板改成 `$this->view->assign()` 与裸变量。不改 C。`$this->user` 仍走 DI。
3. **O2、O4**：小 API，直接删掉 demo 的状态裸 SQL 和手写偏移。
4. **O3**：版本键声明。依赖 P3 不影响它，但应在 O2 之后做，这样 `flip()` 成功时能走同一条 bump。
5. **P4**：缓存槽只留 immutable / internal。可与任意一档同批，互不依赖。
6. **L1**：改注释和 Monitor 字段，避免下一轮把恒为 0 的计数当成「扩容已被拦住」。

P1 的验收用 `audit/repro/session_store_ttl.php`（修复后应打印 `OK: store set() received cookie lifetime`）。P3 不改属性写入的语义，`controller_di_shadow.php` 仍应看到 `shadowed=1`；后台页面改完后，模板数据来自 `assign()`，登录用户仍来自 `Di::set('user')`。O2/O3/O4 用 Sqlite 进 `OrmTest`，并沿用 `audit/repro/orm_v2_leak_probe.php` 的 1 万次循环，要求 `memory_get_usage(true)` 增量为 0。Swoole 真实协程下的会话 TTL 清扫不在本机环境，发布前在 Linux worker 上对 `LocalStore` 写入后等待超过 TTL，确认 `Memory::get` 变为未命中；无该环境则 SKIP，不以 SKIP 为通过。
