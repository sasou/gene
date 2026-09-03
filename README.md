<div align="center">
  <img src="images/logo.png" width="160" alt="Gene Framework Logo">
  <h1>Gene Framework</h1>
  <p><strong>快速、灵活、面向生产环境的高性能 PHP C 扩展框架</strong></p>
  <p>⚡ 纯 C 构建核心执行路径 · 🚀 原生支持 PHP-FPM 与 Swoole 协程常驻模式</p>

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
  <a href="#框架简介">📖 框架简介</a> &nbsp;|&nbsp;
  <a href="#架构特点">🏛️ 架构特点</a> &nbsp;|&nbsp;
  <a href="#620-能力亮点">✨ 版本亮点</a> &nbsp;|&nbsp;
  <a href="#核心能力">🧩 核心能力</a> &nbsp;|&nbsp;
  <a href="#系统要求">💻 系统要求</a> &nbsp;|&nbsp;
  <a href="#快速开始">🚀 快速开始</a> &nbsp;|&nbsp;
  <a href="#运行模式">🔄 运行模式</a> &nbsp;|&nbsp;
  <a href="#性能与容量">📊 性能与容量</a>
</p>

> 💡 **Gene 6.2.0** — 新增结构化 JOIN、UNION、复杂结果分页、原子算术更新、统一请求输入和 HTTP 表单编码，并支持 PHP 8.0–8.5。

---

<a id="框架简介"></a>
## 📖 框架简介

Gene 是使用 C 语言编写、以 PHP 扩展形式运行的高性能 Web 应用框架。框架将路由查找、依赖注入、ORM、缓存管理、HTTP 客户端、请求上下文和常用 Web 原语直接沉淀在扩展层实现，同时完美支持传统 PHP-FPM 短生命周期模型与 Swoole 常驻进程协程模型。

**🌟 核心优势：**

- ⚡ **扩展级执行路径**：核心组件由纯 C 语言编写，大幅减少框架引导、文件加载和 PHP 用户态调度开销。
- 🔄 **双运行模式原生兼容**：同一套业务代码无缝运行于 PHP-FPM 或 Swoole，为常驻进程提供完备的协程级上下文隔离与显式清理机制。
- 🧰 **开箱即用的完整能力**：内置路由、IoC/DI 容器、四种主流 PDO 驱动、高级 ORM、缓存抽象、连接池、出站 HTTP 客户端、Session、验证器、日志、视图引擎与 CLI 工具。
- 🛡️ **安全规范的查询构建**：结构化条件、安全标识符转义和严格参数绑定深度贯穿 JOIN、WHERE、IN、UNION 及原子更新全流程。
- 📈 **深度可观测与可治理**：提供进程内缓存与连接池指标统计、请求计数器、容量水位上限、空闲连接回收及自动化诊断能力。

---

<a id="架构特点"></a>
## 🏛️ 架构特点

### 🪶 轻量架构

- 🧩 **按需自由组合**：各组件既可独立引入，也可通过 Application、Router、Controller、Hook 和 DI 协同构建完整大型企业级应用。
- 🔒 **严格请求隔离**：FPM 模式沿用标准单请求生命周期；Swoole 模式采用协程级隔离上下文与请求快照，杜绝跨请求数据污染与串号隐患。
- 🗄️ **多数据库统一支持**：MySQL、SQL Server、PostgreSQL、SQLite 共享统一的链式查询构建 API，同时完好保留各底层驱动的方言与标识符特性。
- 🎯 **显式行为与零黑盒**：复合查询、Context 注入、内部调用与缓存操作均具备明确直观的 API，最大程度降低隐式副作用。

### ⚡ 性能设计

- ⚡ **C 层路由与极致分发**：路由树匹配、控制器调用和 Hook 调度均在 C 扩展层完成，消除冗余的用户态中间件开销。
- 💾 **高性能进程内缓存**：全局配置、已解析路由及热点业务数据可驻留于 worker 内存，配备容量软硬上限、近似 LRU 淘汰与 TTL 治理策略。
- 🏊 **自动化连接复用**：Swoole 模式内建数据库与 Redis 连接池，支持容量弹性伸缩、等待超时保护、空闲连接定时回收与心跳保活检测。
- 🚀 **协程热路径深度优化**：支持上下文对象池化复用、协程 ID 快速寻址及可选自动 cleanup，专为高并发微服务与常驻 worker 场景量身打造。
- 📦 **批量化与原子级操作**：批量缓存读取/写入、批量插入、upsert（冲突更新）、原子计数器与单语句算术更新，显著压缩网络往返与并发竞争窗口。
- 🌐 **自适应出站 HTTP**：FPM/CLI 模式智能走高效 libcurl，Swoole 模式自动切换非阻塞协程客户端，彻底杜绝常驻协程中执行阻塞 curl 拖垮服务。

### 🛡️ 稳定性设计

- 🔄 **严格对称的生命周期**：精细管控扩展模块初始化/关闭（MINIT/MSHUTDOWN）、请求启动/释放（RINIT/RSHUTDOWN）以及 Swoole 上下文初始化与重置。
- 🧬 **协程级上下文边界**：Context、Request、Response 与运行状态严格绑定当前协程环境，支持手动显式清理与自动安全清理双路径。
- 🧼 **周密的资源卫生机制**：连接池自带事务泄漏自动回滚防护、陈旧断连剔除、主动健康探测与详尽的诊断追踪。
- 📊 **精细化内存防线**：常驻内存缓存设定硬上限防护与主动过期清理机制；上下文表支持容量超限熔断、惰性清理及遥测监控。
- 🚫 **快速失败安全边界**：对于畸形查询结构、非法 JSON 输入与相互冲突的 HTTP 载荷提前拦截并快速失败，避免发送半成或歧义指令。
- 🧪 **多平台回归验证体系**：代码库内置完整单元测试集、各场景审计复现脚本，并配套 Windows、macOS、Linux/Swoole 一致性验收工具链。

---

<a id="620-能力亮点"></a>
## ✨ 6.2.0 能力亮点

| 领域 | 6.2.0 新能力 | 核心价值 |
|:---|:---|:---|
| 🗄️ **ORM 查询** | `joinOn()`、`union()`、`unionAll()`、`paginateResult()` | 安全构建复杂 JOIN 与联合查询，精准分页多表复合结果集 |
| ✍️ **ORM 写入** | `increment()`、`decrement()` | 单条 SQL 即可完成行级原子自增/自减，缩短并发竞争窗口 |
| 🌐 **HTTP 客户端** | `query`、`form`、multipart 字段及严格 Payload 校验 | 规范统一 curl 与 Swoole 协程客户端后端的编码与传输行为 |
| 📥 **请求处理** | GET → POST → JSON 深度融合的 `Request::input()` | 统一高效的参数提取入口，多处调用共享请求级 JSON 解析缓存 |
| 🧬 **上下文** | `Context::has()` | 严格区分键不存在与显式赋值为 `null` 的边界场景 |
| 💻 **跨平台兼容** | PHP 8.0–8.5 支持、Windows x64/x86、macOS 构建工具 | 广泛覆盖主流开发系统、CI/CD 流水线与生产部署环境 |

---

<a id="核心能力"></a>
## 🧩 核心能力

| 领域 | 能力矩阵 |
|:---|:---|
| 🌐 **路由与调度** | RESTful 路由注册、路由分组、URI 参数动态捕获、正则匹配、控制器安全转发、Hook 切面生命周期 |
| 💉 **依赖与调用** | 依赖注入容器 (IoC/DI)、显式服务实例化、Controller/Service/Hook 注入、进程内高效 `Invoke`、本地/远程 `Rest` 调用 |
| 🗄️ **数据库与 ORM** | MySQL / SQL Server / PostgreSQL / SQLite 全驱动覆盖、事务隔离、连接池、结构化 JOIN、UNION、复杂结果集分页、批量插入、upsert、悲观行锁、原子增减 |
| ⚡ **缓存与并发原语** | 进程内 Memory、Redis、Memcached 统一驱动、版本化缓存、批量缓存、TTL 与 LRU 淘汰、令牌限流、分布式锁、原子计数器 |
| 📡 **HTTP 与输入输出** | curl / Swoole 协程双引擎自适应、query / form / json / multipart 规范化传输、`Request::input()`、JSON 响应、SSE 流式推送、大文件下载 |
| 🧬 **请求上下文** | 协程上下文隔离 (Context)、Request snapshot/restore/scope 快照栈、端到端生命周期清理及可选自动 cleanup |
| 🔒 **安全与认证** | 强类型数据验证器、严格 Bearer Token 解析、Session 多后端驱动、Session ID 安全重生成、HMAC 签名与 AES-256-GCM 高强度加密 |
| 🛠️ **工程与运维治理** | 结构化 Log 记录、Monitor 性能统计、Benchmark 基准压测套件、CLI 命令行支撑、IDE 代码补全 Helper、审计复现脚本 |

---

<a id="系统要求"></a>
## 💻 系统要求

### 📦 必需依赖

- 🐘 **PHP 8.0–8.5** — 最低要求 PHP 8.0；已在 PHP 8.1.30、8.2.33、8.3.33、8.4.25、8.5.10 完整通过验证
- 🗄️ **PDO 扩展** — 数据库操作必需核心扩展，支持 MySQL / PostgreSQL / SQLite / SQL Server 等

### 🔌 可选依赖

**缓存系统**
- 🔴 **Redis 扩展** — 使用 Redis 缓存及连接池时必需：`extension=redis`
- 🟢 **Memcached 扩展** — 使用 Memcached 缓存时必需：`extension=memcached`

**高性能常驻进程**
- 🚀 **Swoole 扩展** — 启用常驻内存模式、协程上下文与内置连接池：`extension=swoole`

**数据库驱动**
- 🐬 **MySQL PDO 驱动** — `extension=pdo_mysql`
- 🐘 **PostgreSQL PDO 驱动** — `extension=pdo_pgsql`
- 🪶 **SQLite PDO 驱动** — `extension=pdo_sqlite`
- 🪟 **SQL Server PDO 驱动** — `extension=pdo_sqlsrv`

---

<a id="快速开始"></a>
## 🚀 快速开始

### 1️⃣ 安装框架

```bash
# 编译并安装扩展
phpize
./configure --enable-gene=shared
make
make install

# 在 php.ini 中启用扩展
extension=gene.so
```

> 📖 完整 INI 配置项说明与 FPM / Swoole 生产环境推荐配置见 [配置参考文档](docs/CONFIGURATION.md)。

### 2️⃣ 创建应用入口

```php
<?php
// index.php
$app = \Gene\Application::getInstance();
$app
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->run();
```

### 3️⃣ 配置路由

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

### 4️⃣ 配置服务与连接

```php
<?php
// config.ini.php
$config = new \Gene\Config();
$config->clear();

// 数据库连接配置
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

// 缓存服务配置
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
        'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
        'persistent' => true,
    ]],
    'instance' => true
]);
```

### 5️⃣ 创建控制器

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
        $this->view->title = "Gene 文档";
        $this->view->display('index', 'common');
    }
}
```

### 6️⃣ 使用强大的切面钩子 (Hook)

Gene 框架内置原生 Hook 系统，支持面向切面编程 (AOP) 与事件拦截：

```php
<?php
// application/Hooks/AdminAuth.php
namespace Hooks;

class AdminAuth extends \Gene\Hook
{
    public function before()
    {
        // 管理员权限验证拦截
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
        // 全局前置钩子：记录请求耗时与访问日志
        $this->log->info('Request started: ' . $this->request->uri());
    }
}

// application/Hooks/AfterHook.php
namespace Hooks;

class AfterHook extends \Gene\Hook
{
    public function after()
    {
        // 全局后置钩子：指标统计与环境清理
        $this->log->info('Request finished');
    }
}
```

**在路由中编排切面钩子 (router_hook.ini.php)：**
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

**Hook 核心特性：**
- ⚙️ **扩展层纯 C 分发**：由框架工厂直接调度 Hook，消除用户态深层调用栈开销
- 🔄 **完备生命周期**：原生支持 `before`、`after`、`handle` 三类拦截切入点
- 💉 **原生依赖注入**：可自由调用 `request`、`response`、`view`、`session` 等内置组件
- 🛡️ **严格类型检查**：底层基于 `gene_hook_ce` 保证对象类型安全性

---

<a id="运行模式"></a>
## 🔄 运行模式

### 🌐 PHP-FPM 传统模式

适用于传统 Web 托管环境，兼备高隔离度与极高稳定性：每个请求独享运行上下文，生命周期结束后引擎自动释放全部内存与句柄资源。

### ⚡ Swoole 协程常驻模式

适用于高并发微服务与常驻 API 网关，极致发挥异步协程与持久常驻性能：

```php
<?php
// 声明开启 Swoole 运行模式
\Gene\Application::setRuntimeType('swoole');

$http = new swoole_http_server("0.0.0.0", 9501);

$http->on("request", function ($request, $response) {
    // 注入请求上下文快照
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
        // 显式清理协程请求资源与上下文
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

<a id="性能与容量"></a>
## 📊 性能与容量

Gene 秉持的性能设计哲学是**极致缩短框架内部路径、按需高效复用进程内状态并最大限度减少外部 I/O 往返**：

| 关键场景 | 优化机制 |
|:---|:---|
| 🌐 **路由与调度** | 纯 C 语言哈希与树匹配、支持预编译配置缓存、Controller/Hook 直接跳转分发 |
| 💾 **配置与缓存** | Worker 进程内共享配置/路由缓存、业务缓存容量硬约束、近似 LRU 淘汰、TTL 自动化与批量查询 API |
| 🗄️ **数据访问层** | 高性能 PDO 查询构建器、批量写入、upsert 合并、长连接复用、单语句原子算术更新 |
| ⚡ **Swoole 常驻模式** | 协程上下文轻量复用、智能数据库与 Redis 连接池、纯非阻塞协程 HTTP 客户端 |
| 📈 **全局可观测性** | `Gene\Monitor::stats()` 实时汇聚请求吞吐、缓存命中率、协程上下文分布及连接池健康指标 |

> 📌 **生产建议**：实际性能表现取决于服务器硬件、PHP/Swoole 版本、操作系统内核参数及具体业务逻辑。建议在目标生产环境中基于真实路由与依赖拓扑开展压测，同时密切监控延迟分位数、错误率、RSS 内存占用与连接池等待排队情况。

---

<a id="稳定性与生产运行"></a>
## 🛡️ 稳定性与生产运行

### 🌐 PHP-FPM
- ⏱️ **标准生命周期**：严格遵循 PHP 标准请求模型，请求终结时 Zend 引擎兜底回收资源。
- 📦 **隔离稳定易维护**：天生具备进程级故障隔离能力，适配成熟的传统基础设施与无状态容器部署。
- 🔄 **资源复用优化**：结合持久连接（Persistent PDO）与常驻配置预热，大幅消除重复初始化瓶颈。

### ⚡ Swoole
- 🧬 **上下文隔离与回收**：请求数据按协程严格隔离，提供 `Application::cleanup()` 手动与可选自动清理双保险。
- 🏊 **健壮的连接池治理**：数据库与 Redis 连接池提供容量约束、请求排队超时、空闲连接回收与事务泄漏保护。
- 📊 **精细化监控巡检**：常驻 Worker 需持续观测 RSS 内存增长曲线、并发上下文存量、连接池排队与接口错误率。
- 🧪 **严格准入验收**：新版本上线前务必在目标 PHP/Swoole 运行栈上通过全量测试集与连续稳定性压力测试（Soak Test）。

---

<a id="生产案例"></a>
## 🏢 生产案例

已在多家大型机构与高并发工业级生产场景中稳定运转：

- 🎓 **湖北省教育用户认证中心**：承载全省数百万师生与教育机构的核心统一认证登录与权限分发入口
- 🛒 **尚动电子商务平台**：服务千万级交易流水的高并发电商业务与营销中台
- 🏗️ **生材网**：国内领先的工程材料与供应链数字化 B2B 综合交易平台

---

<a id="技术支持"></a>
## 💬 技术支持

- 📖 **官方文档**：[https://www.1xm.net/](https://www.1xm.net/)
- 🐛 **问题反馈 (Issues)**：[GitHub Issues](https://github.com/sasou/php-gene/issues)
- ✉️ **技术交流邮箱**：[zaipd@qq.com](mailto:zaipd@qq.com)

---

<a id="相关链接"></a>
## 🔗 相关链接 / Links

- 🌐 **官方主页**：[https://www.1xm.net/](https://www.1xm.net/)
- 📦 **PHP 5 遗留版本**：[php-gene v2.1.0 (PHP 5.x)](https://github.com/sasou/php-gene)
- 🪟 **Windows 专用发布**：[php-gene-for-windows](https://github.com/sasou/php-gene-for-windows)
- 📘 **详细配置指南**：[CONFIGURATION.md](docs/CONFIGURATION.md)

---

<div align="center">

<h3>Gene Framework</h3>
<p><em>简单编码，优雅生活！ / Simple Coding, Elegant Life!</em></p>

[![License](https://img.shields.io/badge/License-PHP%203.01-green.svg?style=flat-square)](http://www.php.net/license/3_01.txt)
[![Author](https://img.shields.io/badge/Author-Sasou-blue.svg?style=flat-square)](mailto:zaipd@qq.com)
[![GitHub stars](https://img.shields.io/github/stars/sasou/php-gene?style=flat-square&logo=github)](https://github.com/sasou/php-gene/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/sasou/php-gene?style=flat-square&logo=github)](https://github.com/sasou/php-gene/network)

<br><br>

<a href="https://info.flagcounter.com/AEYx"><img src="https://s11.flagcounter.com/count2/AEYx/bg_FFFFFF/txt_000000/border_CCCCCC/columns_2/maxflags_10/viewers_0/labels_1/pageviews_1/flags_0/percent_0/" alt="Flag Counter" border="0"></a>

</div>
