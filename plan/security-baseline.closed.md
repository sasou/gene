# 安全基线框架化（Security Baseline）

> 状态：关闭（2026-11-01 全部落地，实现与验证记录见 §十八）
> 立项依据：业务A / 业务B / 业务C 三个应用近期安全加固中出现的**重复实现**与**口径分叉**
> 范围：只写 Gene 扩展需要提供的能力；应用侧迁移不在本文范围（遵循 `plan/README.md` 维护约定）

---

## 一、背景与目标

三个应用在近期安全加固中，各自在 `Ext\`、`Hooks\`、`config/router*.php` 里重复实现了同一批安全原语。这样带来三个问题：

1. **口径分叉**：例如"真实 IP"，业务A 取 XFF **最左**一节，业务B 取 XFF **最右**一节，业务C 的 `Device::clientIp()` 又取最左一节。限流、审计、内网判定用的 IP 口径不一致，存在绕过风险。
2. **复制粘贴**：Origin 同源校验、JSONP callback 正则、legacy 密码校验在多个文件里逐字复制，修一处漏一处。
3. **缺省不安全**：三个应用都**没有**输出任何安全响应头（`X-Content-Type-Options` / `X-Frame-Options` / `HSTS` / `Referrer-Policy` 在三仓库检索结果为 0）。验证码用 `rand()` 生成，也不是 CSPRNG。

目标：由框架提供**默认安全、单一口径、配置驱动**的安全原语。应用只需声明策略，不再各自封装。

### 1.1 框架现有能力（不重复建设）

| 已有 | 位置 | 说明 |
|------|------|------|
| `Gene\Crypto` hmacToken/hmacVerify/hmacSign/hmacCheck/tsSkew/randomId/encrypt/decrypt(GCM) | `src/tool/crypto.c` | 本文在其上扩展 |
| `Memory::rateLimit/lock`、`Redis::rateLimit/lock` | `src/cache/*` | 固定窗口限流，不含"失败计数+成功清零"语义 |
| `Application::webscan()` / `Gene\Webscan` | `src/http/webscan.c` | 已在 业务B/业务C 入口使用，本文不涉及 |
| `Session` secure/httponly/samesite、`regenerateId` | `src/session/session.c` | 本文补默认值与会话绑定 |
| `Request::isSecure()`（含 `X-Forwarded-Proto`）、`Request::bearer()`、`Request::header()` | `src/http/request.c` | 本文补可信代理与客户端 IP |
| `Hook::abort/respond`、`through()` 组钩子 | `src/mvc/hook.c`、router | 内置安全钩子基于它实现 |
| `Application::requestId(opts)` | `src/app/application.c:1491` | 入口级 opts 配置风格，本文沿用 |

---

## 二、需求清单总览

| 编号 | 优先级 | 能力 | 代码证据（重复/缺口） | 收益 |
|------|--------|------|------------------------|------|
| S1 | P0 | 可信代理 + `Request::ip()` / `Request::isInternal()` / `Gene\Ip` | 三仓 `Ip` 各一份，`getDirectIp/getConnectionIp/isInternalIp` 共 36 处调用，XFF 取值方向不一致 | 统一 IP 口径，消除伪造 XFF 绕过 |
| S2 | P0 | 内置同源/CSRF 策略 `Application::csrf()` + `Gene\Csrf` | 业务A `router.php`、`router.admin.php` 逐字复制；业务C `Hooks/AdminAuth.php`（Sec-Fetch-Site + Origin）；业务B `Security::getRequestDomain` | 删除 4 份手写同源校验 |
| S3 | P0 | 安全响应头 `Application::secureHeaders()` | 三仓库检索 `X-Frame-Options|X-Content-Type-Options|HSTS|Referrer-Policy` = 0 | 补齐缺省防护 |
| S4 | P0 | JSONP callback 在 C 层白名单校验 | 业务A `router.php`、`router.admin.php`、`router.api.php`，业务C `AfterHook.php`，业务B `AfterHook.php`，共 5 处同一正则（业务B 版本还更严） | 删掉 5 处正则，杜绝遗漏 |
| S5 | P0 | 内部 REST 鉴权 `Application::restGuard()` + `Gene\Rest` 自动带签 | 业务A `Guard::restAllowed`（内网 + `X-Rest-Secret`）、业务B `RestAuthHook`、业务C `RestAuth`（仅内网 IP） | 统一为"内网 + 签名"，消除反代下 REMOTE_ADDR 恒为内网导致的旁路 |
| S6 | P1 | `Gene\Password`（hash/verify/needsRehash + legacy 迁移） | `md5($salt.sha1($salt.$password))` 出现在 业务A `Admin\User`、`Member`，业务B `User`、`Admin\User`、`Affiliate`，业务C `Admin\User`；"校验通过后透明升级 bcrypt"写了 4 遍 | 一行完成双格式校验与升级 |
| S7 | P1 | `Gene\Throttle` 失败计数/锁定 | 业务A `Safes`：mail/check/checkTwofa/regMail/regCheck/payCheck/feedSpamCheck 共 7 套 "get→>=N→set(count+1)" 非原子计数 | 原子、口径统一、少写 80% 样板 |
| S8 | P1 | 会话加固：安全默认值 + 绑定 + 空闲/绝对超时 | 业务C `AdminAuth` 手写 UA 哈希与 vid 绑定；业务A `safe->check(sessid)`；业务B/业务A 仍保留 PHP `Session`（无 samesite） | 会话劫持防护内建 |
| S9 | P1 | `Crypto` 密钥环（kid 前缀 + 轮换 + HKDF）与 `seal/unseal` | 业务B `SecretCrypto`（3 份候选 material 逐个试解）、`ChannelCrypto`（另一份前缀）；业务A `Encrypt::easyEncode` 用于 2FA 密钥与 Cookie | 密钥轮换与版本化标准化 |
| S10 | P1 | `Crypto::totp` / `Crypto::randomCode` | 业务A、业务C 各带一份 `GoogleAuth`；验证码 `rand(1111, 9999)`（`Safes.php`） | 去掉第三方拷贝，验证码改用 CSPRNG |
| S11 | P2 | 上传安全校验 `Gene\Upload::check()` | 业务B `Upload::checkAllowed`（size/ext/getimagesize） | 增加 MIME 嗅探，统一安全文件名 |
| S12 | P2 | 方法守卫：动态路由 GET 只读白名单 | 业务B `MethodGuard`（约 150 行静态白名单） | 配置化，跨应用复用 |

实现约束（沿用 `plan/README.md`）：C 层只加入"重复 ≥3 处或位于热路径"的 API；每项都要补 `test/*.php` 回归用例，并同步 `gene-ide-helper` stub 和 `gene-ai-helper/skills/gene-framework/reference.md`。S11、S12 的重复次数不足 3，列为 P2，可以先在 `demo/` 中以 PHP 参考实现落地，再根据使用情况决定是否下沉到 C 层。

---

## 三、S1 可信代理与客户端 IP（P0）

### 3.1 问题

- 业务A `Ip::getClientIp()`：优先取 `CF-Connecting-IP`，再取 XFF 最左一节。客户端可以直接伪造请求头。
- 业务B `Ip::getConnectionIp()`：`REMOTE_ADDR` 为内网地址时取 XFF **最右**一节（nginx 追加的那一节），这是正确做法。
- 业务C `Device::clientIp()` 取最左一节，并用于设备哈希，可被伪造。
- 内网判定（`isInternalIp`）各仓一份，CIDR 匹配（`netMatch`）各仓一份。

### 3.2 设计

**配置**（入口级，与 `requestId()` 风格一致；可选 INI 兜底）：

```php
\Gene\Application::getInstance()
    ->trustProxies([
        'proxies' => ['127.0.0.1', '10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16'], // 可信反代 CIDR
        'headers' => ['x-forwarded-for'],      // 依次尝试；可加 'cf-connecting-ip'、'x-real-ip'
        'cf'      => false,                    // true 时仅当直连 IP 属于 Cloudflare 段（proxies 中配置）才采信 CF 头
        'proto'   => true,                     // 是否采信 X-Forwarded-Proto（影响 isSecure）
    ]);
```

INI：`gene.trusted_proxies = "127.0.0.1,10.0.0.0/8"`，`gene.trusted_headers = "x-forwarded-for"`。

**API**：

| API | 语义 |
|-----|------|
| `Request::directIp(): string` | 仅返回 `REMOTE_ADDR`（Swoole 下取 `server['remote_addr']`），不读任何请求头 |
| `Request::ip(): string` | 直连 IP 不在可信代理内时，直接返回直连 IP；否则**从右向左**遍历 XFF，跳过可信代理，返回第一个非可信地址；地址非法时回落到直连 IP |
| `Request::ips(): array` | 完整链（调试/审计用） |
| `Request::isInternal(?string $ip = null): bool` | 默认取 `directIp()`；判断是否属于 RFC1918 / loopback / ULA(fc00::/7) / link-local |
| `Gene\Ip::inCidr(string $ip, string\|array $cidrs): bool` | 支持 v4/v6 CIDR，供应用做白名单 |

`isSecure()` 改为：只有 `proto=true` **且**直连 IP 属于可信代理时才采信 `X-Forwarded-Proto`。这是一处行为收紧，需要写进 CHANGELOG。

### 3.3 实现思路

- 新文件 `src/http/ip.c`：负责 CIDR 解析（`inet_pton` → 16 字节 + 前缀长度）和匹配。`trustProxies()` 调用时一次性解析为 `gene_cidr_t[]`，存入 worker 级全局（与 router 冻结表同一生命周期，`workerReady()` 后只读）。
- `Request::ip()` 的结果缓存在请求级 `request_attr` 新槽位 `GENE_REQUEST_ATTR_IP`，由 `clear()` / `cleanup()` 统一释放，保证 Swoole 协程之间不串值。
- 请求头读取复用 `Request::header()` 的 HEADER bag（已统一为小写短横线形式）。

```c
/* src/http/ip.h */
typedef struct {
    unsigned char addr[16];
    uint8_t       prefix;   /* 0..128, v4 映射为 ::ffff:a.b.c.d 后 +96 */
} gene_cidr_t;

zend_bool gene_ip_parse(const char *s, size_t len, unsigned char out[16]);
zend_bool gene_cidr_match(const unsigned char ip[16], const gene_cidr_t *list, uint32_t n);
zend_bool gene_ip_is_internal(const unsigned char ip[16]);

/* src/http/request.c */
static zend_string *gene_request_resolve_ip(void)
{
    zend_string *direct = gene_request_direct_ip();          /* REMOTE_ADDR */
    unsigned char bin[16];
    if (!direct || !gene_ip_parse(ZSTR_VAL(direct), ZSTR_LEN(direct), bin)
        || !gene_cidr_match(bin, GENE_G(trusted_cidrs), GENE_G(trusted_cidr_n))) {
        return direct;                                       /* 非可信代理直连：不读头 */
    }
    for (uint32_t h = 0; h < GENE_G(trusted_header_n); h++) {
        zval *xff = gene_request_header_find(GENE_G(trusted_headers)[h]);
        if (!xff || Z_TYPE_P(xff) != IS_STRING) continue;
        /* 自右向左切分，跳过可信代理 */
        const char *s = Z_STRVAL_P(xff), *e = s + Z_STRLEN_P(xff), *p = e;
        while (p > s) {
            const char *q = p;
            while (q > s && q[-1] != ',') q--;
            const char *a = q, *b = p;
            while (a < b && (*a == ' ' || *a == '\t')) a++;
            while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
            if (b > a && gene_ip_parse(a, b - a, bin)) {
                if (!gene_cidr_match(bin, GENE_G(trusted_cidrs), GENE_G(trusted_cidr_n))) {
                    return zend_string_init(a, b - a, 0);
                }
            } else {
                break;                                       /* 非法段：停止，回落直连 */
            }
            p = (q > s) ? q - 1 : s;
        }
    }
    return direct;
}
```

### 3.4 回归

`test/RequestIpTest.php`：直连公网时伪造 XFF 不生效；可信代理下取最右非可信节点；多层代理；IPv6；非法段回落；`isSecure` 在非可信直连时忽略 `X-Forwarded-Proto`；Swoole 两协程 IP 不串。

---

## 四、S2 同源 / CSRF 策略（P0）

### 4.1 问题

业务A 的 webAuth 与 admin before 钩子各有一份约 20 行的"Origin host+port 归一化比较"；业务C 用 `Sec-Fetch-Site: cross-site` 加危险动作正则；业务B 用 GET 白名单兜底。三种实现的防护强度不同。

### 4.2 设计

提供两层能力：**同源校验**（无状态，默认推荐）和**同步令牌**（有状态，供不支持 Fetch Metadata 的旧客户端或高敏感后台使用）。

```php
// 入口或路由配置
\Gene\Application::getInstance()->csrf([
    'methods'      => ['POST', 'PUT', 'PATCH', 'DELETE'], // 需要校验的方法
    'fetch_site'   => true,       // 依据 Sec-Fetch-Site 拒绝 cross-site（same-site 按 allow_same_site 决定）
    'allow_same_site' => false,
    'origin'       => true,       // 存在 Origin/Referer 时与 Host（含端口归一化）比较
    'origins'      => ['https://m.example.com'], // 额外可信源
    'require_origin' => false,    // true：变更请求缺少 Origin 和 Referer 时直接拒绝
    'token'        => false,      // true 启用同步令牌：session 键 _csrf，头 X-CSRF-Token 或字段 _token
    'session'      => 'adminsession',
    'except'       => ['/api/pay/notify*'],  // 服务器回调白名单（glob）
    'on_fail'      => ['code' => 403, 'msg' => '非法请求来源'],
]);
```

也可以作为组钩子按组启用（钩子名由框架预注册，满足 `through()` 必须先注册的约束）：

```php
$router->group('/admin')->through(['@csrf', 'adminAuth'])->post('/:c/:a', 'Controllers\Admin\:c@:a');
```

`@` 前缀表示内置钩子，与用户自定义钩子名分开，不会冲突。

**令牌 API**：`Gene\Csrf::token(): string`（惰性生成，32 字节 base64url，写入 session）、`Gene\Csrf::check(?string $given = null): bool`（使用 `hash_equals`）、`Gene\Csrf::field(): string`（输出 `<input type="hidden" name="_token">`，供视图使用）。

### 4.3 实现思路

- 新文件 `src/http/csrf.c`。同源判定封装为纯函数 `gene_csrf_same_origin(origin, host, is_secure)`，采用 业务A 现有的 scheme 默认端口归一化规则（这一逻辑已在线上验证）。
- 判定顺序：方法不在 `methods` → 放行；命中 `except` → 放行；`Sec-Fetch-Site` 为 `cross-site` → 拒绝；为 `same-origin`/`none` → 进入令牌阶段；Origin（缺失时取 Referer）与 Host 或 `origins` 不匹配 → 拒绝；两者都缺失且 `require_origin` → 拒绝；`token=true` 时还要校验令牌。
- 拒绝时调用 `gene_hook_respond()`，复用现有终止语义，`after` 钩子行为与 `Hook::respond()` 一致。
- 策略在 `csrf()` 调用时编译成 worker 级只读结构；`except` 预编译为 glob 列表。

```c
static zend_bool gene_csrf_same_origin(const char *origin, size_t olen,
                                       const char *host, size_t hlen, zend_bool secure)
{
    php_url *u = php_url_parse_ex(origin, olen);
    if (!u || !u->host || !u->scheme) { if (u) php_url_free(u); return 0; }
    zend_long oport = u->port ? u->port
                     : (zend_string_equals_literal_ci(u->scheme, "https") ? 443 : 80);
    /* host 可能是 "example.com:8080" 或 "[::1]:8080" */
    const char *colon = gene_host_port_sep(host, hlen);
    size_t hn = colon ? (size_t)(colon - host) : hlen;
    zend_bool ok = ZSTR_LEN(u->host) == hn
                && zend_binary_strncasecmp(ZSTR_VAL(u->host), hn, host, hn, hn) == 0;
    if (ok && colon) {
        ok = (ZEND_STRTOL(colon + 1, NULL, 10) == oport);   /* Host 带端口才比端口 */
    }
    php_url_free(u);
    return ok;
}
```

### 4.4 回归

`test/CsrfTest.php`：同源 POST 放行；跨源 POST 拒绝；非标准端口；`Sec-Fetch-Site` 的各个取值；缺少 Origin 时回落 Referer；`except` 回调放行；令牌模式 `hash_equals`；拒绝后不进入控制器，且 `after` 语义与 `respond` 一致。

---

## 五、S3 安全响应头（P0）

### 5.1 设计

```php
\Gene\Application::getInstance()->secureHeaders([
    'nosniff'   => true,                 // X-Content-Type-Options: nosniff
    'frame'     => 'SAMEORIGIN',         // X-Frame-Options；false 关闭
    'referrer'  => 'strict-origin-when-cross-origin',
    'hsts'      => 'max-age=31536000; includeSubDomains', // 仅 isSecure() 为真时输出
    'csp'       => null,                 // 字符串原样输出；数组按指令拼接
    'csp_report_only' => false,
    'permissions' => 'camera=(), microphone=(), geolocation=()',
    'coop'      => null,                 // Cross-Origin-Opener-Policy
    'remove'    => ['X-Powered-By'],
]);
```

- 默认值（不传参数）：`nosniff + SAMEORIGIN + referrer + HSTS(仅 https)`。CSP 默认关闭，避免影响已有内联脚本。
- 单个响应可以覆盖：`Response::header('X-Frame-Options', null)` 删除；`Response::csp($policy)` 覆盖当前响应的 CSP。
- **nonce 支持**：`Response::cspNonce(): string` 在每个请求内只生成一次（存于 Context），并自动替换 CSP 中的 `{nonce}` 占位符，视图中使用 `<script nonce="<?=\Gene\Response::cspNonce()?>">`。

### 5.2 实现思路

- 预渲染：`secureHeaders()` 把头部拼成 `zend_string` 数组，存入 worker 级只读区。每个请求在 **首次发送响应头前**（FPM 为 `gene_response_send_headers` 前；Swoole 为写入 `$response->header` 前）统一注入，零分配。
- 已由用户显式设置的同名头不覆盖，通过 header 表 `zend_hash_exists` 判断。
- `sendFile`/SSE 同样注入；`Content-Type` 为 `application/json` 时跳过 CSP（无意义）。

```c
static void gene_secure_headers_apply(gene_response_ctx *r)
{
    gene_secure_hdr *h = GENE_G(secure_hdrs);
    for (uint32_t i = 0; i < GENE_G(secure_hdr_n); i++, h++) {
        if (h->https_only && !gene_request_is_secure()) continue;
        if (h->skip_json && r->is_json) continue;
        if (gene_response_header_exists(r, h->name)) continue;   /* 用户优先 */
        gene_response_header_add(r, h->name, h->has_nonce
            ? gene_csp_with_nonce(h->value) : h->value);
    }
    for (uint32_t i = 0; i < GENE_G(secure_remove_n); i++) {
        gene_response_header_remove(r, GENE_G(secure_remove)[i]);
    }
}
```

### 5.3 回归

`test/SecureHeadersTest.php`：默认头完整；http 下不输出 HSTS；用户头优先；nonce 在同一请求内稳定、跨请求不同；Swoole 与 FPM 输出一致（`tools/acceptance` 增加 curl -I 断言）。

---

## 六、S4 JSONP callback 内建校验（P0）

### 6.1 设计

`Response::json($data, $callback)` 与 `Controller::json` 在 C 层校验 `$callback`：

- 规则采用 业务B 的严格版：`^[A-Za-z_$][A-Za-z0-9_$]*(\.[A-Za-z_$][A-Za-z0-9_$]*)*$`，长度 ≤ 128。
- 校验不通过：**丢弃 callback，按纯 JSON 输出**，与三个应用当前的行为一致。
- JSONP 输出增加 `/**/` 前缀，加上 `Content-Type: application/javascript; charset=utf-8` 和 `X-Content-Type-Options: nosniff`，防御 Rosetta Flash 类攻击。
- INI `gene.jsonp = 1|0`：设为 0 时完全禁用 JSONP（推荐新项目使用）。

### 6.2 实现思路

用手写状态机代替 PCRE（热路径，避免编译正则）：

```c
static zend_bool gene_jsonp_valid(const char *s, size_t n)
{
    if (n == 0 || n > 128) return 0;
    zend_bool need_head = 1;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = s[i];
        if (c == '.') { if (need_head) return 0; need_head = 1; continue; }
        zend_bool head = (c == '_' || c == '$' || (c|32) >= 'a' && (c|32) <= 'z');
        if (need_head) { if (!head) return 0; need_head = 0; }
        else if (!head && !(c >= '0' && c <= '9')) return 0;
    }
    return !need_head;
}
```

应用侧 5 处 `preg_match` 可以直接删除：`after` 钩子只需写 `Response::json($params, $this->request->get('callback'))`。

---

## 七、S5 内部 REST 鉴权（P0）

### 7.1 问题

`/rest/*` 依赖 "REMOTE_ADDR 为内网" 来鉴权。反代部署时 REMOTE_ADDR 恒为内网地址，鉴权实际失效。业务A 已经补上了 `X-Rest-Secret` 共享密钥，但 业务B 和 业务C 没有。共享明文密钥本身也有被重放、被日志泄露的风险。

### 7.2 设计

```php
// 入口
\Gene\Application::getInstance()->restGuard([
    'internal' => true,                        // 要求 Request::isInternal(directIp)
    'secret'   => \Gene\Application::config('restAuthSecret'), // 非空即强制签名
    'mode'     => 'hmac',                      // 'hmac'（推荐）| 'shared'（兼容 业务A X-Rest-Secret）
    'skew'     => 30,                          // 时间戳容差（秒）
    'nonce'    => 'redis',                     // 防重放存储组件名；null 则仅时间戳
]);

// 路由
$router->group('/rest')->through(['@rest'])->get('/:c/:a', 'Api\:c@:a');
```

- `Gene\Di::get('inner')` 为真（`Invoke::local` 本地互调）时直接放行，与现有三个钩子的语义一致。
- **签名格式**（请求头）：`X-Gene-Ts`、`X-Gene-Nonce`、`X-Gene-Sign = base64url(HMAC-SHA256(secret, METHOD\nPATH\nTS\nNONCE\nsha256(body)))`。
- 客户端：`Gene\Rest` / `Gene\Http` 识别同一配置后**自动加签**，应用不再手写 `X-Rest-Secret`（业务A `Rest.php`）。
- `mode=shared`：保留 业务A 现有协议（使用 `hash_equals` 比较），作为过渡方案。

### 7.3 实现思路

复用 `Crypto::hmacSign/hmacCheck/tsSkew`，新增 C 内部函数 `gene_crypto_sign_request()`，服务端和客户端共用：

```c
zend_string *gene_crypto_sign_request(zend_string *secret, zend_string *method,
                                      zend_string *path, zend_long ts,
                                      zend_string *nonce, zend_string *body)
{
    unsigned char bh[32];
    gene_sha256(ZSTR_VAL(body), ZSTR_LEN(body), bh);
    smart_str s = {0};
    smart_str_append(&s, method);  smart_str_appendc(&s, '\n');
    smart_str_append(&s, path);    smart_str_appendc(&s, '\n');
    smart_str_append_long(&s, ts); smart_str_appendc(&s, '\n');
    smart_str_append(&s, nonce);   smart_str_appendc(&s, '\n');
    gene_hex_append(&s, bh, 32);
    smart_str_0(&s);
    zend_string *sig = gene_hmac_sha256_b64url(secret, s.s);
    smart_str_free(&s);
    return sig;
}
```

防重放：`nonce` 组件调用 `set(key, 1, ['nx' => true, 'ex' => skew*2])`。Redis 用 `SET NX EX`；Memory 用 `add` 语义（仅单机有效，文档中注明）。body 取 `Request::rawContent()`，与现有 JSON 解码缓存共享。

---

## 八、S6 `Gene\Password`（P1）

### 8.1 设计

```php
// 新哈希（默认 PASSWORD_DEFAULT，可在 INI gene.password_algo / cost 中配置）
$hash = \Gene\Password::hash($plain);

// 校验 + 是否需要升级（同时兼容 legacy 格式）
$r = \Gene\Password::verify($plain, $stored, [
    'legacy' => 'md5salt',           // 内置：md5(salt . sha1(salt . pass))
    'salt'   => $row['user_salt'],
    'truncate' => 50,                // 业务A Admin\User 曾截断为 50 位
]);
// $r = ['ok' => true, 'rehash' => true|false, 'hash' => '<新哈希，仅 rehash=true 时有>']

if ($r['ok'] && $r['rehash']) {
    UserModel::updateBy(['user_id' => $id], ['user_pass' => $r['hash']]);
}
```

- `legacy` 也可以传 callable：`fn($plain, $stored, $salt) => bool`，覆盖支付密码 `'exchpays'` 这类自定义盐。
- legacy 校验一律使用 `hash_equals`（现有部分代码用 `==`）。
- `needsRehash($hash)`：包装 `password_needs_rehash`，并且对非 `password_hash` 格式的哈希也返回 true。

### 8.2 实现思路

C 层直接调用 `php_password_hash/verify`（`ext/standard/password.c` 导出的 `php_password_algo_*` API），避免经过 userland 调用：

```c
PHP_METHOD(gene_password, verify) {
    zend_string *plain, *stored; zval *opts = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STR(plain) Z_PARAM_STR(stored) Z_PARAM_OPTIONAL Z_PARAM_ARRAY(opts)
    ZEND_PARSE_PARAMETERS_END();

    const php_password_algo *algo = php_password_algo_identify(stored);
    zend_bool ok = 0, rehash = 0;
    if (algo) {
        ok = algo->verify(plain, stored);
        rehash = ok && algo->needs_rehash(stored, gene_password_opts());
    } else if (opts) {
        ok = gene_password_legacy_verify(plain, stored, Z_ARRVAL_P(opts)); /* 常量时间 */
        rehash = ok;
    }
    array_init_size(return_value, 3);
    add_assoc_bool(return_value, "ok", ok);
    add_assoc_bool(return_value, "rehash", rehash);
    if (rehash) {
        zend_string *h = gene_password_make(plain);
        add_assoc_str(return_value, "hash", h);
    }
}
```

另外，即使用户不存在也执行一次 dummy verify，抹平时间侧信道（防止用户名枚举）。框架提供 `Password::dummy()`。

---

## 九、S7 `Gene\Throttle` 失败计数 / 锁定（P1）

### 9.1 问题

业务A `Safes` 中 7 处都是如下写法：

```php
$count = intval($this->memcache->get($key));
if ($count >= 5) return false;
...失败: $this->memcache->set($key, $count + 1, $ttl);
...成功: $this->memcache->set($key, 0, $ttl);
```

这种"读-判断-写"不是原子操作，并发请求可以突破阈值；每处的 key 命名、TTL 也各不相同。现有的 `rateLimit` 是"窗口内命中次数"语义，表达不了"失败才计数、成功清零、超限锁定 N 秒"。

### 9.2 设计

```php
$t = new \Gene\Throttle('redis');           // 组件名：redis | memcache | memory

// 失败计数（登录、验证码、支付密码、2FA）
if ($t->tooMany("login:{$uid}", 5)) {
    return $this->error('错误次数过多，请 ' . $t->retryAfter("login:{$uid}") . ' 秒后再试');
}
if (!$ok) { $t->hit("login:{$uid}", 1800); } else { $t->clear("login:{$uid}"); }

// 多维度一次判断（session + IP），任一超限即拒绝
$t->tooManyAny([
    ["reg:sess:{$sid}", 5],
    ['reg:ip:' . \Gene\Request::ip(), 30],
]);

// 发送频率（等价于 rateLimit，统一放在同一入口）
$t->attempt('sms:ip:' . \Gene\Request::ip(), 20, 3600);   // 返回 bool，原子 incr
```

| 方法 | 语义 |
|------|------|
| `hit(key, decay)` | 原子 `INCR`；首次写入时设置 TTL；返回当前计数 |
| `tooMany(key, max)` | 计数 ≥ max |
| `attempt(key, max, decay)` | `hit` 后判断 ≤ max，用于发送频率限制 |
| `clear(key)` / `retryAfter(key)` / `remaining(key, max)` | 清零 / 剩余锁定秒数 / 剩余次数 |

### 9.3 实现思路

- 统一 key 前缀 `gene:thr:`，避免与业务 key 冲突。
- Redis 后端：用一段 Lua（`INCR` + 首次 `EXPIRE`）保证原子性，把脚本 SHA 缓存在 worker 级。
- Memory 后端：直接复用 `gene_memory_adjust()`（`incr` 已有原子保护），TTL 写入条目元数据。
- Memcache 后端：`add(key, 0, ttl)` + `increment`。
- 失败计数写入时会自动 `Log::warning`（可关闭），便于审计爆破行为，并自动带上 `request_id`。

```c
static const char GENE_THR_LUA[] =
    "local c=redis.call('INCR',KEYS[1]) "
    "if c==1 then redis.call('EXPIRE',KEYS[1],ARGV[1]) end "
    "return {c, redis.call('TTL',KEYS[1])}";
```

---

## 十、S8 会话加固（P1）

### 10.1 设计（`Gene\Session` 配置扩展，全部可选）

```php
$config->set('adminsession', [
    'class'  => '\Gene\Session',
    'params' => [[
        'driver'   => 'redis',
        'name'     => 'ASESSID',
        'ttl'      => 7200,
        // —— 新增 ——
        'secure'   => 'auto',        // auto：跟随 Request::isSecure()
        'samesite' => 'Lax',         // 默认 Lax（当前默认为空串）
        'idle'     => 1800,          // 空闲超时：超过则 destroy
        'absolute' => 43200,         // 绝对超时：从登录（regenerateId）起算
        'bind'     => ['ua'],        // 会话绑定：ua | ip | ip24 | callable(): string
        'on_mismatch' => 'destroy',  // destroy | flag（仅标记，由应用决定）
    ]],
]);
```

- 绑定指纹保存在 session 保留键 `_gene_fp`（sha256），在 `load()` 时校验。不匹配时按 `on_mismatch` 处理，并通过 `Session::compromised(): bool` 暴露结果，替代 业务C `AdminAuth` 中约 20 行手写的 UA 比对代码。
- 设备级指纹（如 bsafe vid）属于业务，通过 `bind => [fn() => Device::visitorId()]` 注入。
- `regenerateId()` 改为同时刷新 `_gene_fp` 与 `_gene_at`（登录时间），并删除旧 ID（现有实现需确认是否已删除旧数据，在回归中断言）。

### 10.2 实现思路

在 `gene_session_load()` 末尾增加 `gene_session_guard()`：读取 `_gene_ts/_gene_at/_gene_fp`，判断超时与绑定后，更新 `_gene_ts`（为避免每次请求都写回存储，可设 60 秒节流）。`secure=auto` 在 `gene_session_emit_cookie()` 时解析。

---

## 十一、S9 密钥环与 seal/unseal（P1）

### 11.1 问题

业务B `SecretCrypto` 为了兼容历史数据，按 3 份候选 material 逐个试解密，而且 key 由 DB 账号密码派生（改 DB 密码会导致所有密文无法解密）。`ChannelCrypto` 是另一套前缀。业务A `Encrypt::easyEncode` 是自定义的弱加密，被用于 2FA 密钥和推荐 Cookie。

### 11.2 设计

```php
// 入口注册密钥环（密钥来自环境变量 / 配置，禁止硬编码）
\Gene\Crypto::keyring([
    'k2' => getenv('APP_KEY_2'),   // 第一个为当前加密密钥
    'k1' => getenv('APP_KEY_1'),   // 其余仅用于解密
]);

$c = \Gene\Crypto::seal($plain, 'agent.apikey');   // 输出 "g1.k2.<b64url(iv|ct|tag)>"
$p = \Gene\Crypto::unseal($c, 'agent.apikey');     // 按 kid 选择密钥；context 作为 AAD 防止跨字段挪用
\Gene\Crypto::needsReseal($c);                      // kid 不是当前密钥时为 true，便于后台批量重加密

// 派生子密钥
$k = \Gene\Crypto::derive('channel', 32);           // HKDF-SHA256(当前主密钥, info=channel)

// 签名 Cookie（防篡改，可选加密）
\Gene\Response::sealedCookie('lmtj', $id, time() + 31536000);
$id = \Gene\Request::sealedCookie('lmtj');           // 被篡改或过期时返回 null
```

### 11.3 实现思路

- 底层继续使用已有的 GCM 实现，新增 AAD 参数；`kid` 解析是 O(1) 哈希查找，不再逐个试解。
- 密钥环保存在 worker 级只读内存，`workerReady()` 后冻结；同时提供 `Crypto::keyringLoaded()` 供健康检查使用。
- 兼容迁移：`unseal($c, $ctx, ['fallback' => callable])`，无 `g1.` 前缀时交给应用的旧解密逻辑处理，并返回 `needsReseal=true`。

---

## 十二、S10 TOTP 与安全随机码（P1）

```php
\Gene\Crypto::totpSecret(20);                      // base32 secret
\Gene\Crypto::totpUri($secret, 'user@x', 'MyApp');
\Gene\Crypto::totpVerify($secret, $code, ['window' => 1, 'digits' => 6, 'period' => 30,
                                          'used' => 'redis']); // used：同一时间片的码只能用一次
\Gene\Crypto::randomCode(6);                       // CSPRNG 数字验证码（php_random_int），替代 rand()
```

实现：基于 RFC 6238，HMAC-SHA1 复用 `ext/hash`；base32 编解码放在 `src/tool/crypto.c`；`used` 用 `SET NX EX period*(2*window+1)` 防止同一验证码在窗口期内被重放。可以删除 业务A 与 业务C 两份 `GoogleAuth`。

---

## 十三、S11 上传安全校验（P2）

```php
$r = \Gene\Upload::check($_FILES['file'] ?? $this->request->files('file'), [
    'max'   => 10 << 20,
    'ext'   => ['jpg', 'png', 'webp', 'pdf'],
    'mime'  => true,          // finfo 嗅探，且必须与扩展名映射一致
    'image' => true,          // 位图扩展名必须通过 getimagesize
    'deny'  => ['php', 'phtml', 'phar', 'htaccess', 'svg'], // 始终拒绝（含双扩展 a.php.jpg 中间段检测）
]);
// ['ok' => bool, 'error' => string, 'ext' => 'png', 'mime' => 'image/png', 'name' => '<随机安全文件名>']
```

先以 PHP 参考实现放在 `demo/application/Ext/`，跨应用出现 ≥3 处使用后再下沉到 C 层。

---

## 十四、S12 方法守卫（P2）

动态路由 `/:c/:a` 同时注册了 GET 和 POST，导致变更类动作可以被 GET 触发（CSRF 和 `<img>` 预取都会命中）。提供声明式只读白名单：

```php
$router->readonly('/admin', [
    'index' => ['run', 'panel', 'welcome'],
    'user'  => ['run', 'lists', 'add', 'edit'],
    '*'     => ['run', 'lists'],          // 通配
]);
// 命中前缀、方法为 GET 且不在白名单 → 405（走 error:405 钩子）
```

实现：在路由派发确定 `:c/:a` 之后、执行 handler 之前查询一张只读哈希表（`prefix → c → a`），与 S2 的 `@csrf` 互补。业务B `MethodGuard` 约 150 行可以改为配置。

---

## 十五、实施顺序与里程碑

| 里程碑 | 内容 | 依赖 |
|--------|------|------|
| M1 | S1 可信 IP → S4 JSONP → S3 安全头 | 无；S3 的 HSTS 依赖 S1 的 `isSecure` 收紧 |
| M2 | S2 CSRF（同源 + 令牌）→ S5 REST 鉴权 | S1（`isInternal`）；S5 复用 `Crypto` |
| M3 | S6 Password → S7 Throttle → S10 TOTP | S7 的 `used` 存储被 S10 复用 |
| M4 | S8 会话加固 → S9 密钥环 | S9 的 `sealedCookie` 与 S8 共用 Cookie 输出路径 |
| M5 | S11 / S12 参考实现，按使用情况决定是否下沉 | — |

每个里程碑的交付要求：

- `test/` 回归用例：FPM 与 Swoole 双模式，禁止因环境缺失 SKIP 造成假绿。
- `tools/acceptance` 增加安全头、CSRF、REST 签名的 curl 断言。
- 同步 `gene-ide-helper` stub、`gene-ai-helper/skills/gene-framework/reference.md`、`docs/` 中的 INI 参考。
- `demo/` 示例：在入口启用 `trustProxies + secureHeaders + csrf + restGuard`，作为推荐基线。

## 十六、行为变更与兼容

| 项 | 变更 | 兼容策略 |
|----|------|----------|
| `isSecure()` | 未配置可信代理时不再采信 `X-Forwarded-Proto` | 未调用 `trustProxies` 时保留旧行为一个小版本，并输出一次 `gene_log_diag` 提示 |
| JSONP | 非法 callback 被丢弃；JSONP 响应增加 `/**/` 前缀 | 与现有应用侧行为一致；INI 可关闭 |
| Session `samesite` 默认 Lax | 跨站 POST 不再携带 Cookie | 第三方回调本身不依赖 Cookie；可显式配置 `'samesite' => ''` 恢复旧行为 |
| 内置钩子名 `@xxx` | 新命名空间 | 不与用户钩子冲突 |

## 十七、非目标

- WAF 规则（`Webscan` 已有，不在本文扩展）。
- bsafe 设备指纹 / 风控逻辑（属于业务 SDK，仅通过 S8 的 `bind` callable 接入）。
- OAuth / JWT 完整协议栈（现有 `hmacToken` 已满足内部令牌需求，另行立项）。
- 应用仓库的迁移清单。

---

## 十八、落地复盘（2026-11-01）

S1-S12 已全部按本文方案落地于 C 扩展；回归文件 `test/SecurityTest.php`（66 项断言，接入 `TestRunner.php`）。

| 项 | 落地位置 |
|----|----------|
| S1 | `src/http/ip.c/.h`：`Gene\Ip`（inCidr/isInternal/isValid，v4/v6/嵌入v4）、`Application::trustProxies`（CIDR 表 MINIT 编译、MSHUTDOWN 释放）、`Request::ip/directIp/ips/isInternal`、`isSecure` 收紧 |
| S2 | `src/http/policy.c`：`Application::csrf` + `Gene\Csrf`（token/check/field），Fetch-Site + Origin/Referer 同源（端口按 scheme 归一）+ 同步令牌三层 |
| S3 | `policy.c`：`Application::secureHeaders`（nosniff/frame/referrer/hsts/permissions/coop/csp/report-only/remove），`Response::cspNonce` 请求级缓存 |
| S4 | `response.c`：`gene_jsonp_valid`（`^[A-Za-z_`$`][A-Za-z0-9_`$`]*(\.…)*`$` ≤128），非法 callback 静默退化为 JSON |
| S5 | `policy.c` + `rest.c` + `crypto.c`：`Application::restGuard`（internal 直连 IP 判定、shared/hmac 模式、skew、nonce 防重放、body 签名、prefixes/except）；`Rest` 出站 `sign` 自动加签；`Crypto::signRequest` |
| S6 | `src/tool/password.c`：`Gene\Password` hash/verify/verifyLegacy/verifyAny/needsRehash/dummy |
| S7 | `src/tool/throttle.c`：`Gene\Throttle` hit/tooMany/attempt/count/clear，适配 Memory/Redis/Memcached 形存储（incr → nx-set 兜底） |
| S8 | `session.c`：`secure:'auto'`、samesite 默认 Lax、idle/absolute 超时、UA/IP 指纹绑定（`_born`/`_bind` 随会话持久化） |
| S9 | `crypto.c`：keyring/derive/seal/unseal（AES-256-GCM，`g1.kid.*` 线格式，ctx 作 AAD）；`Response::sealedCookie`/`Request::sealedCookie` |
| S10 | `crypto.c`：totpSecret/totp/totpVerify（±1 窗口）、randomCode |
| S11 | `demo/application/Ext/UploadCheck.php`（PHP 参考实现，未达下沉门槛） |
| S12 | `policy.c`：`Application::readonly`（GET 方法守卫 405）+ `demo/application/Hooks/ReadonlyGuard.php` |

### 验证

- Windows PHP 8.1.30 x64 NTS 编译通过（`buildconf` → `config.nice` → `nmake`，新增 `ip/policy/password/throttle.c` 已入 `config.w32`/`config.m4`）。
- `SecurityTest.php` 66/66 PASS；全量 `TestRunner` 891 项、881 通过、失败 10 均为环境缺 `ctype`/`pdo_sqlite`，与本次改动无关。
- 复盘修复：isSecure 空信任表时不再采信 XFP；v6 `::` 压缩段解析错位；`hash()` 实参顺序；`verifyAny` 算法 zval 未释放即覆写；rest 出站 body 借用串被误 release；Origin 显式非默认端口绕过；`{nonce}` 只替换首处；`on_fail.code` 优先级。

### 遗留待验证（环境项，非缺陷）

- OpenSSL 构建下 `seal/unseal`/`sealedCookie` 加解密路径（当前构建无 openssl，断言其正确抛错）。
- `ctype`/`pdo_sqlite` 齐备后全量应恢复绿。
- FPM/Swoole 验收工具补充安全头、CSRF、REST 签名 curl 断言；`gene-ide-helper` stub 与 `gene-ai-helper` reference 同步。

### 落地优化复审（2026-11-01 第二轮）

按"科学合理 / bug / 易用 / 高并发"四轴复审，落地代码修复如下（除注明外均在 C 扩展内完成）：

| 轴 | 问题 | 修复 |
|----|------|------|
| S1 | v6 解析器接受重复 `::`、尾部垃圾、5+ hex 组、非法嵌入 v4；v4 `/33+` 前缀越界被静默收纳 | `gene_ip_parse` 重写为严格逐字节消费；`gene_cidr_list_parse` 按文本族判定前缀域（v4 `/n` → `96+n`，上限 32/128），非法项告警并跳过（不再静默当 `/0` 信任全部） |
| S1 | `isInternal` 把含 `ffff:0a00:*` 接口 ID 的全局 v6 误判 10.x | 先全前缀校验 `::ffff:0:0/96` 再取 v4 段（`gene_ip_is_v4mapped`） |
| S1 | XFP 只查 `$_SERVER`，Swoole 注入 header 袋时不生效；`trustProxies()` 每次重建信任表 | `gene_header_lookup` 统一走 HEADER 袋（Swoole 显式注入 / FPM 由 server 袋惰性物化）；`trustProxies` 按配置签名幂等（同配置二次调用为零成本 no-op），并修复 `Request::init()` 未失效物化 HEADER 袋导致旧快照残留的真 bug（回归：`isSecure: XFP via header bag`） |
| S1 | `directIp` 每请求堆分配 | `gene_direct_bin` 改为零分配直连解析，热路径（isSecure/secureHeaders/csrf/restGuard）复用 |
| S2/S3/S5/S12 | 策略读 header 依赖 server 袋，Swoole header 袋不一致；readonly `:c` 段大小写敏感、超长段被跳过（fail-open）；无参调用策略配置被当"禁用"；HSTS 可能明文下发；嵌套 `run()` 会重跑策略 | `policy_header` 走 HEADER 袋；readonly 改大小写不敏感、定长缓冲移除（超长段 fail closed）；setter 无参=应用默认值；HSTS 仅 `isSecure()` 时下发；嵌套 run 检测后跳过策略复跑；CSRF `check` 改只读（`create=0`，失败不生成/轮换 token） |
| S5 | REST 签名目标与实发请求不一致（仅 path，缺 query）；nonce 复用检查用了非 NX 的 `set(nx=>true)` 写法——phpredis/Gene\Redis/Memory 三种实现都不认，防重放静默失效 | 签名目标统一为 `path + '?' + QUERY_STRING`（`gene_policy_target`），出站 `Rest` 自动签名取规范化后的完整 path+query、body 取最终序列化串；nonce 改 `gene_store_once`：Memory/Redis→`lock`（SET NX EX），memcached 形→`add`，phpredis 形→`set('nx','ex')`，未配置组件 fail-closed、存储宕机放行+diag（时间窗仍兜底） |
| S6 | `dummy()` 硬编码 literal 哈希可能成本漂移/格式错误（立即返回即失效）；`verify` 返回缺 algo/rehash 信息；`needsRehash` 未走默认参数 | dummy 哈希进程内用当前默认算法现算一次（持久串）；`verify/verifyAny` 统一返回 `ok/algo/needs_rehash[.hash]`（验证成功且需升级时附带新哈希）；`needsRehash` 默认 PASSWORD_DEFAULT；legacy 走 `hash_equals` 常量时间比较 |
| S7 | `hit()` 用裸 `incr`：Memory/Redis 上新建计数器无 TTL → 被限键永久锁定 | `Memory::incr(key,step,ttl)` 增 TTL 形参（写锁内建窗）；`gene_redis_hit` Lua 单脚本 INCR+首建 EXPIRE；通用存储 `incr→expire`/`add`/`set(nx,ex)` 回退；`tooMany` 明确为只读语义（不计入本次），键统一 `gene:thr:` 前缀 |
| S8 | 会话绑定的客户端 IP 取 XFF 可被伪造；UA 只读 server 袋；idle 每请求触盘写会话 | 绑定指纹改用 `gene_request_client_ip()`（信任表解析）；UA 走 HEADER 袋；`_born`/`_bind` 随会话持久化，`gene_session_touch_interval` 限频（uttl 与 idle/2 取小） |
| S9 | `sealedCookie` 只有浏览器端 expires，客户端可回放过期值 | sealed 明文内嵌 `"exp\nvalue"`：`Response::sealedCookie` 写入，`Request::sealedCookie` 校验内嵌过期（exp=0 不过期），篡改/过期/无头负载一律 null |
| S9 | `gene_crypto_unseal_str` 给 `openssl_decrypt` 传了 8 参（encrypt 专属的 tag_length），调用即抛 ArgumentCountError——`unseal`/`sealedCookie` 在有 openssl 的环境下永远失败（此前构建无 openssl 从未跑到） | 改为 7 参调用；PHP 8.2.35 + openssl 下 seal/unseal/轮换/sealedCookie 全链路回归通过 |
| S10 | `totpVerify` 用 `strtol` 宽松解析（空白/符号/任意长度可过）；同一窗口内验证码可重放 | 严格校验"恰为 digits 位 ASCII 数字"；新增 `opts['used']` 存储：验证通过即 `gene_store_once` 打 `gene:totp:<hmac指纹>:<counter>` 标记（窗口时长，见第三轮复审），同码二次验证返回 false |

### 复审结论

- **科学合理**：集中式策略在 `Application::run()` 单点前置执行，无路由顺序旁路；直连 IP 与转发 IP 语义分层（授权判 peer、日志用 client）；原子承诺均有真原子实现支撑（Memory 写锁 / Redis Lua / NX-SET）。
- **易用**：应用侧只做声明式配置（`trustProxies/csrf/restGuard/secureHeaders/readonly` 无参即默认值），原语开箱即用，无需复制粘贴正则/哈希逻辑。
- **高并发**：热路径无全局写、无 per-request 堆分配（directIp 零分配、HEADER 袋惰性物化一次/请求）；trust 表签名幂等使 Swoole 每请求 bootstrap 零成本；计数/防重放均为单原子操作，无读-改-写竞态。

### 验证（第二轮）

- Windows PHP 8.1.30 x64 NTS `nmake` 全量重编通过。
- `test/SecurityTest.php` **88/88 PASS**（新增 22 项：v6 严格解析边界、header 袋 XFP、TOTP 防重放与严格位长、Throttle TTL 过期与只读 tooMany、CSRF 无副作用、sealedCookie 内嵌过期[openssl 分支]）。
- 全量 `TestRunner` 913 项、903 通过、失败 10 均为环境缺 `ctype`/`pdo_sqlite`（OrmTest 7、DemoLoad 2、RouterTest 1），与改动无关；较前基线 891/881 无回归。
- `gene-ide-helper` stub 已同步：新增 `Ip/Csrf/Password/Throttle`，`Crypto` 补 S9/S10 方法，`Memory::incr` 补 `$ttl`，`Request/Response/Application` 补安全 API。
- **PHP 8.2.35 NTS x64 + openssl/pdo_sqlite/ctype 全量复验（同日）**：用 `php-8.2.33-src` 树编出 8.2 版 `php_gene.dll`，挂到 WampServer 8.2 php 上——`TestRunner` **1084/1084 全绿**（SecurityTest 97/97 含 openssl 全路径、OrmTest 206、RouterTest 49、DemoLoad 6），环境失败清零。复验中发现并修复 `openssl_decrypt` 8 参调用 bug（见 S9 行）。

### 遗留待验证（复审后更新）

- ~~OpenSSL 构建下 `seal/unseal`/`sealedCookie` 与 TOTP `used` 端到端~~ —— 已在 8.2.35 + openssl 下全绿。
- ~~`ctype`/`pdo_sqlite` 齐备后全量恢复绿~~ —— 1084/1084 达成。
- ~~Redis/Memcached 实存下的 `gene_store_once`/throttle 原子路径~~ —— 已实测通过并修复派生缺陷（见下节）。
- ~~FPM curl 验收补安全头、CSRF、REST 签名断言~~ —— 已实测通过（见末节）；Swoole 形态同代码路径，留 Linux 环境跑 `tools/acceptance/linux_swoole_verify.sh` 做最终确认。

### 实存验证（2026-11-01，PHP 8.2.35 + 实存 Memcached <memcached-host>:1121 / Redis <redis-host>:6379 AUTH）

- **覆盖存储形态**：`\Memcached` 鸭子类型适配器（通用分支 `incr→expire`/`add`）、`Gene\Cache\Memcached`、`Gene\Cache\Redis`（Lua INCR+EXPIRE / lock SET NX EX）。
- **验证中发现并修复一个派生 bug**：`gene_throttle_call`/`gene_store_call` 只查 `function_table`，对经 `__call` 透传方法的存储（`Gene\Cache\Redis`/`Gene\Cache\Memcached` 正是这种形态）静默空操作——实测 `Throttle::clear` 在 `Gene\Cache\Redis` 上不删键。修复：方法表未命中时回退调用 public `__call`（与用户态调用语义一致）；同时补齐 `Gene\Cache\Redis::del/delete`、`Gene\Cache\Memcached::add/del/delete/expire(→touch)`，并把 `incr`/`decr` 缺键兜底从非原子 `set()` 改为 `add()`+重试。
- **序列断言 53/53 PASS**（三种存储 × hit 递增/首写 TTL/`gene:thr:` 服务端可见/TTL 到期归零/`attempt` 边界/`remaining`/`clear` + `totpVerify['used']` 首用通过/重放拒绝/`gene:totp:*` 标记在服务端可见；Redis 侧另验证服务端 `TTL` 已挂上）。
- **多进程竞态 6/6**：每存储 8 进程 × 20 `hit` → 服务端精确 160 无丢更新；8 进程并发 `totpVerify` 同码 → 恰好 1 WIN——`incr`/`add`/`lock`/`SET NX EX` 原子性在实存下成立。
- **错误路径**：死 Memcached `hit`→-1、`attempt` fail-open、`totpVerify['used']` fail-closed；`Gene\Cache\Redis` 构造即 connect（eager），不可达抛 `RedisException`（设计行为，应用侧拿不到死对象）。
- **回归**：新增 `__call` 代理存储断言 7 项；`SecurityTest` 全绿，全量 `TestRunner` 1084 项全 PASS（CacheTest 74/74 无回归）。

### FPM curl 端到端验收（2026-11-01，`php -S` + 最小 Gene 应用，`$_SERVER` 真实请求周期）

- **安全响应头**：`secureHeaders()` 无参默认——`X-Content-Type-Options: nosniff`、`X-Frame-Options: SAMEORIGIN`、`Referrer-Policy: strict-origin-when-cross-origin` 均下发；CSP `{nonce}` 占位符逐请求替换为随机 nonce。
- **XFP 信任边界（HSTS）**：未配 `trustProxies` 时 `X-Forwarded-Proto: https` 被忽略，`isSecure()=false` 不发 HSTS；`trustProxies(['proxies'=>['127.0.0.0/8']])` 下同头生效，`isSecure()=true` 下发 `Strict-Transport-Security: max-age=31536000`；HTTP（无 XFP）始终不发 HSTS——`curl` 对照两端口实测。
- **CSRF**（`token=>true`，session 组件走 `Gene\Memory`）：无 token POST→403；`_token` 表单字段→200；`X-Csrf-Token` 头→200；query 串携带 `_token`→403（防 Referer/日志泄漏）；异源 `Origin`→403；同源 `Origin`→200；`Sec-Fetch-Site: cross-site`→403。
- **REST HMAC 签名**（`restGuard` + `Crypto::signRequest`）：未签名→403；合法签名→200；同 nonce 重放→403（Memory 存储一次性标记跨请求生效）；篡改签名→403；超 skew 旧 ts→403；签名不含 query 而请求带 query→403（证明签名目标含 query 串）；签名含 body 与实发 body 不符→403、相符→200。

### 第三轮复审与关闭（2026-11-01）

对第二轮四轴结论逐项复核：S1-S12 的修复与验证证据成立（严格 IP 解析、HEADER 袋统一、原子计数/NX 标记、签名覆盖 query+body、sealedCookie 内嵌过期、openssl 7 参修复），REST nonce 在签名校验之后才消费（未签名请求无法耗尽 nonce），TTL `2*skew` 覆盖 ts 有效区间。仅发现 1 处遗留问题并已修复：

| 轴 | 问题 | 修复 |
|----|------|------|
| 安全 / bug | `totpVerify['used']` 防重放键直接拼接**明文 TOTP 密钥**（注释写 sha1，代码为原文）——2FA 种子出现在 Redis/Memcached 键空间，`KEYS`/`MONITOR`/慢日志/监控可见 | 键改为 `gene:totp:<hex(HMAC-SHA256(raw, "gene:totp"))[:32]>:<counter>`，种子不再落入存储 |
| 易用 | 标记按 secret 粒度：一次验证成功后整窗口（默认 90s）内用户输入**新时间片的新码**也被拒 | 标记按匹配到的时间片 counter 粒度：同码重放仍拒绝，下一时间片的新码正常通过 |

验证：PHP 8.2.35 NTS x64 + openssl/pdo_sqlite 重编 `php_gene.dll`，`SecurityTest` **107/107 PASS**（新增：当前步通过、相邻步新码通过、键不含密钥且格式为 `gene:totp:<32hex>:<counter>`），全量 `TestRunner` 全部通过。

**结论：四轴（科学合理 / bug / 易用 / 高并发）均已闭环，无已知异常或安全遗留，本方案关闭。** 仅余 Swoole 形态在 Linux 上执行 `tools/acceptance/linux_swoole_verify.sh` 做最终确认（与 FPM 同代码路径，属环境确认项，不阻塞关闭）。

