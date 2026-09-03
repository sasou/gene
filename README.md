# Gene 简单编码，优雅生活！

<div align="center">

**A fast, flexible, and production-oriented PHP extension framework**

快速、灵活、面向生产环境的 PHP 扩展框架

[![Version](https://img.shields.io/badge/version-6.2.0-blue.svg)](https://github.com/sasou/php-gene)
[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Website](https://img.shields.io/badge/website-1xm.net-orange.svg)](https://www.1xm.net/)

[中文](README.md) | [English](README_EN.md)

</div>

<img src="images/logo.png" width="175" alt="logo" align="right">

## 中文文档

### 框架简介

Gene 是使用 C 编写、以 PHP 扩展形式运行的 Web 应用框架。框架将路由、依赖注入、ORM、缓存、HTTP、请求上下文和常用 Web 原语放在扩展层实现，同时支持传统 PHP-FPM 请求模型与 Swoole 常驻协程模型。

**核心优势：**

- **扩展级执行路径**：核心组件在 C 层实现，减少框架引导、文件加载和用户态调度开销。
- **双运行模式**：同一套应用结构可运行于 PHP-FPM 或 Swoole，并为常驻进程提供请求上下文隔离与显式清理机制。
- **完整应用能力**：覆盖路由、DI、四种 PDO 驱动、ORM、缓存、连接池、出站 HTTP、会话、验证、日志、视图和 CLI。
- **安全的查询构建**：结构化条件、标识符引用和参数绑定贯穿 JOIN、WHERE、IN、UNION 与原子更新。
- **可观测与可治理**：提供缓存和连接池统计、请求计数、容量上限、空闲回收与诊断能力。

### 架构特点

#### 轻量架构

- **按需组合**：组件可独立使用，也可通过 Application、Router、Controller、Hook 和 DI 组成完整应用。
- **请求隔离**：FPM 使用标准请求生命周期；Swoole 使用协程级上下文和请求快照，避免跨请求数据污染。
- **多数据库支持**：Mysql、Mssql、Pgsql、Sqlite 共享统一构建能力，并保留各驱动的标识符方言。
- **显式行为**：复杂查询、Context、内部调用和缓存能力均通过明确 API 启用，降低隐式副作用。

#### 性能设计

- **C 层路由与分发**：路由查找、控制器和 Hook 调度位于扩展层，减少 PHP 用户态中间层。
- **进程内缓存**：配置、路由和业务缓存可驻留于 worker，并支持容量限制、近似 LRU 与 TTL 治理。
- **连接复用**：Swoole 模式提供数据库和 Redis 连接池，支持容量控制、等待超时、空闲回收和健康检查。
- **协程热路径优化**：请求上下文复用、协程 ID 快速路径及自动 cleanup 选项面向常驻 worker 场景。
- **批量与原子能力**：批量缓存、批量写入、upsert、原子计数及单语句算术更新减少往返和竞争窗口。
- **后端适配**：出站 HTTP 在 FPM/CLI 使用 curl，在 Swoole 使用协程客户端，避免常驻协程中执行阻塞 curl。

#### 稳定性设计

- **生命周期对称**：覆盖扩展启动/关闭、请求启动/关闭以及 Swoole 请求上下文的初始化、重置和释放。
- **协程隔离**：Context、Request、Response 和运行状态按协程维护，并提供手动及可选自动清理路径。
- **资源卫生**：连接池包含事务泄漏防护、失效连接处理、健康检查和诊断统计。
- **内存治理**：常驻缓存提供容量上限和过期策略；上下文表提供上限、回收扫描与遥测。
- **失败边界**：非法查询结构、JSON 输入和互斥 HTTP 载荷在执行前失败，避免发送部分或歧义请求。
- **回归验证**：仓库包含组件测试、审计复现脚本及 Windows、macOS、Linux/Swoole 验收工具。

### 6.2.0 能力亮点

- ORM：`joinOn()`、`union()`、`unionAll()`、`paginateResult()`、`increment()`、`decrement()`。
- HTTP：`query`、`form`、multipart 字段及严格的选项和载荷校验。
- Request：GET → POST → JSON 合并的 `input()` 与请求级 JSON 解析缓存。
- Context：可区分缺失键和显式 `null` 的 `has()`。
- 兼容性：支持 PHP 8.0–8.5，并提供 Windows x64/x86 与 macOS 构建工具。

### 核心能力

| 领域 | 能力 |
|------|------|
| **路由与调度** | REST 路由、分组、参数捕获、纯匹配、Controller 转发、Hook 生命周期 |
| **依赖与调用** | IoC/DI、显式实例化、Controller/Service/Hook、进程内 `Invoke`、本地/远端 `Rest` |
| **数据库与 ORM** | Mysql/Mssql/Pgsql/Sqlite、事务、连接池、结构化 JOIN、UNION、复杂分页、批量写、upsert、行锁、原子增减 |
| **缓存与并发原语** | Memory、Redis、Memcached、版本缓存、批量缓存、TTL/LRU 治理、限流、锁、原子计数 |
| **HTTP 与输入输出** | curl/Swoole 出站 HTTP、query/form/json/multipart、统一 `Request::input()`、JSON、SSE、文件响应 |
| **请求上下文** | 协程级 Context、Request snapshot/restore/scope、请求清理与可选自动 cleanup |
| **安全与会话** | 输入验证、严格 Bearer 解析、Session 多后端、Session ID 再生成、HMAC 与 AES-256-GCM |
| **工程与运维** | Log、Monitor、Benchmark、CLI、IDE helper、审计脚本及多平台构建工具 |

### 系统要求

#### 必需依赖
- **PHP 8.0–8.5** - 最低要求 PHP 8.0；已验证 PHP 8.1.30、8.2.33、8.3.33、8.4.25、8.5.10
- **PDO扩展** - 数据库操作必需，支持MySQL/PostgreSQL/SQLite等

#### 可选依赖

**缓存系统**
- **Redis扩展** - 使用Redis缓存时必需：`extension=redis`
- **Memcached扩展** - 使用Memcached缓存时必需：`extension=memcached`

**高性能模式**
- **Swoole扩展** - 常驻进程模式和高性能HTTP服务：`extension=swoole`

- **MySQL PDO驱动** - `extension=pdo_mysql`
- **PostgreSQL PDO驱动** - `extension=pdo_pgsql`
- **SQLite PDO驱动** - `extension=pdo_sqlite`
- **SQL Server PDO驱动** - `extension=pdo_sqlsrv`

### 快速开始

#### 1️⃣ 安装框架

```bash
# 编译安装
phpize
./configure --enable-gene=shared
make
make install

# 配置php.ini
extension=gene.so
```

> 完整 INI 配置项说明与 FPM / Swoole 生产推荐配置见 [配置参考文档](docs/CONFIGURATION.md)。

#### 2️⃣ 创建应用入口

```php
<?php
// index.php
$app = \Gene\Application::getInstance();
$app
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->run();
```

#### 3️⃣ 配置路由

```php
<?php
// router.ini.php
$router = new \Gene\Router();
$router->clear()
    ->get("/", "\Controllers\Index@run")
    ->get("/test", "\Controllers\Index@test", "@clearAll")
    ->post("/", function() {
        echo "index post";
    })
    ->group("/admin")
        ->get("/:name/", function($params) {
            var_dump($params);
        })
    ->group()
    ->error(404, function() {
        echo "404 Not Found";
    });
```

#### 4️⃣ 配置服务

```php
<?php
// config.ini.php
$config = new \Gene\Config();
$config->clear();

// 数据库配置
$config->set("db", [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
        'dsn' => 'mysql:dbname=gene_web;host=127.0.0.1;port=3306;charset=utf8',
        'username' => 'root',
        'password' => '',
        'options' => [PDO::ATTR_PERSISTENT => true]
    ]],
    'instance' => true
]);

// 缓存配置
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
        'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
        'persistent' => true,
    ]],
    'instance' => true
]);
```

#### 5️⃣ 创建控制器

```php
<?php
// Controllers/Index.php
namespace Controllers;
class Index extends \Gene\Controller
{
    public function run()
    {
        echo 'Hello World!';
    }
    
    public function test()
    {
        $this->view->title = "文档";
        $this->view->display('index', 'common');
    }
}
```

#### 6️⃣ 使用钩子系统

Gene框架提供了强大的钩子系统，支持面向切面编程和事件驱动开发：

```php
<?php
// application/Hooks/AdminAuth.php
namespace Hooks;
class AdminAuth extends \Gene\Hook
{
    public function before()
    {
        // 管理员权限验证
        if (!$this->checkAdminAuth()) {
            $this->redirect('/login');
        }
    }
    
    private function checkAdminAuth()
    {
        $token = $this->cookie->get('admin_token');
        return $token && $this->validateToken($token);
    }
}

// application/Hooks/BeforeHook.php
namespace Hooks;
class BeforeHook extends \Gene\Hook
{
    public function before()
    {
        // 全局前置钩子：日志记录、初始化等
        $this->log->info('Request started: ' . $this->request->uri());
    }
}

// application/Hooks/AfterHook.php
namespace Hooks;
class AfterHook extends \Gene\Hook
{
    public function after()
    {
        // 全局后置钩子：清理、统计等
        $this->log->info('Request finished');
    }
}
```

**钩子配置 (router_hook.ini.php)：**
```php
<?php
$router = new \Gene\Router();
$router->clear()
    ->get("/", "\Controllers\Index@run", "@BeforeHook,AdminAuth")
    ->post("/api/data", "\Controllers\Api@data", "@AdminAuth")
    ->group("/admin")
        ->get("/*", "\Controllers\Admin@dashboard", "@AdminAuth")
    ->group();
```

**钩子特性：**
- **扩展层分发**：通过框架工厂加载并调用 Hook，减少用户态调度层级
- **生命周期**：支持 before、after、handle 三种钩子类型
- **依赖注入**：可注入 request、response、view 等服务
- **类型约束**：基于 `gene_hook_ce` 进行实例类型检查

### 运行模式

#### PHP-FPM 模式
```php
// 传统Web环境，高稳定性
// 每个请求独立上下文，自动内存清理
```

#### Swoole 模式
```php
<?php
// 常驻进程模式，高性能
\Gene\Application::setRuntimeType('swoole');

$http = new swoole_http_server("0.0.0.0", 9501);
$http->on("request", function ($request, $response) {
    \Gene\Request::init($request->get, $request->post, $request->cookie, $request->server, null, $request->files, null, $request->header);
    \Gene\Application::setResponse($response);

    ob_start();
    $error = false;
    try {
        \Gene\Application::getInstance()->run();
    } catch (\Throwable $e) {
        $error = true;
        \Gene\Log::exception($e);
    } finally {
        $out = ob_get_clean();
        \Gene\Application::cleanup();
    }

    if ($error) {
        $response->redirect('/50x.html');
        return;
    }

    if (!$response->isWritable()) {
        return;
    }
    $response->end($out);
});
$http->start();
```

### 性能与容量

Gene 的性能策略是缩短框架路径、复用进程内状态并减少外部往返，而不是承诺脱离硬件、PHP 配置和业务负载的固定 QPS：

| 场景 | 机制 |
|------|------|
| 路由与调度 | C 层路由查找、预编译缓存选项、Controller/Hook 直接分发 |
| 配置与缓存 | worker 内配置/路由缓存、业务缓存容量上限、近似 LRU、TTL 与批量 API |
| 数据访问 | PDO 构建器、批量写、upsert、连接复用、原子算术更新 |
| Swoole 常驻模式 | 协程上下文复用、数据库/Redis 连接池、协程 HTTP 客户端 |
| 可观测性 | `Gene\Monitor::stats()` 汇总请求、缓存、上下文及连接池指标 |

实际吞吐取决于 PHP/Swoole 版本、扩展配置、数据库、网络、路由规模和业务代码。建议在目标部署环境使用真实路由和依赖进行压测，并同时观察延迟分位数、错误率、RSS、缓存命中率和连接池等待情况。

### 稳定性与生产运行

#### PHP-FPM

- 使用 PHP 标准请求生命周期，请求结束后由引擎统一回收请求级资源。
- 适合需要进程隔离、部署简单以及兼容传统 PHP 基础设施的应用。
- 可使用持久连接和进程内配置缓存降低重复初始化开销。

#### Swoole

- 请求数据按协程上下文隔离，并提供 `Application::cleanup()` 和可选自动 cleanup。
- 数据库与 Redis 连接池提供容量、超时、空闲回收、事务卫生和诊断能力。
- 常驻 worker 应持续监控 RSS、上下文数量、连接池等待/超时、缓存规模与请求错误数。
- 上线前应在目标 PHP/Swoole 组合上执行仓库验收脚本和长期 soak 测试。

### 生产案例

- **湖北省教育用户认证中心**：全省几百万学生、教育用户的登录入口
- **尚动电子商务平台**：高性能电商平台
- **生材网**：材料行业B2B平台

### 技术支持

- 📖 [官方文档](https://www.1xm.net/)
- 🐛 [问题反馈](https://github.com/sasou/php-gene/issues)
- 💬 [技术交流](mailto:zaipd@qq.com)
    
---

## Links

- **Official Website**: [https://www.1xm.net/](https://www.1xm.net/)
- **PHP5 Version**: [https://github.com/sasou/php-gene](https://github.com/sasou/php-gene) (Version 2.1.0)
- **Windows Version**: [https://github.com/sasou/php-gene-for-windows](https://github.com/sasou/php-gene-for-windows)

---

<div align="center">

**Gene Framework - 简单编码，优雅生活！**

[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Author](https://img.shields.io/badge/author-Sasou-blue.svg)](mailto:zaipd@qq.com)

</div>


<a href="https://info.flagcounter.com/AEYx"><img src="https://s11.flagcounter.com/count2/AEYx/bg_FFFFFF/txt_000000/border_CCCCCC/columns_2/maxflags_10/viewers_0/labels_1/pageviews_1/flags_0/percent_0/" alt="Flag Counter" border="0"></a>