<div align="center">
  <img src="images/logo.png" width="152" alt="Gene Framework Logo">
  <h1>Gene Framework</h1>
  <p><strong>将 PHP 框架的关键路径，推进到 C 扩展层。</strong></p>
  <p>面向高并发 API、企业级 Web 应用与 Swoole 常驻服务的高性能全栈框架</p>

[![PHP](https://img.shields.io/badge/PHP-8.0--8.5-777BB4?style=for-the-badge&logo=php&logoColor=white)](https://www.php.net/)
[![Release](https://img.shields.io/badge/Release-6.2.5-2563EB?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sasou/php-gene/releases)
[![Swoole](https://img.shields.io/badge/Swoole-Ready-16A34A?style=for-the-badge)](https://www.swoole.com/)
[![License](https://img.shields.io/badge/License-PHP_3.01-0F766E?style=for-the-badge)](http://www.php.net/license/3_01.txt)

[🇨🇳 简体中文](README.md) · [🇬🇧 English](README_EN.md) · [⚙️ 配置参考](docs/CONFIGURATION.md) · [🌐 在线文档](https://www.1xm.net/)
</div>

---

## 🚀 为性能而生，不止于性能

Gene 是一个以 **PHP 扩展** 形态交付、核心能力由 **C 语言** 实现的 Web 应用框架。它把路由、依赖注入、数据库访问、ORM、缓存、请求上下文与常用 Web 原语下沉到扩展层，在保留 PHP 开发效率的同时，缩短框架内部执行链路。

它既适配成熟稳定的 **PHP-FPM** 请求模型，也为 **Swoole 常驻进程与协程并发** 提供上下文隔离、连接池、生命周期收口和可观测能力。同一套业务架构，可以从传统 Web 服务平滑演进到高并发常驻服务。

<table>
<tr>
<td width="33%" valign="top">
<strong>⚙️ C 级核心路径</strong><br><br>
路由匹配、组件调度、DI、查询构建等关键能力运行在扩展层，减少框架引导、文件加载与用户态调用开销。
</td>
<td width="33%" valign="top">
<strong>🔁 双运行时架构</strong><br><br>
原生支持 PHP-FPM 与 Swoole；为常驻 Worker 提供协程级 Context、请求快照、显式清理与资源复用。
</td>
<td width="33%" valign="top">
<strong>🧰 完整生产能力</strong><br><br>
从 MVC、ORM、缓存到连接池、HTTP 客户端、Session、安全组件与监控，一套框架覆盖完整服务链路。
</td>
</tr>
</table>

> 🚦 **Gene 6.2.5**：完成路由、DI、数据库与日志热路径优化，加入冻结框架表零拷贝读取、Context 冷热分离、数据库/Redis 双池 C 层空闲栈，以及更完整的视图、日志和 Monitor 可观测配置。Linux + 真实 Swoole 发布门禁 17/17 通过。

## ✨ 为什么选择 Gene

### ⚡ 快，而不以复杂度为代价

Gene 不是将传统 PHP 框架简单地搬进常驻进程，而是重新设计关键执行路径：C 层路由与分发、进程内配置和路由缓存、批量与原子数据操作，以及面向协程的连接复用。业务代码仍然保持熟悉、清晰的 PHP 风格。

### 🔄 一套代码，驾驭两种运行模型

| 🌐 PHP-FPM | ⚡ Swoole / Coroutine |
|:---|:---|
| 标准请求生命周期与进程级故障隔离 | 常驻 Worker、协程并发与低初始化开销 |
| 适合传统 Web、容器和成熟托管环境 | 适合高并发 API、微服务与网关 |
| 请求结束后由 Zend 引擎回收资源 | Context 隔离、连接池与统一 cleanup 生命周期 |

### 🛡️ 不只是组件集合，而是生产级运行底座

- 🔄 **生命周期明确**：覆盖 MINIT/MSHUTDOWN、RINIT/RSHUTDOWN 与 Swoole 请求上下文的对称管理。
- 🧬 **数据边界可靠**：协程级 Context、Request snapshot/restore 与显式作用域，避免跨请求污染。
- 🏊 **资源治理内建**：连接池容量约束、等待超时、空闲回收、健康检查与事务泄漏自动回滚。
- 📏 **内存增长可控**：缓存容量上限、TTL、近似 LRU、Context 水位线与常驻进程诊断指标。
- 🚫 **异常快速失败**：在执行前拒绝非法查询结构、无效 JSON 与冲突 HTTP Payload。
- ✅ **验收链路完整**：覆盖 Windows、macOS、Linux、FPM 与真实 Swoole 的回归、审计复现和长跑测试。

## 🧩 能力全景

| 领域 | 核心能力 |
|:---|:---|
| 🌐 **Application & Routing** | RESTful 路由、分组、动态参数、正则匹配、错误路由、Controller 分发、Hook 生命周期 |
| 💉 **IoC / DI** | 请求级服务注册、配置回落、Controller / Service / Hook 注入、高效进程内 `Invoke` |
| 🗄️ **Database & ORM** | MySQL、PostgreSQL、SQLite、SQL Server；事务、结构化 JOIN、UNION、分页、批量插入、upsert、行锁、原子增减 |
| ⚡ **Cache & Concurrency** | 进程内 Memory、Redis、Memcached、版本化缓存、批量操作、TTL、近似 LRU、限流、分布式锁、原子计数 |
| 📡 **HTTP & I/O** | curl / Swoole 协程自适应客户端，query、form、JSON、multipart，SSE、JSON Response 与大文件下载 |
| 🧬 **Coroutine Runtime** | Context 隔离、Request 快照栈、自动/手动 cleanup、数据库与 Redis 连接池、Worker 引导 |
| 🔐 **Security & State** | 强类型验证、Bearer Token、Session 多后端、Session ID 重生成、HMAC、AES-256-GCM |
| 🛠️ **Engineering** | 结构化日志、`Monitor::stats()`、Benchmark、CLI、IDE Helper、验收脚本与审计复现工具 |

## 🌟 6.2 系列亮点

| 能力 | 代表 API / 机制 | 带来的价值 |
|:---|:---|:---|
| 🔗 复杂查询表达 | `joinOn()`、`union()`、`unionAll()`、`paginateResult()` | 安全构建复合查询并对最终结果集精准分页 |
| ✍️ 原子数据更新 | `increment()`、`decrement()` | 单条 SQL 完成算术更新，缩短并发竞争窗口 |
| 📥 统一输入模型 | `Request::input()` | 按 GET → POST → JSON 融合输入，并共享请求级 JSON 解析缓存 |
| 🌍 自适应 HTTP | query、form、JSON、multipart | curl 与 Swoole 协程后端保持一致的载荷语义 |
| 🧭 上下文精确判断 | `Context::has()` | 区分键不存在与显式 `null`，消除边界歧义 |
| 📊 运行时可观测性 | `Monitor::stats()`、慢查询计数、池指标 | 让吞吐、缓存、Context 与连接池状态可度量、可治理 |

## 🏛️ 架构一览

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

## 🧭 快速开始

### 1️⃣ 编译并启用扩展

```bash
cd src
phpize
./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
make install
```

在 `php.ini` 中启用：

```ini
extension=gene.so
```

确认扩展已加载：

```bash
php --ri gene
```

> macOS 可直接使用 `tools/mac_build.sh`；Windows 构建说明见 [AGENTS.md](AGENTS.md)。完整生产配置见 [配置参考](docs/CONFIGURATION.md)。

### 2️⃣ 定义应用入口

```php
<?php

$app = \Gene\Application::getInstance();
$app
    ->load('router.ini.php')
    ->load('config.ini.php')
    ->run();
```

### 3️⃣ 注册路由

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

### 4️⃣ 配置服务

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

### 5️⃣ 编写控制器

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

## ⚡ Swoole：一个入口收口完整生命周期

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

`handleSwoole()` 统一完成 Worker 就绪等待、请求初始化、Response 绑定、应用执行、异常边界、响应结束与 Context 清理，减少手写生命周期遗漏。

## 📈 性能设计

Gene 不使用脱离场景的单一数字承诺性能，而是提供可解释、可验证的优化机制：

| 热点路径 | 设计 |
|:---|:---|
| 🌐 路由与分发 | C 层哈希/树匹配、可选路由预编译、Controller 与 Hook 直接调度 |
| 💾 配置与缓存 | Worker 内共享、容量硬约束、近似 LRU、TTL 与批量 API |
| 🗄️ 数据访问 | PDO 查询构建、批量写入、upsert、原子更新与连接复用 |
| ⚡ Swoole Runtime | Context 池、协程 ID 快速路径、数据库/Redis 连接池、非阻塞 HTTP |
| 📊 可观测性 | 请求、缓存、Context、慢查询与连接池指标统一汇入 `Monitor::stats()` |

实际吞吐取决于硬件、PHP/Swoole 版本、内核参数、数据库与业务逻辑。请在目标环境中使用真实路由和依赖拓扑压测，并同时观察 p95/p99、错误率、RSS 与连接池等待情况。

## 💻 系统要求

| 类型 | 要求 |
|:---|:---|
| 🐘 PHP | PHP 8.0–8.5；当前矩阵覆盖 8.1.30、8.2.33、8.3.33、8.4.25、8.5.10 |
| 🖥️ 平台 | Linux、macOS、Windows |
| 📦 必需扩展 | PDO（使用数据库能力时） |
| 🔌 可选扩展 | Swoole、Redis、Memcached，以及对应 PDO 数据库驱动 |

## ✅ 验证与生产准入

```bash
# 回归测试
php test/TestRunner.php

# Linux + Swoole 一键验证
bash tools/acceptance/linux_swoole_verify.sh

# 无外部 MySQL / Redis 的本地 Demo 闭环
bash tools/acceptance/linux_swoole_verify.sh --demo
```

测试说明见 [test/README.md](test/README.md)，FPM/Swoole 验收、连接池并发、长跑和日志轮转门禁见 [tools/acceptance/README.md](tools/acceptance/README.md)。

## 🏢 生产实践

Gene 已用于教育认证、电商交易与 B2B 供应链等长期运行场景：

- 🎓 **湖北省教育用户认证中心**：服务全省师生与教育机构的统一认证入口。
- 🛒 **尚动电子商务平台**：支撑高并发电商业务与交易中台。
- 🏗️ **生材网**：工程材料与供应链数字化 B2B 交易平台。

## 📚 文档与社区

- 📖 [官方文档](https://www.1xm.net/)
- ⚙️ [配置参考](docs/CONFIGURATION.md)
- 🐞 [GitHub Issues](https://github.com/sasou/php-gene/issues)
- 🪟 [Windows 发布版本](https://github.com/sasou/php-gene-for-windows)
- 📦 [PHP 5 遗留版本](https://github.com/sasou/php-gene)
- ✉️ 技术交流：<zaipd@qq.com>

---

<div align="center">
  <h3>🧬 Gene Framework</h3>
  <p><strong>更短的执行路径，更完整的生产能力。</strong></p>
  <p><em>Simple Coding, Elegant Life.</em></p>

[![GitHub stars](https://img.shields.io/github/stars/sasou/php-gene?style=social)](https://github.com/sasou/php-gene/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/sasou/php-gene?style=social)](https://github.com/sasou/php-gene/network)

<sub>Released under the <a href="http://www.php.net/license/3_01.txt">PHP License 3.01</a>.</sub>
</div>
