# Gene 扩展 — 构建速查

## 仓库地图

| 目录 | 职责 |
|------|------|
| `src/` | 扩展 C 源码（按模块分子目录） |
| `test/` | 回归测试套件（`TestRunner.php` 驱动），用法与免部署参数见 `test/README.md` |
| `tools/` | 构建脚本（索引见 `tools/README.md`）；`tools/acceptance/` 为 FPM/Swoole 验收工具 |
| `audit/` | 审计档案：时点报告、`repro/` 复现脚本，见 `audit/README.md` |
| `plan/` | 演进计划与审计驱动待办（`audit-backlog.md`）；`.closed.md` 后缀 = 已关闭只读，详见 `plan/README.md` 索引 |
| `docs/` | 用户文档（INI 配置参考等） |
| `demo/` | 示例应用（FPM / CLI / Swoole 入口） |
| `gene-ai-helper/` / `gene-ide-helper/` | AI 协作规则与 skill / IDE stub |

## macOS 构建（phpize，Homebrew / 源码 PHP）

### 前置依赖

- **Xcode Command Line Tools**：`xcode-select --install`
- **PHP 8.0+** 及匹配的 `phpize` / `php-config`（`php` 与 `phpize` 必须来自同一安装）
- **autoconf**（Homebrew：`brew install autoconf pkg-config`）

Homebrew 示例（Apple Silicon / Intel 通用）：

```bash
brew install php@8.1 autoconf pkg-config
export PATH="$(brew --prefix php@8.1)/bin:$PATH"
```

### 一键构建

```bash
# 仓库根目录
chmod +x tools/mac_build.sh
tools/mac_build.sh
```

脚本执行 `phpize` → `./configure --enable-gene=shared --with-php-config=...` → `make`。成功标准：`src/modules/gene.so` 存在且 `php --ri gene` 能加载。不跑 Swoole 验收。

选项：`--install`（`sudo make install`）、`--test`（额外跑 TestRunner）、`--clean`、`--php /path/to/php`。

### 手动构建

```bash
cd src
phpize
./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
make -j$(sysctl -n hw.ncpu)
```

### 免安装确认已加载

```bash
GENE_SO="$(pwd)/src/modules/gene.so"
PHP="$(brew --prefix php@8.1)/bin/php"
"$PHP" -n -d "extension=$GENE_SO" --ri gene
```

### 常见问题

| 现象 | 处理 |
|------|------|
| `phpize: command not found` | 将 `$(brew --prefix php@8.1)/bin` 加入 `PATH` |
| `Cannot find autoconf` | `brew install autoconf` |
| 扩展加载架构不匹配 | `file "$(command -v php)"` 与 `file src/modules/gene.so` 须同为 `arm64` 或 `x86_64` |

## Windows 构建（本机已验证 2026-08-20）

- PHP SDK：`F:\php-sdk-2.6.0`，但部分环境下实际安装的是 `F:\php-sdk-2.3.0`（两者 `phpsdk-vs16-x64.bat` 用法相同）——
  若 2.6.0 路径不存在，先用 `Test-Path`/`Get-ChildItem F:\` 确认实际版本号再调用；构建树：
  `F:\php_src\php-8.1.30-src`（PHP 8.1 NTS x64，VS2019/vs16）。
- `F:\php_src\php-8.1.30-src\ext\gene` 是指向本仓库 `src/` 的 **Junction**，改源码即改构建树。
- 构建步骤（config.nice.bat 已配好 `--enable-gene=shared`）：

```bat
rem task.bat 内容:
cd /d F:\php_src\php-8.1.30-src
call config.nice.bat
nmake php_gene.dll
F:\php-sdk-2.6.0\phpsdk-vs16-x64.bat -t <task.bat>
```

- **注意**：Makefile 必须在 x64 环境下生成（`BUILD_DIR=x64\Release`，不含 `_USE_32BIT_TIME_T`）。
  若 Makefile 被误在 x86 环境下重新 configure，需在 phpsdk-vs16-x64 环境中重跑 `config.nice.bat`。
  新版 Windows SDK (10.0.26100.0) 的 `corecrt.h` 会对 x64 构建中出现的 `_USE_32BIT_TIME_T` 报 `#error`。

- 产物：`F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll`。
- 部署：`copy /Y` 到 `D:\wampServer-php8.1_x64_nts\php_ext\php_gene.dll`。
  **注意**：WampServer 的 httpd/php-cgi 运行时会锁住旧 dll，需先确认无锁再覆盖。

## 免部署验证（旧 dll 被占用时）

```bat
D:\wampServer-php8.1_x64_nts\bin\php.exe -n -d extension_dir=D:\wampServer-php8.1_x64_nts\php_ext -d extension=pdo_sqlite -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll <script.php>
```

## 验证与测试

- 回归测试套件与免部署运行参数：`test/README.md`
- FPM/Swoole 验收工具与发布准入：`tools/acceptance/README.md`
- 审计结论的一键复现脚本：`audit/repro/`（索引约定见 `audit/README.md`）

## 约定

- 缓存：`gene.cache_reserve <= gene.cache_max_items` 属矛盾配置，`workerReady()` 会**自动向上矫正**
  生效 reserve 为 `max_items + max(64, max_items/4)`（只多占内存，不改淘汰语义），并记一次
  warning 提示修正 php.ini；Swoole 模式下该诊断走 `gene_log_diag()` 只写 error_log，
  不触发用户错误处理器（避免 workerStart 内异常导致 worker 无限重启）。
  `workerReady()` 是**幂等**的一次性引导钩子（`worker_ready` 标记早返回），重复调用
  不会重写日志，也不会 post-freeze 扩容 bucket 数组（扩容会移动 arData → 读者裸指针悬垂）。
- Db 驱动（Mysql/Sqlite/Pgsql/Mssql）的 `insert()` 等写方法是**惰性执行**：下一次读调用
  （`lastId()`/`affectedRows()`/`row()`/`all()` 等）才真正执行，重复调用会重复执行。
  `history()` 返回的是**快照**：返回后引擎继续记录新语句，但不会改动调用方已拿到的数组。
- ORM：`fill()` 含非空主键即视为已持久化（`exists=1`），`find($id, true)` 返回模型实例
  （hydrate 会调用 **public 且**无必填参数的构造函数；private/protected 构造函数跳过）。**自然主键/UUID 表**请用 `fill($data, false)`、
  `setExists(false)` 或 `Model::create()` 插入；hydrate 模型 `save()` 命中 0 行会发
  `E_NOTICE`（不再静默丢失）。`create()`/`save()` 在 payload 自带主键时原样返回该主键，
  否则返回 `lastId()`（数字串归一为 int）。
- `Gene\Di` 注册表是**请求/协程级**（`ctx->di_regs`）：Swoole 下 workerStart 回调跑在独立协程，
  其中 `Di::set` 对 onRequest 协程不可见——worker 级共享服务请用 `Config` 定义
  `class`/`params` 惰性实例化（`gene_di_get` 的 config 缓存回落），或在 onRequest 内注册。
- `Router->error()`/`hook()` 首参（事件名）接受整数等标量：`->error(404, ...)` 会注册为
  `error:404`；非标量回落空名 `error:` 兜底。`hook(503, ...)`、`runError('404')` 同理。
- `Validate::name($f)` 同时写 KEY 与 FIELD：`name('x')->rule_email()` 等 `rule_*` 直调可用且
  返回真实校验结果；FIELD 在 `valid()`/`groupValid()` 内仍按逗号拆分逐字段覆盖。
  `rule_int` 仅认 `IS_LONG`（数字串校验用 `digit`）。
- `Gene\Memory` 有 `delete`（`del` 别名）：满足 Session 存储句柄契约 get/set/delete，
  可作 `session.driver` 的零依赖本地实现。
- demo 自包含验收：`GENE_DEMO_LOCAL=1` 时 `config.ini.php` 将 db 切到
  `demo/database/gene_demo.db`（`demo/database/init_sqlite.php` 幂等初始化）、session driver 与
  cache hook 切到 `localStore`（`Ext\LocalStore`：包 `Gene\Memory`，数组 key 走 `mget`，
  补 `delete` 别名），swoole 入口只建 `dbPool`；`linux_swoole_verify.sh --demo` 用该模式跑
  /healthz + /metrics + wrk，web 阶段固定跑仓库内 `demo/`，不依赖任何外部应用。profile 见 `tools/acceptance/README.md`。
- 视图变量与 DI 是两套存储：`assign()` 写入的变量在模板里以裸 `$name` 访问（extract 进
  符号表）；`View::__get/__set`（模板内 `$this->x`）走 `gene_di_get_class` 解析 DI 组件，
  读不到 assign 的值。控制器内同理，`$this->view->x` 不是取视图变量的方式。
