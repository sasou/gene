<div align="center">
  <img src="images/logo.png" width="152" alt="Gene Framework Logo">
  <h1>Gene Framework</h1>
  <p><strong>Move PHP's critical framework paths into the C extension layer.</strong></p>
  <p>A high-performance full-stack framework for concurrent APIs, enterprise web applications, and resident Swoole services</p>

[![PHP](https://img.shields.io/badge/PHP-8.0--8.5-777BB4?style=for-the-badge&logo=php&logoColor=white)](https://www.php.net/)
[![Release](https://img.shields.io/badge/Release-6.2.5-2563EB?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sasou/php-gene/releases)
[![Swoole](https://img.shields.io/badge/Swoole-Ready-16A34A?style=for-the-badge)](https://www.swoole.com/)
[![License](https://img.shields.io/badge/License-PHP_3.01-0F766E?style=for-the-badge)](http://www.php.net/license/3_01.txt)

[🇨🇳 简体中文](README.md) · [🇬🇧 English](README_EN.md) · [⚙️ Configuration](docs/CONFIGURATION.md) · [🌐 Website](https://www.1xm.net/)
</div>

---

## 🚀 Engineered for performance. Designed for production.

Gene is a web application framework delivered as a **PHP extension**, with its core capabilities implemented in **C**. Routing, dependency injection, database access, ORM, caching, request context, and common web primitives run at the extension layer—preserving PHP's development experience while shortening the framework's internal execution path.

Gene supports the proven **PHP-FPM** request model and provides first-class architecture for **resident Swoole workers and coroutine concurrency**, including context isolation, connection pools, lifecycle orchestration, and observability. The same application architecture can evolve from a conventional web service into a high-concurrency resident service.

<table>
<tr>
<td width="33%" valign="top">
<strong>⚙️ C-Level Critical Paths</strong><br><br>
Routing, component dispatch, DI, and query construction execute in the extension layer, reducing bootstrap, file loading, and userland dispatch overhead.
</td>
<td width="33%" valign="top">
<strong>🔁 Dual-Runtime Architecture</strong><br><br>
Native PHP-FPM and Swoole support, with coroutine-local Context, request snapshots, explicit cleanup, and resource reuse for resident workers.
</td>
<td width="33%" valign="top">
<strong>🧰 Production-Ready Stack</strong><br><br>
MVC, ORM, caching, pools, HTTP, sessions, security, and monitoring cover the complete service lifecycle in one coherent framework.
</td>
</tr>
</table>

> 🚦 **Gene 6.2.5** delivers optimized router, DI, database, and logging hot paths; zero-copy reads for frozen framework tables; hot/cold Context separation; C-level idle stacks for both database and Redis pools; and expanded view, logging, and Monitor telemetry. The Linux + real Swoole release gate passes 17/17 checks.

## ✨ Why Gene

### ⚡ Speed without sacrificing clarity

Gene does not merely place a conventional PHP framework inside a resident process. It redesigns critical execution paths around C-level routing and dispatch, in-process configuration and route caches, batch and atomic data operations, and coroutine-aware connection reuse. Application code remains familiar, expressive PHP.

### 🔄 One codebase, two runtime models

| 🌐 PHP-FPM | ⚡ Swoole / Coroutine |
|:---|:---|
| Standard request lifecycle and process-level fault isolation | Resident workers, coroutine concurrency, and minimal initialization overhead |
| Ideal for conventional web apps, containers, and mature hosting stacks | Ideal for concurrent APIs, microservices, and gateways |
| Zend reclaims request resources at the end of each request | Context isolation, connection pools, and a unified cleanup lifecycle |

### 🛡️ More than components—a production runtime foundation

- 🔄 **Explicit lifecycles** across MINIT/MSHUTDOWN, RINIT/RSHUTDOWN, and Swoole request contexts.
- 🧬 **Reliable data boundaries** through coroutine-local Context, Request snapshot/restore, and explicit scopes.
- 🏊 **Built-in resource governance** with pool capacity limits, wait timeouts, idle recycling, health checks, and automatic transaction rollback on leaks.
- 📏 **Controlled memory growth** through cache limits, TTL, approximate LRU, Context watermarks, and resident-process diagnostics.
- 🚫 **Fail-fast boundaries** that reject malformed query structures, invalid JSON, and conflicting HTTP payloads before execution.
- ✅ **End-to-end verification** across Windows, macOS, Linux, FPM, and real Swoole environments, including regression, audit reproduction, and soak testing.

## 🧩 Capability Map

| Domain | Core Capabilities |
|:---|:---|
| 🌐 **Application & Routing** | RESTful routes, groups, dynamic parameters, regex matching, error routes, Controller dispatch, Hook lifecycle |
| 💉 **IoC / DI** | Request-scoped services, configuration fallback, Controller / Service / Hook injection, fast in-process `Invoke` |
| 🗄️ **Database & ORM** | MySQL, PostgreSQL, SQLite, SQL Server; transactions, structured JOIN, UNION, pagination, batch inserts, upsert, row locks, atomic arithmetic |
| ⚡ **Cache & Concurrency** | In-process Memory, Redis, Memcached, versioned cache, batch operations, TTL, approximate LRU, rate limits, distributed locks, atomic counters |
| 📡 **HTTP & I/O** | Adaptive curl / Swoole coroutine client, query, form, JSON, multipart, SSE, JSON responses, and large-file downloads |
| 🧬 **Coroutine Runtime** | Context isolation, Request snapshot stack, automatic/manual cleanup, database and Redis pools, Worker bootstrap |
| 🔐 **Security & State** | Strict validation, Bearer tokens, multi-backend Session, session ID regeneration, HMAC, AES-256-GCM |
| 🛠️ **Engineering** | Structured logs, `Monitor::stats()`, Benchmark, CLI, IDE Helper, acceptance harnesses, and audit reproduction tools |

## 🌟 6.2 Series Highlights

| Capability | Representative API / Mechanism | Value |
|:---|:---|:---|
| 🔗 Complex query composition | `joinOn()`, `union()`, `unionAll()`, `paginateResult()` | Safely compose advanced queries and paginate the final result set |
| ✍️ Atomic data updates | `increment()`, `decrement()` | Perform arithmetic updates in one SQL statement and narrow race windows |
| 📥 Unified input model | `Request::input()` | Merge GET → POST → JSON with a shared request-level JSON parse cache |
| 🌍 Adaptive HTTP | query, form, JSON, multipart | Preserve payload semantics across curl and Swoole coroutine backends |
| 🧭 Precise context checks | `Context::has()` | Distinguish a missing key from an explicit `null` value |
| 📊 Runtime observability | `Monitor::stats()`, slow-query counters, pool metrics | Measure and govern throughput, cache, Context, and pool health |

## 🏛️ Architecture at a Glance

```text
┌─────────────────────────────────────────────────────────────────────┐
│                         PHP Application                             │
│      Controller · Service · Model · Hook · View · Domain Logic     │
├─────────────────────────────────────────────────────────────────────┤
│                         Gene C Extension                            │
│  Router · DI · ORM · Cache · Context · HTTP · Session · Security  │
├──────────────────────────────┬──────────────────────────────────────┤
│           PHP-FPM            │        Swoole Resident Worker        │
│  Standard request lifecycle  │ Coroutine isolation · Pools · Reuse │
├──────────────────────────────┴──────────────────────────────────────┤
│ MySQL · PostgreSQL · SQLite · SQL Server · Redis · Memcached       │
└─────────────────────────────────────────────────────────────────────┘
```

## 🧭 Quick Start

### 1️⃣ Build and enable the extension

```bash
cd src
phpize
./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
make install
```

Enable Gene in `php.ini`:

```ini
extension=gene.so
```

Verify the installation:

```bash
php --ri gene
```

> On macOS, use `tools/mac_build.sh`. Windows build instructions are available in [AGENTS.md](AGENTS.md). See the [configuration reference](docs/CONFIGURATION.md) for production settings.

### 2️⃣ Create the application entry point

```php
<?php

$app = \Gene\Application::getInstance();
$app
    ->load('router.ini.php')
    ->load('config.ini.php')
    ->run();
```

### 3️⃣ Register routes

```php
<?php

$router = new \Gene\Router();
$router->clear()
    ->get('/', '\Controllers\Home@index')
    ->get('/users/:id', '\Controllers\User@show', '@Auth')
    ->post('/users', '\Controllers\User@create', '@Auth')
    ->group('/admin')
        ->get('/*', '\Controllers\Admin@index', '@AdminAuth')
    ->group()
    ->error(404, function () {
        http_response_code(404);
        \Gene\Http\Response::json(['error' => 'Not Found']);
    });
```

### 4️⃣ Configure services

```php
<?php

$config = new \Gene\Config();
$config->clear();
$config->set('db', [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
        'dsn' => 'mysql:host=127.0.0.1;dbname=gene_demo;charset=utf8mb4',
        'username' => 'root',
        'password' => '',
    ]],
    'instance' => true,
]);
```

### 5️⃣ Write a controller

```php
<?php

namespace Controllers;

class Home extends \Gene\Controller
{
    public function index()
    {
        return \Gene\Http\Response::json([
            'framework' => 'Gene',
            'version' => '6.2.5',
        ]);
    }
}
```

## ⚡ Swoole: One Entry Point for the Entire Lifecycle

```php
<?php

\Gene\Application::setRuntimeType('swoole');

$server = new Swoole\Http\Server('0.0.0.0', 9501);
$app = \Gene\Application::getInstance();

$server->on('WorkerStart', static function () use ($app) {
    $app->workerReady();
});

$server->on('Request', static function ($request, $response) use ($app) {
    $app->handleSwoole($request, $response);
});

$server->start();
```

`handleSwoole()` orchestrates Worker readiness, request initialization, Response binding, application execution, the exception boundary, response completion, and Context cleanup—reducing the risk of incomplete hand-written lifecycle code.

## 📈 Performance by Design

Gene avoids context-free benchmark claims. Instead, it provides optimization mechanisms that are explainable and verifiable:

| Hot Path | Design |
|:---|:---|
| 🌐 Routing & dispatch | C-level hash/tree matching, optional route precompilation, direct Controller and Hook dispatch |
| 💾 Configuration & cache | Worker-local sharing, hard capacity limits, approximate LRU, TTL, and batch APIs |
| 🗄️ Data access | PDO query builder, batch writes, upsert, atomic updates, and connection reuse |
| ⚡ Swoole runtime | Context pool, fast coroutine-ID path, database/Redis pools, and non-blocking HTTP |
| 📊 Observability | Request, cache, Context, slow-query, and pool metrics through `Monitor::stats()` |

Actual throughput depends on hardware, PHP/Swoole versions, kernel tuning, databases, and application logic. Benchmark realistic routes and dependency graphs in the target environment while tracking p95/p99 latency, error rates, RSS, and pool wait times.

## 💻 Requirements

| Type | Requirement |
|:---|:---|
| 🐘 PHP | PHP 8.0–8.5; current matrix covers 8.1.30, 8.2.33, 8.3.33, 8.4.25, and 8.5.10 |
| 🖥️ Platforms | Linux, macOS, Windows |
| 📦 Required extension | PDO when using database capabilities |
| 🔌 Optional extensions | Swoole, Redis, Memcached, and the relevant PDO database drivers |

## ✅ Verification and Production Gates

```bash
# Regression suite
php test/TestRunner.php

# One-command Linux + Swoole verification
bash tools/acceptance/linux_swoole_verify.sh

# Self-contained demo without external MySQL or Redis
bash tools/acceptance/linux_swoole_verify.sh --demo
```

See [test/README.md](test/README.md) for test usage. FPM/Swoole acceptance, pool concurrency, soak tests, and log-rotation gates are documented in [tools/acceptance/README.md](tools/acceptance/README.md).

## 🏢 Production Experience

Gene has powered long-running systems in education identity, e-commerce, and B2B supply-chain scenarios:

- 🎓 **Hubei Province Education User Authentication Center** — a unified identity entry point for students, educators, and institutions across the province.
- 🛒 **Shangdong E-Commerce Platform** — concurrent commerce workloads and transaction middle-office services.
- 🏗️ **Material Network (生材网)** — a digital B2B marketplace for engineering materials and supply chains.

## 📚 Documentation and Community

- 📖 [Official documentation](https://www.1xm.net/)
- ⚙️ [Configuration reference](docs/CONFIGURATION.md)
- 🐞 [GitHub Issues](https://github.com/sasou/php-gene/issues)
- 🪟 [Windows releases](https://github.com/sasou/php-gene-for-windows)
- 📦 [PHP 5 legacy version](https://github.com/sasou/php-gene)
- ✉️ Technical contact: <zaipd@qq.com>

---

<div align="center">
  <h3>🧬 Gene Framework</h3>
  <p><strong>Shorter execution paths. A more complete production stack.</strong></p>
  <p><em>Simple Coding, Elegant Life.</em></p>

[![GitHub stars](https://img.shields.io/github/stars/sasou/php-gene?style=social)](https://github.com/sasou/php-gene/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/sasou/php-gene?style=social)](https://github.com/sasou/php-gene/network)

<sub>Released under the <a href="http://www.php.net/license/3_01.txt">PHP License 3.01</a>.</sub>
</div>
