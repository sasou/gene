---
name: gene-framework
description: >-
  基于 Gene PHP C 扩展框架（6.1.x）开发 Web、REST、CLI 或 Swoole 常驻应用。
  在用户编写/修改 Gene 项目的控制器、路由、配置、Service、Model、钩子、缓存、
  多语言、Session，或询问 Gene API、Swoole 连接池、版本化缓存、ORM v2 时使用。
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
- **Model**：数据模型继承 `\Gene\Orm\Model` 使用 ActiveRecord；需要手写 SQL 时继承 `\Gene\Model` 并使用 `$this->db` 链式调用

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

- 取参优先 **`$this->request->get/post/request()`**（注入组件）；JSON body 用 **`$this->request->json()`**，禁止直接读 `php://input`
- 响应：`success()` / `error()` 返回数组；需直接输出时用 `json()` 或路由 `after` 钩子里的 `\Gene\Response::json()`。JSON API **禁止** `echo` + `exit`
- 默认使用 `\Gene\Log`（自动带 `request_id`）、`Validate`、`Monitor`、`Memory`（单 worker）/ `Redis::rateLimit`（多 worker）

出站 HTTP 用 `\Gene\Http::request()`（FPM=curl，Swoole=协程客户端），不要裸 `curl_exec`。请求级 KV 用 `\Gene\Context`，不要用静态变量。HMAC/随机 ID/AES-GCM 用 `\Gene\Crypto`。

推荐钩子（零 C，见 demo）：

```php
->hook('cors', 'Hooks\Cors@handle')           // OPTIONS 短路；Origin 白名单，禁止反射
->hook('requestId', 'Hooks\RequestId@handle') // Context + X-Request-Id
->hook('adminAuth', 'Hooks\AdminAuth@handle')
```

## 路由（`config/router.ini.php`）

```php
/** @var \Gene\Router $router */
$router->clear()
    ->lang('zh,en')   // 必须在 group/route 之前
    ->get('/admin.html', 'Controllers\Admin\Index@run', 'adminAuth@clearAfter')
    ->group('/:c')
        ->get('/:a.html', 'Controllers\Admin\:c@:a', 'adminAuth@clearAfter')
    ->group()
    ->hook('cors', 'Hooks\Cors@handle')
    ->hook('requestId', 'Hooks\RequestId@handle')
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
    'instance' => true,   // 请求内按类名单例；FPM/Swoole 均可
]);
$config->set('redis', [
    'class'    => '\Gene\Cache\Redis',
    'params'   => [[ 'host' => '127.0.0.1', 'port' => 6379 ]],
    'instance' => true,
]);
```

`instance` 语义（两者均为**请求级**，请求结束随 `di_regs` 销毁，不跨请求复用）：

- `false` — 请求内按 name 单例：同 name 复用，同 class 不同 name 各自新建
- `true` — 请求内按类名单例：同 class 不同 name 共享实例

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
写：`$this->cache->updateVersion($version);` **在事务提交之后**调用（事务中 bump 版本，rollBack 后缓存已失效）。

多表写入用回调事务（PDO 不支持嵌套 begin；内层只跑回调）：

```php
$id = $this->db->transaction(function () use ($data) {
    $id = User::create($data);
    Role::create(['user_id' => $id]);
    return $id;
});
$this->cache->updateVersion(['db.sys_user' => null, 'db.sys_role' => null]);
```

同一连接上也可 `User::transaction($fn)`（走该模型 `$connection`）。

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
