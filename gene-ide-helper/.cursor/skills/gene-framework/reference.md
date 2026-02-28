# Gene 框架 API 参考

从 gene-ide-helper 类文件逐类提炼，供开发时查阅。仅列签名与简要说明。

---

## Controller

控制器基类。继承后可通过 `$this->属性名` 使用 config 中注册的组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
| get($key, $value = null) | 获取 GET 参数 |
| request($key = null, $value = null) | 获取 REQUEST 参数 |
| post($key = null, $value = null) | 获取 POST 参数 |
| cookie($key = null, $value = null) | 获取/设置 Cookie |
| files($key = null, $value = null) | 获取上传文件 |
| server($key = null, $value = null) | 获取 $_SERVER |
| env($key = null, $value = null) | 获取环境变量 |
| params($key = null) | 获取路由参数 |
| isAjax() | 是否 AJAX 请求 |
| getMethod() | 获取请求方法，返回 string\|null |
| getLang() | 获取当前语言 |
| isGet(), isPost(), isPut(), isHead(), isOptions(), isDelete(), isCli() | 请求方法判断 |
| redirect($url, $code = null) | 重定向 |
| assign($name, $value) | 视图变量赋值 |
| display($file, $parent_file = null) | 渲染视图 |
| displayExt($file, $parent_file = null, $isCompile = null) | 扩展渲染 |
| contains(), containsExt() | 包含模板 |
| url($path) | 返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html" |
| success($msg, $code = null), error($msg, $code = null) | 成功/错误响应 |
| data($data, $count = 0, $msg = null, $code = null) | 数据响应 |
| json($data, $callback = null, $code = null) | JSON 响应 |

---

## Application

应用入口与运行上下文。

**@property**（注释中写为 Service）：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcache $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造 |
| getInstance($safe = null) | 单例 |
| load($file, $path, $validity = null) | 加载配置/路由等文件 |
| autoload($app_root, $auto_function) | 注册自动加载 |
| setMode($error_type, $exception_type) | 设置错误/异常模式 |
| setView($view, $tpl_ext = null) | 设置视图类与模板扩展名 |
| error($type, $callback, $error_type = null) | 错误处理回调 |
| exception($type, $callback) | 异常处理回调 |
| run($method, $uri) | 执行请求 |
| getMethod(), getPath(), getRouterUri() | 请求方法、路径、路由 URI |
| getLang() | 获取当前语言 |
| getModule(), getController(), getAction() | 当前模块、控制器、动作 |
| setEnvironment($type), getEnvironment() | 设置/获取环境类型 |
| config($key), params($key) | 配置项、路由参数 |

---

## Request

请求数据访问，用于非控制器场景。静态方法对应 Controller 的请求相关方法。

| 方法 | 说明 |
|------|------|
| get($key, $value = null), request($key, $value = null), post($key, $value = null) | GET/REQUEST/POST |
| cookie($key, $value = null), files($key, $value = null), server($key, $value = null), env($key, $value = null) | Cookie/文件/Server/环境变量 |
| params($key = null) | 路由参数 |
| isAjax(), getMethod() | 是否 AJAX、请求方法 |
| isGet(), isPost(), isPut(), isHead(), isOptions(), isCli() | 请求方法判断 |
| init($get, $post, $cookie, $server, $env, $files, $request) | 初始化请求数据（用于 CLI/测试等） |

---

## Response

响应输出封装。

| 方法 | 说明 |
|------|------|
| redirect($url, $code = null) | 重定向 |
| redirectJs($url, $code = null) | JavaScript 跳转 |
| alert($text, $url = null) | 弹窗并可选跳转 |
| success($msg, $code = null), error($msg, $code = null) | 成功/错误响应 |
| data($data, $count = 0, $msg = null, $code = null), json($data, $callback = null, $code = null) | 数据/JSON 响应 |
| header($key, $value = null) | 设置响应头 |
| cookie($key, $value = null) | 设置 Cookie |
| url($path) | 带当前语言前缀的 URL |
| setJsonHeader(), setHtmlHeader() | 设置 JSON/HTML 响应头 |

---

## View

视图：变量赋值与模板渲染。

| 方法 | 说明 |
|------|------|
| assign($name, $value) | 赋值 |
| display($file, $parent_file) | 渲染模板 |
| displayExt($file, $parent_file, $isCompile) | 扩展渲染 |
| contains(), containsExt() | 包含模板 |
| url($path) | 带语言前缀的 URL |
| scope($num = 0) | 作用域，返回 bool |

---

## Router

路由配置与调度。含 `$safe`、`$prefix` 属性。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
| getEvent(), getTree(), delTree(), delEvent(), clear(), getTime() | 路由事件与树操作 |
| getRouter(), getConf(), delConf() | 路由与配置 |
| getLang() | 获取当前语言 |
| assign($name, $value) | 赋值 |
| display($file, $parent_file), displayExt($file, $parent_file, $isCompile) | 静态视图渲染 |
| runError($method) | 运行错误处理 |
| run($method, $uri) | 执行路由 |
| readFile($file) | 读取文件 |
| dispatch($class, $action, $params) | 分发到类/方法/参数 |
| params() | 路由参数 |
| prefix($prefix) | 设置前缀，返回 $this |
| group($callback) | 路由组 |
| lang($lang) | 设置语言，返回 $this |

---

## Config

配置读写。含 `$safe` 属性。

| 方法 | 说明 |
|------|------|
| __construct($safe = null) | 构造 |
| set($key, $value, $ttl = null) | 设置配置项 |
| get($key) | 获取配置项 |
| del(), clear() | 删除/清空 |

---

## Di

依赖注入容器。单例，静态方法操作。

| 方法 | 说明 |
|------|------|
| getInstance() | 获取单例 |
| get($name) | 获取已注册服务 |
| has($name) | 是否已注册 |
| set($name, $value) | 注册服务 |
| del($name) | 删除注册 |

---

## Load

加载器。

| 方法 | 说明 |
|------|------|
| getInstance() | 单例 |
| import($file) | 引入文件 |
| autoload($className) | 类自动加载 |

---

## Factory

工厂创建实例。

| 方法 | 说明 |
|------|------|
| create($class, $params, $type) | 创建指定类实例 |

---

## Model

模型基类。通过 `$this->属性名` 使用注入组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
| getInstance($params = null) | 获取当前类实例 |
| success($msg, $code = null), error($msg, $code = null) | 成功/错误响应 |
| data($data, $count = 0, $msg = null, $code = null) | 数据响应 |

---

## Exception

异常与错误处理。

| 方法 | 说明 |
|------|------|
| setErrorHandler($callback, $error_type) | 设置错误处理回调 |
| setExceptionHandler($callback) | 设置异常处理回调 |
| doException($ex) | 执行异常处理 |
| doError($code, $msg, $file, $line, $params) | 执行错误处理 |
| __construct($message, $code, $previous) | 构造 |
| getMessage(), getCode(), getFile(), getLine(), getTrace(), getPrevious(), getTraceAsString(), __toString() | 标准异常接口 |

---

## Validate

数据校验。含属性：`$data`, `$key`, `$field`, `$method`, `$config`, `$value`, `$error`, `$closure`。

| 方法 | 说明 |
|------|------|
| __construct($data), init($data) | 构造/初始化数据 |
| name($field) | 指定校验字段 |
| skipOnEmpty() | 空值跳过 |
| filter($method, $args) | 过滤 |
| addValidator($name, $callback, $msg) | 添加自定义校验器 |
| msg($msg) | 设置错误信息 |
| valid(), groupValid() | 执行校验、组校验 |
| error() | 获取错误 |
| getValue($field = null), getError($field = null) | 获取值/错误信息 |
| required() | 必填 |
| match($regex), max($max), min($min), range($min, $max) | 正则、最大、最小、范围 |
| length($min, $max), size($min, $max) | 长度、大小 |
| in($list) | 在列表中 |
| url($flags), email(), ip(), mobile() | URL/邮箱/IP/手机 |
| date(), datetime($format) | 日期/日期时间 |
| number(), int(), digit(), string() | 数字/整数/数字串/字符串 |
| equal($name), equals($value) | 与某字段/某值相等 |

---

## Session

会话。静态方法读写。

| 方法 | 说明 |
|------|------|
| get($name) | 获取会话项 |
| has($name) | 是否存在 |
| set($name, $value) | 设置 |
| del($name) | 删除 |
| clear() | 清空 |

---

## Service

服务基类。通过 `$this->属性名` 使用注入组件。

**@property**：`\Gene\Db\Mysql $db`、`\Gene\Cache\Memcached $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Gene\Validate $validate`、`\Ext\Services\Rest $rest`

| 方法 | 说明 |
|------|------|
| getInstance($params = null) | 获取当前类实例 |
| success($msg, $code = null), error($msg, $code = null) | 成功/错误响应 |
| data($data, $count = 0, $msg = null, $code = null) | 数据响应 |

---

## Benchmark

性能统计。

| 方法 | 说明 |
|------|------|
| start(), end() | 开始/结束计时 |
| time($type) | 获取时间统计 |
| memory($type) | 获取内存统计 |

---

## Gene\Db\Mysql

MySQL 数据库。链式调用：select/count/insert/batchInsert/update/delete + where/in/order/group/having/limit + execute + all/row/cell。

**@property**（注释中写为 Service）：`\Gene\Db\Mysql $mydb`、`\Gene\Cache\Memcache $memcache`、`\Gene\Cache\Redis $redis`、`\Gene\Cache\Cache $cache`、`\Ext\Services\Rest $rest`  
**属性**：`$config`, `$pdo`, `$sql`, `$where`, `$group`, `$having`, `$order`, `$limit`, `$data`

| 方法 | 说明 |
|------|------|
| __construct($config) | 构造 |
| getPdo() | 获取 PDO（返回 self） |
| select($table, $fields = null), count($table, $fields = null) | 查询/统计 |
| insert($table, $fields), batchInsert($table, $fields) | 插入/批量插入 |
| update($table, $fields), delete($table) | 更新/删除 |
| where($where, $fields), in($in, $fields) | 条件 |
| sql($sql, $fields = null) | 原生 SQL |
| limit($limit), order($order), group($group), having($having) | 子句 |
| execute() | 执行 |
| all(), row(), cell() | 多行/单行/单列值 |
| lastId(), affectedRows() | 最后 ID、影响行数 |
| print() | 打印 SQL |
| beginTransaction(), inTransaction(), rollBack(), commit() | 事务 |
| free(), history() | 释放、历史 SQL |

---

## Gene\Db\Sqlite

SQLite 数据库。API 与 Mysql 一致（select/count 的 $fields 必填，sql 的 $fields 必填）。

**@property**：`\Gene\Db\Sqlite $sqdb`、Cache/Redis/Cache、Rest

---

## Gene\Db\Pgsql

PostgreSQL 数据库。API 与 Mysql/Sqlite 一致。

**@property**：`\Gene\Db\Pgsql $pgdb`、Cache/Redis/Cache、Rest

---

## Gene\Db\Mssql

MS SQL Server 数据库。API 与 Mysql/Sqlite 一致。

**@property**：`\Gene\Db\Mssql $msdb`、Cache/Redis/Cache、Rest

---

## Gene\Cache\Cache

缓存版本与本地缓存管理。含 `$config`。

| 方法 | 说明 |
|------|------|
| cached(), localCached() | 获取缓存/本地缓存 |
| unsetCached(), unsetLocalCached() | 清除缓存/本地缓存 |
| cachedVersion(), localCachedVersion() | 缓存版本 |
| getVersion(), updateVersion() | 获取/更新版本 |

---

## Gene\Cache\Memcached

Memcached。含 `$config`, `$obj`, `$mehandler`。

| 方法 | 说明 |
|------|------|
| get($key) | 获取 |
| set($key, $value, $ttl = 1, $flag = null) | 设置 |
| incr($key, $value = 1), decr($key, $value = 1) | 自增/自减 |
| __call($method, $params) | 透传其他 Memcached 方法 |

---

## Gene\Cache\Redis

Redis。含 `$config`, `$obj`, `$rehandler`。

| 方法 | 说明 |
|------|------|
| get(), set() | 获取/设置（参数见扩展实现） |
| __call($method, $params) | 透传 Redis 命令 |

---

*文档由 gene-ide-helper 下 Gene 命名空间及 Gene\Db、Gene\Cache 子命名空间内全部类文件提炼生成。*
