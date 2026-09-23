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
/** @var array<string,string> 版本键 => 行内列名（C 层父类无类型，子类不得加类型） */
protected static $versionKeys = [
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

---

## 八、落地复核（2026-09-23 追加）

> 复核对象：提交 `f330c73`（`fix(session): pass cookie TTL to session store set()`，54 个文件，+1680/−447）。
> 复核方式：逐条对照源码；用 `x64\Release\php_gene.dll`（构建时间 09:53，晚于全部 `src/` 修改）免部署运行两份 repro、`OrmTest` / `SessionTest` / `CacheTest` / `RouterTest` / `MvcTest`，另写一次性探针验证 O3 边界。
> 本节只追加，不改写第一至第七节；与前文矛盾之处以本节为准。

### 8.1 落地总览

| 项 | 状态 | 说明 |
|----|------|------|
| P1 会话 TTL | 已落地 | `session_store_ttl.php` 输出 `argc=3 ttl=86400 / argc2=2 / OK`。残留见 R5、R8 |
| P2 helper 冻结描述 | **部分落地** | `SKILL.md`、`swoole.md` §4.4/§6 已改；仍有 3 处旧说法，见 R9 |
| P3 / O1 模板走 `assign()` | 已落地 | 后台 6 个控制器、全部模板已改为 `$this->view->assign()` + 裸变量；模板内仅剩 `$this->user/request/contains()`。`controller_di_shadow.php` 仍为 `shadowed=1`，符合预期 |
| P4 函数槽 | 已落地 | 只缓存 internal / `ZEND_ACC_IMMUTABLE`，ZTS 整体关闭。残留见 R10 |
| L1 `cache_insert_refused` | 已落地（有尾巴） | 注释与两处 stats 输出已改。残留见 R11 |
| O2 `flip()` | C 已落地 | `User`、`Group` 改用 `flip()`；`Log`、`Module`、`Mark` 继承旧 `\Gene\Model`，改成带绑定的 CASE 裸 SQL（Sqlite 实测参数顺序正确）。PostgreSQL 风险见 R6 |
| O3 `versionKeys` | 已落地，**语义有缺口** | 见 R2、R3、R4 |
| O4 `page()` | 已落地 | `$order = null` 触发 deprecation，见 R7 |
| §五 demo 清理 | 已落地 | `join()+fields()`、`password_hash`、`Validate`、删 `RequestId` / `Ext\Session`、`through(['adminAuth'])`、口令注释均已完成。库表迁移缺失，见 R8 |
| 测试 | 通过 | `OrmTest` 185/0，其余 4 个套件 exit 0。覆盖缺口见 R12 |

结论：C 层 6 项都已按报告动手，但 **demo 后台从 2026-08-08 起就无法加载 ORM 模型（R1）**，因此 O2/O3/O4 在 demo 上的改动从未真正运行过。O3 按报告第四节的设计实现，而这个设计本身比原先手写的 Service 失效得更少，引入了缓存一致性回归（R2）。

### 8.2 问题清单

| # | 等级 | 问题 | 证据 |
|---|------|------|------|
| **R1** | 阻断 | demo 的 ORM 模型用 `protected static string/array $x` 重声明基类的无类型静态属性，PHP 报致命错误；后台登录、用户页、角色页全部不可用。helper 文档和本报告 O3 示例也在教这种写法 | 运行时 |
| **R2** | 高（安全相关） | `versionKeys` 只在 payload 含有映射列时才 bump 二级键。`flip()`、改密码、改资料都不会失效「按登录名」的缓存；账号被禁用或改密后，旧状态和旧口令摘要最长还能用 3600 秒。旧 Service 的 `bumpUserCacheForUser` 每次写入都会 bump 当前登录名，所以这是回归 | 运行时 |
| **R3** | 高 | `updateBy()` 的 where 不含主键时，命中行数 > 0，但一个版本键都不 bump，也没有告警 | 运行时 |
| **R4** | 中 | 事务挂起列表挂在请求级、不区分连接：A 连接提交会把 B 连接未提交的 bump 提前 flush；A 回滚会丢掉 B 的 bump | 运行时 |
| **R5** | 中 | P1 把 `cookie_lifetime` 原样传给 `Memcached::set`；超过 30 天（2592000 秒）时 Memcached 把它当成 Unix 时间戳，会话写入即过期 | 静态 |
| **R6** | 中（未实测） | `flip()` 生成 `CASE WHEN col = ? THEN ? ELSE ? END`，PostgreSQL 原生预处理下 THEN/ELSE 的参数会被推断为 `text`，写入整型列时报类型错误 | 静态 |
| **R7** | 低 | `page()` / `paginate()` 的 `$order` 用 `S` 解析，传 `null` 在 PHP 8.1 触发 deprecation，与 stub 的 `$order = null` 不一致 | 运行时 |
| **R8** | 中 | `user_pass` 只在 `gene_demo.sql` 里改成 `varchar(255)`，已有库没有迁移；`password_hash` 的 60 字符结果会被截断或插入失败。另有 cookie 寿命 `<= 0` 的不一致 | 静态 |
| **R9** | 低 | P2 残留：`reference.md:63`、`swoole.md:49` 仍写「冻结进程级 Memory」，`swoole.md:380` 仍写 `Memory::rateLimit`「`workerReady()` 后冻结」 | 静态 |
| **R10** | 低 | P4 残留：opcache 重启（`opcache_reset()` 或 SHM 耗尽）后，immutable 指针所在的 SHM 会被重建，进程级槽仍可能命中旧地址 | 静态 |
| **R11** | 低 | L1 残留：`GENE_G(cache_insert_refused)` 仍在声明和清零；`audit/repro/swoole_route_probe.php:40` 仍读这个字段；stats 删字段未写 CHANGELOG | 静态 |
| **R12** | 低（流程） | 单个 `fix(session)` 提交混入 3 个新公开 API；CHANGELOG、`docs/`、`AGENTS.md` 行为约定、`audit/README.md` 都未同步；关键路径缺回归用例 | 静态 |

### 8.3 问题详情与解决思路

#### R1 — 带类型的静态属性重声明导致 demo 模型致命错误

复现（Release DLL，CLI）：

```text
php -r "require 'demo/application/Models/Admin/User.php';"
Fatal error: Type of Models\Admin\User::$table must not be defined (as in class Gene\Orm\Model)
```

`Gene\Orm\Model` 在 MINIT 里用 `zend_declare_property_string/null` 声明 `$table`、`$primaryKey`、`$fields`、`$timestamps`、`$connection`、`$versionKeys`，全部无类型。PHP 7.4+ 规定：父类属性无类型时，子类重声明不能加类型；父类有类型时，子类必须写相同类型。`git log -S` 显示 `protected static string $table` 从 `7bcc5b0`（2026-08-08，ORM v1）起就在 demo 里。`Services\Admin\User::checkUser()` 通过 `cachedVersion(["\Models\Admin\User", ...])` 加载模型，因此后台登录本身就会致命错误。`linux_swoole_verify.sh --demo` 只打 `/healthz`、`/metrics` 和 wrk，一直没有发现。

同样的写法出现在：`demo/application/Models/Admin/User.php`、`Group.php`，`gene-ai-helper/AGENTS.md:97-99`，`gene-ai-helper/skills/gene-framework/swoole.md:182-185`，以及本报告第四节 O3 的示例（`protected static array $versionKeys`）。`test/OrmTest.php` 和 `gene-ide-helper` 用的是无类型写法，所以测试全绿。

**解决思路：**

1. **C 层保持无类型，不要改成 typed property。** 改成 `zend_declare_typed_property` 后，所有现有的无类型子类都会反过来报 `must be string (as in class ...)`，破坏面更大。无类型是唯一能同时兼容「写了 `$table = 'x'`」和「用 stub 生成代码」的选择。
2. demo 两个模型、helper 两份文档、本报告 O3 示例统一改成 `protected static $table = ...;`。`$fields` / `$versionKeys` 的类型意图写进 `@var array` 注释。
3. 加加载冒烟测试，防止再次出现「语法对、加载即死」：在 `test/MvcTest.php`（或新 `DemoLoadTest.php`）里注册 demo 的 PSR-4 前缀，遍历 `demo/application/{Models,Services,Controllers,Hooks,Ext}/**/*.php`，逐个 `class_exists($fqcn, true)`，放在子进程里跑，并断言 exit 0。`php -l` 查不出继承期错误，必须真正加载。
4. `tools/acceptance` 的 `--demo` profile 增加一个会加载 ORM 模型的请求，例如对 `/login.action` POST 错误口令，期望返回 JSON 的「密码错误」或「用户名不存在」，而不是 500。

#### R2 — versionKeys 只 bump payload 里出现的二级键

探针（Sqlite，`versionKeys = ['v.id' => 'id', 'v.name' => 'name']`）：

```text
flip($id,'status')                  bumps: [{"v.id":1}]
updateBy($id, ['pass' => 'h2'])     bumps: [{"v.id":1}]
```

根因在第四节 O3 的规则 2：「若写入数组改到了非主键的映射列，先 SELECT 旧值」。`gene_orm_version_prefetch()` 按这条规则只在 `gene_orm_version_column_in_payload()` 为真时才读旧行，`gene_orm_version_commit_write()` 对 payload 里没有的二级列直接跳过。但版本键保护的是**整行缓存**，不只是那一列：`checkUser()` 按 `db.sys_user.user_name` 缓存的行里包含 `status`、`user_pass`、`user_salt`。

demo 上的直接后果：

- 后台点「禁用」→ `flip($id, 'status')` → 登录名版本不变 → 被禁用的账号最长 3600 秒内仍能登录。
- `/save.html` 修改自己的口令 → `edit($uid, ['user_pass', 'user_realname', 'status'])`，payload 里没有 `user_name` → 最长 3600 秒内**旧口令能登录，新口令不能**。

旧实现 `bumpUserCacheForUser()` 每次写入都先 `userNameById()`，再 bump 当前登录名，没有这个缺口。

**解决思路：语义改为「行被写，就 bump 这一行在所有 versionKeys 下的当前键；映射列被改名时再加上新值」。**

1. **更新类写入（`updateBy` / `save` / `flip`）：** 只要模型有非主键映射列，就按主键预读这些列的**当前值**，不再看 payload 里有没有。bump 集合 = 旧值 ∪ payload 新值（只有映射列本身被改名时才有新值）。
2. **省掉预读的场景：**
   - hydrate 模型的 `save()`：`attrs` 里已有这些列的原值（`find($id, true)` 按 `$fields` 投影加载），直接取，不发 SQL。缺列时再回落预读。
   - `flip()` 不改二级列，预读可以放在 UPDATE 之后按主键读，顺序不影响正确性。
   - 模型没有非主键映射列（只有 `id => pk`）时，行为与现在相同，零额外 SQL。
3. **成本：** 每次有二级键的更新多一次按主键的单行 SELECT，与旧 Service 的 `userNameById()` 相同，没有回退性能。
4. **回归：** 在 `OrmTest::testFlipPageVersion` 里补三条断言：`flip()` 后 bumps 里有 `v.name => 'ada'`；`updateBy($id, ['pass' => ...])` 后同样有；`save()`（hydrate）不发预读 SQL（可用 `history()` 快照计数）。
5. 在 `AGENTS.md` 的「ORM」约定里写一行：`versionKeys` 以行为单位失效，任何成功写入都会 bump 该行全部映射键。

#### R3 — 非主键条件的 updateBy 静默不失效

```text
updateBy(['name' => 'ada'], ['pass' => 'h3'])   affected=1  bumps: []
```

`gene_orm_pk_from_where()` 在数组 where 里找不到主键就返回 NULL，于是 `updateBy` 跳过预读和 `commit_write`。按主键批量更新（`['id' => [1, 2]]`）时，旧值是 `all()` 返回的列表，但 `commit_write` 的非删除分支按单行 `row_col(old, col)` 取值，拿不到旧的二级键。

**解决思路：**

1. versionKeys 生效且 where 不是纯主键时，在 UPDATE 之前用**同一个 where**（复用 `gene_orm_apply_where`，不拼字符串）做一次 `SELECT pk, 映射列...`，得到受影响行集合；写成功后按 R2 的规则逐行 bump。
2. 行数上限：预读加 `LIMIT N+1`（N 默认 1000，可做成模型静态属性 `$versionScanLimit`）。超过 N 行时仍执行写入，但发 `E_WARNING`（`versionKeys: updateBy matched more than N rows; cache not invalidated`），**不静默**。与「hydrate `save()` 命中 0 行发 `E_NOTICE`」是同一原则。
3. `commit_write` 的非删除分支同样要识别 `old` 是行列表的情况（与删除分支的 `gathered` 逻辑合并成一个取值辅助函数）。
4. 回归：非主键 where 命中 1 行时有 bump；命中超过上限时收到 warning。

#### R4 — 挂起列表不区分连接

```text
B 连接 beginTransaction → PLog::create()      bumps: []            （正确挂起）
A 连接 transaction(updateBy) 提交              bumps: [{"v.log":1},{"v.id":1}]   （B 尚未提交就被 flush）
```

`orm_version_pending` 是请求上下文里的单个数组；`gene_pdo_commit()` / `gene_pdo_rollback()` 分别无条件调用 `gene_orm_version_flush()` / `gene_orm_version_discard()`。多连接（读写分离、业务库 + 日志库）时：

- A 提交 → B 的 bump 提前生效；B 提交前，其他请求重新读库，拿到旧数据并按新版本号写回缓存，B 提交后缓存保持旧值直到 TTL。
- A 回滚 → B 的 bump 被丢掉，B 提交后缓存没有失效。

**解决思路：**

1. `orm_version_pending` 改为 `HashTable<pdo 对象 handle → list<map>>`。`gene_orm_version_publish()` 挂起时用 `Z_OBJ_HANDLE_P(pdo)` 作为键；`gene_pdo_commit(pdo)` / `gene_pdo_rollback(pdo)` 把 `pdo_object` 传进 flush/discard，只处理自己的那一条。
2. 请求 `cleanup()` 时，如果某条连接的挂起列表还在，且该 PDO 已不在事务中（说明用户绕过 Gene 直接调了 `$pdo->commit()`，或用 `sql('COMMIT')` 提交），**flush 而不是丢弃**。原则：不确定时多失效一次，代价是一次缓存回源；少失效一次，就会读到脏缓存。连接仍在事务中的，交给现有的事务卫生回滚，回滚路径会按连接 discard。
3. 句柄复用问题：挂起列表存活时间不超过请求上下文，PDO 对象在这期间被 DI 持有，handle 不会被复用。
4. 回归：用两个 Sqlite 内存库复现上面的时序，断言 A 提交后 bumps 里没有 `v.log`；A 回滚后再提交 B，bumps 里有 `v.log`。

#### R5 — Memcached 把超过 30 天的 TTL 当成时间戳

P1 修复后，`Gene\Session` 把 `cookie_lifetime` 作为第三个参数传给 `Gene\Cache\Memcached::set`，后者经 `gene_memcached_set()` 原样转给 `Memcached::set($key, $value, $ttl)`。Memcached 协议规定 expiration 大于 2592000 时按 Unix 时间戳解释，所以 `ttl = 90 天` 会被当成 1970 年，写入即过期，用户永远无法保持登录。修复前不传 TTL，反而不会触发这个问题，所以这是 P1 引入的回归（仅在配置 >30 天时）。

**解决思路：** 在驱动层收口，不在 Session 里特判。`gene_memcached_set()`（以及 `mset` / `add` / `touch` 这类带 expiration 的转发，如有）在 `ttl > 2592000` 且 `ttl < time(NULL)` 时改写为 `time(NULL) + ttl`；已经是绝对时间戳的值原样放行。这样所有调用 `Gene\Cache\Memcached` 的业务都受益。同时把 `memory.c` 的 `gene_memory_set_expiry_nolock(..., int validity)` 改为 `zend_long`，避免 `(int)` 截断超大 TTL。

#### R6 — flip() 在 PostgreSQL 下的参数类型推断

pdo_pgsql 默认使用服务端预处理，普通参数以未指定类型发送。PostgreSQL 解析 `CASE ... THEN $2 ELSE $3 END` 时，如果所有分支都是 unknown，就解析为 `text`，赋给 `integer` 列时报 `column "status" is of type integer but expression is of type text`。`WHEN col = $1` 能从列推断类型，但 THEN/ELSE 不能。MySQL 默认模拟预处理、pdo_sqlsrv 按 PHP 类型发送参数，这两个不受影响。本机没有 PG 环境，**尚未实测**。

**解决思路：**

1. `$values` 两个元素都是 `IS_LONG`（默认的 `[0, 1]` 就是）时，THEN/ELSE 直接用 `ZEND_LONG_FMT` 内联成整数字面量。这是 C 层格式化出来的整数，不存在注入面，四种数据库都能正确推断类型。WHEN 分支仍然绑定参数。
2. `IS_TRUE/IS_FALSE` 按驱动内联为 `1/0`（PG 用 `TRUE/FALSE`）。
3. 字符串值继续绑定；`text → varchar` 在 PG 有赋值转换，可以正常写入。枚举等其他列类型在 stub 注释里说明需要自行 CAST。
4. `DatabaseTest` 的 PG 段加一条 `flip()` 用例，无服务器时 SKIP。demo 里 `Log`、`Module`、`Mark` 的 CASE 裸 SQL 同样改成内联整数（`CASE WHEN status = ? THEN 1 ELSE 0 END`）。

#### R7 — page()/paginate() 不接受 null 排序

```text
Gene\Orm\Model::page(): Passing null to parameter #4 ($order) of type string is deprecated
```

`page()` 照抄了 `paginate()` 的 `"zll|S"`，stub 却写 `$order = null`。**解决：** 两者都改为 `"zll|S!"`，`order == NULL` 时不传第 4 个参数。`page()` 内部调用 `paginate` 时用的是基类的 `zend_function`，子类覆写 `paginate()` 会被绕过；这点在 stub 里写明，或改为从 `ce->function_table` 查找，与静态调用的解析保持一致。

#### R8 — 口令列没有迁移；cookie 寿命 <= 0 的不一致

1. `gene_demo.sql` 把 `user_pass` 改成 `varchar(255)`，只对新装生效。已有 MySQL 库仍是 `varchar(50)`，而 `password_hash()` 的结果是 60 个字符：严格模式下 `add()/edit()` 插入失败，非严格模式下被静默截断，之后 `password_verify` 永远失败，账号从此无法登录。**解决：** 增加幂等迁移（如 `demo/database/migrate_2026_09_23_user_pass.sql`：`ALTER TABLE sys_user MODIFY user_pass varchar(255) NOT NULL DEFAULT ''`），`init_sqlite.php` 不受影响（SQLite 不限制长度）。部署说明写在 CHANGELOG。
2. `verifyPassword()` 旧摘要校验通过后没有升级。**建议：** 旧摘要登录成功后，立即用 `password_hash()` 重写并 `updateBy`（会走 versionKeys）；已是新哈希但 `password_needs_rehash()` 为真时同样重写。`$salt . $password` 的拼接只用于兼容旧格式；新哈希直接 `password_hash($password)`，避免 bcrypt 的 72 字节截断吃掉长口令的尾部。
3. P1 把 `cookie_lifetime <= 0` 的存储 TTL 回落到 86400，但 `gene_cookie()` 对同样的值写 `expires = now + 0`，cookie 立即过期，而不是浏览器会话 cookie。**解决：** `gene_cookie()` 在 lifetime `<= 0` 时传 `expires = 0`（会话 cookie），存储侧保持 86400 作为兜底。二者分别表示「浏览器关了就忘」和「服务端最多留一天」，语义一致。

#### R9 — P2 文档残留

- `reference.md:63` 与 `swoole.md:49`：「冻结进程级 Memory」→ 改为「冻结路由/配置表；用户态 `Memory::*` 写业务分区」。
- `swoole.md:380`：「`Memory::rateLimit` 仅当前 worker 且 `workerReady()` 后冻结」→ 改为「仅当前 worker；请求期可用」。
- 用 `rg "冻结进程级|后冻结|只读" gene-ai-helper gene-ide-helper docs` 做一次全量复查，把结果写进 PR 描述。

#### R10 — opcache 重启后的函数槽

immutable 只保证 SHM 生命周期内地址稳定。FPM 下 `opcache_reset()` 或 SHM 耗尽会触发重启：在下一次 `accel_activate()`（RINIT）时清空 SHM 并重新装载脚本，类条目和 interned 方法名都可能落到旧地址，4 槽缓存会命中已不属于它的 `zend_function*`。概率低，但与 P4 属于同一类问题。

**解决：** 在 Gene 的 RINIT 里清零这 4 个槽，成本是 4×24 字节的 memset。opcache 重启只会发生在 RINIT，Swoole worker 不经过逐请求 RINIT，也不会在进程内完成 opcache 重启，因此这样足以覆盖。不需要引入代际计数。

#### R11 — L1 的尾巴

- `src/gene.h:336` 的 `cache_insert_refused` 字段，以及 `gene.c:1319`、`monitor.c:264` 的清零，一并删除，避免下一轮审计又把它当成有效计数。
- `audit/repro/swoole_route_probe.php:40` 改读 `business_cache_items` / `business_cache_table_size`。
- `Memory::stats()` / `Monitor::stats()` 删除字段属于对外观测接口变更。CHANGELOG 里写明：字段已移除、替代字段是什么、Prometheus 等看板需要同步调整。

#### R12 — 流程与覆盖

1. **提交粒度：** 标题为 `fix(session)` 的单个提交里包含 3 个新公开 API（`flip`、`page`、`versionKeys`）、事务钩子、demo 重构和文档。建议按报告第七节的顺序拆成 P1+P2、P3/O1、O2+O4、O3、P4+L1 五个提交，出问题时可以单独回滚（R2 只需要回滚 O3）。
2. **文档同步：** CHANGELOG 没有 6.2.6/Unreleased 条目；`docs/` 没有 `flip`/`page`/`versionKeys`；`AGENTS.md` 行为约定没有 versionKeys 的失效语义和事务延迟规则；`audit/README.md` 没有本报告的索引说明（如仓库约定需要）。
3. **回归用例缺口：**
   - `SessionTest`：没有 P1 的常驻用例，只有 repro。应把「3 参数句柄收到 TTL、2 参数句柄不报 `ArgumentCountError`、`ttl <= 0` 回落 86400」三条沉淀进去。
   - `OrmTest`：没有「事务**提交成功**后才 bump」的正向用例（只有回滚用例），也没有 R2/R3/R4 的用例。
   - 第七节要求用 `orm_v2_leak_probe.php` 跑 1 万次、`memory_get_usage(true)` 增量为 0，但探针没有扩展到 `flip()`、`page()` 和带 versionKeys 的写入。应补上，尤其是 `gene_orm_version_prefetch()` 的 smart_str / 结果集释放路径，以及挂起列表在 `cleanup()` 中的释放。
   - Swoole：挂起列表按协程隔离的断言（两个协程各自开事务，互不 flush），无 Swoole 环境时 SKIP。

### 8.4 建议修复顺序

1. **R1**（立即）：demo 与文档去掉静态属性类型，并加 demo 加载冒烟测试。不修这一条，其余 demo 改动都无法验证。
2. **R2 + R3 + R4**（同一批，O3 语义修正）：行级失效、非主键 where 预读加上限告警、挂起列表按连接分桶。修完之前，建议 demo 的 `Models\Admin\User` 暂时去掉 `$versionKeys`，并在 Service 里恢复 `bumpUserCacheForUser()`，先堵住「禁用账号仍可登录」的问题。
3. **R8**：库表迁移与旧口令升级。和 R1 一起上线，否则修复 R1 后，已有库在第一次改密时就会写坏口令。
4. **R5、R6、R7**：驱动层 TTL 规范化、`flip()` 整数内联、`S!`。
5. **R9、R10、R11、R12**：文档、RINIT 清槽、字段清理、CHANGELOG 与回归用例。

验收：R1 以 demo 加载冒烟测试 + `/login.action` 返回业务 JSON 为准；R2/R3/R4 以 `OrmTest` 新增断言为准；R6 需要 PG 实例，无环境时明确 SKIP，不以 SKIP 为通过。

---

## 九、第二轮落地复核（2026-09-23 追加）

> 复核对象：提交 `91d0442`（`fix: apply 2026-09-23 audit landing-review remediation (R1-R12)`，46 个文件，+1322/−259）。
> 复核方式：逐条对照 diff 与源码；`x64\Release\php_gene.dll`（19:14:21）晚于 `src/` 最后修改（`orm/meta.c` 19:13:50），免部署运行 TestRunner、`session_store_ttl.php`、`orm_v2_leak_probe.php`，新增 `audit/repro/version_keys_review2.php` 验证 versionKeys 边界。
> 本节只追加，不改写第一至第八节；与前文矛盾之处以本节为准。

### 9.1 R1–R12 落地状态

| 项 | 状态 | 核对结果 |
|----|------|----------|
| R1 静态属性类型 | 已落地，**验收只做了一半** | demo 模型、helper 两份文档、第四节 O3 示例均改为无类型 + PHPDoc；5 个模型、Service、Controller、Hook 实测可加载。但冒烟测试没有加载任何 ORM 模型，见 S3 |
| R2 行级失效 | 已落地，**引入新缺口** | `updateBy` / `flip` / `destroy` 均按主键预读当前行，非 payload 映射列也会 bump。hydrate 路径的 `save()` 省略预读，改名时漏掉旧键，见 S1 |
| R3 非主键 where | 已落地 | `gene_orm_version_prefetch_where()` 复用 `gene_orm_apply_where()`，`LIMIT N+1`，超限时 `E_WARNING` 并跳过失效；`$versionScanLimit` 已在 MINIT 声明（默认 1000）。非原子问题见 S7 |
| R4 按连接分桶 | 已落地 | `{handle => {pdo, maps}}`；commit/rollback 只处理本连接的桶。请求结束时的兜底实测正确：绕过 Gene 直接提交的桶会被冲刷，被事务卫生回滚的桶会被丢弃 |
| R5 Memcached TTL | 已落地 | `gene_memcached_set` / `gene_memcache_set` 对 `IS_LONG` 且位于 (2592000, now) 区间的值改写为 `now+ttl`；`validity` 全线改为 `zend_long`。字符串 TTL 未覆盖，见 S6 |
| R6 flip PG 类型 | 已落地，**未实测** | THEN/ELSE 中的 `IS_LONG` 内联为整数字面量，布尔在 PG 下输出 `TRUE/FALSE`、其它驱动输出 `1/0`；demo 三处裸 SQL 已改为 `THEN 1 ELSE 0`。本机仍无 PG 环境 |
| R7 `S!` 与子类派发 | 已落地 | `page()` / `paginate()` 已改为 `S!`；`page()` 从被调类的 `function_table` 查找 `paginate`。测试断言偏弱，见 S5 |
| R8 口令迁移与 cookie | 已落地，**部分** | 迁移脚本已补；旧摘要登录后自动升级；`cookie_lifetime <= 0` 输出 `expires=0`。新哈希仍拼接 salt，没有接 `password_needs_rehash()`，见 S4 |
| R9 文档残留 | 已落地 | `rg "冻结进程级\|后冻结\|只读"` 已无旧说法；`Memory.php` stub 与源码一致 |
| R10 RINIT 清槽 | 已落地 | 槽位移到文件作用域，由 `gene_cache_call_reset()` 在 RINIT 清零；ZTS 下不做缓存 |
| R11 字段清理 | 已落地 | 全局变量、清零语句、两处 stats 输出、`swoole_route_probe.php` 均已清理；CHANGELOG 已记录字段移除 |
| R12 流程与覆盖 | **部分** | CHANGELOG、`AGENTS.md`、`docs/CONFIGURATION.md` 已同步；`SessionTest` / `OrmTest` / `DemoLoadTest` 已补。仍为单个提交，泄漏探针覆盖有偏差，见 S2、S8 |

测试结果（新 DLL，`GENE_TEST_PHP_ARGS` 注入 `-n -d extension=...`）：21 个套件中 20 个全绿，`OrmTest` 193/193、`SessionTest` 57/57、`DemoLoadTest` 4/4。`LifecycleTest` 20/21，原因是 `-n` 模式未加载 openssl，`Gene\Crypto` 直接致命退出，不是回归。`session_store_ttl.php` 输出 `argc=3 ttl=86400 / argc2=2 / OK`；`orm_v2_leak_probe.php` 各项增量为 0 B。

结论：R1–R12 的 C 层修复基本到位，demo 后台已能加载 ORM 模型。仍需处理一处由 R2 修复引入的缓存一致性回归（S1），以及几项验收、测试口径问题（S2、S3、S5）。

### 9.2 问题清单

| # | 等级 | 问题 | 证据 |
|---|------|------|------|
| **S1** | 高 | `save()` 在 `attrs` 覆盖全部映射列时，把**已修改的** `attrs` 当作写前旧行：`find($id, true)` 或 `fill()` 之后修改映射列再 `save()`，旧键不会 bump，新值还重复出现两次 | 运行时：`version_keys_review2.php` S1 |
| **S2** | 中 | 泄漏探针的「非主键 where 预读 + bump」一项实际走的是超限告警分支，预读与 bump 路径的泄漏没有被测到 | 运行时：6000+ 行时每次都报 `matched more than 1000 rows` |
| **S3** | 中 | R1 的验收没做完：`DemoLoadTest` 只跑 `/healthz` 和未知路由，不加载任何 ORM 模型；`tools/acceptance` 的 `--demo` 没有增加登录请求。demo Service 另有 PHP 8.1 deprecation | 静态 + 运行时 |
| **S4** | 低（安全） | 新口令仍为 `password_hash($salt . $password)`，16 字符 salt 占用 bcrypt 72 字节输入上限；没有 `password_needs_rehash()` 升级路径 | 静态 |
| **S5** | 低 | 测试口径：`page()` 子类派发的断言分不出子类收到的是页码还是偏移量；TestRunner 默认不把 `-d extension=` 传给子进程，容易误测旧 DLL；`LifecycleTest` 缺 openssl 时致命退出而不是 SKIP | 运行时 |
| **S6** | 低 | 字符串 TTL 不参与规范化：Memcached 组件配置里的 `ttl` 若为字符串，不走 R5 的改写；`Gene\Session` 的 `ttl` 配置只接受 `IS_LONG`，`"3600"` 被静默忽略 | 静态 |
| **S7** | 低 | R3 的「先 SELECT 后 UPDATE」在事务外不是原子的；`flip()` 在影响 0 行时仍多一次预读 | 静态 |
| **S8** | 低（流程） | 仍是单个提交；PG 仍未实测；`src/` 下 6 个文件没有 UTF-8 BOM（之前就是这样，但违反 `AGENTS.md` 的 Windows 约定）；工作区遗留未跟踪的 `audit/repro/_tmp_load_check.php` | 静态 |

### 9.3 问题详情与解决思路

#### S1 — hydrate / fill 后改映射列再 `save()`，旧键不失效

复现（Release DLL，`audit/repro/version_keys_review2.php`）：

```text
find($id, true) -> name: old => new -> save()
S1 bumps: [{"v.id":1,"v.name":["new","new"]}]      期望包含 "old"

(new U)->fill(['id' => $id, 'name' => 'new', ...])->save()
bumps:    [{"v.id":1,"v.name":["new","new"]}]      同样缺 "old"
```

根因在 `model.c` `save()` 的 R2 优化：

```c
if (gene_orm_version_covered(&ver_keys, &meta, attrs)) {
    ZVAL_COPY(&ver_old, attrs);      /* attrs 已经是改过的值 */
} else {
    gene_orm_version_prefetch(...);
}
```

`data_copy` 同样复制自 `attrs`，于是 `commit_write` 得到 `prev == neu == 'new'`，`add_pair` 输出 `["new","new"]`。第八节 R2 的建议「hydrate 模型的 `save()` 直接取 `attrs` 里的原值」有前提：模型得保存加载时的原始值。Gene 的模型只有一份 `attributes`，没有 original/dirty 快照，这个前提不成立。`fill()` 的情况更糟：`attrs` 来自调用方，根本不是库里的行。

影响：凡是用 `find($id, true)->xxx = ...; save()` 修改登录名、邮箱等映射列的业务，旧键缓存会保留到 TTL，其间按旧登录名还能查到这一行（含旧的 `status` / 口令摘要）。demo 的 `Models\Admin\User` 走 `updateBy()`，不经过 `save()`，所以 demo 本身不受影响。在 `91d0442` 之前，payload 里有映射列时会按主键预读，不存在这个问题，所以这是 `91d0442` 引入的回归。

**解决思路（二选一，推荐第 1 种先落地）：**

1. **去掉 `covered` 捷径，`save()` 的 update 分支一律按主键预读。** 成本是有二级映射列时每次 `save()` 多一次单行主键 SELECT，与 `updateBy` 一致，也与第八节 R2「成本：与旧 Service 的 `userNameById()` 相同」的承诺一致。同时删除 `gene_orm_version_covered()`，避免被再次误用。
2. **引入原始值快照（后续优化）。** hydrate（`find($id, true)` / `all(true)` 等）完成后，仅在模型声明了 `versionKeys` 时，把二级映射列的值复制到一个受保护的实例属性（例如 `__versionOrig`，只存映射列，不存整行）；`save()` 成功后用新值刷新它；`fill()` / `setExists()` / `create()` 清空它。`save()` 有快照时用快照当旧行，没有快照时回落第 1 种的预读。这样 hydrate 路径保持零额外 SQL，`fill()` 路径也一定正确。快照挂在实例上，随对象释放，不引入请求级或进程级状态。

配套：

- `gene_orm_version_add_pair()` 在 `prev` 与 `neu` 相等（`zend_is_identical` 或 `zend_compare == 0`）时只写一次，避免 `["new","new"]` 这种重复 bump。
- 回归：`OrmTest::testVersionKeysRowLevel` 增加两条断言：「`find($id, true)` 改名后 `save()`，bumps 同时含旧值和新值」「`fill()` 带主键改名后 `save()`，同样含旧值」。若采用第 2 种，再加一条「hydrate `save()` 不发预读 SQL」（用 `history()` 快照计数）。
- `AGENTS.md` 的 ORM 约定已写明「写入前按主键或同一 where 预读受影响行」；修复后 `save()` 与这句话才一致，无需改文字。

#### S2 — 泄漏探针没有测到非主键预读路径

`orm_v2_leak_probe.php` 在 versionKeys 段之前已经 `createMany` 了约 6000 行，`LMV::updateBy(['status' => 1], ['status' => 1])` 每次命中的行数都超过默认上限 1000，于是走 `ver_overflow` 分支：预读结果集被立即释放，`commit_write` 不执行。输出里连续的 `matched more than 1000 rows` 就是证据。「+0 B」只说明告警分支不泄漏。

**解决思路：**

1. 该项改为只命中少量行的条件，例如 `LMV::updateBy(['name' => 'seed'], ['status' => 1])`（`updateOrCreate` 段保证 `seed` 存在且只有 1 行）；或者为探针单独声明 `protected static $versionScanLimit = 100000;` 的子类，让大结果集也走完整的 bump 路径，同时覆盖 `gene_orm_version_gather_col()` 的批量取值。
2. 探针在该项期间装一个 `set_error_handler`，出现 `E_WARNING` 就判失败，防止将来又悄悄退化成测告警分支。
3. 超限分支本身保留一个独立探针项（名字写明 overflow），两条路径各测一次。

#### S3 — R1 验收缺口：demo 冒烟没有触达 ORM 模型

第八节 R1 要求「遍历 `demo/application/{Models,Services,Controllers,Hooks,Ext}` 逐个 `class_exists`」和「`--demo` profile 对 `/login.action` POST 错误口令」。`DemoLoadTest` 实际只跑 `init_sqlite.php`、`cli.php /healthz` 和一个未知路由，`healthz` 不加载任何模型。要是再有人给 `$table` 加上类型，这个测试照样全绿，R1 的原始故障会原样复发。

另外，实测加载 `Services\Admin\User` 时 PHP 8.1 报两条 deprecation：`lists($page = 1, $limit = 10, $search)` 把可选参数放在必填参数之前（`Services\Admin\Log::lists` 同样如此）。demo 在 debug 模式下若把 deprecation 转成异常，就会变成 500。

**解决思路：**

1. `DemoLoadTest` 增加一个子进程用例：注册与 demo 相同的 PSR-4 规则，`RecursiveDirectoryIterator` 遍历上述五个目录，逐个 `class_exists($fqcn, true)`，并用 `set_error_handler` 把 `E_DEPRECATED` / `E_WARNING` 也计为失败；断言 exit 0 且没有输出。工作区里的 `audit/repro/_tmp_load_check.php` 可以作为雏形，收进测试后删除这个临时文件。
2. 本地模式已经补了 sqlite 的 `sys_*` 表和 admin 种子，`DemoLoadTest` 可以再加一个 `cli.php` 用例，直接调用 `Services\Admin\User::getInstance()->checkUser('admin', 'wrong')`（或通过 CLI 路由 POST），期望得到「密码错误」的业务结果而不是致命错误。这一步会真正经过 `cachedVersion` → `Models\Admin\User` → `join()+fields()` → `verifyPassword`。
3. `tools/acceptance` 的 `--demo` profile 同步增加一个 `/login.action` 错误口令请求，期望 HTTP 200 + 业务 JSON。
4. 两个 `lists()` 签名改为 `lists($search = [], $page = 1, $limit = 10)` 并同步调用方，或者给 `$search` 也加默认值 `[]`。

#### S4 — 口令哈希仍拼接 salt，缺少 rehash 路径

`generatePasswordHash()` 是 `password_hash($salt . $password, PASSWORD_DEFAULT)`，`verifyPassword()` 对新格式用 `password_verify($salt . $password, ...)`。bcrypt 只取前 72 字节，16 字符的 salt 占掉其中 16 字节，口令超过 56 字节的部分不参与哈希。`checkUser()` 只在旧 md5 摘要时升级，以后更换算法或 cost 时，已有 bcrypt 哈希不会被升级。

**解决思路：**

1. 新哈希改为 `password_hash($password, PASSWORD_DEFAULT)`，不再拼接 salt（`password_hash` 自带随机盐）。`user_salt` 列保留，只供旧 md5 格式校验使用。
2. `verifyPassword()` 按格式分三类：`$2y$` / `$argon2` 开头的先试 `password_verify($password, $stored)`，失败再试 `password_verify($salt . $password, $stored)`（兼容 `91d0442` 期间写入的拼接格式）；非 `$` 开头的走 legacy md5。
3. `checkUser()` 校验通过后，只要命中「legacy md5」「拼接格式」或 `password_needs_rehash($stored, PASSWORD_DEFAULT)` 任一条件，就用第 1 条的方式重写。重写仍走 `Models\Admin\User::edit()` → `updateBy()`，由 versionKeys 失效按登录名缓存的行。
4. `Services\Admin\User::edit()` 改密时不再生成新 salt（或保留生成，但不参与哈希），与第 1 条一致。

#### S5 — 测试口径问题

1. **`page()` 子类派发断言无法区分偏移量。** `OrmRvSub::paginate()` 返回 `"page" => $page`，但 `page()` 随后用 `add_assoc_long("page", 3)` 覆盖了这个键，所以断言 `page === 3` 永远成立，即使子类收到的第二个参数错成页码而不是偏移量 10，也发现不了。**解决：** 子类返回 `"offset" => $page`（第二个参数），断言 `offset === 10`、`limit === 5`、`order === null`。
2. **TestRunner 默认不把扩展参数传给子进程。** `runIsolated()` 只拼 `PHP_BINARY` 和 `GENE_TEST_PHP_ARGS`。用 `php -n -d extension=<新 DLL> TestRunner.php` 启动时，子进程读的是默认 php.ini，加载的是部署目录里的旧 DLL。本轮第一次运行就因此出现 18 个假失败。**解决：** Runner 启动时若 `GENE_TEST_PHP_ARGS` 为空而当前进程加载的 gene 不是 php.ini 里的那份，就自动转发 `-n` 与 `-d extension=` 参数（可从 `(new ReflectionExtension('gene'))` 与 `php_ini_loaded_file()` 判断），或者至少打印醒目警告；子进程第一行输出 `phpversion('gene')` 与 DLL 路径，便于核对。`AGENTS.md`「验证与测试」一节把 `GENE_TEST_PHP_ARGS` 的用法写进免部署命令。
3. **`LifecycleTest` 缺 openssl 时致命退出。** 按 `test/README.md`「无环境时 SKIP」的约定，`Gene\Crypto` 段在 `!extension_loaded('openssl')` 时应输出 SKIP 并继续，而不是让整个套件 exit 255。

#### S6 — 字符串 TTL 不参与规范化

- `Gene\Cache\Memcached::set()` 未传 TTL 时回落组件配置的 `ttl`。配置经 `Config` / ini 加载时可能是字符串，此时 `gene_memcached_set()` 走 `params[2] = *ttl` 原样透传，超过 30 天的字符串值不会被改写。**解决：** `Z_TYPE_P(ttl) == IS_STRING` 且为数字字符串（`is_numeric_string`）时，先转为 long 再规范化；其它类型原样透传。
- `Gene\Session` 构造时 `ttl` / `uttl` 只接受 `IS_LONG`，`'ttl' => '3600'` 被静默忽略，实际仍用 86400。**解决：** 同样接受数字字符串（`zval_get_long` 前先 `is_numeric_string` 校验），非数字值发一次 `E_WARNING`。这项不是本轮回归，但 P1 之后 `ttl` 直接决定存储寿命，配置写错的代价变大了。

#### S7 — 预读与写入之间的窗口

- R3 的 `prefetch_where` 先 SELECT 再 UPDATE。事务外执行时，两条语句之间被其它连接插入或修改、进而满足 where 的行会被 UPDATE 命中，却不在 bump 集合里；其映射键不会失效。**解决：** 不在 C 层隐式开事务（会改变调用方的事务语义）。在 stub 与 `reference.md` 的 `$versionScanLimit` 说明里写明：非主键批量更新需要严格失效时，应在 `transaction()` 内执行；MySQL/PG 下可以考虑预读时加 `FOR UPDATE`，但仅在已处于事务中时才加（`gene_pdo_in_transaction` 为真），避免事务外锁语义不一致。
- `flip()` 的写后预读在 `affectedRows == 0` 时仍会执行。**解决：** 把 `gene_orm_version_prefetch()` 移到 `n > 0` 的判断之后，省一次无效 SELECT。

#### S8 — 流程与环境

1. **提交粒度：** `91d0442` 仍然一次性包含 ORM 语义修正、Session、Cache、demo 迁移和文档。S1 的修复只涉及 `save()` 与 `meta.c`，建议单独提交，便于回滚。
2. **PG 实测：** R6 仍未在 PostgreSQL 上跑过。`DatabaseTest` 的 PG 段应加上 `flip()` 用例（默认 `[0, 1]` 与 `[true, false]` 各一条），无 PG 时明确 SKIP；发布前在 Linux 验收机上补跑，不以 SKIP 为通过。
3. **BOM：** `src/app/application.c`、`application.h`、`src/cache/cache.h`、`memcached.c`、`src/gene.c`、`src/orm/meta.c` 没有 UTF-8 BOM，而 `meta.c` 本轮新增了含 `—`、`→` 的注释。按 `AGENTS.md`，Windows 下会产生 C4819 类告警。**解决：** 随 S1 一并统一加 BOM（`2594ec7` 的做法），并在 CI 或 pre-commit 加一个「`src/**/*.{c,h}` 必须以 EF BB BF 开头」的检查。
4. **临时文件：** `audit/repro/_tmp_load_check.php` 未跟踪，按 S3 第 1 条收进 `DemoLoadTest` 后删除。

### 9.4 建议修复顺序

1. **S1**（立即）：去掉 `save()` 的 `covered` 捷径并给 `add_pair` 去重，补两条 `OrmTest` 断言；`version_keys_review2.php` 应输出 `S1 OK` 并 exit 0。
2. **S2 + S3**：修正泄漏探针路径，补 demo 全量加载与登录冒烟，修两个 `lists()` 签名。这两项决定了后续回归能否拦住同类问题。
3. **S5**：TestRunner 转发扩展参数（或告警）、`page()` 断言、`LifecycleTest` SKIP。
4. **S4、S6、S7**：口令格式与 rehash、字符串 TTL、预读窗口的文档与 `flip()` 的顺序调整。
5. **S8**：BOM 统一与检查、PG 实测、清理临时文件。

验收：S1 以 `audit/repro/version_keys_review2.php` exit 0 与 `OrmTest` 新断言为准；S2 以探针期间无 `E_WARNING` 且增量为 0 为准；S3 以 `DemoLoadTest` 覆盖模型加载与登录业务 JSON 为准；R6/S8 的 PG 项无环境时明确 SKIP，不以 SKIP 为通过。

## 十、第三轮落地复核（2026-09-23 追加）

> 复核对象：提交 `e337c02`（S1，4 文件 +92/−57）与 `60b7be6`（S2–S8，31 文件 +549/−61）。
> 复核方式：逐条对照 diff；重建 `x64\Release\php_gene.dll`（PHP 8.1.34 NTS x64），免部署运行 TestRunner 全量（`GENE_TEST_PHP_ARGS` 注入 `-n -d extension_dir=... -d extension=pdo_sqlite -d extension=<新 DLL>`）、`version_keys_review2.php`、`orm_v2_leak_probe.php`、`session_store_ttl.php`、`tools/check_src_bom.php`。
> 本节只追加，不改写前文；与前文矛盾之处以本节为准。

### 10.1 S1–S8 落地状态

| 项 | 状态 | 核对结果 |
|----|------|----------|
| S1 `save()` 预读 | 已落地 | `gene_orm_version_covered()` 及其 `orm.h` 声明已删除，`save()` update 分支一律 `gene_orm_version_prefetch()` 按主键预读；`add_pair` 在 prev==new 时只写一次。`version_keys_review2.php` 输出 `S1 bumps: [{"v.id":1,"v.name":["old","new"]}]` → `S1 OK`；`OrmTest` 新增「hydrated save() bumps old and new」「fill()+save() bumps old and new」两条断言 |
| S2 泄漏探针 | 已落地 | 非主键 where 项改为小结果集走完整 prefetch+bump 路径（输出 `updateBy non-pk where prefetch+bump (1 row)/(all rows)` 各 +0 B）；探针段内 `set_error_handler` 把任何 `E_WARNING` 判失败；overflow 独立成项（`updateBy non-pk where OVERFLOW (warn+skip)` +0 B） |
| S3 demo 冒烟 | 已落地 | 新增 `test/demo_class_load.php`（遍历 Models/Services/Controllers/Hooks/Ext 逐个 autoload，`E_DEPRECATED`/`E_WARNING` 计失败）与 `test/demo_login_probe.php`（`checkUser('admin','wrong')` 断言业务 JSON，走通 cachedVersion→ORM→join→verifyPassword 全链）；`DemoLoadTest` 由 4 项扩到 6 项全绿；`Services\Admin\{User,Log}::lists` 必填参数后置改为 `$search = []`；`linux_swoole_verify.sh --demo` 新增 `/doc/1.html`（ORM 路径）与 `/login.action` 错误口令 JSON 探针 |
| S4 口令哈希 | 已落地 | `generatePasswordHash` 不再拼接 salt；`verifyPassword` 三分类（`$` 开头先试裸口令再试 salt 拼接兼容 `91d0442` 存量，非 `$` 走 legacy md5）；`checkUser` 在 legacy/拼接/`password_needs_rehash` 任一命中时重写；`add()`/`edit()` 不再写 `user_salt`（列有 `DEFAULT ''`） |
| S5 测试口径 | 已落地 | `page()` 派发断言改为核对 `offset===10 && lim===5 && order===null`；`TestRunner` 在 `-n` 启动时自动为子进程推导 `-d extension=`（PHP<8.4 无 `ReflectionExtension::getFileName`，回落 extension_dir 文件探测），并输出 `[child-env] gene=<ver>` 横幅与父进程版本比对——实测不设 `GENE_TEST_PHP_ARGS` 时正确捕获 `gene 6.2.4 vs 6.2.5` 错配并告警；`LifecycleTest` 缺 openssl 输出 `SKIP AES-256-GCM` |
| S6 字符串 TTL | 已落地 | `gene_memcached_ttl_zval()` 统一 `IS_LONG`/数字字符串→`normalize_ttl`，`gene_memcached_set`/`gene_memcache_set` 两处接入；`gene_session_config_long` 接受数字字符串，非数字值 `E_WARNING`；`ttl<=0` 仍回落默认 86400，语义一致 |
| S7 预读窗口 | 已落地 | stub `Model.php` 与 `reference.md` 写明预读 SELECT 与 UPDATE 事务外非原子、严格失效应放进 `transaction()`；`flip()` 预读+`commit_write` 整体移入 `n > 0` 分支 |
| S8 流程 | 已落地（PG 除外） | S1 单独提交 `e337c02`；实际扫描 `src/` 有 16 个含非 ASCII 字节且无 BOM 的文件（比第九节点名的 6 个多），全部统一 UTF-8 BOM；新增 `tools/check_src_bom.php`（88 个 .c/.h，0 违规）；`DatabaseTest` PG 段新增 `flip()` 整型/布尔两条用例，本机无 PG → SKIP；`_tmp_load_check.php`、`_tmp_changelog_insert.php`、测试运行日志均已清理 |

### 10.2 验证结果

- **TestRunner**：21 个套件全绿，959/959，`OrmTest` 195/195、`SessionTest` 57/57、`DatabaseTest` 41/41、`DemoLoadTest` 6/6、`LifecycleTest` 22/22（含 openssl SKIP）；无假失败、无致命退出。
- **`version_keys_review2.php`**：exit 0 —— S1 旧键已 bump；S2 开放事务的桶被丢弃、绕过 Gene 的 raw `commit()` 桶被 teardown 冲刷，两侧均符合语义。
- **`orm_v2_leak_probe.php`**：19 项全部 +0 B，含新版 versionKeys 写路径与 overflow 项；期间无 `E_WARNING`。
- **`session_store_ttl.php`**：`argc=3 ttl=86400 / argc2=2 / OK`（P1 复验）。
- **`tools/check_src_bom.php`**：88 files checked, 0 non-ASCII without BOM。
- **`[child-env]` 横幅**：`gene=6.2.5 php=8.1.34`，确认子进程加载的是新构建的 DLL。

### 10.3 遗留与说明

1. **PG 实测仍缺**：`flip()` 的 PG 用例已进 `DatabaseTest`，但本机无 PostgreSQL，按约定记 SKIP，不以 SKIP 为通过；发布前需在 Linux 验收机补跑。
2. **PHP<8.4 的参数推导是尽力而为**：无 `ReflectionExtension::getFileName` 时按 extension_dir 文件名猜测，命中同名旧 DLL 的风险存在；`[child-env]` 版本比对横幅是兜底，实测可拦下错配。显式 `GENE_TEST_PHP_ARGS` 仍是推荐用法。
3. **`user_salt` 列保留**：新记录写入 `''`，仅服务于存量 legacy md5 摘要校验；待旧摘要全部迁移后可再评估下线。
4. 本轮收口后，第一至第九节列出的 P1–P4、L1、O1–O4、R1–R12、S1–S8 全部闭环；剩余的只有需要外部环境的验证项（PG）与约定级说明。

结论：S1–S8 全部按第九节方案落地，构建与全部免部署验证通过；`e337c02`/`60b7be6` 两提交可直接进入验收与发布流程。
