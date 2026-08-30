# Gene 框架 API 参考

供开发时查阅。仅列签名与简要说明。

---

## Controller

控制器基类。继承后可通过 `$this->属性名` 使用 config 中注册的组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`、`\Gene\Rest $rest`

| 方法 | 说明 |
|------|------|
| __construct() | 控制器基类构造函数；子类可自由重写以替代已回退的 `init()` 生命周期钩子 |
| get($key, $default = null) | 获取 GET 参数 |
| request($key = null, $default = null) | 获取 REQUEST 参数 |
| post($key = null, $default = null) | 获取 POST 参数 |
| cookie($key = null, $default = null) | 获取 Cookie |
| files($key = null, $default = null) | 获取上传文件 |
| server($key = null, $default = null) | 获取 $_SERVER |
| env($key = null, $default = null) | 获取环境变量 |
| params($key = null) | 获取路由路径参数（不传则返回全部数组） |
| isAjax() | 是否 AJAX 请求（X-Requested-With 头） |
| getMethod() | 获取请求方法（小写），返回 string\|null |
| getLang() | 获取当前语言前缀 |
| isGet(), isPost(), isPut(), isHead(), isOptions(), isDelete(), isCli() | 请求方法判断 |
| redirect($url, $code = null) | 重定向（默认 302） |
| assign($name, $value) | 视图变量赋值 |
| display($file, $parent_file = null) | 渲染视图模板 |
| displayExt($file, $parent_file = null, $isCompile = false) | 扩展渲染（模板引擎编译模式） |
| contains(), containsExt() | 返回子视图路径（供 layout 嵌套使用） |
| url($path) | 返回带当前语言前缀的 URL，如 `url("login.html")` → `"/en/login.html"` |
| success($msg, $code = 2000) | 构建成功响应数组 `[code, msg]` |
| error($msg, $code = 4000) | 构建失败响应数组 `[code, msg]` |
| data($data, $count = -1, $msg = null, $code = 2000) | 构建带数据的响应数组 `[code, msg, data, count]` |
| json($data, $callback = null, $code = 256) | JSON 编码并直接输出，支持 JSONP |

---

## Application

应用入口与运行上下文。

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造，`$safe` 为隔离命名空间 key |
| getInstance($safe = null) | 获取/创建单例实例 |
| autoload($app_root = null, $auto_function = null) | 设置应用根目录及自定义自动加载函数，返回 $this |
| load($file, $path = null, $validity = null) | 加载配置/路由文件，`$path` 默认用 APP_ROOT，`$validity` 为文件变更检测间隔（秒），返回 $this |
| setMode($error_type = null, $exception_type = null, $ex_callback = null, $error_callback = null) | 设置错误（1=内置HTML）与异常处理器，返回 $this |
| setView($view = null, $tpl_ext = null) | 设置视图目录名与模板后缀，返回 $this |
| error($type, $callback = null, $error_type = null) | 注册错误处理回调，返回 $this |
| exception($type, $callback = null) | 注册异常处理回调，返回 $this |
| run($method = null, $uri = null) | 启动路由分发；FPM 无参读 $_SERVER；Swoole 无参读 Request 上下文 |
| webscan(...) | 内置 Web 扫描防护（开关、白名单目录/URL、GET/POST/Cookie/Referer） |
| waitWorkerReady() | Swoole：阻塞直到 workerStart 调用 workerReady() |
| workerReady() | Swoole：标记 Worker 就绪，冻结进程级 Memory，预热请求上下文池 |
| prewarmCtxPool($count = -1) | Swoole：预热请求上下文池到指定数量（-1 表示填充到 `gene.ctx_pool_max`），返回实际新增的上下文数 |
| setRuntimeType($type) | 设置运行时类型：`'fpm'`/1、`'swoole'`/2、`'coroutine'`/3，返回 bool |
| getRuntimeType() | 返回当前运行时类型整数 |
| getRuntimeTypeName() | 返回运行时类型名称字符串（`"fpm"` / `"swoole"` / `"coroutine"`） |
| setEnvironment($type = null) | 设置运行环境（1=dev, 2=test, 3=prod），返回 bool |
| getEnvironment() | 返回当前环境整数 |
| getEnvironmentName() | 返回环境名称字符串（`"dev"` / `"test"` / `"prod"`） |
| getMethod() | 获取当前 HTTP 请求方法 |
| getPath() | 获取当前请求路径 |
| getRouterUri() | 获取路由模式 URI（:m/:c/:a 已替换） |
| getLang() | 获取当前语言前缀 |
| getModule(), getController(), getAction() | 当前模块、控制器、动作 |
| clearState() | 软重置当前请求上下文（释放用户数据但保留上下文结构体），Swoole 下建议用 cleanup() |
| destroyContext() | 销毁当前协程的请求上下文结构体，Swoole 下建议用 cleanup() |
| cleanup($gc = false) | **Swoole 推荐**：合并 clearState + destroyContext 的两阶段清理，FPM 下等同 clearState；`$gc=true` 在 Swoole 下额外触发 `gc_collect_cycles()` |
| setResponse($response) | 将 Swoole Response 对象存入当前请求上下文 |
| config($key) | 从内存缓存读取配置项 |
| params($name = null) | 获取路由路径参数（不传则返回全部数组） |

---

## Request

请求数据访问，用于非控制器场景（Swoole/Service 内）。方法均为静态。

| 方法 | 说明 |
|------|------|
| get($key, $default = null) | 获取 GET 参数 |
| request($key, $default = null) | 获取 REQUEST 参数 |
| input(?string $key = null, $default = null) | 合并 GET+POST 与 JSON 对象，JSON 覆盖同名字段；仅解析 `application/json`/`application/*+json`，非法或非对象 JSON 抛异常；与 `json()` 共用每请求解析缓存 |
| post($key, $default = null) | 获取 POST 参数 |
| cookie($key, $default = null) | 获取 Cookie |
| files($key, $default = null) | 获取上传文件 |
| server($key, $default = null) | 获取 $_SERVER |
| env($key, $default = null) | 获取环境变量 |
| params($key = null) | 获取路由路径参数 |
| isAjax() | 是否 AJAX 请求 |
| getMethod() | 获取请求方法 |
| isGet(), isPost(), isPut(), isHead(), isOptions(), isCli() | 请求方法判断 |
| header($key, $default = null) | 获取 HTTP 请求头 |
| clear() | 清除请求数据缓存 |
| init($get, $post, $cookie, $server, $env, $files, $request = null, $header = null, $rawContent = null) | Swoole 注入请求；未传 $request 时合并 GET+POST；$rawContent 对应 Swoole `$request->rawContent()` |
| json() | 解析 rawContent 为 JSON 对象/数组；空 body → `null`；非法 JSON / JSON `null` / 标量抛异常。禁止直接读 `php://input` |
| bearer() | 从 header/server 读 `Authorization`；仅接受大小写不敏感的 `Bearer` scheme + SP/HTAB，非 Bearer、裸 token 或空 token 返回 `null` |
| snapshot() | 压入 get/post/files/request/header/raw 快照，返回新深度；上限 8 |
| restore() | 弹出快照；栈空返回 false |
| scope($get, $post, $files = null, $request = null) | 只改入参袋；`$request===null` 时合并 get+post。业务互调应走 `Invoke`/`Rest`，不要 `init` 整包覆盖 |

---

## Invoke

进程内隔离调用：切 Request 再 `new` Controller，异常也会 restore。与 `Controller::forward`（不切 Request）并存。

| 方法 | 说明 |
|------|------|
| local($class, $action, $params = [], $files = []) | 深度上限 8；动作从 Request 取参；只返回 action 返回值 |

---

## Rest

命名出站 REST。配置只读；`use('name')` 返回新 proxy，不写回 DI 单例。无 `__call`、无 `init(app_key)`。本地 `class_exists` 走 Invoke，否则必须给 `path`。

```php
$config->set('rest', [
    'class' => '\\Gene\\Rest',
    'params' => [[
        'timeout' => 5,
        'connect_timeout' => 2,
        'ssl_verify' => true,
        'keep_alive' => true,
        'headers' => ['Accept' => 'application/json'],
        'pass_request_id' => true,
        'services' => [
            'user' => ['base_url' => 'http://127.0.0.1:8081', 'local' => 'Api\\', 'timeout' => 8],
        ],
    ]],
    'instance' => true,
]);
$rest->use('user')->call('Ping', 'pong', $params);
```

| 方法 | 说明 |
|------|------|
| use($name) | 新 proxy，共享只读配置 |
| local($class, $action, $params = [], $files = []) | 同 `Invoke::local` |
| http($method, $path, $options = []) | `base_url` + 以 `/` 开头的 path；`decode=>true` 时 JSON 解码 body，失败抛 |
| call($class, $action, $params = [], $options = []) | 可 dispatch 则 local；否则要求 `options['path']` |

---

## Response

响应输出封装。方法均为静态。

| 方法 | 说明 |
|------|------|
| redirect($url, $code = null) | 发送 HTTP 重定向 |
| redirectJs($url) | JS `window.location.href` 跳转 |
| alert($text, $url = null) | JS `alert` 弹窗，可选跳转 |
| success($msg, $code = 2000) | 构建成功响应数组 |
| error($msg, $code = 4000) | 构建失败响应数组 |
| data($data, $count = -1, $msg = null, $code = 2000) | 构建带数据响应数组 |
| json($data, $callback = null, $code = 256) | JSON 编码并输出，支持 JSONP |
| header($key, $value) | 设置自定义响应头 |
| cookie($name, $value = null, $expires = null, $path = null, $domain = null, $secure = null, $httponly = null, $samesite = null) | 设置 Cookie（samesite: "Lax"/"Strict"/"None"，设为 "None" 时通常需同时 secure=true） |
| url($path) | 带当前语言前缀的 URL |
| end($data = null) | 结束响应（Swoole `$response->end` / FPM `php_write`） |
| write($chunk) | 分块写出且不结束响应。FPM: `php_write`+flush；Swoole: `$response->write` |
| sseStart() | `text/event-stream`，关 gzip / output_buffering，`X-Accel-Buffering: no`。CLI/Swoole SAPI 名是 `cli`，不丢用户 `ob_start` |
| sseEvent($event, $data) | 写一帧 SSE；数组/对象会 JSON 编码；多行 `data:` |
| sseEnd() | 等价 `end()` |
| setJsonHeader() | 设置 `Content-Type: application/json` |
| setHtmlHeader() | 设置 `Content-Type: text/html` |

---

## View

视图：变量赋值与模板渲染。

| 方法 | 说明 |
|------|------|
| assign($name, $value) | 赋值模板变量 |
| display($file, $parent_file = null) | 渲染视图模板 |
| displayExt($file, $parent_file = null, $isCompile = false) | 扩展渲染（编译模板引擎） |
| contains() | 返回子视图文件路径 |
| containsExt() | 返回子视图编译文件路径 |
| url($path) | 带语言前缀的 URL |
| scope($num = 0) | 管理视图变量作用域版本号（多层模板嵌套隔离） |

---

## Router

路由配置与调度。含 `$safe`、`$prefix` 属性。

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造，`$safe` 为路由树隔离命名空间 |
| get/post/put/patch/delete($path, $handler, $hooks = null) | 注册对应 HTTP 方法路由（通过 `__call` 实现）|
| hook($name, $callback) | 注册钩子（通过 `__call` 实现） |
| error($code, $callback) | 注册错误处理（通过 `__call` 实现） |
| group($prefix = null) | 开始/结束路由分组，无参数时结束当前分组 |
| prefix($name = null) | 设置全局路由前缀，返回 $this |
| lang($lang_list) | 启用多语言路由，`$lang_list` 为逗号分隔语言列表，返回 $this |
| run($method = null, $uri = null) | 执行路由匹配与分发 |
| runError($method) | 触发指定错误路由（如 `"404"`） |
| dispatch($class, $action, $params) | 直接实例化类并调用方法，支持 :c/:a 替换 |
| params($name = null) | 获取路由路径参数 |
| getLang() | 获取当前语言 |
| getTree(), getEvent(), getConf() | 获取路由树/事件/配置（用于调试） |
| delTree(), delEvent(), delConf() | 清除路由树/事件/配置缓存 |
| clear() | 同时清除树和事件缓存 |
| getTime() | 路由树创建后经过的秒数 |
| getRouter() | 返回自身实例（链式辅助） |
| readFile($file) | 读取文件内容并返回字符串 |
| assign($name, $value) | 向视图赋值 |
| display($file, $parent_file = null) | 渲染视图 |
| displayExt($file, $parent_file = null, $isCompile = false) | 扩展渲染 |

---

## Gene\Language

语言组件，加载 `Language/{Dir}/{Lang}.php` 中返回的数组，配合路由多语言 `url()` 使用。
通常通过配置注入为 `$this->language`。

```php
// 配置注入
$config->set('language', [
    'class'    => '\Gene\Language',
    'params'   => ['web', 'en'],  // 默认目录 web、默认语言 en
    'instance' => true,
]);

// 控制器内使用
$this->language->web('zh');           // 切换到 Language/Web/Zh.php
$title = $this->language->login_title; // 读取键值
```

| 方法 | 说明 |
|------|------|
| __construct($dir, $defaultLang = 'en') | 构造，设置默认目录与语言 |
| lang($lang = null) | 设置当前语言代码（如 zh/en/ru），返回 $this |
| __call($name, $args) | `$language->web('zh')` → 设置目录=web、语言=zh，返回 $this |
| __get($name) | 读取当前目录/语言文件的键值，文件不存在返回空字符串 |

---

## Config

配置读写。含 `$safe` 属性（命名空间）。

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造 |
| set($key, $value, $ttl = 0) | 写入配置项，支持 TTL（秒） |
| get($key) | 读取配置项，支持点号路径（如 `"db.host"`） |
| del() | 清除整个 safe 命名空间下的配置 |
| clear() | 同 `del()`（别名） |

---

## Di

依赖注入容器。单例，使用静态方法操作。

| 方法 | 说明 |
|------|------|
| getInstance() | 获取容器单例 |
| get($name) | 获取已注册对象（优先内存，再读 config 自动创建） |
| has($name) | 判断容器中是否存在指定 key |
| set($name, $value) | 向容器注册对象或值 |
| del($name) | 从容器删除指定 key |

---

## Load

加载器。

| 方法 | 说明 |
|------|------|
| getInstance() | 获取单例 |
| import($file = null) | 加载（include）指定 PHP 文件，返回 $this |
| autoload($className) | 类自动加载器（命名空间/下划线风格 → 文件路径） |

---

## Factory

工厂创建实例。

| 方法 | 说明 |
|------|------|
| create($class, $params = null, $type = 0) | 创建指定类实例并调用构造函数，`$type > 0` 时缓存到 DI 容器 |

---

## Model

模型基类。通过 `$this->属性名` 使用注入组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`

| 方法 | 说明 |
|------|------|
| getInstance($params = null) | 获取当前类的单例实例（通过 DI 容器） |
| success($msg, $code = 2000) | 构建成功响应数组 |
| error($msg, $code = 4000) | 构建失败响应数组 |
| data($data, $count = -1, $msg = null, $code = 2000) | 构建带数据响应数组 |

---

## Orm\Model / Orm\Query（ActiveRecord v2，6.1.0+）

数据 Model 继承 `\Gene\Orm\Model`（本身继承 `\Gene\Model`）。声明 `static $table` / `$primaryKey` / `$fields`。

| 静态属性 | 说明 |
|----------|------|
| $timestamps | true 时 create/save/updateBy/toggle/createMany 自动填充时间列；payload 已含该列则不覆盖 |
| $createdAt / $updatedAt | 时间列名，默认 `created_at`/`updated_at`；设为 `null`/`''` 则该列不写（6.1.0+） |
| $timestampFormat | `'datetime'`（Y-m-d H:i:s，默认）或 `'unix'`（int 秒）（6.1.0+） |

| 方法 | 说明 |
|------|------|
| find($id, $asModel = false) / findAll($where = []) | 查询；默认返回数组，`find($id, true)` 返回模型实例 |
| findMany($ids, $preserveOrder = false) | 主键 IN 批量取（一次查询）；空数组返回 [] 不发 SQL；>1000 发 E_NOTICE（6.1.0+） |
| paginate($where, $offset, $limit, $order = null) | {count, list}；order 仅作用于列表阶段（6.1.0+） |
| query() / where($where, $bind = null) | 返回 `Gene\Orm\Query` |
| create($data) / updateBy($where, $data) / destroy($id) / destroyAll($ids) | 写入。**$where 语义**：数组=条件集合（键须为字符串列名，值可为标量或 `[val, op]`；数字键 / 空 op 数组等「非空但语义为空」形态由 makeWhere 响亮失败或匹配 0 行，**不会**静默全表写）、标量=主键值（**非 raw SQL 片段**；'status=1' 这类字符串会静默匹配 0 行）；空数组/null 抛异常（拒绝全表更新，6.1.0+）。raw 片段请用 `query()->where('…')->update($data)` |
| createMany($rows) | 批量插入（一次 round-trip），返回影响行数；每行键集合与顺序必须一致，否则抛异常；大批量在调用方分片（建议 500/批），>5000 发 E_NOTICE（6.1.0+） |
| insertIgnore($data) | 幂等写入（MySQL INSERT IGNORE / SQLite INSERT OR IGNORE；Pgsql/Mssql 抛异常），返回影响行数（6.1.0+） |
| updateOrCreate($where, $data) | 查到则更新（返回影响行数），否则插入（返回新 id，关联数组 where 并入新行）；非原子，并发竞争用唯一键 + insertIgnore（6.1.0+）。$where 语义同 updateBy，空数组/null 在更新分支抛异常 |
| toggle($id, $field, $values = [0, 1]) | 状态翻转（CAS：WHERE pk=? AND field=?，并发败者返回 0）；$timestamps 开启时同步 $updatedAt（6.1.0+） |
| transaction($fn) / transact($fn) | 在本模型 `$connection` 对应 Db 上执行回调事务，语义同 `Gene\Db\*::transaction()`（6.1.0+） |
| fill($data, $hydrate = true) / setExists($exists = false) / save() / delete() / toArray() | 实例 ActiveRecord；含非空主键的 `fill()` 默认标记为已持久化，新增自然主键/UUID 数据请用 `fill($data, false)`、`setExists(false)` 或 `create()` |

### Query（有序 ops 列表，6.1.0+）

Query 是**一次性构建器**（构建 → 执行 → 丢弃），不可缓存或交错构建两个（共享同一 DI Db 句柄，执行时先 reset）。条件**累加**而非覆盖：

```php
User::query()
    ->fields(['u.id', 'u.name'])
    ->join('orders o', ['o.user_id' => 'u.id'], 'LEFT')
    ->joinOn('flags f', [
        ['left' => 'f.user_id', 'op' => '=', 'column' => 'u.id'],
        ['left' => 'f.enabled', 'op' => '=', 'value' => 1],
    ], 'LEFT')
    ->where(['u.status' => 1])          // 数组条件
    ->where('u.name != ?', $x)          // 原始片段（之间自动 AND）
    ->where('u.id', '>=', $anchor)      // 比较简写：> >= < <= != =（白名单外抛异常）
    ->in('u.id', $ids)                  // 列形式；空数组 → 终端直接返回空，不发 SQL
    ->whereLike('u.name', $kw)          // LIKE %kw%（自动转义 \ % _，勿双转义）
    ->selectSub('SELECT count(*) FROM orders', 'oc')  // 子查询列（$sql 开发者书写不转义）
    ->group('u.id')->having('count(o.id) >= 1')
    ->order('u.id desc')->limit($off, $n)
    ->all();
```

| 终端方法 | 说明 |
|----------|------|
| all() / row() / cell() / first() | 查询；first() = limit(1)+row() |
| count() | 继承 where/join，忽略 order/limit/lock；**不得与 group() 组合**（count over GROUP BY 语义会错，检测到即抛异常），分组统计用 count()+all() 两步 |
| paginate($offset, $limit) | {count, list}；普通单表快速路径；与 group() 组合抛异常；UNION 自动按最终复合结果统计 |
| paginateResult($offset, $limit) | 用只读冻结编译快照统计最终 JOIN/GROUP/HAVING/UNION 结果，count 去除外层 order/limit/lock，list 仅覆盖外层 limit |
| joinOn($table, $predicates, $type = 'INNER') | 结构化 ON：每项严格为 `left/op` 加且仅加 `column` 或 `value`；op 仅 `= != > >= < <=`，null 仅 `=`/`!=`；值始终绑定且绑定按 JOIN→WHERE 顺序 |
| union($query) / unionAll($query) | 仅接受同一 Db 的 Query；调用时冻结子分支；拒绝 self/环/超过 8 层、子分支 order/limit/lock、复合查询写入或加锁 |
| update($data) / delete() / increment($column, $amount = 1) / decrement(...) | 立即执行，返回影响行数；算术步长须为有限正数。**必须**带**有效** where()/in() 条件——无条件、`where([])`、`where('')` 一律抛异常；`in('id', [])` 为安全空操作（返回 0）；不支持 join/union |
| lockForUpdate() / sharedLock() | 行锁（仅 select 终端；MySQL FOR UPDATE / LOCK IN SHARE MODE，Pgsql FOR UPDATE / FOR SHARE，Sqlite no-op+E_NOTICE，Mssql 抛异常）；须在事务内，否则 E_NOTICE |

复杂 SQL 仍用 `$this->db`。Swoole 下配合 `instance=>true` + Pool；见 `swoole.md` §4.3。

---

## Service

服务基类。通过 `$this->属性名` 使用注入组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`

| 方法 | 说明 |
|------|------|
| getInstance($params = null) | 获取当前类的单例实例（通过 DI 容器） |
| success($msg, $code = 2000) | 构建成功响应数组 |
| error($msg, $code = 4000) | 构建失败响应数组 |
| data($data, $count = -1, $msg = null, $code = 2000) | 构建带数据响应数组 |

---

## Exception

异常与错误处理。继承自 PHP 内置 `Exception`。

| 方法 | 说明 |
|------|------|
| setErrorHandler($callback, $error_type = null) | 注册 PHP 错误处理器 |
| setExceptionHandler($callback = null) | 注册 PHP 异常处理器 |
| doException($ex) | 内置异常展示器（输出美化 HTML 页面） |
| doError($code, $msg, $file = null, $line = null, $params = null) | 内置错误处理器（转为异常抛出） |
| getMessage(), getCode(), getFile(), getLine(), getTrace(), getPrevious(), getTraceAsString(), __toString() | 标准异常接口 |

---

## Validate

数据校验。链式调用：`init($data)->name('field')->规则()->msg('错误')->valid()`。

| 方法 | 说明 |
|------|------|
| __construct($data = null) | 构造，可传入待验证数据数组 |
| init($data) | 重置所有状态并设置验证数据，返回 $this |
| name($field) | 设置当前字段（支持逗号分隔多字段），返回 $this |
| skipOnEmpty() | 空值时跳过后续验证规则，返回 $this |
| filter($method, $args = null) | 对字段值应用过滤函数，返回 $this |
| extend($rule, $fn) | 静态（5.6.8+）：注册全局自定义规则，分派先于内置表；回调 `fn($value, ...$args): bool`，false 判失败；FPM 请求级 / Swoole worker 级注册表 |
| addValidator($name, $callback, $msg) | 注册自定义验证器闭包，返回 $this |
| msg($msg) | 为上一条规则设置自定义错误消息，返回 $this |
| valid() | 执行验证，遇第一个错误即停止，返回 bool |
| groupValid() | 执行验证，收集所有字段错误，返回 bool |
| error() | 返回第一条验证错误消息 |
| getValue($field = null) | 返回验证通过的字段值（或全部） |
| getError($field = null) | 返回错误信息（指定字段或全部） |
| required() | 必填 |
| match($regex) | 正则匹配 |
| max($max), min($min), range($min, $max) | 数值最大值/最小值/范围 |
| length($min, $max) | 字符串长度范围（mb_strlen） |
| size($min, $max) | 数组元素数量范围 |
| in($list) | 值在指定列表中 |
| url($flags = null), email(), ip(), mobile() | URL/邮箱/IP/手机号格式验证 |
| date(), datetime($format = null) | 日期/日期时间格式验证 |
| number(), int(), digit(), string() | 数值/整数/纯数字串/字符串类型 |
| equal($name), equals($value) | 与另一字段值相等 / 与指定值相等 |

---

## Session

会话管理。实例方法，需先通过配置注入或手动实例化。
驱动对象需实现 `get(id)`、`set(id, data)`、`delete(id)` 三个方法。

```php
// 配置注入（推荐使用自定义 \Ext\Session 或 \Gene\Session）
$config->set('websession', [
    'class'  => '\Gene\Session',
    'params' => [[
        'driver'   => 'redis',   // DI 中注册的存储驱动组件名
        'name'     => 'SESSID',  // Cookie 名
        'ttl'      => 86400,     // Session 数据过期时间（秒）
        'uttl'     => 0,         // Cookie Max-Age（0=浏览器会话）
        'secure'   => true,      // 是否仅 HTTPS（samesite='None' 时通常必须为 true）
        'samesite' => 'None',    // Cookie SameSite 属性："Lax"/"Strict"/"None"
    ]],
]);

// 使用
$user = $this->websession->get('user'); // 支持点号路径
$this->websession->set('user', $data); // 支持点号路径
$this->websession->del('user.id');    // 支持点号路径
$this->websession->destroy();       // 全部清除并重新生成 SessionId
```

| 方法 | 说明 |
|------|------|
| __construct(array $config) | 构造，必须传入配置数组（含 `driver`/`name`/`ttl` 等） |
| load() | 从存储驱动加载 Session 数据，返回 $this，非必须调用 |
| save() | 持久化数据到存储驱动并刷新 Cookie，返回 $this ，非必须调用|
| get($name = null) | 获取值（支持点号路径，不传返回全部） |
| set($name, $value) | 设置值（支持点号路径，自动持久化），返回 bool |
| del($name) | 删除指定键（支持点号路径），返回 bool |
| has($name) | 判断键是否存在，返回 bool |
| destroy() | 清除所有数据并重新生成 SessionId，返回 bool |
| cookie($name, $value, $time) | 直接设置 Cookie（使用 Session 的 path/domain 配置），返回 $this |
| getSessionId() | 获取当前 SessionId（不含前缀），返回 string\|null |
| setSessionId($cookie) | 手动设置 SessionId（Swoole 场景），返回 $this |
| setLifeTime($time) | 设置 Cookie 生存时长（秒），返回 bool |

---

## Benchmark

性能统计。所有方法均为静态。

| 方法 | 说明 |
|------|------|
| start() | 记录起始时间点与峰值内存 |
| end() | 记录结束时间点与峰值内存 |
| time($type = false) | 返回耗时字符串；`true` 精确浮点，`false` 3位小数（默认） |
| memory($type = false) | 返回内存差字符串；`true` 单位 KB，`false` 单位 MB（默认） |

---

## Gene\Memory

进程级共享内存缓存。基于 Zend 持久化 HashTable，跨请求存活（Worker 进程生命周期），不依赖外部存储。
适合高频读取、低频更新的数据（配置、权限表、路由预热等）。

> **注意**：每个 Worker 进程独立内存空间，多 Worker 模式下数据不互通。

```php
// 配置注入
$config->set('memory', [
    'class'    => '\Gene\Memory',
    'params'   => [['myapp']],   // 命名空间前缀
    'instance' => true,
]);

// 使用
$this->memory->set('config', $data, 3600); // 缓存1小时
$data = $this->memory->get('config');
$this->memory->exists('config');           // 检查是否存在
$this->memory->del('config');              // 删除
$this->memory->clean();                    // 清空全部
```

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造，`$safe` 为命名空间前缀（默认用 app_key 或目录路径） |
| set($key, $value, $ttl = 0) | 存入进程内共享内存，`$ttl=0` 永不过期 |
| get($key) | 读取值，key 不存在返回 null |
| getTime($key) | 获取某 key 的写入时间戳 |
| exists($key) | 检查 key 是否存在，返回 bool |
| del($key) | 删除指定 key |
| clean() | 销毁并重新初始化整个共享内存 HashTable |
| stats() | 分区观测：缓存条目数、协程上下文/ctx pool/sweep 遥测、闭包源码缓存等 |
| incr/decr($key, $step = 1) | 写锁内原子加减；缺失键以步进值创建 |
| rateLimit($key, $max, $windowSec) | 单进程固定窗口；超限 `false`。多 worker 不共享，请用 Redis |
| lock($key, $ttlSec) / unlock($key, $token) | 进程内 NX+TTL 锁。Swoole `workerReady()` 后冻结写入 |

---

## Gene\Monitor

聚合可观测出口（5.6.8+）。单一静态入口读取 Memory 分区统计、命名连接池统计与请求计数，纯读、零副作用。

```php
$stats = \Gene\Monitor::stats();
// [
//   'memory'        => [...同 Gene\Memory::stats() 键...],
//   'db_pools'      => ['dbPool'    => [total,idle,using,overflow,min,max,closed]],
//   'redis_pools'   => ['redisPool' => [total,idle,using,overflow,min,max,closed]],
//   'requests'      => ['count' => n, 'errors' => n],
//   'redis_pool_cas_abandoned'      => n,   // RedisPool CAS 放弃计数（防御观测）
//   'swoole_auto_cleanup_defers'    => n,   // 自动 cleanup defer 注册数
//   'swoole_auto_cleanup_reclaimed' => n,   // 自动 cleanup 实际归还数
// ]
```

| 方法 | 说明 |
|------|------|
| stats() | 静态；返回上示聚合数组 |
| reset() | 静态；重置累计监控计数器 |
| prometheus() | 静态；导出 Prometheus 文本格式指标 |

相关 INI：`gene.swoole_auto_cleanup`（默认 0，协程自动 cleanup 兜底）、`gene.cache_easy_ttl`（默认 0 关闭，cache_easy 惰性过期秒数）。

---

## Gene\Db\Mysql

MySQL 数据库。链式调用示例：
```php
// 查询列表 + 分页
$list = $this->db
    ->select("sys_user", "user_id, user_name")
    ->where(['status' => 1])
    ->order("user_id desc")
    ->limit($start, $pageSize)   // 双参：$start=偏移量, $pageSize=返回行数；单参 $num 为返回行数
    ->all();

// 条件写法
->where(['name' => ['%keyword%', 'like']])  // 关联数组自动解析 like/in
->where("user_id=?", $id)                   // 原始 SQL + 绑定参数
->in("group_id in(?)", [1, 2, 3])           // IN 条件
```

**属性**：`$config`, `$pdo`, `$sql`, `$where`, `$group`, `$having`, `$order`, `$limit`, `$lock`, `$data`

| 方法 | 说明 |
|------|------|
| __construct($config) | 构造，`$config` 含 `dsn`/`username`/`password`/`options` |
| getPdo() | 返回底层 PDO 对象 |
| select($table, $fields = null) | 构建 SELECT，返回 $this |
| count($table, $fields = null) | 构建 SELECT count，返回 $this |
| insert($table, $fields) | 构建 INSERT（$fields 为关联数组），返回 $this |
| batchInsert($table, $fields) | 构建批量 INSERT（$fields 为二维数组），返回 $this |
| insertIgnore($table, $fields) | 幂等写入（MySQL INSERT IGNORE / SQLite INSERT OR IGNORE；Pgsql/Mssql 抛异常），惰性执行（6.1.0+） |
| upsert($table, $fields, $updateCols) | INSERT ... ON DUPLICATE KEY UPDATE（MySQL 专属；SQLite/Pgsql/Mssql 抛异常），惰性执行（6.1.0+） |
| update($table, $fields) | 构建 UPDATE（$fields 为关联数组），返回 $this |
| delete($table) | 构建 DELETE，返回 $this |
| where($where, $fields = null) | 设置 WHERE 条件，返回 $this |
| in($in, $fields = null) | 设置 IN 条件（含 `in(?)` 占位符），返回 $this |
| join($table, $on, $type = 'INNER') | JOIN：$on 为 leftColumn=>rightColumn 关联数组；type 支持 INNER/LEFT/RIGHT/CROSS/FULL（6.1.0+） |
| leftJoin($table, $on) / rightJoin($table, $on) | LEFT/RIGHT JOIN 简写（6.1.0+） |
| union($query, $all = false) | UNION / UNION ALL；$query 可为 SQL 字符串或构建器对象（6.1.0+） |
| reset() | 重置构建器状态（sql/join/where/group/having/union/order/limit/data），便于复用同一实例（6.1.0+） |
| sql($sql, $fields = null) | 设置原始 SQL，返回 $this |
| limit($num, $offset = null) | 单参时 `$num` 为返回行数，生成 `LIMIT $num`；双参时 `$num` 为偏移量、`$offset` 为返回行数，生成 `LIMIT $num, $offset`，返回 $this |
| order($order), group($group), having($having) | ORDER BY / GROUP BY / HAVING，返回 $this |
| lockForUpdate() | SELECT ... FOR UPDATE（MySQL/Pgsql）；Sqlite no-op+E_NOTICE；Mssql 抛异常；须在事务内（6.1.0+） |
| sharedLock() | SELECT ... LOCK IN SHARE MODE（MySQL）/ FOR SHARE（Pgsql）；Sqlite no-op+E_NOTICE；Mssql 抛异常；须在事务内（6.1.0+） |
| execute() | 执行 SQL，返回 PDOStatement |
| all() | 执行并 fetchAll()，返回数组或 null |
| row() | 执行并 fetch()，返回单行或 null |
| cell() | 执行并 fetchColumn()，返回单列值或 null |
| lastId() / lastInsertId() | 写操作后返回最后插入 ID（lastInsertId 为 PDO 命名别名，5.7.0+） |
| affectedRows() / rowCount() | 写操作后返回受影响行数（rowCount 为 PDO 命名别名，5.7.0+） |
| quote($str, $paramType = PDO::PARAM_STR) | PDO::quote 透传，字符串字面量转义（5.7.0+） |
| print() | 不执行，返回 `['sql' => ..., 'param' => ...]`（调试用） |
| beginTransaction(), inTransaction(), rollBack(), commit() | 事务操作（手动） |
| transaction($fn) / transact($fn) | 回调事务：未在事务中则 begin→$fn→commit；已在事务中只执行 $fn。异常时仅本层 begin 才 rollBack 并原样抛出。PDO 不支持嵌套 begin（6.1.0+） |
| release() | 将 PDO 连接归还连接池（启用 pool 时），非 pool 模式为空操作；归还时若仍有未提交事务会先 rollBack 并 E_WARNING（6.1.0+） |
| free() | 释放/归还 PDO 连接；启用 pool 时等价于 `release()`，否则关闭连接 |
| history() | 返回 SQL 执行历史数组（含 sql/param/time/memory） |

---

## Gene\Db\Sqlite

SQLite 数据库。API 与 Mysql 一致，差异如下：
- `limit($num, $offset)` 生成 `LIMIT $offset OFFSET $num`
- `insertIgnore` 走 `INSERT OR IGNORE`（6.1.0+）；`upsert` 抛异常（用 `sql()` + ON CONFLICT）
- `lockForUpdate`/`sharedLock`：no-op + E_NOTICE（SQLite 写锁为整库级，6.1.0+）
- `attach($path, $schema)` / `detach($schema)`：附加/分离外部 SQLite 数据库文件

---

## Gene\Db\Pgsql

PostgreSQL 数据库。API 与 Mysql 一致，差异如下：
- `limit($num, $offset)` 生成 `LIMIT $offset OFFSET $num`
- `insertIgnore`/`upsert` 抛异常（用 `sql()` + ON CONFLICT DO NOTHING/UPDATE，6.1.0+）
- `lockForUpdate` → `FOR UPDATE`；`sharedLock` → `FOR SHARE`（6.1.0+）

---

## Gene\Db\Mssql

MS SQL Server 数据库。API 与 Mysql 一致，差异如下：
- 表名使用方括号包裹；`limit($num, $offset)` 生成 `OFFSET $num ROWS FETCH NEXT $offset ROWS ONLY`
- `insertIgnore`/`upsert` 抛异常（用 `sql()` + MERGE/IF NOT EXISTS，6.1.0+）
- `lockForUpdate`/`sharedLock` 抛异常（WITH (UPDLOCK)/HOLDLOCK 是表提示，需写在 FROM 处，6.1.0+）

---

## Gene\Cache\Cache

透明缓存装饰器。代理调用 `[$instance, 'method']` 并将返回值缓存到外部存储（Redis/Memcached），支持版本号失效策略。1、项目内数据更新位置调用updateVersion方法自动更新版本失效缓存；2、配合数据库同步插件https://github.com/sasou/syncClient可以异步自动更新版本失效缓存；3、也可以手动更新版本（不建议）；版本号名字遵循一定规则：表级缓存（数据库名.数据表名）、数据行级缓存（数据库名.数据表名.字段名）；

```php
// 配置注入
$config->set('cache', [
    'class'    => '\Gene\Cache\Cache',
    'params'   => [['hook' => 'redis', 'sign' => 'app:', 'versionSign' => 'db:']],
    'instance' => false,
]);

// 使用：缓存 UserService::getList() 的结果 300 秒
$list = $this->cache->cached(
    ['\Models\User', 'getList'],
    [$page, $limit],
    300
);

// 表级版本号缓存：user 表更新后该表缓存自动失效
$list = $this->cache->cachedVersion(
    ['\Models\User', 'getList'],
    [$page],
    ['db.user' => null],   // 关联版本字段
    300
);
$this->cache->updateVersion(['db.user' => null]); // 使 user 版本失效


// 字段级版本号缓存：user.id = $id 条件下的数据更新后自动失效该字段相关缓存
$list = $this->cache->cachedVersion(
    ['\Models\User', 'row'],
    [$id],
    ['db.user.id' => $id],   // 关联版本字段
    300
);
$this->cache->updateVersion(['db.user.id' => $id]); // 使 user 表id=$id的版本失效

// 多值字段版本号缓存：批量查询多个用户，任一用户更新时自动失效
$result = $this->cache->cachedVersion(
    ['\Models\User', 'batch'],
    [[$id1, $id2, $id3]],
    ['db.user.id' => [$id1, $id2, $id3]],   // 多值版本字段
    300
);
$this->cache->updateVersion(['db.user.id' => [$id1, $id2, $id3]]); // 批量更新版本
```

| 方法 | 说明 |
|------|------|
| __construct($configs) | 构造，含 `hook`（外部缓存组件名）/`sign`（key前缀）/`versionSign`（版本前缀） |
| cached($obj, $args, $ttl = null) | 外部缓存代理，缓存未命中时执行调用 |
| localCached($obj, $args, $ttl = null) | 本地 APCu 缓存代理 |
| unsetCached($obj, $args, $ttl = null) | 删除外部缓存中对应项 |
| unsetLocalCached($obj, $args, $ttl = null) | 删除本地 APCu 缓存中对应项 |
| cachedVersion($obj, $args, $versionField, $ttl = null, $mode = null) | 带版本号控制的外部缓存，versionField 支持多值数组 |
| localCachedVersion($obj, $args, $versionField, $ttl = null, $mode = null) | 带版本号控制的本地 APCu 缓存，versionField 支持多值数组 |
| getVersion($version) | 从外部缓存读取指定版本字段的当前值 |
| updateVersion($version) | 对版本字段执行 incr，支持多值数组批量更新，使关联缓存失效 |

---

## Gene\Cache\Memcached

Memcached 缓存封装。支持多节点、持久连接、自动序列化。

```php
$config->set('memcache', [
    'class'    => '\Gene\Cache\Memcached',
    'params'   => [['servers' => [['host' => '127.0.0.1', 'port' => 11211]],
                    'persistent' => true, 'serializer' => 2]],
    'instance' => true,
]);
```

| 方法 | 说明 |
|------|------|
| __construct($config) | 构造，含 `servers`/`persistent`/`options`/`serializer`/`ttl` |
| get($key) | 获取，`$key` 可为数组（批量 getMulti），自动反序列化 |
| set($key, $value, $ttl = null, $flag = null) | 设置，自动序列化 |
| incr($key, $value = null) | 自增（默认 +1） |
| decr($key, $value = null) | 自减（默认 -1） |
| __call($method, $params) | 透传调用底层 Memcached/Memcache 对象的任意方法 |

---

## Gene\Cache\Redis

Redis 缓存封装。支持单节点/集群（随机选节点），自动序列化，断线自动重连。

```php
$config->set('redis', [
    'class'    => '\Gene\Cache\Redis',
    'params'   => [['host' => '127.0.0.1', 'port' => 6379,
                    'persistent' => true, 'serializer' => 1, 'ttl' => 0]],
    'instance' => true,
]);
```

| 方法 | 说明 |
|------|------|
| __construct($config) | 构造，含 `host`/`port`/`servers`（集群）/`timeout`/`persistent`/`password`/`options`/`serializer`/`ttl` |
| get($key) | 获取，`$key` 可为数组（批量 mGet），自动反序列化，断线自动重连 |
| set($key, $value, $ttl = null) | 设置，有 TTL 时用 setEx，自动序列化，支持断线重连 |
| rateLimit($key, $max, $windowSec) | 原子固定窗口限流（Lua INCR+EXPIRE）；超限返回 `false`，不抛 |
| lock($key, $ttlSec) | `SET key token NX EX`；成功返回 token，失败 `false` |
| unlock($key, $token) | Lua 比对后 DEL；token 不符返回 `false` |
| __call($method, $params) | 透传调用底层 Redis 对象任意命令，支持断线重连 |

---

---

## Hook

路由钩子基类，与 Controller 类似可访问 DI 组件。推荐替代闭包钩子（直接 C 分发，无 eval）。

```php
// router.ini.php
->hook('adminAuth', 'Hooks\AdminAuth@handle')

// application/Hooks/AdminAuth.php
class AdminAuth extends \Gene\Hook {
    public function handle() {
        if (!$this->session->get('admin')) {
            $this->redirect('/login.html');
            return false;
        }
        return true;
    }
}
```

| 方法 | 说明 |
|------|------|
| handle() | 命名钩子入口；返回 false 中止请求 |
| before() / after($params) | 全局前后钩子可覆写 |
| get/post/request/params/... | 与 Controller 相同的静态取参 |
| redirect($url, $code), assign, display, success/error/data/json | 实例或静态辅助 |

**@property**：`db`、`memcache`、`redis`、`cache`、`validate`、`session`、`request`、`response` 等（随 config 注入）

---

## Gene\Pool

Swoole 协程 **PDO 连接池**（FPM 无效）。

| 方法 | 说明 |
|------|------|
| create($name, $configKey, $options = []) | 从 Config 键读取 dsn 等并注册池 |
| getInstance($name) | 获取池实例 |
| get() | 借出 PDO |
| put($pdo) | 归还 |
| remove() | 连接失效，不归还 |
| close() | 关闭单池 |
| recycleIdle() | 立即回收空闲超时的连接，通常由定时器自动调用 |
| closeAll() / stopTimers() | Worker 停止/退出时清理 |
| stats() | 连接数、空闲、overflow 等 |

`config` 中 db：`params[0]['pool'] => 'dbPool'` 与 `create('dbPool', 'db')` 对应。

---

## Gene\Cache\RedisPool

Swoole 协程 **Redis 连接池**（FPM 无效）。API 与 `Gene\Pool` 对称：`create`、`get`、`put`、`remove`、`close`、`recycleIdle`、`closeAll`、`stopTimers`、`stats`。

`Gene\Cache\Redis` 配置 `'pool' => 'redisPool'` 后自动借还；`release()` 显式归还。

---

## Log

| 方法 | 说明 |
|------|------|
| debug/info/notice/warning/error($message, $context = []) | 写日志；`$context` 会 JSON 追加。袋中有 `request_id` 时自动合并（调用方已给的 `request_id` 优先） |
| exception(\Throwable $e, $message = null) | 记录异常 |
| setFile($file) / setLevel($level) | 日志文件与级别 |

---

## Gene\Context

请求级 KV，挂在 `gene_request_context`，FPM RSHUTDOWN / Swoole `cleanup()` 释放。常驻进程勿用静态变量存请求态。

| 方法 | 说明 |
|------|------|
| set($key, $value) | 写入 |
| get($key, $default = null) | 读取 |
| has($key) | 判断键是否存在，可区分缺失与显式 `null` |
| all() | 返回全部 |

## Gene\Json

| 方法 | 说明 |
|------|------|
| encode($data) | `JSON_UNESCAPED_UNICODE\|UNESCAPED_SLASHES`，失败抛 |
| decode($str) | 失败抛，禁止静默 `[]` |

## Gene\Http

出站 HTTP。FPM/CLI 走 PHP `curl_*`（缺扩展则抛清晰异常）；`runtime_type >= 2` 且存在 `Swoole\Coroutine\Http\Client` 则走协程客户端，避免阻塞 worker。`Http::multi` 在 FPM/CLI 以及 Swoole Native CURL hook 下走 `curl_multi`（`concurrency` 生效）；无 Native hook 时顺序 Client 并 `E_NOTICE`。不跨请求连接池；FPM 请求内复用 curl 句柄（`multi` 用独立 easy，不用 ctx 单例）；Swoole `keep_alive=>true` 按 `host:port:ssl` 在**当前请求/协程**内复用 Client（`cleanup()` 时释放）。

**Swoole SSE / stream 非 TTFB**：Client 无 body write 回调，`stream`/`sse` 在 `execute` 完成后按 8KB 切片喂解析器；解析正确，但首字节延迟等于整包收完。FPM 的 `WRITEFUNCTION` 才是真增量。

```php
$r = \Gene\Http::request([
    'method'  => 'POST',
    'url'     => $url,
    'headers' => ['Authorization' => 'Bearer ...'],
    'query'   => ['page' => 2, 'tag' => ['a', 'b']], // RFC3986，追加已有 query，fragment 留在末尾
    'json'    => $payload,        // 与 body / form / files 互斥
    'form'    => ['grant_type' => 'client_credentials'], // 无文件 urlencoded；有文件为 multipart 字段
    'files'   => ['f' => $path],  // multipart；值可为路径或 ['tmp_name','name','type']
    'timeout' => 60,
    'connect_timeout' => 3,
    'ssl_verify' => true,
    'retry'   => 0,               // 仅 GET/HEAD；5xx/超时；上限 3
    'keep_alive' => false,        // Swoole：请求内复用 Client
    'stream'  => function (string $chunk) {},
    'sse'     => function (string $event, $data) {}, // 与 stream 互斥
    'sse_forward' => false,       // true 时每帧 Response::sseEvent
    'discard_body' => false,      // true 不累积 body（流式转发省 RSS）
]);
// ['status'=>int, 'headers'=>array, 'body'=>string]

// query 可与任一 body 类型组合。json、字符串 body、无文件 form 互斥；files + form 推荐用于 multipart。
// files + array body 仅为兼容写法，body + form 始终抛异常。自动 Content-Type 不覆盖任意大小写的调用方 header。
// query/form 的对象、资源值会被拒绝；未知 option 发 E_NOTICE（含 key），且不会发送到后端。

$batch = \Gene\Http::multi([
    ['method' => 'GET', 'url' => $u1],
    ['method' => 'GET', 'url' => $u2],
], ['concurrency' => 8]); // 默认 8，上限 16；请求数上限 64
// 单项失败不抛：status=0 + error。禁止 stream/sse。
// FPM/CLI 与 Swoole Native CURL hook：curl_multi 并行（RETURNTRANSFER，不走 request 的 WRITEFUNCTION）。
// Swoole 无 Native hook：顺序 Coroutine\Http\Client，并 E_NOTICE。
```

禁止嵌套 `request()`/`multi()`（`http_busy`）。应用层 WAF 用 `Application::webscan()`，不要 PHP `Ext\Webscan`。

## Gene\Text

无请求状态。知识库 ingest 原语，不含 RAG 打分。

| 方法 | 说明 |
|------|------|
| utf8Len($s) | UTF-8 码点数；非法序列按 U+FFFD 计 1，不抛 |
| sanitizeMb4($s) | 去 NUL；非法 UTF-8 替换 U+FFFD；保留 4 字节 emoji |
| chunk($s, $maxChars = 1200, $overlap = 80) | 空行合并后硬切；`$maxChars`≤8192；chunk 数≤4096 |

## Gene\Crypto

不是 JWT。密钥从 config/env 注入，禁止从数据库口令派生。旧 CBC 不兼容。Cookie 值用 token，写入仍走 `Response::cookie`。

```php
\Gene\Response::cookie('vbd', \Gene\Crypto::hmacToken(['wid' => 1, 'c' => $code], $secret, 2592000), time() + 2592000, '/', '', true, true);
$payload = \Gene\Crypto::hmacVerify($_COOKIE['vbd'] ?? '', $secret, 60); // leeway 秒
\Gene\Crypto::tsSkew((int)$timestamp, 1800);
$sig = \Gene\Crypto::hmacSign($canonical, $secret);
```

| 方法 | 说明 |
|------|------|
| base64UrlEncode / base64UrlDecode | URL-safe Base64 |
| hmacToken($payload, $secret, $ttl = 0) | 载荷 + HMAC-SHA256；`$ttl>0` 写入 `exp` |
| hmacVerify($token, $secret, $leeway = 0) | 校验签名；`now > exp+leeway` 过期；`now+leeway < nbf` 未生效 |
| hmacSign($data, $secret) | HMAC-SHA256 原始字节再 base64url |
| hmacCheck($data, $sig, $secret) | 恒定时间比较，失败返回 false 不抛 |
| tsSkew($unix, $maxSkew = 1800) | `|now-unix| <= maxSkew` |
| randomId($prefix = '', $bytes = 16) | `prefix + bin2hex(random_bytes)` |
| encrypt / decrypt($data, $key) | AES-256-GCM；`$key` 必须 32 字节 |

限流：单 worker 用 `Memory::rateLimit`；多 worker / Swoole 用 `Redis::rateLimit`（Lua）。
