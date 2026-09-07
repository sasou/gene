# Hook 使用驱动：请求策略与派发链增强

> Gene 版本基线：6.1.x。  
> 代码证据：典型常驻进程应用的全局 Hook 同时承担 request-id、语言注入、JSON→POST 兼容；Swoole 入口又重复 JSON Content-Type 检测、解码与合并；约 170 条路由逐条组合身份 Hook 和 `clearBefore/clearAfter`。
>
> **实施记录（2026-09-07）**：已落地请求级 `response_ended`、`Response::isEnded()`、`Hook::abort()`、`Hook::respond()`、redirect/end/sendFile 终止标记和 cleanup 隔离；已实现默认关闭的 `Application::requestId()`（可信 header 校验、生成、Context 与响应头写入），并同步基础 ide-helper/Hook 测试。Windows PHP 8.1 NTS x64 构建通过。复核时确认组级多 Hook 会触及 route leaf 不可变描述符、closure/event 缓存和 PC_DIRECT 三套结构，本轮尚未安全完成，相关 API、快照/基准、Swoole/ASAN/Linux O2/O6 验收仍为待办，不能标记“全部完成”。

## 一、结论

Hook 应只保留业务决策，例如“当前用户是否允许访问”。请求解析、请求上下文初始化、响应终止和组级 Hook 策略属于 Gene 生命周期能力。

建议按以下顺序增强：

1. **P0：完成 `Request::input()` 应用收口，不自动改写 POST。** 该能力已在 C 层实现并缓存 JSON 解码结果；补齐文档、版本状态和迁移测试。
2. **P0：统一 Hook 中止/响应终止语义。** 消除“先输出，再 `return false`，入口仍尝试补默认响应”的组合约定。
3. **P1：组级 Hook 策略。** 让路由组继承命名 Hook 和 before/after 开关，减少逐路由字符串解析与配置重复。
4. **P1：标准 request-id 策略。** 由请求上下文初始化阶段完成可信输入、生成、Context 写入和响应头回写；默认关闭，显式配置启用。
5. **不进入 C 层：** session 用户字段、权限表、登录地址、内网判定、语言业务默认值和业务响应信封。

不建议把多用途“万能 Hook”或认证 DSL 放进扩展。它们会把业务语义固化到 C 层，也不会减少 Redis/数据库交互。

---

## 二、现状与缺口

### 2.1 已有能力

- Router 已把 route/before/after/named Hook 解析成预编译描述符；Swoole 冻结后按 route leaf 缓存，不应推翻当前快路径。
- `Gene\Hook` 子类通过轻量装载执行，不调用构造函数。
- `Request::json()` 与 `Request::input()` 共用请求级解码缓存；`input()` 支持 `application/json`、`application/*+json`，JSON 覆盖 GET/POST，并保留 raw body。
- `Gene\Context`、请求级 DI 和 `Application::cleanup()` 已具备常驻进程隔离；Log 可自动合并 Context 中的 `request_id`。
- before/named Hook 返回 `false` 或 `0` 可中止 Controller；after Hook 返回值当前被忽略。

### 2.2 暴露的问题

1. 应用为兼容 `$this->request()`，在 FPM Hook 中二次调用 `Request::init()`，复制 GET/POST/cookie/server/files/header/raw 多个参数袋；Swoole 入口又独立解码一次 JSON。
2. 请求 ID 需要多次用户态调用：大小写 header 查询、随机生成、DI/Context 写入、响应头写入；每个项目容易产生不同的校验和信任边界。
3. 认证失败通常要“输出/跳转 + return false”。框架只理解 Hook 返回值，不显式表达响应是否已经结束。
4. Route 只接受单个命名 Hook 字符串，并把 before/after 控制编码进 `name@clearAfter`；同一组大量重复，组合身份加载、认证和其他策略时只能继续造复合 Hook。
5. 身份 Hook 为避免重复 session 读取，应用自行在 DI 写 `_loaded` 标记。该优化只有在同一请求链重复读取同一身份时才有收益，且缓存键、合法用户结构均是业务语义。

---

## 三、P0：输入收口与兼容边界

### 3.1 正式推荐 API

```php
$data = $this->input();
$id = $this->input('id', 0);
```

`Controller`、`Hook` 代理和静态 `Request::input()` 保持同一签名。

### 3.2 约束

- 保持 `post()` 只表示表单/运行时传入的 POST；不在 `Application::run()` 隐式 JSON→POST。
- 非法 JSON和 JSON 顶层列表继续抛异常，不能静默伪装成字段缺失。
- FPM/Swoole 都只把原始 header/raw body 传给一次 `Request::init()`；业务入口不再自行 `json_decode()`。
- 迁移期允许应用保留 JSON→POST 兼容适配器，但适配器必须集中一处，最终以控制器改用 `input()` 后删除。
- `rawContent()` 字节不变，保证 webhook 验签。

### 3.3 性能

- JSON 请求每个 request context 最多解码一次；普通表单请求只做一次 Content-Type 快速判断。
- 删除 Hook 中第二次 `Request::init()` 和数组袋复制。
- 不改变不调用 `input()` 的路由热路径。

---

## 四、P0：显式 Hook 中止结果

### 4.1 API 方向

新增不可实例化的轻量结果类型或两个静态终止入口，二选一后以基准决定：

```php
return \Gene\Hook::abort();
return \Gene\Hook::respond($payload, 401);
```

重定向和已由 `Response::end/json/sendFile` 结束的响应也应设置 request context 的 `response_ended` 标记。Router 在每个 before/named Hook 后只检查一个分支：中止则不 dispatch，且入口可查询 `Response::isEnded()`，不再补默认 body/header。

### 4.2 兼容性

- 原 `false`/`0` 中止语义永久保留。
- 原 Hook 内 `json()/redirect()` + `return false` 保持有效。
- after Hook 不允许覆盖已结束响应；第一版继续忽略其返回值。
- 不自动把任意数组解释为响应，避免破坏已有返回值。

### 4.3 验收

1. before、named Hook 在 false/abort/respond/redirect 下均不执行 Controller。
2. FPM 与 Swoole 不重复 end、不补默认 Content-Type、不产生双 body。
3. 异常、sendFile、SSE 和普通输出缓冲路径保持原行为。
4. PC_DIRECT、closure、eval fallback 三条派发路径结果一致。
5. 无 Hook、Hook 放行和 Hook 拒绝三组基准；放行路径回退应低于噪声阈值。

---

## 五、P1：组级、可组合 Hook 策略

### 5.1 API 方向

不立即扩展每条 route 的第三参数类型，优先增加显式组策略：

```php
$router->group('/admin')
    ->through(['adminUser', 'adminAuth'])
    ->withoutAfter()
    ->get('/users', 'Controllers\\Admin\\User@index')
    ->group();
```

也可在实作评审时采用 `group('/admin', ['hooks' => [...], 'after' => false])`，但只保留一种公共写法。

### 5.2 规格

- Hook 顺序稳定；任一 before/named Hook 中止后不执行后续 Hook 和 Controller。
- 子组继承父组策略，可追加；清除必须用显式 API，不能依赖空字符串。
- route 级旧字符串 `hook@clearAfter` 保持原行为，并在注册阶段归一成同一不可变 Hook 数组和 flags。
- `before/after` 是 phase flag，不再作为 `@` 后缀继续扩展新语义。
- workerReady 冻结后，PC 描述符直接借用持久化的已解析 Hook 数组；请求期禁止 `strtok/snprintf` 和临时字符串分配。
- 第一版只支持已注册命名 Hook，不引入 PSR middleware 对象和 `next()` 闭包。

### 5.3 性能门槛

- 无 Hook 路由保持当前 direct dispatch。
- 单 Hook 路由不得比当前 PC_DIRECT 有显著回退。
- 多 Hook 只增加实际 PHP Hook 调用成本；解析与组合在注册/冻结阶段完成。
- Windows/FPM 功能测试后，还必须通过 Linux O2/O6、Swoole 并发、ASAN 和 route cache 冻结回归，再允许默认采用。

---

## 六、P1：标准 request-id 策略

### 6.1 配置方向

```php
$app->requestId([
    'header' => 'X-Request-Id',
    'bytes' => 8,
    'trust' => true,
    'max_length' => 128,
]);
```

默认关闭，避免升级后无条件随机数开销和响应头变化。

### 6.2 规格

- header 查找大小写不敏感；仅接受可见 ASCII，拒绝 CR/LF/NUL，限制最大长度。
- `trust=false` 时忽略入站值；缺失或非法时使用现有 `Crypto::randomId()` 内部 helper 生成。
- 只写 `Context['request_id']`，Log/Rest 继续读取同一真相源；不同时写 DI。
- 在 dispatch 前设置响应 `X-Request-Id`；cleanup 自动释放。
- `Invoke::local` 继承同一请求 ID，不重新生成。

### 6.3 性能

关闭时只有一个 unlikely 配置分支；启用时每请求一次 header 查找，只有缺失/非法时生成随机 ID。禁止通过用户态 Hook 再做重复大小写查询和双袋写入。

---

## 七、明确保留在应用 Hook 的逻辑

- session key、用户主键字段和“可选身份/强制登录”的业务定义；
- Ajax/页面登录失败文案与跳转地址；
- URI 对权限 ID 的映射及权限集合判断；
- 直连 IP 的内网规则；
- 语言回退和语言组件选择；
- JSONP 是否允许。JSONP callback 必须由应用白名单校验，Gene 不应默认开启。

应用可以保留一个请求级身份加载器避免重复 Redis 读取；在出现至少三个独立项目采用同一无业务语义模式前，不新增 `Session::once()` 或认证 DSL。

---

## 八、实施顺序与验收清单

1. 更新 ide-helper、AI reference 和 demo，统一推荐 `input()`；标记该项已落地。
2. 增加 Response ended 状态与 Hook abort/respond 测试，再接 Router 三条执行路径。
3. 设计组策略的注册期数据结构，先写 route tree/PC 快照测试和基准，再实现公共 API。
4. request-id 作为独立、默认关闭的策略实现，不与组 Hook 改造绑在同一提交。
5. 每项分别同步 CHANGELOG、ide-helper、reference 和 `test/*.php`。

必须覆盖：FPM、CLI、Swoole 两协程隔离、`Invoke::local`、cleanup、workerReady 后冻结、PC_DIRECT/closure/eval fallback，以及无 Hook 基准。任何为了减少 PHP 行数却让无 Hook 热路径增加 HashTable 构造或用户态回调的方案均不接受。
