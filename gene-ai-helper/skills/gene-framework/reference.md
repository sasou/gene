# Gene 框架 API 参考

供开发时查阅。仅列签名与简要说明。

---

## Controller

控制器基类。继承后可通过 `$this->属性名` 使用 config 中注册的组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
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
| run($method = null, $uri = null) | 启动路由分发，参数不传时自动从 $_SERVER 读取，返回 $this |
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
| config($key) | 从内存缓存读取配置项 |
| params($name = null) | 获取路由路径参数（不传则返回全部数组） |

---

## Request

请求数据访问，用于非控制器场景（Swoole/Service 内）。方法均为静态。

| 方法 | 说明 |
|------|------|
| get($key, $default = null) | 获取 GET 参数 |
| request($key, $default = null) | 获取 REQUEST 参数 |
| post($key, $default = null) | 获取 POST 参数 |
| cookie($key, $default = null) | 获取 Cookie |
| files($key, $default = null) | 获取上传文件 |
| server($key, $default = null) | 获取 $_SERVER |
| env($key, $default = null) | 获取环境变量 |
| params($key = null) | 获取路由路径参数 |
| isAjax() | 是否 AJAX 请求 |
| getMethod() | 获取请求方法 |
| isGet(), isPost(), isPut(), isHead(), isOptions(), isCli() | 请求方法判断 |
| init($get = null, $post = null, $cookie = null, $server = null, $env = null, $files = null, $request = null) | 批量注入自定义请求数据（Swoole 等场景使用） |

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
| cookie($name, $value = null, $expires = null, $path = null, $domain = null, $secure = null, $httponly = null) | 设置 Cookie |
| url($path) | 带当前语言前缀的 URL |
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

---

## Gene\Db\Mysql

MySQL 数据库。链式调用示例：
```php
// 查询列表 + 分页
$list = $this->db
    ->select("sys_user", "user_id, user_name")
    ->where(['status' => 1])
    ->order("user_id desc")
    ->limit($start, $pageSize)   // $start=偏移量, $pageSize=返回行数
    ->all();

// 条件写法
->where(['name' => ['%keyword%', 'like']])  // 关联数组自动解析 like/in
->where("user_id=?", $id)                   // 原始 SQL + 绑定参数
->in("group_id in(?)", [1, 2, 3])           // IN 条件
```

**属性**：`$config`, `$pdo`, `$sql`, `$where`, `$group`, `$having`, `$order`, `$limit`, `$data`

| 方法 | 说明 |
|------|------|
| __construct($config) | 构造，`$config` 含 `dsn`/`username`/`password`/`options` |
| getPdo() | 返回底层 PDO 对象 |
| select($table, $fields = null) | 构建 SELECT，返回 $this |
| count($table, $fields = null) | 构建 SELECT count，返回 $this |
| insert($table, $fields) | 构建 INSERT（$fields 为关联数组），返回 $this |
| batchInsert($table, $fields) | 构建批量 INSERT（$fields 为二维数组），返回 $this |
| update($table, $fields) | 构建 UPDATE（$fields 为关联数组），返回 $this |
| delete($table) | 构建 DELETE，返回 $this |
| where($where, $fields = null) | 设置 WHERE 条件，返回 $this |
| in($in, $fields = null) | 设置 IN 条件（含 `in(?)` 占位符），返回 $this |
| sql($sql, $fields = null) | 设置原始 SQL，返回 $this |
| limit($num, $offset = null) | `LIMIT $num` 或 `LIMIT $num, $offset`（$num 为偏移量，$offset 为行数），返回 $this |
| order($order), group($group), having($having) | ORDER BY / GROUP BY / HAVING，返回 $this |
| execute() | 执行 SQL，返回 PDOStatement |
| all() | 执行并 fetchAll()，返回数组或 null |
| row() | 执行并 fetch()，返回单行或 null |
| cell() | 执行并 fetchColumn()，返回单列值或 null |
| lastId() | 写操作后返回最后插入 ID |
| affectedRows() | 写操作后返回受影响行数 |
| print() | 不执行，返回 `['sql' => ..., 'param' => ...]`（调试用） |
| beginTransaction(), inTransaction(), rollBack(), commit() | 事务操作 |
| free() | 释放 PDO 连接（GC 回收） |
| history() | 返回 SQL 执行历史数组（含 sql/param/time/memory） |

---

## Gene\Db\Sqlite

SQLite 数据库。API 与 Mysql 完全一致。
`limit($num, $offset)` 生成 `LIMIT $offset OFFSET $num`。

---

## Gene\Db\Pgsql

PostgreSQL 数据库。API 与 Mysql 完全一致。
`limit($num, $offset)` 生成 `LIMIT $offset OFFSET $num`。

---

## Gene\Db\Mssql

MS SQL Server 数据库。API 与 Mysql 完全一致。
表名使用方括号包裹；`limit($num, $offset)` 生成 `OFFSET $num ROWS FETCH NEXT $offset ROWS ONLY`。

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
| __call($method, $params) | 透传调用底层 Redis 对象任意命令，支持断线重连 |

---

*文档由 gene-ide-helper 下 Gene 命名空间及 Gene\Db、Gene\Cache 子命名空间内全部类文件提炼生成，版本 5.2.0。*
