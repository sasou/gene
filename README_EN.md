<div align="center">
  <img src="images/logo.png" width="160" alt="Gene Framework Logo">
  <h1>Gene Framework</h1>
  <p><strong>A fast, flexible, and production-oriented high-performance PHP C extension framework</strong></p>
  <p>⚡ Pure C core execution paths · 🚀 Native PHP-FPM and Swoole resident coroutine support</p>

[![PHP](https://img.shields.io/badge/PHP-8.0~8.5-777BB4?style=flat-square&logo=php&logoColor=white)](https://www.php.net/)
[![Language](https://img.shields.io/badge/Language-C-00599C?style=flat-square&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Release](https://img.shields.io/badge/Release-v6.2.0-blue?style=flat-square&logo=github)](https://github.com/sasou/php-gene/releases)
[![Swoole](https://img.shields.io/badge/Swoole-Supported-brightgreen?style=flat-square&logo=swoole&logoColor=white)](https://www.swoole.com/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20|%20macOS%20|%20Windows-lightgrey?style=flat-square&logo=linux&logoColor=white)](https://github.com/sasou/php-gene)
[![License](https://img.shields.io/badge/License-PHP%203.01-green.svg?style=flat-square)](http://www.php.net/license/3_01.txt)
[![Website](https://img.shields.io/badge/Website-1xm.net-orange.svg?style=flat-square)](https://www.1xm.net/)

[🇨🇳 简体中文](README.md) &nbsp;·&nbsp; [🇬🇧 English](README_EN.md)
</div>

---

<p align="center">
  <a href="#framework-introduction">📖 Introduction</a> &nbsp;|&nbsp;
  <a href="#architecture">🏛️ Architecture</a> &nbsp;|&nbsp;
  <a href="#620-highlights">✨ Highlights</a> &nbsp;|&nbsp;
  <a href="#core-capabilities">🧩 Capabilities</a> &nbsp;|&nbsp;
  <a href="#system-requirements">💻 Requirements</a> &nbsp;|&nbsp;
  <a href="#quick-start">🚀 Quick Start</a> &nbsp;|&nbsp;
  <a href="#runtime-modes">🔄 Runtime Modes</a> &nbsp;|&nbsp;
  <a href="#performance-and-capacity">📊 Performance</a>
</p>

> 💡 **Gene 6.2.0** — Structured JOINs, UNION queries, complex-result pagination, atomic arithmetic updates, unified request input, HTTP form encoding, and PHP 8.0–8.5 support.

---

<a id="framework-introduction"></a>
## 📖 Framework Introduction

Gene is a Web application framework written in C and distributed as a high-performance PHP extension. It implements routing lookup, dependency injection, ORM, caching management, HTTP client, request context, and common Web primitives directly at the extension layer while natively supporting both the traditional PHP-FPM request model and Swoole resident coroutine workers.

**🌟 Core Advantages:**

- ⚡ **Extension-Level Execution Paths**: Core components run in pure C, substantially reducing framework bootstrap, file-loading, and userland dispatch overhead.
- 🔄 **Native Dual-Runtime Models**: The same application codebase seamlessly runs on PHP-FPM or Swoole, providing complete coroutine-local context isolation and explicit cleanup for resident workers.
- 🧰 **Complete Out-of-the-Box Stack**: Built-in routing, IoC/DI container, four mainstream PDO drivers, advanced ORM, caching abstractions, connection pools, outbound HTTP client, sessions, validation, logging, view engine, and CLI tooling.
- 🛡️ **Safe & Structured Query Construction**: Structured conditions, automatic identifier quoting, and strict parameter binding are applied throughout JOIN, WHERE, IN, UNION, and atomic updates.
- 📈 **Deep Observability & Governance**: Worker-local cache and pool statistics, request counters, capacity watermarks, idle connection recycling, and automated diagnostics.

---

<a id="architecture"></a>
## 🏛️ Architecture

### 🪶 Lightweight Composition

- 🧩 **Composable Components**: Use components independently or combine Application, Router, Controller, Hook, and DI into a complete enterprise-grade application.
- 🔒 **Strict Request Isolation**: FPM adheres to the standard request lifecycle; Swoole leverages coroutine-local context and request snapshots to eliminate cross-request data leaks.
- 🗄️ **Multi-Database Support**: MySQL, SQL Server, PostgreSQL, and SQLite share unified query-building APIs while preserving driver-specific identifier syntax and dialect features.
- 🎯 **Explicit Behavior & Zero Magic**: Compound queries, Context injection, internal dispatch, and caching operations utilize clear, explicit APIs to minimize hidden side effects.

### ⚡ Performance Design

- ⚡ **C-Level Routing & Fast Dispatch**: Route tree lookups, Controller invocations, and Hook dispatches stay inside the C extension layer, bypassing userland middleware overhead.
- 💾 **High-Performance In-Process Cache**: Global configurations, compiled routes, and hot business data can reside in worker memory with capacity limits, approximate LRU eviction, and TTL governance.
- 🏊 **Automated Connection Pooling**: Swoole mode features built-in database and Redis connection pools with capacity autoscaling, wait timeout protection, idle connection recycling, and health checks.
- 🚀 **Coroutine Hot-Path Optimization**: Context pooling/reuse, coroutine-ID fast paths, and optional automatic cleanup are tailored for high-concurrency microservices and resident workers.
- 📦 **Batch & Atomic Operations**: Batch cache/write APIs, upsert (conflict updates), atomic counters, and single-statement arithmetic updates reduce round trips and race windows.
- 🌐 **Adaptive Outbound HTTP**: Uses fast libcurl under FPM/CLI and automatically switches to non-blocking coroutine clients under Swoole, preventing blocking curl operations in coroutines.

### 🛡️ Stability Design

- 🔄 **Symmetric Lifecycles**: Strict lifecycle management across extension initialization/shutdown (MINIT/MSHUTDOWN), request startup/shutdown (RINIT/RSHUTDOWN), and Swoole context lifecycles.
- 🧬 **Coroutine-Level Context Boundaries**: Context, Request, Response, and runtime state are strictly scoped to the active coroutine, offering both explicit and automatic cleanup paths.
- 🧼 **Resource Hygiene Safeguards**: Connection pools include automatic transaction rollback on leak, stale connection pruning, proactive health checks, and diagnostic statistics.
- 📊 **Fine-Grained Memory Governance**: In-memory caches enforce hard memory thresholds and proactive TTL purging; context tables provide overflow limits, sweep recycling, and telemetry.
- 🚫 **Fail-Fast Error Boundaries**: Malformed queries, invalid JSON inputs, and conflicting HTTP payloads fail fast prior to execution, preventing ambiguous or partial requests.
- 🧪 **Multi-Platform Regression Matrix**: Built-in unit test suite, audit reproduction scripts, and automated acceptance toolchains for Windows, macOS, and Linux/Swoole.

---

<a id="620-highlights"></a>
## ✨ 6.2.0 Highlights

| Area | New in 6.2.0 | Core Benefit |
|:---|:---|:---|
| 🗄️ **ORM Queries** | `joinOn()`, `union()`, `unionAll()`, `paginateResult()` | Safely build complex JOINs and UNION queries, and paginate final composite result sets |
| ✍️ **ORM Writes** | `increment()`, `decrement()` | Perform atomic row-level arithmetic updates in a single SQL statement, eliminating race windows |
| 🌐 **HTTP Client** | `query`, `form`, multipart fields, strict payload validation | Consistent and validated encoding semantics across curl and Swoole coroutine backends |
| 📥 **Request Processing** | Unified `Request::input()` merging GET → POST → JSON | Single entry point for input parameters with a shared per-request JSON parsing cache |
| 🧬 **Context** | `Context::has()` | Strictly distinguish missing keys from explicitly assigned `null` values |
| 💻 **Platform Support** | PHP 8.0–8.5, Windows x64/x86, macOS build tooling | Comprehensive support across modern development environments, CI/CD, and production platforms |

---

<a id="core-capabilities"></a>
## 🧩 Core Capabilities

| Domain | Capability Matrix |
|:---|:---|
| 🌐 **Routing & Dispatch** | RESTful routing, route groups, dynamic URI parameters, regex matching, Controller forwarding, Hook lifecycle |
| 💉 **Dependencies & Calls** | IoC/DI container, explicit service instantiation, Controller/Service/Hook injection, fast in-process `Invoke`, local/remote `Rest` |
| 🗄️ **Database & ORM** | MySQL / SQL Server / PostgreSQL / SQLite full coverage, transactions, connection pools, structured JOIN, UNION, complex pagination, batch inserts, upsert, row locks, atomic math |
| ⚡ **Cache & Concurrency** | In-process Memory, Redis, Memcached drivers, versioned caching, batch operations, TTL & LRU eviction, rate limiting, distributed locks, atomic counters |
| 📡 **HTTP & I/O** | Adaptive curl / Swoole coroutine engines, query / form / json / multipart serialization, `Request::input()`, JSON responses, SSE streaming, file downloads |
| 🧬 **Request Context** | Coroutine Context isolation, Request snapshot/restore/scope stack, end-to-end request cleanup, optional automatic cleanup |
| 🔒 **Security & Auth** | Strict validator, Bearer token parsing, multi-driver Session, secure session ID regeneration, HMAC signing, AES-256-GCM encryption |
| 🛠️ **DevOps & Diagnostics** | Structured Log, Monitor metrics, Benchmark suite, CLI runner, IDE Helper stubs, audit verification scripts |

---

<a id="system-requirements"></a>
## 💻 System Requirements

### 📦 Required Dependencies

- 🐘 **PHP 8.0–8.5** — Requires PHP 8.0 or later; fully tested and verified on PHP 8.1.30, 8.2.33, 8.3.33, 8.4.25, and 8.5.10
- 🗄️ **PDO Extension** — Required for database operations, supports MySQL, PostgreSQL, SQLite, SQL Server, etc.

### 🔌 Optional Dependencies

**Cache Systems**
- 🔴 **Redis Extension** — Required for Redis cache and connection pooling: `extension=redis`
- 🟢 **Memcached Extension** — Required for Memcached cache: `extension=memcached`

**High-Performance Resident Mode**
- 🚀 **Swoole Extension** — Required for resident process mode, coroutine context, and connection pools: `extension=swoole`

**Database Drivers**
- 🐬 **MySQL PDO Driver** — `extension=pdo_mysql`
- 🐘 **PostgreSQL PDO Driver** — `extension=pdo_pgsql`
- 🪶 **SQLite PDO Driver** — `extension=pdo_sqlite`
- 🪟 **SQL Server PDO Driver** — `extension=pdo_sqlsrv`

---

<a id="quick-start"></a>
## 🚀 Quick Start

### 1️⃣ Install Framework

```bash
# Compile and install extension
phpize
./configure --enable-gene=shared
make
make install

# Enable extension in php.ini
extension=gene.so
```

> 📖 See [Configuration Guide](docs/CONFIGURATION.md) for full INI directive explanations and production recommendations for FPM & Swoole.

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

### 4️⃣ Configure Services & Connections

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
        $this->view->title = "Gene Documentation";
        $this->view->display('index', 'common');
    }
}
```

### 6️⃣ Using the Aspect Hook System

Gene Framework provides a native Hook system for Aspect-Oriented Programming (AOP) and lifecycle event interception:

```php
<?php
// application/Hooks/AdminAuth.php
namespace Hooks;

class AdminAuth extends \Gene\Hook
{
    public function before()
    {
        // Admin authorization check
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
        // Global before hook: request logging and metrics
        $this->log->info('Request started: ' . $this->request->uri());
    }
}

// application/Hooks/AfterHook.php
namespace Hooks;

class AfterHook extends \Gene\Hook
{
    public function after()
    {
        // Global after hook: cleanup, telemetry, etc.
        $this->log->info('Request finished');
    }
}
```

**Attach Hooks to Routes (router_hook.ini.php):**
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

**Hook Features:**
- ⚙️ **Pure C Extension Dispatch**: Loaded and dispatched directly by the C engine, eliminating deep userland call stacks
- 🔄 **Full Lifecycle Interception**: Native support for `before`, `after`, and `handle` hook stages
- 💉 **Built-In Dependency Injection**: Easily inject `request`, `response`, `view`, `session`, and other services
- 🛡️ **Strict Type Validation**: Enforced at the engine layer via `gene_hook_ce` instance checks

---

<a id="runtime-modes"></a>
## 🔄 Runtime Modes

### 🌐 PHP-FPM Traditional Mode

Ideal for traditional web hosting environments prioritizing process-level isolation and maximum reliability: each request operates in an independent context, and Zend engine reclaims all memory and handles upon request completion.

### ⚡ Swoole Coroutine Resident Mode

Ideal for high-concurrency microservices and resident API gateways, unlocking full coroutine concurrency and resident performance:

```php
<?php
// Set Swoole runtime mode
\Gene\Application::setRuntimeType('swoole');

$http = new swoole_http_server("0.0.0.0", 9501);

$http->on("request", function ($request, $response) {
    // Inject request context snapshot
    \Gene\Request::init(
        $request->get,
        $request->post,
        $request->cookie,
        $request->server,
        null,
        $request->files,
        null,
        $request->header
    );
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
        // Clean up coroutine-local request resources
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

---

<a id="performance-and-capacity"></a>
## 📊 Performance and Capacity

Gene's performance philosophy is to **minimize internal framework paths, maximize reuse of in-process state, and reduce unnecessary external I/O round trips**:

| Critical Scenario | Optimization Mechanisms |
|:---|:---|
| 🌐 **Routing & Dispatch** | Pure C hash & tree lookups, optional precompiled cache, direct Controller/Hook dispatch |
| 💾 **Configuration & Cache** | Worker-local configuration/route caching, memory capacity limits, approximate LRU eviction, TTL, and batch APIs |
| 🗄️ **Data Access Layer** | Fast PDO query builder, batch writes, upsert, persistent connection reuse, single-statement atomic arithmetic updates |
| ⚡ **Swoole Resident Mode** | Lightweight coroutine context reuse, automated database & Redis connection pools, non-blocking coroutine HTTP client |
| 📈 **Global Observability** | `Gene\Monitor::stats()` aggregates real-time request throughput, cache hit rate, context count, and connection pool metrics |

> 📌 **Production Benchmark Note**: Actual throughput depends on hardware, PHP/Swoole versions, kernel tuning, network topology, and application logic. We recommend benchmarking with realistic routes and dependencies in your target environment while monitoring latency percentiles, error rates, RSS memory usage, and connection pool wait queues.

---

<a id="stability-and-production-operations"></a>
## 🛡️ Stability and Production Operations

### 🌐 PHP-FPM
- ⏱️ **Standard Lifecycle**: Adheres strictly to the PHP request model, allowing Zend engine to reclaim request-scoped resources reliably.
- 📦 **Simple & Resilient**: Inherent process-level fault isolation, well-suited for traditional deployments and stateless containers.
- 🔄 **Resource Reuse**: Combine persistent connections (Persistent PDO) and in-process configuration caching to minimize repeated initialization costs.

### ⚡ Swoole
- 🧬 **Context Isolation & Recycling**: Request data is strictly scoped by coroutine context, backed by `Application::cleanup()` with optional automatic cleanup.
- 🏊 **Robust Connection Pool Governance**: Database and Redis pools provide capacity limits, queue timeouts, idle recycling, and transaction leak safeguards.
- 📊 **Proactive Monitoring**: Resident workers should continuously track RSS memory usage, concurrent context counts, pool wait times, and request error rates.
- 🧪 **Pre-Release Verification**: Run the repository test suites and long-duration soak tests on the target PHP/Swoole runtime before production rollout.

---

<a id="production-cases"></a>
## 🏢 Production Cases

Proven stability in demanding, large-scale enterprise environments:

- 🎓 **Hubei Province Education User Authentication Center**: Core login portal and identity authentication service for millions of students and educators
- 🛒 **Shangdong E-Commerce Platform**: High-performance e-commerce platform and transaction processing middle-office
- 🏗️ **Material Network (生材网)**: Leading B2B digital supply chain and engineering materials trading platform

---

<a id="technical-support"></a>
## 💬 Technical Support

- 📖 **Official Documentation**: [https://www.1xm.net/](https://www.1xm.net/)
- 🐛 **Issue Tracker**: [GitHub Issues](https://github.com/sasou/php-gene/issues)
- ✉️ **Technical Support Email**: [zaipd@qq.com](mailto:zaipd@qq.com)

---

<a id="links"></a>
## 🔗 Links

- 🌐 **Official Website**: [https://www.1xm.net/](https://www.1xm.net/)
- 📦 **PHP 5 Legacy Version**: [php-gene v2.1.0 (PHP 5.x)](https://github.com/sasou/php-gene)
- 🪟 **Windows Version**: [php-gene-for-windows](https://github.com/sasou/php-gene-for-windows)
- 📘 **Configuration Guide**: [CONFIGURATION.md](docs/CONFIGURATION.md)

---

<div align="center">

<h3>Gene Framework</h3>
<p><em>Simple Coding, Elegant Life!</em></p>

[![License](https://img.shields.io/badge/License-PHP%203.01-green.svg?style=flat-square)](http://www.php.net/license/3_01.txt)
[![Author](https://img.shields.io/badge/Author-Sasou-blue.svg?style=flat-square)](mailto:zaipd@qq.com)
[![GitHub stars](https://img.shields.io/github/stars/sasou/php-gene?style=flat-square&logo=github)](https://github.com/sasou/php-gene/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/sasou/php-gene?style=flat-square&logo=github)](https://github.com/sasou/php-gene/network)

<br><br>

<a href="https://info.flagcounter.com/AEYx"><img src="https://s11.flagcounter.com/count2/AEYx/bg_FFFFFF/txt_000000/border_CCCCCC/columns_2/maxflags_10/viewers_0/labels_1/pageviews_1/flags_0/percent_0/" alt="Flag Counter" border="0"></a>

</div>
