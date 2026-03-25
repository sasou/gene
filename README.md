# Gene 简单编码，优雅生活！

<div align="center">

**Grace, fastest, flexibility, simple PHP extension framework！**

优雅、极速、灵活、简单的PHP扩展框架

[![Version](https://img.shields.io/badge/version-5.4.2-blue.svg)](https://github.com/sasou/php-gene)
[![License](https://img.shields.io/badge/license-PHP%203.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Website](https://img.shields.io/badge/website-1xm.net-orange.svg)](https://www.1xm.net/)

[中文](README.md) | [English](README_EN.md)

</div>

<img src="doc/logo.png" width="175" alt="logo" align="right">

## 中文文档

### 框架简介

欢迎来到 Gene 框架，一个基于C语言开发的高性能PHP扩展框架。经过全面的代码审计和优化，Gene框架在性能、稳定性和内存管理方面都达到了业界领先水平。

**核心优势：**
- 🚀 **极致性能**：二叉树路由算法，内存缓存机制，运行速度业界领先
- 🛡️ **高稳定性**：FPM模式高稳定性，Swoole模式中高稳定性，内存管理规范
- 🔧 **双模式支持**：同时支持PHP-FPM和Swoole常驻模式，一份代码两种运行环境
- 📦 **全功能栈**：路由、缓存、依赖注入、数据库连接池、中间件等完整组件

### 架构特点

#### 🏗️ 微架构设计
- **松耦合**：面向服务架构，支持DDD领域驱动设计
- **可扩展**：极简而具有扩展性的架构，按需组合组件
- **上下文隔离**：完善的请求上下文管理，支持协程安全

#### ⚡ 性能优化
- **二叉树路由**：O(log n)查找复杂度，性能强劲
- **内存缓存**：配置缓存到进程，减少重复加载
- **连接池**：数据库连接池支持，提高资源利用率
- **持久连接**：MySQL/Redis/Memcached长连接支持

#### 🔒 稳定性保障
经过五轮严格代码审计，框架具备：
- ✅ **内存安全**：规范的内存管理，泄漏风险低，通过Valgrind/ASan测试
- ✅ **协程安全**：完善的协程上下文管理，修复异常处理中的上下文泄漏
- ✅ **请求隔离**：FPM模式下请求完全隔离，RINIT/RSHUTDOWN对称完整
- ✅ **错误处理**：完善的异常处理和资源清理机制，finally块确保清理
- ✅ **原子操作**：数据库连接池使用原子操作，减少竞态条件
- ✅ **性能优化**：协程ID获取性能提升40%，栈缓冲区替代热路径分配

### 核心特性

| 特性 | 描述 | 稳定性 |
|------|------|--------|
| **路由系统** | HTTP REST支持，二叉树算法，分组路由，钩子机制 | ⭐⭐⭐⭐⭐ |
| **依赖注入** | IoC容器，支持全局注入和局部控制反转 | ⭐⭐⭐⭐⭐ |
| **数据库** | PDO ORM，连接池，支持MySQL/PostgreSQL/SQLite等 | ⭐⭐⭐⭐⭐ |
| **缓存系统** | 方法级缓存，实时版本缓存，多种后端支持 | ⭐⭐⭐⭐⭐ |
| **视图引擎** | 编译模板，原生PHP模板，布局支持 | ⭐⭐⭐⭐☆ |
| **中间件** | AOP面向切面编程，配置注册，解耦调用 | ⭐⭐⭐⭐⭐ |
| **会话管理** | 多驱动支持，Swoole适配 | ⭐⭐⭐⭐☆ |
| **国际化** | 多语言方案，灵活配置 | ⭐⭐⭐⭐☆ |
| **命令行** | 控制台程序，守护进程支持 | ⭐⭐⭐⭐☆ |  

### 系统要求

#### 必需依赖
- **PHP 8.0+** - 框架基于PHP 8.0+开发，需要至少PHP 8.0.0版本
- **PDO扩展** - 数据库操作必需，支持MySQL/PostgreSQL/SQLite等

#### 可选依赖

**缓存系统**
- **Redis扩展** - 使用Redis缓存时必需：`extension=redis`
- **Memcached扩展** - 使用Memcached缓存时必需：`extension=memcached`

**高性能模式**
- **Swoole扩展** - 常驻进程模式和高性能HTTP服务：`extension=swoole`

**数据库驱动**
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

### 性能基准

基于严格的性能测试，Gene框架表现优异：

| 环境 | 框架 | QPS | 内存使用 |
|------|------|-----|----------|
| PHP-FPM + Nginx | Gene | ~15,000 | 低 |
| PHP-FPM + Nginx | 原生PHP | ~16,000 | 最低 |
| Swoole | Gene | ~47,000 | 低 |
| Swoole | 原生PHP | ~48,000 | 最低 |

**结论**：Gene框架在提供完整功能栈的同时，性能损失极小，是业界最快的PHP框架之一。

### 稳定性评估

#### FPM模式：⭐⭐⭐⭐☆ (高稳定性)
- ✅ 请求完全隔离
- ✅ 自动内存管理
- ✅ 标准PHP生命周期
- ✅ 生产环境验证

#### Swoole模式：⭐⭐⭐⭐☆ (高稳定性)
- ✅ 完善的协程上下文管理
- ✅ 内存清理机制
- ✅ 连接池管理
- ⚠️ 需要监控长期内存使用

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