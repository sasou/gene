---
name: gene-framework
description: >-
  基于 Gene PHP C 扩展框架（5.6.x）开发 Web、REST、CLI 或 Swoole 常驻应用。
  在用户编写/修改 Gene 项目的控制器、路由、配置、Service、Model、钩子、缓存、
  多语言、Session，或询问 Gene API、Swoole 连接池、版本化缓存时使用。
---

# Gene 框架开发

Gene 是 **PHP 8.0+ 的 C 扩展框架**（非 Composer 包）。开发时以本仓库 `gene-ide-helper/` 与 `demo/` 为准，**不要臆造不存在的 API**。

## 何时使用本技能

- 新增/修改 `application/` 下业务代码或 `config/` 配置
- 设计路由、认证钩子、统一 JSON 响应
- 配置 DI 组件（db、redis、cache、session 等）
- 实现 Service 层版本化缓存
- Swoole 常驻、连接池、`cleanup()` 生命周期

## 项目结构

| 路径 | 命名空间 | 职责 |
|------|----------|------|
| `application/Controllers/` | `Controllers` | HTTP 页面/API，方法宜短 |
| `application/Api/` | `Api` | 内网 REST（常配 `/rest/:c/:a`） |
| `application/Cli/` | `Cli` | CLI（`/cli/:c/:a`） |
| `application/Services/` | `Services` | 业务逻辑、缓存策略 |
| `application/Models/` | `Models` | 数据访问（SQL） |
| `application/Hooks/` | `Hooks` | 继承 `\Gene\Hook` 的类钩子（推荐） |
| `application/Views/` | — | 视图模板 |
| `application/Language/{Dir}/{Lang}.php` | — | 多语言数组 |
| `application/Ext/` | `Ext` | 第三方 SDK、Session 封装等 |
| `config/router.ini.php` | — | 路由与钩子 |
| `config/config.ini.{env}.php` | — | 组件注入 |

## 分层约定（必须遵守）

```
Controller → Service → Model
```

- **Controller**：`$this->request` 取参 → `$this->validate` 校验 → 调 `XxxService::getInstance()` → `return $this->data()/success()/error()` 或 `display()`
- **Service**：继承 `\Gene\Service`，业务与 `cachedVersion` / `updateVersion`
- **Model**：继承 `\Gene\Model`，仅 `$this->db` 链式 SQL

继承：`\Gene\Controller`、`\Gene\Service`、`\Gene\Model`、`\Gene\Hook`。

## 控制器模板

```php
namespace Controllers\Admin;

use Services\Admin\User as UserService;

class User extends \Gene\Controller
{
    public function save()
    {
        $data = $this->request->post();
        $this->validate->init($data)
            ->name('user_name')->required()->msg('用户名不能为空')
            ->valid() || return $this->error($this->validate->error());

        $result = UserService::getInstance()->save($data);
        return $this->data($result['data'] ?? [], $result['count'] ?? -1, $result['msg'] ?? 'ok');
    }
}
```

- 取参优先 **`$this->request->get/post/request()`**（注入组件）；静态 `Controller::get()` 等价但少用
- 响应：`success()` / `error()` 返回数组；需直接输出时用 `json()` 或路由 `after` 钩子里的 `\Gene\Response::json()`

## 路由（`config/router.ini.php`）

```php
/** @var \Gene\Router $router */
$router->clear()
    ->lang('zh,en')   // 必须在 group/route 之前
    ->get('/admin.html', 'Controllers\Admin\Index@run', 'adminAuth@clearAfter')
    ->group('/:c')
        ->get('/:a.html', 'Controllers\Admin\:c@:a', 'adminAuth@clearAfter')
    ->group()
    ->hook('adminAuth', 'Hooks\AdminAuth@handle')   // 推荐类钩子
    ->hook('after', 'Hooks\AfterHook@handle')
    ->error(404, function () { echo '404'; });
```

| 后缀/钩子 | 含义 |
|-----------|------|
| `@clearAfter` | 输出后清缓冲 |
| `@clearBefore` | 输出前清缓冲 |
| `@clearAll` | 不输出，仅清缓冲 |
| `@` | 仅挂钩子，不额外清缓冲 |
| handler | `"Controllers\Xxx@action"` 或 `"Hooks\Xxx@handle"` |

认证钩子返回 **`false`** 中止请求；未登录可 `$this->redirect()` 或 `\Gene\Response::json(\Gene\Response::error('...'))`。

## 配置注入（`config/config.ini.*.php`）

```php
$config->set('db', [
    'class'    => '\Gene\Db\Mysql',
    'params'   => [[ 'dsn' => '...', 'username' => '...', 'password' => '...' ]],
    'instance' => false,  // FPM：每次新建，避免链式状态污染
]);
$config->set('redis', [
    'class'    => '\Gene\Cache\Redis',
    'params'   => [[ 'host' => '127.0.0.1', 'port' => 6379 ]],
    'instance' => true,
]);
```

常用组件名：`view`、`request`、`response`、`validate`、`session` / `websession` / `adminsession`、`db`、`memcache`、`redis`、`cache`、`memory`、`language`。

控制器/Service/Hook 内通过 **`$this->组件名`** 访问（由 DI 注入）。

## Session

`\Gene\Session` 为**实例方法**；配置 `driver` 指向已注册的 redis/memcache 组件名。

```php
$user = $this->session->get('admin');       // 支持点号路径 user.id
$this->session->set('admin', $data);
$this->session->destroy();
```

## 版本化缓存（Service 层）

读：`$this->cache->cachedVersion([$callable, $method], $args, $version, $ttl);`  
写：`$this->cache->updateVersion($version);`

版本键约定：

- 表级：`'db.表名' => null`
- 字段级：`'db.表名.字段' => $value`
- 批量：`'db.表名.id' => [$id1, $id2]`
- 跨表：同一 `$version` 数组放入多张表相关键

## 入口

**FPM / CLI**

```php
define('APP_ROOT', dirname(__DIR__) . '/application');
define('CONF_DIR', dirname(__DIR__) . '/config');

\Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->load('config.ini.php', CONF_DIR)
    ->setMode(1, 1)
    ->run();                    // FPM：自动读 $_SERVER
// CLI：->run('get', $argv[1] ?? '/');
```

**Swoole**：见 [swoole.md](swoole.md)（连接池、`waitWorkerReady`、`cleanup`、禁止 `PDO::ATTR_PERSISTENT`）。

## 视图

```php
$this->assign('list', $list);
$this->display('admin/user/run', 'admin/parent');  // 子视图 + layout
$this->display('web/page');                       // 无 layout
```

Layout 内嵌子视图：`$this->view->contains()`。

## AI 行为准则

1. **先搜现有代码**再写新逻辑，保持命名与目录风格一致
2. **不编造** Gene 类方法；不确定时查 [reference.md](reference.md)
3. 外部输入必须经 **Validate**；权限接口加钩子或 Service 内校验
4. Swoole 场景必读 [swoole.md](swoole.md)；`workerReady()` 后勿在请求中写 `\Gene\Memory`
5. 改动范围最小化，不重构无关模块

## 延伸阅读

| 文档 | 内容 |
|------|------|
| [reference.md](reference.md) | 全量 API 签名 |
| [swoole.md](swoole.md) | Swoole、Pool、RedisPool、生命周期 |
| [../../AGENTS.md](../../AGENTS.md) | 仓库级 AI 协作约定 |
