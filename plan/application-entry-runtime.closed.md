# Application 入口与 Swoole 请求生命周期收口

> Gene 版本基线：6.1.x。  
> 代码证据：典型应用的 FPM 与 Swoole 入口重复环境映射、应用装载和 request-id 配置；Swoole 每请求手工完成九参数 `Request::init()`、Response 绑定、输出缓冲、异常处理与 `cleanup()`，已经出现 raw body 调用错误和 Content-Type 覆盖。  
> 定位：补齐无业务语义的运行时适配能力，减少入口样板并固化常驻进程正确性；不把 Swoole Server 配置、业务异常信封和部署策略放进扩展。
>
> **实施记录（2026-09-12，随 6.2.3 发布）**：P0 两项、P1 gray 映射、P1 `bootstrap()` 与 P2 显式 Pool 编排全部落地（后两项按用户指示不受「≥3 真实样本」前置限制）。
>
> **已实现**：
> - `Request::initSwoole(object $request)`（`src/http/request.c`）：抽出 `init()` 主体为 `gene_request_init_bags()` 共享体（含无条件 JSON/raw 失效 + `GENE_REQUEST_ATTR_RAW` 删除前缀）；`gene_request_init_swoole()` 经 `zend_read_property` 静默读取 get/post/cookie/server/files/header 六属性，缺失/非数组/引用一律置显式空数组；`rawContent()` 用函数表显式查找 + `zend_call_known_function` 调一次——方法缺失抛可捕获 `Error`（`zend_call_method` 对缺失方法是不可捕获 E_ERROR，会杀死 worker），非字符串返回按空请求体处理；袋值在 rawContent 成功后才提交，失败不留半初始化请求；ENV 永不注入；REQUEST 沿用 init() 的 GET+POST 自动合并。
> - `Application::handleSwoole($request, $response, $options = [])`（`src/app/application.c`）：waitWorkerReady → initSwoole → `gene_application_bind_response`（setResponse 共享体）→ 本层 `php_output_start_internal` buffer → `zend_call_known_function(run)` → Throwable 边界 → 收敛本层输出（业务泄漏的嵌套 buffer 逐级 end 收敛进本层，入口前的外层 buffer 不动）→ 仅当 Swoole 响应仍可写时 `gene_response_end(输出)` → `gene_application_cleanup_ctx(cleanup_gc)`。
> - 收口判断**只看 Swoole 侧 `isWritable()`**，不看 `response_ended`：`Response::json()` 只 `php_write` 进 buffer 并置 ended 标记、不调 Swoole `end()`，若以 ended 为闸门 JSON body 会丢失；而已 `end/redirect/sendFile` 的请求 `isWritable()` 天然为 false，杜绝二次 end。
> - options：`cleanup_gc`（默认 false）与 `catch`（`function(\Throwable $e, $request, $response)`）。未配 catch 时记 `Gene\Log::exception()` 并在响应可写时补 `status(500)` + `end(已缓冲输出)`——不吞业务已产出输出、不内置 `/50x.html` 等业务信封；catch 再抛的异常向外传播，cleanup 照常执行；非 callable catch 抛 `ValueError`。
> - 新增全局 `swoole_handle_depth`：抑制 `run()` 内降级 auto-cleanup（旧 Swoole 无 `Coroutine::defer` 路径），cleanup 时机由 handleSwoole 独占；`env_fallback_warned` 支撑 unknown env 每进程告警一次。
> - `getEnvironmentName()`：`3 → "gray"`；未知编号保持回落 `"dev"`（BC）但不再静默——FPM 走 `php_error_docref(E_WARNING)`，Swoole workerStart 内走 `gene_log_diag()` 仅写 error_log（沿用 workerReady 矛盾诊断约定，避免用户错误处理器触发 worker 重启循环）。
> - `demo/public/swoole.php`：入口一行 `$app->handleSwoole($request, $response)`；移除无条件 `Content-Type: text/html` 与固定 `/50x.html` 跳转；worker 生命周期改用 `bootstrap()` + `pools()`/`startPools()`/`stopPoolTimers()`/`closePools()`，`workerReady()` 保留在应用层。
> - `Application::bootstrap($appRoot, $confDir, $options)`（`src/app/application.c`）：FPM/Swoole 共享装载收口，等价于 `autoload` → `load(router)` → `load(config)` → `setMode(mode ?? 1, debug)`；`router`/`config` 文件名支持 `{env}` 占位符经 `getEnvironmentName()` 展开；`debug_envs` 命中环境 debug=1；全程经 `zend_call_known_function` 调本类现有方法，不复制装载逻辑。不含 request-id/webscan/时区/池名/业务信封。
> - 显式 Pool 编排（`src/app/application.c`）：`pools($decls)` 登记 `name => ['driver' => 'db'|'redis', 'component' => config键, 'params' => array?]` 到 worker 级全局表 `GENE_G(pool_decls)`（RSHUTDOWN 释放）；`startPools()` FPM 返回 false、逐声明预检 config 存在性（缺失抛 `Exception`、不留半初始化/timer）、经 `zend_call_known_function` 调 `Pool::create`/`RedisPool::create`、幂等且失败声明可重试；`stopPoolTimers()`/`closePools()` 只作用于已启动驱动族，`closePools()` 复位 started 标记允许重入。不扫描配置猜类型，不注册 Swoole 回调。
> - 测试 `test/SwooleEntryTest.php`（27 断言，duck-typed mock，无需真实 Swoole）：覆盖六袋提取、缺失/非法属性 → `[]`、rawContent 缺失抛 Error、JSON body + `input()` 合并、REQUEST 合并、小写 `request_method/request_uri` 快路径、两次 initSwoole 间缓存失效、handleSwoole 输出缓冲/响应绑定/成功与异常 cleanup/异常最小 500 + 缓冲输出/catch 消费与再抛传播/泄漏嵌套 buffer 收敛/直调 `$response->end()` 与 `Response::end()` 不二次 end/redirect 保留 status+Location/`write()` 分块 + 尾部缓冲/`cleanup_gc` 选项/非 callable catch 拒绝、bootstrap {env} 展开与选项校验、pools 声明校验/FPM 拒绝/缺配置明确失败+可重试/未启动可重声明/fail-fast 顺序/stop-close 空转、3→gray 与未知编号回落。
> - 文档同步：`gene-ide-helper/Gene/{Application,Request}.php`（含 `setEnvironment` 原 1-based 注释勘误为 0=dev/1=test/2=prod/3=gray）、`reference.md`、`swoole.md`、`rules/gene-project.mdc`、`AGENTS.md`、`README.md`、`README_EN.md`、`CHANGELOG.md`、`test/TestRunner.php`。
>
> **验证结果（Windows PHP 8.1.30 NTS x64，VS2019，`F:\php-sdk-2.3.0`；openssl + pdo_sqlite 已加载）**：
> - 构建：`EXT gene build complete`（`x64\Release\php_gene.dll`）；`php --ri gene` 显示 `gene version: 6.2.2`。注意 Makefile 必须在 x64 SDK 环境生成（`BUILD_DIR=x64\Release`），此前曾被 x86 环境污染过，需重跑 `config.nice.bat`。
> - 全量 `TestRunner`：**911 passed / 0 failed / 100%**，约 9.2s；`SwooleEntryTest` 27/27。`GENE_TEST_PHP_ARGS` 必须带 `-d extension=openssl`，否则 `LifecycleTest` 的 Crypto 用例环境性失败（非回归）。
>
> **与原方案的偏差**：
> 1. `rawContent()` 不用 `zend_call_method`——缺失方法是不可捕获 E_ERROR；改为函数表查找 + `zend_call_known_function`，缺失时抛可捕获 `Error`，满足"明确失败"且不杀 worker。
> 2. 最终 `end()` 闸门用 Swoole `isWritable()` 而非 `Response::isEnded()`（上文 JSON 语义）；`isWritable` 方法不可解析时按未发送处理交给 `end()` 兜底，无 Swoole 响应对象时回落 `SG(headers_sent)`。
> 3. handleSwoole 的本层 buffer 收敛策略：dispatch 期间业务泄漏的嵌套 buffer 逐级并入本层后整体取回，**不转发给入口前的外层 buffer**——避免嵌套 ob 场景下输出穿透。
> 4. 异常路径保留业务已缓冲输出（`status(500)` + `end($out)`），而非清空重建——与"不吞业务输出"一致；需要自定义错误页的经 `catch` 选项自行 `redirect`/`end`。
> 5. `bootstrap()`/`pools()` 按用户指示落地，放宽「≥3 真实样本」前置约束；`bootstrap()` 的 `{env}` 展开、`debug_envs` 语义严格按 §6.2 等价式实现，未加入样本外选项。
>
> **未实施**：无——P0/P1/P2 全部落地。`default → dev` 未知编号维持 BC 回落，仅加一次告警（非异常）。
>
> **未覆盖（环境限制，待 Linux 执行已就绪的脚本）**：本机无 Swoole 扩展，全部用 duck-typed mock 验证（30 断言）。Linux 侧脚本已补齐：`tools/acceptance/swoole_entry_verify.php`（自包含服务端+协程客户端，`--entry=manual|init|handle` 三入口同路由响应等价、`--soak=N` 断言 `co_contexts_items=0`、`--bench` 出四类路径吞吐/p50/p99/RSS，env 给 DSN/Redis 时 `pools()`/`startPools()`/`stopPoolTimers()`/`closePools()` 走真实路径）；`linux_swoole_verify.sh` 新增 `entry-matrix`（handleSwoole × 四格）、`entry-soak`、`entry-bench-equiv`（三入口 digest 一致性）阶段。待执行项：真实 HTTP 端到端、四格矩阵、10 万 soak、worker reload/exit/stop 无池定时器挂起、三入口性能对比。
>
> **附带发现并已修复**：`Response::sendFile()` 的 wrapper 检查（2026-08-07 批次引入）在 `STREAM_LOCATE_WRAPPERS_ONLY` 下把 `NULL`（纯本地路径返回值）当作拒绝条件，导致所有本地文件被拒——本次 sendFile 用例首次暴露；已改为「解析到任何 wrapper 才拒绝」并在 `response.c` 内注释说明。

---

## 一、结论

按以下顺序实施：

1. **P0：`Request::initSwoole(object $request)`**，统一提取 Swoole 请求属性并正确调用 `rawContent()`。
2. **P0：`Application::handleSwoole(object $request, object $response, array $options = [])`**，统一绑定、派发、输出和清理边界。
3. **P1：补齐 `Application::getEnvironmentName()` 的 `gray` 映射**，消除应用重复 switch 和环境语义分叉。
4. **P1：评估轻量 `Application::bootstrap()`**，只收口 autoload、路由和环境配置装载，不承载业务策略。
5. **P2：评估显式声明的 Pool 生命周期编排**；不扫描配置并猜测连接池类型。

其中 P0 同时解决正确性和热路径重复；P1/P2 主要改善可维护性，不能包装成显著性能提升。

---

## 二、代码证据与现状

### 2.1 FPM 入口

典型入口执行：

```php
$envType = \Gene\Application::getEnvironment();
// PHP switch: dev / test / prod / gray

\Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->load("config.ini.{$env}.php", CONF_DIR)
    ->setMode(1, $debug)
    ->requestId($requestIdConfig)
    ->webscan(...)
    ->run();
```

现有 `Application::load()` 已有文件缓存与有效期检查，FPM 入口不是无条件重复完整解析。环境 switch 和链式调用的主要问题是 FPM/Swoole 配置漂移，而不是 PHP opcode 成本。

当前 `getEnvironmentName()` 只返回 `dev/test/prod`，其他编号回落为 `dev`；典型应用却把其他编号映射为 `gray`，无法直接复用该 API。

### 2.2 Swoole Worker 初始化

典型 `workerStart` 重复 FPM 的环境映射和装载链，随后：

```php
\Gene\Pool::create('dbPool', 'db');
\Gene\Cache\RedisPool::create('redisPool', 'redis');
\Gene\Application::getInstance()->workerReady();
```

这部分总体正确：`workerReady()` 已幂等执行 Memory/cache reserve、冻结和 request-context pool 预热，不应再新增独立 prewarm 调用。

### 2.3 Swoole 请求热路径

应用当前需要手工实现：

```php
\Gene\Application::waitWorkerReady();
\Gene\Request::init($get, $post, $cookie, $server, null, $files, null, $headers, $raw);
\Gene\Application::setResponse($response);
ob_start();
try {
    \Gene\Application::getInstance()->run();
} catch (\Throwable $e) {
    // log / fallback
} finally {
    $out = ob_get_clean();
    \Gene\Application::cleanup();
}
$response->end($out);
```

已观察到两个实际错误：

1. 把 Swoole 的 `rawContent()` 当作 `$request->rawContent` 属性读取，导致 JSON、PUT/PATCH 和 webhook 验签原文为空。
2. 派发完成后无条件设置 `Content-Type: text/html`，可能覆盖 `Response::json()`、文件、XML 或 SSE 已设置的类型。

此外，FPM 配置了 `webscan()` 而 Swoole bootstrap 遗漏，证明复制入口容易造成策略漂移。

---

## 三、设计原则

1. C 层只接收**无业务语义、跨项目重复、位于请求热路径**的适配逻辑。
2. 不创建或托管 `Swoole\Http\Server`；监听地址、worker 数、静态目录、pid、reload 和 max_request 仍由应用配置。
3. 采用 duck typing 接收 object，不在编译期依赖 Swoole 头文件或类符号。
4. `Request::post()` 继续只表示表单 POST；不隐式把 JSON 改写进 POST。
5. raw body 必须保持字节不变，并与 `Request::json()` / `input()` 共享现有请求级缓存。
6. 所有请求上下文必须在成功、路由未命中、业务已结束响应和异常路径释放。
7. 不覆盖业务设置的 status、header 或 Content-Type。
8. 不在默认热路径执行 `gc_collect_cycles()`；只有明确 option 才调用 `cleanup(true)`。
9. 现有手工 `Request::init()`、`setResponse()`、`run()`、`cleanup()` 永久保留。
10. `run()` 的降级 auto-cleanup 仍作为兜底；标准适配路径继续显式 cleanup。

---

## 四、P0：`Request::initSwoole()`

### 4.1 API

```php
public static function initSwoole(object $request): bool;
```

典型使用：

```php
\Gene\Request::initSwoole($request);
```

### 4.2 提取规则

| Gene 请求袋 | Swoole 来源 | 缺失/非法类型 |
|---|---|---|
| GET | `$request->get` | `[]` |
| POST | `$request->post` | `[]` |
| COOKIE | `$request->cookie` | `[]` |
| SERVER | `$request->server` | `[]` |
| ENV | 不注入 | `null` |
| FILES | `$request->files` | `[]` |
| REQUEST | GET+POST，沿用现有 `init()` 语义 | 自动合并 |
| HEADER | `$request->header` | `[]` |
| RAW | `$request->rawContent()` | 方法不存在时 `''` 或抛错，实施前基准决定 |

推荐在方法不存在时抛 `ValueError`/`RuntimeException`，避免把错误适配静默伪装成空 body。空请求体由 `rawContent()` 正常返回空字符串表达。

### 4.3 实现约束

- 复用 `gene_request_set_server_val()`，保留大小写兼容的 `request_method` / `request_uri` 快路径。
- 复用现有 `Request::init()` 内部 helper，避免 C 内复制两份请求袋设置逻辑。
- 对 Swoole 属性只做 shallow share，与当前 server/header 优化一致；上下文销毁只减引用。
- `rawContent()` 每请求只调用一次。
- 不调用 `json_decode()`，保持按需解析。

### 4.4 验收

1. GET/POST/cookie/server/files/header 均能读取。
2. `request_method`、`request_uri` 小写键可直接 `run()`。
3. JSON body 经 `Request::json()` / `input()` 正确读取，且 raw 字节不变。
4. 空 body、缺失可选属性和空数组正常工作。
5. 缺失 `rawContent()` 的伪对象按最终约定明确失败。
6. 两个并发协程请求袋、raw body 和 JSON cache 不串请求。
7. 100k 请求 soak 后 `co_contexts_items=0`，context pool 不超限。

---

## 五、P0：`Application::handleSwoole()`

### 5.1 API

```php
public function handleSwoole(
    object $request,
    object $response,
    array $options = []
): bool;
```

第一版 options 只保留可稳定定义的运行时选项：

```php
[
    'cleanup_gc' => false,
    'catch' => null,
]
```

`catch` 为可选 callable：

```php
function (\Throwable $e, object $request, object $response): void
```

未配置时由框架记录 `Gene\Log::exception($e)`，并在响应仍可写且尚未结束时返回最小 500 响应。框架不猜测 HTML/JSON 业务信封，也不重定向固定 `/50x.html`。

### 5.2 等价生命周期

```text
waitWorkerReady()
Request::initSwoole($request)
Application::setResponse($response)
记录当前 ob level 并启动本层 buffer
run()
捕获 Throwable
仅收集本层 output
检查 Response::isEnded()/isSent()
未结束时 response->end(output)
finally: 恢复 buffer + cleanup(cleanup_gc)
```

### 5.3 输出语义

- Controller/Hook 已调用 `Response::end/json/sendFile/redirect`：不得二次 `end()`。
- 只 `echo` 的传统 Controller：收集 buffer，并在未结束时作为 body 发送。
- 空输出且未结束：发送空 body，不自行构造业务 JSON。
- 不设置默认 Content-Type；视图、Controller 或 Response API 自行决定。
- 异常发生前已经结束响应：只记录异常并 cleanup，不尝试改写已发送内容。
- `isWritable()` 不可用时复用 `Response::isSent()` 的兼容策略。

### 5.4 输出缓冲约束

不能简单假设只有一层 `ob_start()`：

1. 进入时记录 `ob_get_level()`。
2. 只关闭本方法创建的 buffer。
3. 业务多开 buffer 时，按明确策略收敛到入口层；不得清除进入前已有的外层 buffer。
4. 异常路径与正常路径执行相同恢复逻辑。
5. 增加嵌套 buffer、Hook clearBefore/clearAfter 和业务提前 end 的组合测试。

### 5.5 异常边界

C 方法直接捕获并消费 `Throwable` 的实现复杂度较高。实施前比较两种方式：

- **方案 A：C 层完整 handle**：热路径调用最少，但异常、callable 和 ob 恢复实现风险较高。
- **方案 B：扩展提供 `Gene\Swoole::handle()` 内部桥接 PHP callable**：实现安全，但用户态调用更多。

若 C 层无法可靠恢复 `EG(exception)` 和输出缓冲，先落地 `initSwoole()`，`handleSwoole()` 暂以 Gene 随附 PHP helper/demo 验证语义，禁止为少量 opcode 引入生命周期缺陷。

### 5.6 验收矩阵

| 场景 | 预期 |
|---|---|
| echo HTML | body 正确，仅 end 一次 |
| `Response::json()` | JSON header/body 保留，不二次 end |
| redirect | status/location 保留，不补 body |
| sendFile | 不二次 end |
| SSE/write | 不缓冲覆盖流式输出 |
| Hook::abort/respond | Controller 不执行，响应不重写 |
| 路由未命中 | 沿用现有 Router/Response 语义 |
| Controller 抛异常 | catch 或默认 500，必 cleanup |
| catch 再抛异常 | 必 cleanup，异常向外传播 |
| 嵌套 output buffer | 入口前 buffer 不受损 |
| 并发协程 | Request/Response/Context/DI 隔离 |

---

## 六、P1：环境名称与 bootstrap

### 6.1 `getEnvironmentName()`

修正固定映射：

```text
0 => dev
1 => test
2 => prod
3 => gray
```

未知值不应静默回落为 dev。候选策略：

- 返回 `gray`，保持现有典型应用兼容；或
- 抛清晰异常，要求先 `setEnvironment()` 为合法编号。

第一版优先兼容既有编号 3；自定义环境名称暂不进入 C 层。

### 6.2 `bootstrap()` 候选 API

```php
$app->bootstrap(APP_ROOT, CONF_DIR, [
    'router' => 'router.ini.php',
    'config' => 'config.ini.{env}.php',
    'mode' => 1,
    'debug_envs' => ['dev'],
]);
```

只等价于：

```php
$app->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->load('config.ini.' . $app->getEnvironmentName() . '.php', CONF_DIR)
    ->setMode(1, $debug);
```

> 实施注记：`$debug` 即 `setMode` 的 exception_type——环境未命中
> `debug_envs` 时**不注册异常处理器**（未捕获异常直接 fatal，而非渲染
> Gene Exception 页）。旧式 `setMode(1,1)` 恒开语义的等价写法是列出
> 全部非 prod 环境 `['dev','test','gray']`；demo 各入口已按此修正
> （扩展默认 `gene.run_environment=1` → env=`test`，仅写 `['dev']`
> 会导致默认环境下异常无人接管）。

### 6.3 不进入 bootstrap 的内容

- request-id 的 trust/header 策略；
- webscan 白名单和业务拒绝响应；
- 时区；
- Swoole host/port/server settings；
- Memory 业务预热；
- DB/Redis pool 名；
- 业务异常页面或 JSON envelope。

这些应由应用显式配置，或放进应用自己的共享 bootstrap 文件。若真实样本不足 3 个，`bootstrap()` 不进入 C 层，只修复 `getEnvironmentName()` 并推荐应用抽共享函数。

---

## 七、P2：Pool 生命周期编排

不建议扫描所有 DI config，依据 `pool` 字段自动猜测 Db 或 Redis Pool 类型。该行为会引入隐式连接、启动失败策略和组件类型判断。

如后续至少三个应用重复，可考虑显式 API：

```php
$app->pools([
    'dbPool' => ['driver' => 'db', 'component' => 'db'],
    'redisPool' => ['driver' => 'redis', 'component' => 'redis'],
]);

$app->startPools();
$app->stopPoolTimers();
$app->closePools();
```

或者让 `workerReady()` / `workerStop()` 接收已声明的 pool 集合。无论采用哪种形式：

- FPM 下必须 no-op 或明确拒绝；
- 重复启动/停止必须幂等；
- 部分 pool 创建失败不能留下 timer 或半初始化注册项；
- 不替应用注册 Swoole worker 回调。

当前只有一个典型应用证据，暂列 P2 调研，不直接实施。

---

## 八、迁移后的典型入口

### 8.1 FPM

```php
<?php

date_default_timezone_set('Asia/Shanghai');
define('APP_ROOT', dirname(__DIR__) . '/application');
define('CONF_DIR', dirname(__DIR__) . '/config');

$app = \Gene\Application::getInstance();
$env = \Gene\Application::getEnvironmentName();

$app->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->load("config.ini.{$env}.php", CONF_DIR)
    ->setMode(1, $env === 'dev' ? 1 : 0)
    ->requestId($requestIdConfig)
    ->webscan(1, 'cms', $illegalAccessHandler)
    ->run();
```

### 8.2 Swoole

```php
$http->on('workerStart', static function () use ($app, $requestIdConfig, $illegalAccessHandler) {
    $env = \Gene\Application::getEnvironmentName();

    $app->autoload(APP_ROOT)
        ->load('router.ini.php', CONF_DIR)
        ->load("config.ini.{$env}.php", CONF_DIR)
        ->setMode(1, $env === 'dev' ? 1 : 0)
        ->requestId($requestIdConfig)
        ->webscan(1, 'cms', $illegalAccessHandler);

    \Gene\Pool::create('dbPool', 'db');
    \Gene\Cache\RedisPool::create('redisPool', 'redis');
    $app->workerReady();
});

$http->on('request', static function ($request, $response) use ($app) {
    $app->handleSwoole($request, $response);
});
```

Server settings、workerExit 和 workerStop 仍由应用保留。

---

## 九、实施拆分

### 阶段 A：先修应用与建立回归

1. 典型应用把 `$request->rawContent` 修为 `$request->rawContent()`。
2. 删除请求结束时无条件 HTML Content-Type。
3. Swoole bootstrap 补齐与 FPM 相同的 `webscan()`。
4. 添加 JSON body、JSON Content-Type 和 cleanup 回归脚本，证明当前缺陷及修复。

### 阶段 B：`Request::initSwoole()`

1. 提取共享 init helper。
2. 实现 object 属性读取与 `rawContent()` 调用。
3. 增加 arginfo、方法表和 IDE helper。
4. 增加单请求、异常输入和双协程隔离测试。
5. 更新 README、CHANGELOG 和 AI reference。

### 阶段 C：`handleSwoole()`

1. 先以测试固定 response-ended、buffer、异常和 cleanup 语义。
2. 实现最小 options，不提前扩展 status/header/body DSL。
3. 跑 Linux Swoole 四格矩阵和 100k soak。
4. 基准对比手工入口，分别报告吞吐、p50/p99、分配和 RSS；不只报告源码行数。

### 阶段 D：非热路径便利能力

1. 修正 environment name 映射。
2. 收集至少三个项目后再决定 `bootstrap()` 是否进 C。
3. Pool 编排继续保持候选，除非重复证据满足 `plan/README.md` 准入规则。

---

## 十、验证与准入

### 10.1 功能验证

- Windows：全量 `TestRunner`，不得降低现有通过数。
- Linux：PHP 8.1+、Swoole 已加载，运行真实 HTTP 请求而不是纯 mock。
- request-id、Hook abort/respond、JSON input、redirect、sendFile、SSE 回归全部通过。
- FPM 现有入口和手工 Swoole 入口保持兼容。

### 10.2 常驻进程验证

- `swoole_getcid_capi=0/1` × `route_precompile=0/1` 四格矩阵。
- 手动 cleanup 与 `handleSwoole()` 各跑 100k 请求/协程。
- `co_contexts_items=0`，无跨请求 Context/DI/Response 泄漏。
- worker reload、workerExit、workerStop 后无 Pool timer 阻止退出。

### 10.3 性能验证

至少比较：

1. 当前九参数手工入口；
2. `Request::initSwoole()` + 手工 dispatch；
3. 完整 `handleSwoole()`。

测试空 Controller、echo、JSON 和一次 DI 获取四类路径。只有在结果稳定后才能声明性能提升；否则将收益表述为正确性和入口维护成本下降。

### 10.4 文档同步

实现后同步：

- `gene-ide-helper/Gene/Application.php`
- `gene-ide-helper/Gene/Request.php`
- `gene-ai-helper/skills/gene-framework/reference.md`
- `README.md` / `README_EN.md`
- `CHANGELOG.md`
- Swoole demo

---

## 十一、明确不做

- 不封装 `Swoole\Http\Server` 创建和启动。
- 不把监听地址、worker 数、daemon、pid、静态目录写入 Gene API。
- 不自动选择 HTML 或 JSON 异常信封。
- 不自动把 JSON 合并进 POST。
- 不替应用决定是否信任外部 request-id。
- 不默认每请求 GC。
- 不隐藏连接池创建失败。
- 不为了减少入口行数改变现有 Router、Response ended 或 cleanup 语义。
