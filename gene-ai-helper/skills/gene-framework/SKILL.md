---
name: gene-framework
description: 基于 Gene PHP 扩展框架开发 Web 或常驻应用。在用户进行 Gene 框架开发、编写控制器/路由/配置、使用 Gene 的 Request/View/Controller/Application 或询问 Gene API 时使用。
---

# Gene 框架开发

## 何时使用

- 开发或修改 Gene 项目中的控制器、路由、配置、视图、模型、接口、进程等
- 查询 Gene 某类/某方法的用法、参数或返回值

## Service 层缓存使用规范（字段级 + 版本号）

- **统一用 `Cache\Cache::cachedVersion / updateVersion`**：
  - 读：`$this->cache->cachedVersion([$obj, $fun], $args, $version, $ttl);`
  - 写：`$this->cache->updateVersion($version);`
- **版本键命名约定**：
  - 表级：`'db.table' => null`，例如：`'db.im_user' => null`
  - 字段级：`'db.table.field' => $value`，例如：`'db.im_user.id' => $id`、`'db.im_chat.name' => $name`
  - 跨表：在同一个 `$version` 数组中同时放入多张表的字段键，实现跨表联合失效
- **常见读写模式示例**：
  - 按主键查询单行：
    - 读：`$version = ['db.im_user.id' => $id];`
    - 调用：`$this->cache->cachedVersion(['\Models\Im\User', 'row'], [$id], $version, 0);`
    - 写（新增/编辑/状态变更/删除）：`$this->cache->updateVersion(['db.im_user.id' => $id, 'db.im_user' => null]);`
  - 按业务字段查询（如 `name` 字段）：
    - 读：`$version = ['db.im_chat.name' => $name];`
    - 写：所有会影响该查询结果的地方统一更新：`updateVersion(['db.im_chat.name' => $user, 'db.im_chat' => null]);`
  - 跨表联合查询（如聊天列表依赖用户和群）：
    - 读：`$version = ['db.im_chat.name' => $name, 'db.im_user.id' => $userId, 'db.im_group.id' => $groupId];`
    - 写：涉及用户或群变更时，在各自 Service 中更新对应字段键与表级键。

## 目录与模块

| 目录 | 命名空间 | 说明 |
|------|----------|------|
| application/Controllers/ | Controllers | 不同业务创建子目录，方法保持简短 |
| application/Api/ | Api | REST 接口（/rest/:c/:a） |
| application/Cli/ | Cli | 命令行，路由 /cli/:c/:a |
| application/Services/ | Services | 业务逻辑、缓存封装，不同业务创建子目录 |
| application/Models/ | Models | 数据访问，不同业务创建子目录 |
| application/Language/ | - | 语言包文件：`Language/{Dir}/{Lang}.php` |
| application/Ext/ | Ext | 扩展（第三方 SDK 等） |
| application/Views/ | 视图 | 不同业务创建子目录 |
| config/ | - | router.ini.php、config.ini.{env}.php |

## 快速约定

- 控制器继承 `\Gene\Controller`，通过 `$this->db`、`$this->cache`、`$this->validate` 使用注入组件
- 取参：`Controller::get()/post()/params()`；渲染视图：`assign()` + `display()`
- 入口链式：`Application::getInstance()->autoload(APP_ROOT)->load("router.ini.php")->load("config.ini.php")->setMode(1,1)->setRuntimeType('fpm')->run()`
- 配置中通过 `'class' => '\Gene\Xxx'` 注册组件，控制器/Service 内用 `$this->xxx` 访问
- `run()` 无参时自动从 $_SERVER 读取方法和 URI；CLI 场景用 `->run('get', $path)`

## 路由与钩子（router.ini.php）

- **语言**：`->lang('zh,en,ru')`（须在 group/route 之前配置）
- **分组**：`->group("/admin/")`、`->group("/rest/:c")`，后接 `->get/post(...)`，`->group()` 无参结束分组
- **钩子名**：`adminAuth`（后台登录与权限）、`webAuth`（前台登录）、`affAuth`（推广员）、`restAuth`（REST 内网或授权）
- **输出控制**：`@clearAfter`（输出后清空缓冲）、`@clearBefore`（输出前清空缓冲）、`@clearAll`（不输出，仅清空）
- **通配**：Admin 用 `Controllers\Admin\:c@run` 或 `Controllers\Admin\:c@:a`
- **after 钩子**：统一 `\Gene\Response::json($params, $callback)` 输出 JSON
- **错误路由**：`->error(404, function() { ... })`，可用 `\Gene\Router::runError('404')` 手动触发

## 配置注入（config.ini.*.php）

常用组件名：`view`、`request`、`response`、`validate`、`websession`、`adminsession`、`db`、`memcache`、`redis`、`cache`、`memory`、`language`、`rest`。控制器/Service 内用 `$this->组件名` 访问。

### Session 组件（websession / adminsession）

`\Gene\Session` 构造时需传配置数组（含 `driver` 等）。推荐使用自定义 `\Ext\Session` 封装。

```php
$config->set('websession', [
    'class'  => '\Ext\Session',   // 或 '\Gene\Session'
    'params' => [['driver' => 'redis', 'name' => 'WSESSID', 'ttl' => 86400]],
]);

// 控制器/钩子内使用（实例方法，非静态）
$this->websession->load();              // before 钩子中调用
$user = $this->websession->get('user'); // 支持点号路径
$this->websession->set('user', $data);
$this->websession->destroy();
```

### Memory 组件（进程级共享内存）

```php
$config->set('memory', [
    'class'    => '\Gene\Memory',
    'params'   => [['myapp']],   // 命名空间前缀
    'instance' => true,
]);

// 跨请求缓存，不依赖外部服务
$cached = $this->memory->get('config');
if (!$cached) {
    $cached = /* 数据库查询 */;
    $this->memory->set('config', $cached, 3600); // TTL=3600s
}
```

### Language 组件（多语言）

```php
$config->set('language', [
    'class'    => '\Gene\Language',
    'params'   => ['web', 'en'],  // 默认目录 web、默认语言 en
    'instance' => true,
]);

// 控制器内使用
$this->language->web('zh');             // 切换到 Language/Web/Zh.php
$title = $this->language->login_title;  // 读取语言键值
```

## Session 正确用法（Gene\Session）

`\Gene\Session` 所有方法均为**实例方法**（非静态），使用前须 `load()` 加载数据。

```php
// before 钩子（每请求初始化）
->hook("before", function() {
    $this->websession->load();
    \Gene\Di::set('user', $this->websession->get('user'));
})

// after 钩子（保存并输出）
->hook("after", function($params) {
    $this->websession->save();
    \Gene\Response::json($params);
})
```

## 视图路径约定

- 前台：`web/xxx` + layout `web/layout`
- 后台：`admin/xxx`（如 parent、welcome、login），无第二参数则无 layout
- 嵌套视图：layout 中用 `$this->view->contains()` 嵌入子视图

## 常用 API 速查

| 类 | 方法 | 说明 |
|----|------|------|
| Controller | `get($key, $default)`, `post($key, $default)`, `params($key)` | 获取 GET/POST/路由参数 |
| Controller | `assign($name, $value)`, `display($file, $parent)` | 视图变量与渲染 |
| Controller | `success($msg)`, `error($msg)`, `data($data, $count)`, `json($data)` | 统一响应输出 |
| Controller | `url($path)` | 带当前语言前缀的 URL |
| Controller | `redirect($url, $code)` | 重定向 |
| Application | `run($method = null, $uri = null)` | 启动（无参时自动读取 $_SERVER） |
| Application | `setRuntimeType('fpm'\|'swoole'\|'coroutine')` | 设置运行时类型 |
| Application | `setMode($error, $exception, $exCb, $errCb)` | 设置错误/异常处理 |
| Application | `config($key)`, `params($name)` | 配置项与路由参数 |
| Application | `getModule()`, `getController()`, `getAction()` | 当前模块/控制器/动作 |
| Application | `getEnvironmentName()`, `getRuntimeTypeName()` | 环境与运行时名称 |
| Db\Mysql | `select/count/insert/update/delete($table)` | 构建 SQL（链式） |
| Db\Mysql | `where($where, $fields = null)` | WHERE 条件 |
| Db\Mysql | `limit($start, $count)` | 分页（$start=偏移量，$count=行数） |
| Db\Mysql | `all()`, `row()`, `cell()`, `lastId()`, `affectedRows()` | 执行并获取结果 |
| Db\Mysql | `beginTransaction()`, `commit()`, `rollBack()` | 事务 |
| Cache\Redis | `get($key)`, `set($key, $value, $ttl = null)` | Redis 读写 |
| Cache\Cache | `cached($obj, $args, $ttl)`, `cachedVersion($obj, $args, $ver, $ttl)` | 透明缓存代理 |
| Cache\Cache | `updateVersion($version)` | 使版本号缓存失效 |
| Memory | `get($key)`, `set($key, $value, $ttl = 0)`, `exists($key)`, `clean()` | 进程级内存缓存 |
| Session | `load()`, `save()`, `get($name)`, `set($name, $value)`, `destroy()` | 会话（实例方法） |
| Benchmark | `start()`, `end()`, `time()`, `memory()` | 耗时与内存统计 |
| Validate | `init($data)->name('f')->required()->msg('err')->valid()` | 链式校验 |
| Response | `json($data, $callback)`, `redirect($url)`, `setJsonHeader()` | 响应输出 |
| Di | `set($name, $value)`, `get($name)`, `has($name)` | 依赖注入容器 |

## 详细 API

完整类与方法说明见 [reference.md](reference.md)。
