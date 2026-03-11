## Gene 框架 AI 协作说明

本文件给 AI 助手一个**精简且可操作的约定集合**，用于在本项目中基于 Gene PHP 扩展框架安全、高效地生成代码。  
如果框架 API 有疑问，应优先**沿用现有代码写法**，而不是发明新的用法。

---

## 1. 整体架构与职责边界

- **目录与命名空间（只记这些）**
  - `application/Controllers/` → `Controllers`：HTTP 控制器，处理 Web/API 请求
  - `application/Api/` → `Api`：内部 REST 接口（`/rest/:c/:a`）
  - `application/Cli/` → `Cli`：命令行（`/cli/:c/:a`）
  - `application/Services/` → `Services`：业务逻辑
  - `application/Models/` → `Models`：数据访问
  - `application/Views/`：视图模板
  - `application/Ext/` → `Ext`：第三方 SDK/扩展
  - `application/Language/`：多语言（`Language/{Dir}/{Lang}.php`）
  - `config/`：`router.ini.php` 路由、`config.ini.{env}.php` 组件/配置

- **继承关系（务必遵守）**
  - 控制器：继承 `\Gene\Controller`
  - Service：继承 `\Gene\Service`，通过 `XxxService::getInstance()` 使用
  - Model：继承 `\Gene\Model`

- **三层职责**
  - Controller：取参、校验、调用 Service、统一响应/视图
  - Service：业务逻辑、缓存策略、调用 Model
  - Model：SQL/表操作，不写业务逻辑

---

## 2. 控制器开发（HTTP / REST 的标准写法）

### 2.1 典型控制器方法模板

**AI 生成控制器时，应优先使用如下结构：**

```php
use Services\XxxService;

class DemoController extends \Gene\Controller
{
    public function action()
    {
        // 1. 取参
        $data = $this->request->post();

        // 2. 校验（推荐使用 Validate）
        $this->validate->init($data)
            ->name('field')->required()
            ->msg('字段不能为空')
            ->valid() || return $this->error($this->validate->error());

        // 3. 调用 Service
        $result = XxxService::getInstance()->method($data);

        // 4. 统一响应（返回数组或 JSON）
        return $this->data($result['data'] ?? [], $result['count'] ?? -1, $result['msg'] ?? 'ok');
    }
}
```

### 2.2 取参与响应（只用以下套路）

- **取参**

```php
// 单字段
$value = $this->request->post('key', 'default');
$value = $this->request->get('key', 'default');
$value = $this->request->request('key', 'default'); // GET+POST 统一入口

// 全部参数
$post = $this->request->post();
$get  = $this->request->get();
```

- **统一响应**

```php
return $this->success('操作成功');              // 仅消息
return $this->error('错误消息');                // 错误
return $this->data($data, $count);             // 列表/分页
return $this->json($data);                     // 自定义 JSON
```

### 2.3 视图渲染（有页面时）

```php
// 传变量
$this->assign('name', $value);

// 使用 layout
$this->display('admin/list', 'admin/parent');

// 无 layout
$this->display('web/page');
```

**约定**：  
AI 在已有页面风格内新增页面时，优先**复用现有 layout 与目录结构**（如 `admin/*`、`web/*`）。

---

## 3. 路由与钩子

### 3.1 基础路由（`config/router.ini.php`）

```php
/** @var \Gene\Router $router */

// 简单路由
$router->get('/path', "Controllers\Xxx@action");
$router->post('/path', "Controllers\Xxx@action");

// 带钩子（如输出控制）
$router->get('/path', "Controllers\Xxx@action", "after@clearAfter");
```

**注意**：handler 统一使用 `"命名空间\\类@方法"` 写法。

### 3.2 分组与多语言

```php
// 多语言必须先注册
$router->lang('zh,en,ru');

// 分组
$router->group("/admin/")
    ->get("list",  "Controllers\Admin\User@list")
    ->post("save", "Controllers\Admin\User@save")
    ->group(); // 结束
```

**AI 约定**：新增路由时，优先按现有分组（如 `/admin/`、`/api/`）放置，避免散落。

### 3.3 钩子与输出控制

```php
// 示例：认证钩子
$router->hook("adminAuth", function () {
    if (!$this->websession->get('user')) {
        \Gene\Response::json(\Gene\Response::error('请先登录'));
        return false;
    }
});
```

- 常用输出控制后缀：
  - `@clearAfter`：输出后清缓冲
  - `@clearBefore`：输出前清缓冲
  - `@clearAll`：仅清空缓冲

---

## 4. 配置与组件注入（`config/config.ini.{env}.php`）

### 4.1 组件注册模板

```php
// 数据库（每次新建实例）
$config->set('db', [
    'class'    => '\Gene\Db\Mysql',
    'params'   => [$dbConfig],
    'instance' => false,
]);

// Redis（单例）
$config->set('redis', [
    'class'    => '\Gene\Cache\Redis',
    'params'   => [$redisConfig],
    'instance' => true,
]);
```

### 4.2 常用组件名（可通过 `$this->xxx` 使用）

- 基础：`view`、`request`、`response`、`validate`
- 会话：`websession`、`adminsession`
- 存储：`db`、`memcache`、`redis`、`cache`
- 其他：`memory`（进程缓存）、`language`（多语言）

### 4.3 Session / Memory / Language 示例

```php
// Session
$config->set('websession', [
    'class'  => '\Gene\Session',
    'params' => [[
        'driver' => 'redis',
        'name'   => 'WSESSID',
        'ttl'    => 86400,
    ]],
]);

// 进程级缓存
$config->set('memory', [
    'class'    => '\Gene\Memory',
    'params'   => [['myapp']],
    'instance' => true,
]);

// 多语言
$config->set('language', [
    'class'    => '\Gene\Language',
    'params'   => ['web', 'en'],
    'instance' => true,
]);
```

---

## 5. Service / Model 与缓存规范

### 5.1 Service 基本写法

```php
namespace Services;

class XxxService extends \Gene\Service
{

    public function method(array $params)
    {
        // 业务逻辑，调用 Model
        // return $this->success(...) / $this->data(...);
    }
}
```

**AI 约定**：  
Service 中多返回 `['code', 'msg', 'data', 'count]` 结构，可通过基类的 `success/error/data` 构造。

### 5.2 Model 基本写法

```php
namespace Models;

class XxxModel extends \Gene\Model
{
    public function getList(array $where, int $start = 0, int $count = 20)
    {
        return $this->db->select('table_name')
            ->where($where)
            ->order('id DESC')
            ->limit($start, $count)
            ->all();
    }

    public function getRow(int $id)
    {
        return $this->db->select('table_name')
            ->where(['id' => $id])
            ->row();
    }
}
```

### 5.3 版本化缓存（读用 `cachedVersion`，写用 `updateVersion`）

**命名约定：**
- 表级：`'db.im_user' => null`
- 字段级：`'db.im_user.id' => $id`
- 多值字段：`'db.im_user.id' => [$id1, $id2, $id3]`

**读：**

```php
public function getUserById($id)
{
    $version = ['db.im_user.id' => $id];
    return $this->cache->cachedVersion(
        ['\Models\User', 'row'],
        [$id],
        $version,
        3600
    );
}

// 批量查询示例
public function getUsersByIds(array $ids)
{
    $version = ['db.im_user.id' => $ids];
    return $this->cache->cachedVersion(
        ['\Models\User', 'batch'],
        [$ids],
        $version,
        3600
    );
}
```

**写：**

```php
public function updateUser($id, array $data)
{
    $result =\Models\User::getInstance()->update($id, $data);

    $this->cache->updateVersion([
        'db.im_user.id' => $id,
        'db.im_user'    => null,
    ]);

    return $result;
}

// 批量更新示例
public function updateUsers(array $ids, array $data)
{
    $result =\Models\User::getInstance()->update($ids, $data);;

    $this->cache->updateVersion([
        'db.im_user.id' => $ids,  // 多值批量更新
        'db.im_user'    => null,
    ]);

    return $result;
}
```

**AI 约定：**  
- 涉及读写分离和列表/详情缓存时，**务必维护对应版本键**，避免脏读。
- 批量操作时，优先使用多值数组形式 `['field' => [$val1, $val2]]` 进行批量版本更新，提高效率。

---

## 6. 应用入口模式（只保留典型写法）

### 6.1 FPM Web

```php
\Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php')
    ->load('config.ini.php')
    ->setMode(1, 1)
    ->run(); // 从 $_SERVER 读取请求
```

### 6.2 Swoole / 常驻进程

```php
\Gene\Request::init(
    $request->get, $request->post, $request->cookie,
    $request->server, null, $request->files
);

\Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php')
    ->load('config.ini.php')
    ->setRuntimeType(2)
    ->run($type, $url);
```

### 6.3 CLI

```php
\Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php')
    ->load('config.ini.php')
    ->run('get', $path); // $path 来自 argv
```

---

## 7. 关键辅助类的使用原则（去掉细节 API）

**AI 不需要记住全部方法，只需按下述原则调用：**

- **`\Gene\Request`**：在 Service / 常驻进程中获取请求上下文，使用静态方法 `get/post/request/cookie/files/server/params/isAjax` 等。
- **`\Gene\Response`**：用于重定向与直接输出 JSON；若在控制器中已有 `$this->success/error/data/json`，优先使用控制器方法。
- **`\Gene\Session`**：通过配置注入为 `$this->websession` / `$this->adminsession`，用于登录态与简单会话。
- **`\Gene\Memory`**：同一 Worker 进程内的短期缓存，适合配合外部缓存作热点数据加速。
- **`\Gene\Validate`**：统一数据校验入口，推荐在控制器中使用链式写法。
- **数据库 (`\Gene\Db\Mysql` 等)**：只在 Model 或极少数底层 Service 中直接操作；使用链式 `select/where/order/limit/insert/update/delete` 即可。
- **缓存装饰器 (`\Gene\Cache\Cache`)**：需要函数式缓存时优先用 `cached`/`cachedVersion`，避免自行拼接 key。

如需更完整方法列表，可参考框架官方文档或现有代码示例，**本文件不再重复罗列**。

---

## 8. 开发最佳实践（AI 必须遵守）

- **代码组织**
  - 控制器方法尽量短小，以“取参 → 校验 → 调 Service → 返回”为核心流程。
  - 业务逻辑集中在 Service；SQL/数据访问集中在 Model。
  - 新业务按模块划分子目录（如 `Controllers\Admin`、`Services\Chat`）。

- **错误与异常**
  - 业务错误用 `success/error/data` 统一结构，不在 Controller 中抛底层异常。
  - Service 负责业务层异常处理；Controller 负责 HTTP 维度处理（如 404/权限）。

- **缓存与性能**
  - 优先使用版本化缓存（`cachedVersion` + `updateVersion`）, 需要更高性能的场景可以控制器增加一级进程缓存。
  - 避免循环中频繁 SQL；能批量就批量。
  - 合理使用 Memory（进程级缓存）和外部缓存（Redis/Memcache）。

- **安全**
  - 所有外部输入（请求参数、Cookie、Header）**必须**经过 Validate。
  - 数据库操作使用参数化查询（Gene 链式调用已默认支持）。
  - 涉及权限的接口，务必加认证钩子或在 Controller/Service 内校验会话。

---

## 9. 调试与环境

- **性能调试**

```php
\Gene\Benchmark::start();
// ... 代码 ...
\Gene\Benchmark::end();
echo \Gene\Benchmark::time();
echo \Gene\Benchmark::memory();
```

- **环境区分**
  - 开发：`config.ini.dev.php`
  - 测试：`config.ini.test.php`
  - 生产：`config.ini.prod.php`

**结论**：  
AI 在本项目中生成代码时，应**优先遵循本文件的约定和现有代码风格**，并在不了解细节 API 时，参考已有实现而不是随意扩展框架能力。

