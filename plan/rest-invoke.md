# Gene 框架级 REST 互调

> 基线：Gene **6.1.x（已落地，2026-08-23）**。`Gene\Http` / `Context` / `cleanup()` 已按 [lifecycle-completeness.md](lifecycle-completeness.md) 落地。  
> 定位：补齐**进程内隔离调用**与**命名出站 REST**，同一套 API 覆盖 **FPM/CLI** 与 **Swoole 协程**，请求级生命周期、无业务语义。  
> 本文只写扩展：`src/`、ide-helper、`test/`、`demo/`、`gene-ai-helper`。不写业务仓库迁移。

---

## 一、为何立项

业务侧反复出现同一模式（证据在典型用法，不把那些项目的类下沉到扩展）：

1. **同进程优先**：`api_path` 能落到本进程 `Api\{Class}::{action}` 时直接调方法，否则才 HTTP。
2. **本地调用污染入站 Request**：用 `Request::init` 覆盖 GET/POST/FILES，调用结束不恢复；嵌套互调串参；Swoole 下还会脏到下一请求。
3. **出站样板**：每项目自研 curl，FPM 尚可、Swoole 会堵 worker；无请求内 keep-alive、无统一 multipart。
4. **共享可变客户端**：DI `instance=true` 上写 `serviceName`，协程交错不安全。

框架已有：`Http::request`（双后端）、`Router::dispatch`、`Controller::forward`（不切 Request）、请求级 `gene_request_context`。缺的是 **Request 栈**、**隔离 Invoke**、**命名 Rest 句柄**。

```mermaid
flowchart TD
  app[App_code]
  rest[Gene_Rest]
  inv[Gene_Invoke]
  stack[Request_stack]
  http[Gene_Http]
  app --> rest
  rest -->|class_exists| inv
  inv --> stack
  stack --> dispatch[Router_dispatch]
  rest -->|else_or_force| http
```

---

## 二、原则

1. **无业务语义**：不做注册中心、app_key、签名、`api_param` 映射、业务信封 `code`、httpmq/队列、熔断、服务发现。
2. **一套 API，两种运行时**：`runtime_type < 2`（FPM/CLI）与 `>= 2`（Swoole）行为一致；Swoole **禁止**阻塞 `curl_exec`。
3. **状态一律请求/协程级**：进 `gene_request_context`；FPM 靠 RSHUTDOWN；Swoole 靠每请求 `cleanup()`。禁止模块全局、禁止 worker 级 Rest 游标。
4. **本地路径不发 HTTP**；远程路径只走已有 `Gene\Http`。
5. **可覆盖应用互调，但不实现应用**：能力清单以「应用还能否自己写薄门面」为验收，不在扩展里复刻网关。

---

## 三、能力必须覆盖的应用需求（反推，非实现清单）

应用门面（`init(app)->call/sync/uploadFile`、Redis 节点、鉴权）仍由应用写。框架需让门面只剩「查元数据 + 调下面 API」：

| 应用需要 | 框架能力 |
|----------|----------|
| 同机 `Api\*` 热路径 | `Invoke::local` / `Rest::call` 本地枝 |
| 本地结束后外层 Controller 仍读原 Request | Request 快照栈，异常也 restore |
| 跨机 `base_url + path` | `Rest::http` → `Http::request` |
| JSON POST / 上传文件 | `json` + **multipart `files`** |
| 超时 / SSL / 内网 HTTP | 调用级 + 命名服务默认值 |
| 透传排障 ID | 可选合并入站 `X-Request-Id` / Context `request_id` |
| `$this->rest` DI 单例 + 连续 `init(A)/init(B)` | **不可变 proxy**，无共享可变当前服务 |
| FPM 短请求 + Swoole worker 复用 | 句柄与栈随 ctx 销毁 |
| 异步 `sync` / 计划任务 | **不做**；应用队列消费时再调同一 `call` |
| 网关鉴权 / 注册表 | **不做** |

覆盖即止：应用能删掉自研 curl 与 `Request::init` 覆盖，不必改扩展才能接队列。

---

## 四、运行时与泄漏约束

### 4.1 双模式

| | FPM / CLI (`runtime_type < 2`) | Swoole (`>= 2`) |
|--|-------------------------------|-----------------|
| 出站 | PHP `curl_*`，`ctx->http_curl` 请求内 `curl_reset` 复用 | `Swoole\Coroutine\Http\Client`；`keep_alive` 按 `host:port:ssl` 存在 **当前协程 ctx** |
| 本地 | 同；Request 栈在当前 request ctx | 同；ctx 必须是协程隔离（已有模型） |
| 结束 | RSHUTDOWN → `free_fields`：弹空栈、dtor curl | 请求末 **`Application::cleanup()`**：同上 + 协程 Client 全部释放 |
| 禁止 | 把 Rest「当前服务」放模块全局 | 裸 curl、跨请求/跨协程复用 Client、本地调用后不 restore |

### 4.2 内存与安全（必须测）

- Request 栈深度 **上限 8**；超限失败，不无限 push。
- `snapshot`/`restore` 成对：`zend_try` 统一出口；`free_fields` 若栈非空则循环 restore（防异常/提前 return）。
- snapshot 只拷贝 get/post/files/request/header/raw；**不替换** cookie/server（本地互调不是伪造整请求）。
- `Invoke::local` **每次 new Controller**（`Factory::create(..., false)`），禁止单例 Controller 残留属性。
- 与 `forward` 深度分开计数或共用上限（建议 Invoke 自计，超限抛 Exception，与 Http 一致）。
- `Http` 调用期间 `http_body_buf` / `http_header_buf` 仍仅栈上有效；嵌套 `Http::request` 若出现，须可重入或明确禁止（建议：**禁止 Rest 本地枝内再重入同一 curl 回调脏 buf**；本地枝不走 Http，远程枝串行即可）。
- DI `instance=true` 的 `Gene\Rest`：**配置只读**；`use('name')` 返回新 proxy，不写回共享实例。

### 4.3 并发

- 一请求内多次 `http`：FPM 复用一只 curl；Swoole `keep_alive=true` 复用同 peer Client。
- 不做跨请求连接池（与现网 Http 文档一致）。
- POST 默认 **不重试**（防写放大）；GET/HEAD 仍用现有 `retry` 规则。

---

## 五、API 规格

### 5.1 `Gene\Request` 栈

```php
\Gene\Request::snapshot(): int;   // 返回新深度
\Gene\Request::restore(): bool;   // 栈空则 false
\Gene\Request::scope(array $get, array $post, ?array $files = null, ?array $request = null): void;
```

- `scope` 只改入参袋；`$request === null` 时按现 `init` 规则合并 get+post。
- 文档：业务互调应走 `Invoke`/`Rest`，不要自己 `init` 整包覆盖。

### 5.2 `Gene\Invoke`

```php
\Gene\Invoke::local(string $class, string $action, array $params = [], array $files = []): mixed
```

1. 深度 +1，超限抛 Exception。  
2. `snapshot` + `scope($params, $params, $files, $params)`。  
3. `Router::dispatch($class, $action, [])`（动作从 Request 取参，兼容现有 Api Controller）。  
4. `finally`：`restore`，深度 -1。  
5. 只返回 action 返回值，不改全局输出。

与 `Controller::forward`：**forward** 把 `$params` 当方法参数、不切 Request；**Invoke** 切 Request。两者都保留。

### 5.3 `Gene\Rest`

构造只收**无业务**配置：

```php
$config->set('rest', [
    'class' => '\Gene\Rest',
    'params' => [[
        'timeout' => 5,
        'connect_timeout' => 2,
        'ssl_verify' => true,
        'keep_alive' => true,
        'headers' => ['Accept' => 'application/json'],
        'pass_request_id' => true,
        'services' => [
            'user' => [
                'base_url' => 'http://127.0.0.1:8081',
                'local' => 'Api\\',   // 空则只 HTTP
                'timeout' => 8,
            ],
        ],
    ]],
    'instance' => true,
]);
```

```php
$rest->use('user'): Gene\Rest;   // 新 proxy，共享只读配置
$rest->local(string $class, string $action, array $params = [], array $files = []): mixed;
$rest->http(string $method, string $path, array $options = []): array;
$rest->call(string $class, string $action, array $params = [], array $options = []): mixed;
```

- `http`：`base_url` + path（path 必须以 `/` 开头）；`options` 对齐并扩展 `Http::request`（`json`/`body`/`headers`/`files`/`timeout`…）。返回 `{status, headers, body}`；`options['decode']===true` 时对 body 做 `Gene\Json` 解码（失败不吞，抛或 status 保留 + body 原样，建议抛）。
- `call`：`class_exists` 且 `method_exists`（或可 dispatch）→ `local`；否则要求 `options['path']` + 当前服务 `base_url` 走 `http`。不猜测 URL。
- `pass_request_id`：若 Context/`Request` 已有 id，合并到出站 header，不覆盖调用方已设的同名头。
- **无** `__call`、无 `init(app_key)`：避免和业务门面同名抢语义。

实现：C 类或 C 栈 + 薄 PHP 均可；热路径（栈、dispatch、Http）必须在 C。Rest 配置解析可 C。

### 5.4 `Gene\Http` 增量

在现有 `request($options)` 上增加 **`files`**：`array<string, string|array>`（路径或 `['tmp_name','name','type']`），与 `json` 互斥，走 multipart。FPM：`CURLFile`；Swoole：Client 等价上传 API。缺此则应用上传仍会自造 curl。

---

## 六、文件与落地顺序

| 顺序 | 项 | 位置 |
|------|----|------|
| 1 | Request 栈 + `free_fields` 排空 | `src/http/request.c`、`src/gene.c`、`gene.h` |
| 2 | `Gene\Invoke` | 新 `src/http/invoke.c` 或挂 `router.c` |
| 3 | `Gene\Rest` + 只读 proxy | 新 `src/http/rest.c` |
| 4 | Http `files` | `src/http/http.c` |
| 5 | ide-helper、`reference.md`、SKILL 出站约定 | 禁止互调再裸 curl |
| 6 | 测试 / demo | 见下 |

**测试（必须双模式意识）**

- `test/RestInvokeTest.php`：外层已 `init` 真实入站 → 内层 `local` 改参 → 返回后外层 Request 不变；内层抛异常同样还原；深度超限。
- `test/HttpClientTest.php`：multipart；`decode` 行为。
- Swoole：有环境则跑「调用后 `cleanup()`，下一协程 Request/Http 句柄为空」；无环境 SKIP，禁止假绿。
- 嵌套 8 层 + 中途 Exception，无泄漏（Windows 以 PHP 用例为准；ASAN 跟现有 audit 节奏）。

**demo**：`Api\Ping::pong` + CLI `Rest->use('demo')->call(...)`，配置写死 `base_url`，无 Redis。

---

## 七、刻意不做

注册中心、加权选节点、鉴权、业务 code、异步投递、跨请求连接池、熔断、HTTP/2 专项、并行 fan-out、SSE/MCP 客户端解析、魔术方法服务调用。

---

## 八、与生命周期文档的关系

[lifecycle-completeness.md](lifecycle-completeness.md) §3.1 的 `Gene\Http` 是运输层。本文是其上的 **互调原语**（隔离本地 + 命名客户端）。不重复实现第二套 HTTP 后端。

---

## 九、落地复盘（2026-08-23）

环境：PHP 8.1.30 NTS x64，`phpversion('gene')=6.1.0`，产物 `F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll`。验证入口 `test/RestInvokeTest.php`、`test/HttpClientTest.php`（`-n` + curl + 本 dll）。本机 SDK 为 `F:\php-sdk-2.3.0`（非文档里的 2.6.0）。

### 9.1 结论

| 维度 | 判定 |
|------|------|
| 方案是否科学 | **是**：状态只进 `gene_request_context`；本地切 Request、远程复用 `Http`；Rest 不可变 proxy；无业务语义。 |
| 与规格对齐 | **对齐**，下列偏差已核对且可接受（§9.3）。 |
| 请求级泄漏 | **无已知泄漏**：栈深度封顶 8；`free_fields` 先 `drain` 再拆 `request_attr`；Invoke 成对 restore；Http `http_busy` / buf 指针收口。 |
| FPM/CLI | **已实测**（隔离、异常还原、8 层嵌套超限、proxy、`decode` 失败抛、multipart）。 |
| Swoole | **代码按同一 ctx 模型**；无 Swoole 环境，测试 **SKIP**，不能宣称协程路径已跑绿。 |
| ASAN | **未跑**（跟现有 audit 节奏）。Windows 以 PHP 用例为准。 |

**生产口径**：FPM/CLI 互调与出站 multipart 可用。Swoole 需在 `runtime_type>=2` 下补跑「Invoke 后 `cleanup()`，下一协程栈/Http 句柄为空」。

### 9.2 实现对照

| 规格 | 位置 | 要点 |
|------|------|------|
| Request 栈 | `src/http/request.c`，ctx 字段 `request_stack` | 只拷贝 post/get/files/request/header/raw；`zend_array_dup` 顶层袋；cookie/server 不动 |
| 栈排空 | `gene_request_stack_drain` ← `free_fields` **先于** `request_attr` 回收 | 异常/提前结束也不会把内层袋留到下一请求 |
| Invoke | `src/http/invoke.c` | `gene_factory` 每次 new（非 DI 单例）；深度 `invoke_depth` 与栈分开计 |
| Rest proxy | `src/http/rest.c` | `use()` `object_init_ex` 新对象，只读共享 `config`，不写回 `$this` |
| Http `files` | `src/http/http.c` | 与 `json` 互斥；FPM `curl_file_create`；Swoole `addFile` 且上传时关 keep-alive 复用 |
| 禁嵌套 Http | `ctx->http_busy` / `http_body_buf` | 防 curl 写回调叠脏 `smart_str` |
| 文档 / demo | ide-helper、`reference.md`、SKILL、`demo/application/Api/Ping.php`、`demo/public/rest_invoke.php` | 禁止互调裸 curl / `Request::init` 覆盖 |

### 9.3 相对原文的有意偏差

| 原文 | 落地 | 理由 |
|------|------|------|
| Invoke 用 `zend_try` 统一出口 | **不用** `zend_try`；用户异常走 `EG(exception)`，在 `call` 之后 **必定** `restore_ctx` + `depth--` | PHP 8 用户异常一般不 bailout。套 `zend_try` 反而有把 `EG(bailout)` 指到已返回栈帧的风险。`E_ERROR`/真正 bailout 靠 `free_fields` drain |
| `Router::dispatch($c,$a,[])` | `gene_factory` + `gene_factory_call_1(..., NULL)`（**0 个方法参数**） | PHP 8 对无参 action 传入空数组会 `ArgumentCountError`；规格本意是「从 Request 取参」 |
| 栈 pop 用「元素个数 - 1」当下标 | **按最后一枚真实 key 弹出**，空栈 `zend_hash_clean` | 删掉 packed 下标 0 后 `nNextFreeElement` 变成 1，再 `add_next_index` 插在 1，用 `n-1==0` 会取空槽 → **restore 静默失败、内层袋泄漏到外层**（落地时已踩中并修） |
| `zend_try` 成对 | 用户路径成对 restore；请求结束 drain | 见上 |

### 9.4 内存与生命周期（为何认为无泄漏）

1. **有界**：`GENE_REQUEST_STACK_MAX` / `GENE_INVOKE_DEPTH_MAX` 均为 8，超限抛、不 push。
2. **所有权**：snapshot/scope 对袋 `zend_array_dup` + `setVal` 替换槽位，调用方数组与栈上快照不共享顶层 HashTable（`dup` 是浅拷贝：袋内嵌套 array 仍可能共享内层，互调 payload 一般为扁平标量，可接受）。
3. **成对释放**：Invoke 在 factory 失败、方法缺失、action 抛异常后都 restore；Controller 对象 `zval_ptr_dtor`。超长 action 名 `estrndup` 必 `efree`。
4. **请求边界**：`free_fields` 先 drain（往仍活着的 `request_attr` 回写再丢栈），再回收 attr / `http_curl`；`invoke_depth`、`http_busy` 置 0。Swoole `cleanup()` 走同一条。
5. **Rest**：无模块全局「当前服务」；proxy 只多一个对象 + 共享 config 引用，随 DI/`di_regs` 请求结束释放。
6. **Http**：`files` 建的 `CURLFile`/multipart 数组在 `setopt` 后 `dtor`；Swoole 上传不把 Client 放回 peer map，避免 `addFile` 残留。

**剩余风险（非已证实泄漏）**

- 真正的 executor **bailout**（少见）会跳过 Invoke 函数尾部；依赖 drain。与原文 `zend_try` 目标相同，实现换成边界回收。
- `zend_array_dup` 浅拷贝：若业务在 snapshot 之后原地改**嵌套**袋，快照可能看到改动。`scope` 会整袋替换，Invoke 热路径通常无此问题。
- Linux ASAN、Swoole 实跑未做。
- `HttpClientTest` multipart 路径有一条 PHP `Array to string conversion` 警告（回显仍正确），属 curl 选项边角，未当失败。

### 9.5 验证记录

| 项 | 结果 |
|----|------|
| `RestInvokeTest` | 外层 init → local 改参后外层不变；内层 Exception 还原；`nest` 递归超限抛且外层袋仍在；`use()` 新 proxy；`call` 本地枝；`http`+`decode`；非法 JSON 抛 |
| `HttpClientTest` | GET / POST json / stream / 5xx retry / multipart files+form |
| Swoole 分支 | `SKIP`（无扩展），禁止假绿 |
| Windows 构建 | `config.w32` 已列 `invoke.c` `rest.c`；`configure.js` **未**把新源写进 Makefile，需补 `GENE_GLOBAL_OBJS` / http 编译规则 / `GENE_GLOBAL_OBJS.txt` 后再 `nmake`（干净重配后应核一次） |

### 9.6 应用侧怎么接

门面只保留「查节点 / 鉴权 / 信封」，互调用：

```php
$rest->use('user')->call('Api\\Foo', 'bar', $params);
// 或强制远程
$rest->use('user')->http('POST', '/foo/bar', ['json' => $params, 'decode' => true]);
```

不要 `Request::init` 覆盖入站，不要裸 `curl_exec`。异步/队列消费再调同一 `call`，扩展不提供 `sync`。
