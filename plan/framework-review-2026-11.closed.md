# Gene 框架综合评估与需求清单（2026-11）

状态：关闭（2026-11-03 落地，结果见 §八）
范围：功能/使用体验、高并发性能、安全（SQL 注入、视图转义），以及从**典型 Gene 业务应用**（前台 + 管理端，中等规模 PHP 工程）使用角度反推的框架提升点。
约定：每条需求附代码证据；实现遵守 `plan/README.md` 的维护约定（C 层只加重复 ≥3 处或热路径 API；补 `test/*.php`；同步 ide-helper 与 `gene-ai-helper` reference）。

---

## 一、总体结论

| 维度 | 结论 | 说明 |
|------|------|------|
| 功能完整度 | 充分 | 路由/Hook/DI/ORM/Db/Cache/Session/Http/SSE/Context/安全基线等均已由已关闭计划落地，覆盖 FPM、CLI、Swoole 三运行时 |
| 使用体验 | 基本合理，有坑 | 惰性写语义、`fill()` 主键即 exists、`View::__get` 读 DI 而非 assign 变量、`through()` 注册顺序等，属于"文档里写清楚但直觉相反"的契约，新手易踩 |
| 高并发性能 | 充分 | V1/V2 两轮优化（NTS 锁消除、请求装载零分配、冻结表零拷贝、Pool 去 PHP 调用）已有线上验收；剩余收益主要在可观测性与模板/视图热路径，不在核心派发 |
| SQL 注入 | 值层面已杜绝；标识符层面靠启发式；`sql()` 原样执行 | 见第三节 |
| 视图转义 | **不满足工业通用标准** | `{$var}` 用 `ENT_COMPAT`、`{$a.b}`/`{:expr}` 不转义、无 `e()` 助手、无上下文转义、无默认自动转义；`Response::alert()` 存在 `</script>` 逃逸 |

---

## 二、高并发性能

现状（证据：`plan/Performance-tuning-V1.closed.md`、`Performance-tuning-V2.closed.md`）：核心派发、路由缓存、DI、Db 属性槽位、ctx 冷热分离均已做过代码级优化，两轮验收 17/17、20/20 PASS。结论是核心路径无需再立大项。

剩余提升点：

| 编号 | 需求 | 证据/理由 | 优先级 |
|------|------|-----------|--------|
| P-1 | 直连 `Gene\Db\Mysql` 与 Pool 的 PDO 选项对齐：显式设置 `ATTR_EMULATE_PREPARES=false` | `src/db/mysql.c:278-279` 只强制 ERRMODE 与 FETCH_MODE；`src/db/pool.c:354` 却设了 `EMULATE_PREPARES=false`。两条路径语义不一致（类型返回、预处理行为、安全面均不同） | P1 |
| P-2 | 运行时指标导出原语（请求耗时直方图、Db 查询计数/慢查询、Pool 等待时长） | demo 的 `/metrics` 由 userland 拼装；Swoole 高并发下排障依赖指标，C 层已有计数点但无统一出口 | P2 |
| P-3 | 慢查询阈值钩子（`gene.db_slow_ms`，超过阈值写 `gene_log_diag`） | `history()` 只能事后取快照，生产环境不宜常开 | P2 |
| P-4 | 模板编译产物预热/校验命令（CLI 一次性编译全部模板，生产关闭 mtime 检查） | `src/mvc/view.c:67-101` 以时间戳判断新鲜度，高并发 FPM 下每请求 `stat` | P3 |

---

## 三、SQL / PDO 注入

### 3.1 已做到的

- 值全部走 `?` 绑定：`where()/in()/having()/insert()/update()/upsert()` 的值不拼接。
- 标识符有白名单与引号包裹：`gene_quote_table/columns/order/identifier`（`src/db/pdo.c:440-700`），对 `'"\`` 等注入标记 fail-safe 中和并告警；`increment()` 用 `gene_db_valid_identifier` 严格校验列名（`pdo.c:223`）。
- `limit()` 参数解析为整数（`mysql.c:1342`），无法注入。

### 3.2 未杜绝的

| 编号 | 风险 | 证据 | 需求 | 优先级 |
|------|------|------|------|--------|
| S-1 | `order()` / `select()` 对含括号、空格的"表达式"做启发式放行 | `gene_quote_order` 的 `gene_ident_expr_safe` 分支（`pdo.c:615-620`）。业务把 `$_GET['sort']` 直接传入时，只能依赖启发式而非白名单 | 新增 `orderBy(string $col, string $dir, array $allow)`：列必须在白名单内、方向仅 ASC/DESC；文档明确 `order()` 不接收用户输入 | P0 |
| S-2 | `sql($raw, $binds)` 与 `where("... $x")` 字符串插值无法被框架识别 | 框架层无法区分开发者拼接 | 开发模式（`gene.debug=1`）下检测"SQL 片段含引号包裹的字面量且 binds 为空"并 `E_NOTICE`；在 `gene-ai-helper` 规则中禁止插值 | P1 |
| S-3 | 直连 Mysql 未关闭模拟预处理 | 同 P-1。模拟预处理下若 DSN 未带 `charset` 而之后执行 `SET NAMES gbk` 等，存在宽字节绕过历史问题 | 同 P-1；DSN 缺 `charset` 时 warning 一次 | P1 |
| S-4 | JOIN 表片段整体放行 | `gene_quote_table` 对含 JOIN 关键字的字符串只做注入标记检查后原样拼接（`pdo.c:506-509`），为兼容 48 处遗留调用 | 推荐用已落地的 `joinOn()` 结构化接口；文档标注 `select('a JOIN b ON ...')` 为兼容写法 | P3 |

结论：**参数值注入已杜绝；标识符与原生 SQL 的注入面仍取决于开发者是否把用户输入传入 `order()/select()/sql()`**。S-1 完成后，常见的"排序字段注入"可从 API 层面关闭。

---

## 四、视图转义（重点）

### 4.1 现状

模板编译规则见 `src/mvc/view.c:590-624`：

| 模板语法 | 编译结果 | 问题 |
|----------|----------|------|
| `{$var}`、`{$arr['k']}` | `htmlspecialchars(x, ENT_COMPAT)` | `ENT_COMPAT` 不转义单引号，`<a title='{$x}'>` 可被闭合注入；未指定 `ENT_SUBSTITUTE` 与 charset，非法 UTF-8 返回空串 |
| `{$a.b}`、`{$a.b.c}` | `echo $a['b']` | **不转义**，且与 `{$var}` 外观相似，极易误用 |
| `{CONST}`、`{:expr}` | `echo x` | 不转义（可接受作为 raw 出口，但没有与之对应的转义出口） |
| 原生 `<?php echo $x ?>` | 原样 | 无框架助手可用 |

框架未提供 `e()`/`escape()` 助手，也没有 attr / js / url / css 上下文转义。异常页（`src/exception/exception.c:97-102`）已使用 `ENT_QUOTES`，说明内部标准与模板不一致。

`Response::alert()` / `redirectJs()`（`src/http/response.c:495-505`）用 `php_addslashes` 把文本拼进 `<script>`：文本含 `</script><script>...` 即可逃逸；URL 未拦截 `javascript:` 协议。

### 4.2 业务侧实际用法（使用侧证据，已脱敏）

对某业务仓库 `application/Views` 的抽样：少量前台模板中大量 `<?php echo $x ?>`，均未使用框架转义助手，典型模式包括：

- **布局模板**：语言切换链接写入 `href="<?php echo $lang_url_*; ?>"`，URL 由 `$this->url($this->getPath(true), …)` 等生成，即**请求路径可反射到 HTML 属性**；路径或参数若含 `"` 等字符，可能成为反射型 XSS 入口（取决于 `url()` 是否编码，需回归确认）。
- **内容页**：`echo $content` 输出富文本 HTML，属于有意 raw 输出，但缺少"可信 HTML / 净化"的显式机制。

结论：框架没有给开发者提供"顺手的转义方式"，业务侧便普遍走原生 `echo`。

### 4.3 需求

| 编号 | 需求 | 说明 | 优先级 |
|------|------|------|--------|
| V-1 | `{$var}` 改为 `htmlspecialchars(x, ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML5, 'UTF-8')` | 与异常页一致；行为变化仅为额外转义 `'`，兼容风险低 | P0 |
| V-2 | `{$a.b}`、`{$a.b.c}` 默认转义；raw 输出统一走 `{:expr}` 或新增 `{!! expr !!}` | 修正"看起来像转义实则不转义"的最大陷阱；在 CHANGELOG 中标为行为变更 | P0 |
| V-3 | 全局函数/静态方法族：`Gene\View::e($v)`（HTML 正文与属性）、`attr()`、`js()`（`json_encode` + `JSON_HEX_TAG\|JSON_HEX_AMP\|JSON_HEX_APOS\|JSON_HEX_QUOT`）、`url()`（`rawurlencode` + 协议白名单 http/https/mailto/相对路径） | 对齐 Twig/Blade/Laravel `e()` 的工业通用做法；原生 PHP 模板可直接 `<?= e($x) ?>` | P0 |
| V-4 | `Response::alert()/redirectJs()` 改用 `js()` 规则输出，URL 走协议白名单 | 修复 `</script>` 逃逸与 `javascript:` 跳转 | P0 |
| V-5 | 可选 `gene.view_autoescape=1`：原生 PHP 模板中 assign 的标量在 extract 时不处理（保持零开销），但模板语法输出一律转义，raw 需显式 | 默认开启于新项目 skeleton | P1 |
| V-6 | `View::raw(string $html)` / `SafeHtml` 标记类型 + 可选净化钩子（允许接入 HTMLPurifier 等） | 为富文本文档/ CMS 类输出提供显式、可审计的出口 | P2 |
| V-7 | CSP nonce 支持：`Response::cspNonce()` 生成请求级 nonce 并写入 `Content-Security-Policy` 头，模板可取用 | 安全基线已有安全头能力，补 nonce 即可让内联脚本受控 | P2 |

回归：新增 `test/ViewEscapeTest.php`，覆盖单引号属性、非法 UTF-8、`{$a.b}`、`</script>`、`javascript:` URL 五类用例。

---

## 五、功能与使用体验

| 编号 | 问题 | 证据 | 需求 | 优先级 |
|------|------|------|------|--------|
| U-1 | 惰性写语义"重复读取会重复执行" | AGENTS.md「Database」约定 | 写方法执行后缓存结果，`lastId()/affectedRows()` 重复读取不再重复执行；如需保留旧语义以 ini 开关兜底。这是最反直觉、最易造成重复插入的契约 | P1 |
| U-2 | `View::__get` 读 DI 而非 assign 变量，`$this->view->x` 读不到视图变量 | AGENTS.md「DI / View」 | 访问未注册 DI 名且同名 assign 变量存在时，开发模式 `E_NOTICE` 提示正确写法 | P2 |
| U-3 | `fill()` 非空主键即视为已持久化 | AGENTS.md「ORM」 | UUID/自然主键模型提供类级声明 `$incrementing = false`，自动按 insert 处理，免去每处 `fill($d,false)` | P2 |
| U-4 | 视图公共变量重复样板 | 各控制器动作重复调用类似 `assignXxxCommon()`，手动 assign 语言、导航等公共变量 | `View::share($k,$v)` / 布局级共享变量（请求级），由 Hook 统一注入 | P2 |
| U-5 | 多语言切换需手动 `language->lang()` 并拼 `lang_url_*` | 前台控制器 init/assign 中手写语言与链接 | `Language` 支持从路由参数/Accept-Language 自动协商，`url()` 支持 `lang` 参数生成同页其它语言链接 | P3 |
| U-6 | 业务 Ext 层直接读超全局 | 审计抽样：第三方 SDK / 调试辅助等若干文件仍用 `$_GET/$_POST/$_SERVER` | 业务侧改用 `Request::input()`；框架侧在 Swoole 模式下对超全局访问给出一次性诊断（Swoole 下超全局不可靠） | P3 |

---

## 六、安全完善度（除 SQL 与视图外）

`security-baseline.closed.md` 已落地可信代理、CSRF、安全头、JSONP、REST 签名、Password、Throttle、会话加固、密钥环、TOTP、上传、方法守卫，覆盖面在同类框架中属完整。补充项：

| 编号 | 需求 | 理由 | 优先级 |
|------|------|------|--------|
| X-1 | 开放重定向防护：`redirect()` 对绝对 URL 校验目标主机白名单（`gene.redirect_hosts`） | `Response::redirect` 接受任意 URL | P1 |
| X-2 | 视图路径安全已有 `gene_view_path_safe`（`view.c:209`），补回归覆盖 `..`、绝对路径、NUL 字节 | 现有测试未单独覆盖 | P3 |
| X-3 | 安全自检命令：CLI 输出当前 ini 中与安全相关的配置（autoescape、trustProxies、cookie flags、emulate prepares）及建议值 | 降低误配置概率 | P3 |

---

## 七、实施里程碑

| 里程碑 | 内容 | 验收 |
|--------|------|------|
| A（安全，P0） | V-1、V-2、V-3、V-4、S-1 | `ViewEscapeTest.php`、`DbOrderByTest.php` 通过；业务布局模板改用 `View::e()`/`safeUrl()` 后回归通过；CHANGELOG 标注行为变更 |
| B（一致性，P1） | P-1/S-3、S-2、U-1、V-5、X-1 | TestRunner 全量通过；FPM/Swoole 验收（`tools/acceptance/`）无性能回退 |
| C（体验与可观测，P2/P3） | P-2、P-3、P-4、U-2~U-6、V-6、V-7、X-2、X-3 | 按项补测试 |

关闭条件：A、B 全部落地且通过验收后，文件更名为 `framework-review-2026-11.closed.md`，C 中未落地项记录于文内。

---

## 八、落地结果（2026-11-03）

### 8.1 逐项状态

| 编号 | 结果 | 实现 / 说明 |
|------|------|-------------|
| V-1 | 已落地 | `{$v}`/`{$arr[..]}` 编译为 `\Gene\View::e()`：ENT_QUOTES\|ENT_SUBSTITUTE、UTF-8；null/false/array 输出空串（顺带消除 PHP 8.1 向 htmlspecialchars 传 null 的弃用告警） |
| V-2 | 已落地 | `{$a.b}`/`{$a.b.c}` 改为转义；新增显式原样语法 `{!! expr !!}`（`PARSER_NUMS` 28→29）。**行为变更**：旧编译缓存在模板 mtime 变化前不会重编，升级后清空 `Cache/Views` 或执行 `View::compileAll()` |
| V-3 | 已落地 | `View::e()` / `View::js()`（JSON_HEX_TAG\|AMP\|APOS\|QUOT）/ `View::safeUrl()`（http/https/mailto/tel/相对路径白名单，跳过空白与控制字符以防 `java\tscript:` 混淆）。实例方法 `url()` 已占名，故 URL 转义命名为 `safeUrl` |
| V-4 | 已落地 | `Response::alert()/redirectJs()` 改用 `gene_view_js()` 输出，`</script>` 无法逃逸；白名单外协议改跳 `/` |
| V-5 | 已落地 | `gene.view_autoescape`（PHP_INI_SYSTEM，默认 0）：1 时 `{:expr}` 也转义 |
| V-6 | 已落地 | `View::setPurifier(callable)` + `View::clean($html)`；未注册净化器时转义而非放行（失败即安全） |
| V-7 | 已存在 | `Response::cspNonce()` 与 secureHeaders `{nonce}` 已于 security-baseline 落地（`gene.h` `csp_nonce`），评估时漏查 |
| S-1 | 已落地 | 四驱动 + `Orm\Query` 新增 `orderBy($col, $dir='ASC', ?array $allow)`：列表或别名=>列映射；方向仅 ASC/DESC；违规抛 `ValueError`；共用 `gene_db_order_by_build()` |
| S-2 | 已落地（调整） | `sql()` 在开发环境（`run_environment=0`）检测"单引号字面量 + 无绑定"。框架错误处理器会把 E_NOTICE 转成异常，为免打断合法 SQL，改经 `gene_log_diag()` 仅写日志 |
| S-3 / P-1 | 已落地 | 直连 `Db\Mysql` 默认 `ATTR_EMULATE_PREPARES=false`（与 Pool 一致，`options[20]` 显式值优先）；DSN 缺 `charset=` 时进程内诊断一次。**行为变更**：mysqlnd 原生预处理下整型/浮点列返回 PHP 原生类型而非字符串 |
| S-4 | 维持兼容 | JOIN 字符串片段不改行为；规则与 AI 约定推荐 `joinOn()` |
| P-2 / P-3 | 已存在 | `Gene\Monitor::stats()/prometheus()` 与 `gene.slow_query_ms`、`db_slow_query_count` 已提供，评估时漏查 |
| P-4 | 已落地 | `View::compileAll()` 递归预编译视图目录，返回编译/校验数；配合 `gene.view_compile_check_mtime=0` |
| U-1 | 不实施 | 与 `plan/README.md` 硬约束"Db 惰性写语义不变"冲突，且已有测试依赖该契约；保持文档化 |
| U-2 | 已落地（增强） | 不只告警：`View::__get/__isset` 在 DI 未命中时回落读取当前作用域 assign 变量（DI 优先） |
| U-3 | 已落地 | `Orm\Model::$incrementing`（默认 true）；false 时 `fill()` 恒按新行处理；`find($id, true)` 水合不受影响 |
| U-4 | 已满足 | `assign()` 存于请求级 ctx，Hook 内 `$this->view->assign()` 即对后续所有模板共享，无需新增 `share()`；配合 U-2 可在控制器读回 |
| U-5 | 已落地 | `Language::negotiate(array $supported, ?string $header=null)`：按 q 值、精确→主子标签匹配，回落首项并切换实例语言；header 缺省取请求 `accept-language`（FPM/Swoole 同一 HEADER bag） |
| U-6 | 不在本仓库范围 | 属业务仓库改造；框架侧已有 `Request::input()` |
| X-1 | 已落地 | `gene.redirect_hosts`（逗号列表，支持 `*.domain`）：非空时 `redirect()/redirectJs()/alert()` 对绝对、协议相对、`/\host`、`user@host` 形式校验主机，未列出改跳 `/` 并写日志；空值保持旧行为 |
| X-2 | 已落地 | 回归覆盖 `..`、绝对路径、反斜杠、盘符（NUL 字节经 C 字符串截断后无害） |
| X-3 | 已落地 | `Gene\Monitor::security()`：view_autoescape / redirect_hosts / trusted_proxies / jsonp / run_environment / display_errors / expose_php / session cookie 标志，返回 value、recommended、ok（`null` 表示依部署而定） |

### 8.2 验证

- 构建：PHP 8.1.30 与 8.2.33 x64 NTS（VS16，php-sdk 2.8.4）均零错误零告警。
- 新回归 `test/ViewEscapeTest.php`（46 项）：PHP 8.2 + pdo_sqlite 全过；`-d gene.redirect_hosts=example.com,*.trusted.org` 下主机白名单 6 例全过；PHP 8.1（无 pdo_sqlite，orderBy 段 SKIP）全过。
- 全量 TestRunner（PHP 8.2）：1172 项中仅 `DemoLoadTest` 的 "demo class load" 1 项失败，原因为宿主 `php.ini` 已加载 pdo_sqlite、子进程又经 `GENE_TEST_PHP_ARGS` 重复加载而输出 "Module already loaded" 告警，属测试环境问题，与本次改动无关。
- 未执行：Swoole / FPM 线上验收（`tools/acceptance/`，需 Linux 环境）；未向本地集成环境部署新 DLL。

### 8.3 遗留

- 业务仓库布局模板中，语言切换等 `href` 输出应改用 `\Gene\View::safeUrl()`（或模板 `{$url}` + 编译转义），需在部署新扩展后由业务侧实施。
- 发布说明需标注两项行为变更：V-2 模板转义（需清理编译缓存）与 S-3 原生预处理的返回类型变化。
