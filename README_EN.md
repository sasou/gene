<div align="center">
  <img src="images/logo.png" width="150" alt="Gene Framework Logo">
  <h1>Gene Framework</h1>
  <p><strong>A fast, flexible, and production-oriented PHP extension framework</strong></p>
  <p>Core execution paths built in C, with first-class PHP-FPM and Swoole support</p>

[![Version](https://img.shields.io/badge/version-6.2.0-blue.svg)](https://github.com/sasou/php-gene)
[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Website](https://img.shields.io/badge/website-1xm.net-orange.svg)](https://www.1xm.net/)

[中文](README.md) · [English](README_EN.md)
</div>

---

<p align="center">
  <a href="#framework-introduction">Introduction</a> ·
  <a href="#620-highlights">Release Highlights</a> ·
  <a href="#core-capabilities">Capabilities</a> ·
  <a href="#quick-start">Quick Start</a> ·
  <a href="#runtime-modes">Runtime Modes</a> ·
  <a href="#performance-and-capacity">Performance</a>
</p>

> **Gene 6.2.0** — Structured JOINs, UNION queries, complex-result pagination, atomic arithmetic updates, unified request input, HTTP form encoding, and PHP 8.0–8.5 support.

## Framework Introduction

Gene is a Web application framework written in C and distributed as a PHP extension. It implements routing, dependency injection, ORM, caching, HTTP, request context, and common Web primitives in the extension layer while supporting both the traditional PHP-FPM request model and Swoole resident coroutine workers.

**Core advantages:**

- **Extension-level execution paths**: Core components run in C, reducing framework bootstrap, file-loading, and userland dispatch overhead.
- **Two runtime models**: The same application structure can run on PHP-FPM or Swoole, with request-context isolation and explicit cleanup for resident workers.
- **Complete application stack**: Routing, DI, four PDO drivers, ORM, caching, pools, outbound HTTP, sessions, validation, logging, views, and CLI tooling.
- **Safe query construction**: Structured conditions, identifier quoting, and parameter binding across JOIN, WHERE, IN, UNION, and atomic updates.
- **Observable resource control**: Cache and pool statistics, request counters, capacity limits, idle recycling, and diagnostics.

## Architecture

### Lightweight composition

- **Composable components**: Use components independently or combine Application, Router, Controller, Hook, and DI into a complete application.
- **Request isolation**: FPM follows the standard PHP lifecycle; Swoole uses coroutine-local context and request snapshots to prevent cross-request state leaks.
- **Multiple databases**: Mysql, Mssql, Pgsql, and Sqlite share query-building capabilities while preserving driver-specific identifier syntax.
- **Explicit behavior**: Compound queries, Context, internal invocation, and caching features are enabled through explicit APIs to limit hidden side effects.

### Performance design

- **C-level routing and dispatch**: Route lookup and Controller/Hook dispatch stay in the extension layer, reducing userland middleware overhead.
- **In-process caching**: Configuration, routes, and application data can remain in workers with capacity limits, approximate LRU, and TTL controls.
- **Connection reuse**: Swoole database and Redis pools provide capacity limits, wait timeouts, idle recycling, and health checks.
- **Coroutine hot paths**: Request-context reuse, coroutine-ID fast paths, and optional automatic cleanup target resident-worker workloads.
- **Batch and atomic operations**: Batch cache/write APIs, upsert, atomic counters, and single-statement arithmetic updates reduce round trips and race windows.
- **Runtime-aware HTTP**: Outbound HTTP uses curl under FPM/CLI and the coroutine client under Swoole, avoiding blocking curl calls in resident coroutines.

### Stability design

- **Symmetric lifecycles**: Covers extension startup/shutdown, request startup/shutdown, and Swoole request-context initialization, reset, and release.
- **Coroutine isolation**: Context, Request, Response, and runtime state are coroutine-local, with manual and optional automatic cleanup paths.
- **Resource hygiene**: Pools include transaction-leak protection, invalid-connection handling, health checks, and diagnostic statistics.
- **Memory controls**: Resident caches support capacity and expiration policies; context tables provide limits, reclamation scans, and telemetry.
- **Fail-fast boundaries**: Invalid query structures, JSON input, and conflicting HTTP payloads fail before execution.
- **Regression tooling**: The repository includes component tests, audit reproductions, and Windows, macOS, and Linux/Swoole acceptance tools.

## 6.2.0 Highlights

| Area | New in 6.2.0 | Benefit |
|:---|:---|:---|
| **ORM queries** | `joinOn()`, `union()`, `unionAll()`, `paginateResult()` | Safely build JOINs and compound queries, then paginate final result sets correctly |
| **ORM writes** | `increment()`, `decrement()` | Perform atomic arithmetic updates in one SQL statement |
| **HTTP** | `query`, `form`, multipart fields, strict payload validation | Consistent encoding semantics across curl and Swoole backends |
| **Request** | `input()` merging GET → POST → JSON | One input API with a shared per-request JSON parsing cache |
| **Context** | `has()` | Distinguish missing keys from explicit `null` values |
| **Compatibility** | PHP 8.0–8.5, Windows x64/x86, macOS build tooling | Broader runtime and build-platform coverage |

## Core Capabilities

| Area | Capabilities |
|------|--------------|
| **Routing and dispatch** | REST routes, groups, parameter capture, pure matching, Controller forwarding, Hook lifecycle |
| **Dependencies and invocation** | IoC/DI, explicit instantiation, Controller/Service/Hook, in-process `Invoke`, local/remote `Rest` |
| **Database and ORM** | Mysql/Mssql/Pgsql/Sqlite, transactions, pools, structured JOIN, UNION, result pagination, batch writes, upsert, row locks, atomic arithmetic |
| **Cache and concurrency primitives** | Memory, Redis, Memcached, versioned/batch caching, TTL/LRU controls, rate limits, locks, atomic counters |
| **HTTP and I/O** | curl/Swoole outbound HTTP, query/form/json/multipart, unified `Request::input()`, JSON, SSE, file responses |
| **Request context** | Coroutine-local Context, Request snapshot/restore/scope, request cleanup, optional automatic cleanup |
| **Security and sessions** | Input validation, strict Bearer parsing, session backends, session-ID regeneration, HMAC, AES-256-GCM |
| **Engineering and operations** | Log, Monitor, Benchmark, CLI, IDE helper, audit scripts, multi-platform build tools |

## System Requirements

### Required Dependencies
- **PHP 8.0–8.5** - Requires PHP 8.0 or later; verified on PHP 8.1.30, 8.2.33, 8.3.33, 8.4.25, and 8.5.10
- **PDO Extension** - Required for database operations, supports MySQL/PostgreSQL/SQLite etc

### Optional Dependencies

**Cache System**
- **Redis Extension** - Required when using Redis cache: `extension=redis`
- **Memcached Extension** - Required when using Memcached cache: `extension=memcached`

**High Performance Mode**
- **Swoole Extension** - For resident process mode and high-performance HTTP service: `extension=swoole`

**Database Drivers**
- **MySQL PDO Driver** - `extension=pdo_mysql`
- **PostgreSQL PDO Driver** - `extension=pdo_pgsql`
- **SQLite PDO Driver** - `extension=pdo_sqlite`
- **SQL Server PDO Driver** - `extension=pdo_sqlsrv`

## Quick Start

### 1️⃣ Install Framework

```bash
# Compile and install
phpize
./configure --enable-gene=shared
make
make install

# Configure php.ini
extension=gene.so
```

### 2️⃣ Create Application Entry

```php
<?php
// index.php
$app = \Gene\Application::getInstance();
$app
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->run();
```

### 3️⃣ Configure Routing

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

### 4️⃣ Configure Services

```php
<?php
// config.ini.php
$config = new \Gene\Config();
$config->clear();

// Database configuration
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

// Cache configuration
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
        'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
        'persistent' => true,
    ]],
    'instance' => true
]);
```

### 5️⃣ Create Controller

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
        $this->view->title = "Documentation";
        $this->view->display('index', 'common');
    }
}
```

### 6️⃣ Using Hook System

Gene Framework provides a powerful hook system that supports aspect-oriented programming and event-driven development:

```php
<?php
// application/Hooks/AdminAuth.php
namespace Hooks;
class AdminAuth extends \Gene\Hook
{
    public function before()
    {
        // Admin permission verification
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
        // Global before hook: logging, initialization, etc.
        $this->log->info('Request started: ' . $this->request->uri());
    }
}

// application/Hooks/AfterHook.php
namespace Hooks;
class AfterHook extends \Gene\Hook
{
    public function after()
    {
        // Global after hook: cleanup, statistics, etc.
        $this->log->info('Request finished');
    }
}
```

**Hook Configuration (router_hook.ini.php)**:
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

**Hook features**:
- **Extension-level dispatch**: Loads and invokes Hooks through the framework factory, reducing userland dispatch layers
- **Lifecycle**: Supports before, after, and handle Hook types
- **Dependency injection**: Can inject request, response, view, and other services
- **Type constraints**: Uses `gene_hook_ce` instance checks

## Runtime Modes

### PHP-FPM Mode
```php
// Traditional web environment, high stability
// Independent context per request, automatic memory cleanup
```

### Swoole Mode
```php
<?php
// Resident process mode, high performance
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

## Performance and Capacity

Gene optimizes for short framework paths, reuse of in-process state, and fewer external round trips rather than promising fixed QPS independent of hardware, PHP configuration, and application workload:

| Scenario | Mechanisms |
|----------|------------|
| Routing and dispatch | C-level route lookup, optional precompiled cache, direct Controller/Hook dispatch |
| Configuration and cache | Worker-local configuration/route caches, application-cache limits, approximate LRU, TTL, batch APIs |
| Data access | PDO builders, batch writes, upsert, connection reuse, atomic arithmetic updates |
| Swoole resident workers | Coroutine-context reuse, database/Redis pools, coroutine HTTP client |
| Observability | `Gene\Monitor::stats()` aggregates request, cache, context, and pool metrics |

Actual throughput depends on PHP/Swoole versions, extension settings, databases, network latency, route count, and application code. Benchmark the real routes and dependencies in the target environment, and monitor latency percentiles, error rate, RSS, cache hit rate, and pool wait time.

## Stability and Production Operations

### PHP-FPM

- Uses the standard PHP request lifecycle, allowing the engine to reclaim request-scoped resources after each request.
- Fits applications that prioritize process isolation, straightforward deployment, and compatibility with traditional PHP infrastructure.
- Persistent connections and in-process configuration caching can reduce repeated initialization work.

### Swoole

- Isolates request data by coroutine context and provides `Application::cleanup()` plus optional automatic cleanup.
- Database and Redis pools provide capacity, timeout, idle-recycling, transaction-hygiene, and diagnostic controls.
- Resident workers should be monitored for RSS, context count, pool waits/timeouts, cache size, and request errors.
- Run the repository acceptance scripts and long-running soak tests on the target PHP/Swoole combination before production rollout.

## Production Cases

- **Hubei Province Education User Authentication Center**: Login portal for millions of students and education users
- **Shangdong E-commerce Platform**: High-performance e-commerce platform
- **Material Network**: B2B platform for materials industry

## Technical Support

- 📖 [Official Documentation](https://www.1xm.net/)
- 🐛 [Issue Tracker](https://github.com/sasou/php-gene/issues)
- 💬 [Technical Support](mailto:zaipd@qq.com)

---

## Links

- **Official Website**: [https://www.1xm.net/](https://www.1xm.net/)
- **PHP5 Version**: [https://github.com/sasou/php-gene](https://github.com/sasou/php-gene) (Version 2.1.0)
- **Windows Version**: [https://github.com/sasou/php-gene-for-windows](https://github.com/sasou/php-gene-for-windows)

---

<div align="center">

**Gene Framework - Simple Coding, Elegant Life!**

[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Author](https://img.shields.io/badge/author-Sasou-blue.svg)](mailto:zaipd@qq.com)

</div>


<a href="https://info.flagcounter.com/AEYx"><img src="https://s11.flagcounter.com/count2/AEYx/bg_FFFFFF/txt_000000/border_CCCCCC/columns_2/maxflags_10/viewers_0/labels_1/pageviews_1/flags_0/percent_0/" alt="Flag Counter" border="0"></a>