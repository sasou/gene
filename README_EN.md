# Gene Framework - Grace, Fastest, Flexibility, Simplicity

<div align="center">

**A high-performance PHP extension framework developed in C**

[![Version](https://img.shields.io/badge/version-5.3.6-blue.svg)](https://github.com/sasou/php-gene)
[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Website](https://img.shields.io/badge/website-1xm.net-orange.svg)](http://1xm.net/)

[中文](README.md) | [English](README_EN.md)

</div>

<img src="doc/logo.png" width="175" alt="logo" align="right">

## Framework Introduction

Welcome to Gene Framework, a high-performance PHP extension framework developed in C language. After comprehensive code audit and optimization, Gene Framework achieves industry-leading levels in performance, stability, and memory management.

**Core Advantages:**
- 🚀 **Extreme Performance**: Binary tree routing algorithm, memory caching mechanism, industry-leading speed
- 🛡️ **High Stability**: High stability in FPM mode, medium-high stability in Swoole mode, standardized memory management
- 🔧 **Dual Mode Support**: Supports both PHP-FPM and Swoole resident modes with the same codebase
- 📦 **Full Stack**: Complete components including routing, caching, dependency injection, database connection pool, middleware

### Architecture Features

#### 🏗️ Micro-Architecture Design
- **Loose Coupling**: Service-oriented architecture, supports DDD domain-driven design
- **Extensible**: Minimal yet extensible architecture, components on demand
- **Context Isolation**: Complete request context management, coroutine-safe

#### ⚡ Performance Optimization
- **Binary Tree Routing**: O(log n) lookup complexity, powerful performance
- **Memory Caching**: Configuration cached to process, reduces repeated loading
- **Connection Pool**: Database connection pool support, improves resource utilization
- **Persistent Connections**: MySQL/Redis/Memcached persistent connection support

#### 🔒 Stability Assurance
After rigorous code audit, the framework features:
- ✅ **Memory Safety**: Standardized memory management, low leak risk
- ✅ **Coroutine Safety**: Complete coroutine context management
- ✅ **Request Isolation**: Complete request isolation in FPM mode
- ✅ **Error Handling**: Comprehensive exception handling and resource cleanup

### Core Features

| Feature | Description | Stability |
|---------|-------------|-----------|
| **Routing System** | HTTP REST support, binary tree algorithm, group routing, hook mechanism | ⭐⭐⭐⭐⭐ |
| **Dependency Injection** | IoC container, supports global injection and local control inversion | ⭐⭐⭐⭐⭐ |
| **Database** | PDO ORM, connection pool, supports MySQL/PostgreSQL/SQLite etc | ⭐⭐⭐⭐⭐ |
| **Cache System** | Method-level caching, real-time version caching, multiple backend support | ⭐⭐⭐⭐⭐ |
| **View Engine** | Compiled template, native PHP template, layout support | ⭐⭐⭐⭐☆ |
| **Middleware** | AOP aspect-oriented programming, configuration registration, decoupled calls | ⭐⭐⭐⭐⭐ |
| **Session Management** | Multi-driver support, Swoole adaptation | ⭐⭐⭐⭐☆ |
| **Internationalization** | Multi-language solution, flexible configuration | ⭐⭐⭐⭐☆ |
| **Command Line** | Console programs, daemon process support | ⭐⭐⭐⭐☆ |

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
    \Gene\Request::init($request->get, $request->post, $request->cookie, $request->server);
    \Gene\Application::setResponse($response);
    
    ob_start();
    try {
        $app = \Gene\Application::getInstance();
        $app->autoload(APP_ROOT)
            ->load("router.ini.php")
            ->load("config.ini.php")
            ->run();
    } finally {
        // Clean up context to avoid data interference
        \Gene\Application::clearState();
        \Gene\Application::destroyContext();
    }
    
    $out = ob_get_contents();
    ob_end_clean();
    $response->end($out);
});
$http->start();
```

## Performance Benchmarks

Based on rigorous performance testing, Gene Framework performs excellently:

| Environment | Framework | QPS | Memory Usage |
|------------|----------|-----|-------------|
| PHP-FPM + Nginx | Gene | ~15,000 | Low |
| PHP-FPM + Nginx | Native PHP | ~16,000 | Lowest |
| Swoole | Gene | ~45,000 | Medium |
| Swoole | Native PHP | ~48,000 | Lowest |

**Conclusion**: Gene Framework maintains minimal performance loss while providing a complete feature stack, making it one of the fastest PHP frameworks in the industry.

## Stability Assessment

### FPM Mode: ⭐⭐⭐⭐☆ (High Stability)
- ✅ Complete request isolation
- ✅ Automatic memory management
- ✅ Standard PHP lifecycle
- ✅ Production environment proven

### Swoole Mode: ⭐⭐⭐⭐☆ (Medium-High Stability)
- ✅ Complete coroutine context management
- ✅ Memory cleanup mechanism
- ✅ Connection pool management
- ⚠️ Requires monitoring long-term memory usage

## Production Cases

- **Hubei Province Education User Authentication Center**: Login portal for millions of students and education users
- **Shangdong E-commerce Platform**: High-performance e-commerce platform
- **Material Network**: B2B platform for materials industry

## Technical Support

- 📖 [Official Documentation](http://1xm.net/)
- 🐛 [Issue Tracker](https://github.com/sasou/php-gene/issues)
- 💬 [Technical Support](mailto:admin@php-gene.com)

---

## Links

- **Official Website**: [http://1xm.net/](http://1xm.net/)
- **PHP5 Version**: [https://github.com/sasou/php-gene](https://github.com/sasou/php-gene) (Version 2.1.0)
- **Windows Version**: [https://github.com/sasou/php-gene-for-windows](https://github.com/sasou/php-gene-for-windows)

---

<div align="center">

**Gene Framework - Simple Coding, Elegant Life!**

[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Author](https://img.shields.io/badge/author-Sasou-blue.svg)](mailto:admin@php-gene.com)

</div>
