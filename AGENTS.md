# Gene 扩展协作指南

本文记录仓库导航、常用构建/验证命令，以及修改代码时必须保持的行为约定。各子系统的详细用法以对应目录的 `README.md` 为准。

## 仓库导航

| 目录 | 职责 |
|------|------|
| `src/` | 扩展 C 源码，按模块分目录 |
| `test/` | `TestRunner.php` 驱动的回归测试；用法及免部署参数见 `test/README.md` |
| `tools/` | 构建脚本索引见 `tools/README.md`；FPM/Swoole 验收工具位于 `tools/acceptance/` |
| `audit/` | 时点审计报告及 `repro/` 复现脚本；索引见 `audit/README.md` |
| `plan/` | 演进计划；`.closed.md` 表示已关闭、只读，索引见 `plan/README.md` |
| `docs/` | 用户文档，包括 INI 配置参考 |
| `demo/` | FPM、CLI、Swoole 示例应用 |
| `gene-ai-helper/` | AI 协作规则与 skill |
| `gene-ide-helper/` | IDE stub |

## 构建

### macOS（phpize）

#### 前置依赖

- Xcode Command Line Tools：`xcode-select --install`
- PHP 8.0+，且 `php`、`phpize`、`php-config` 必须来自同一安装
- `autoconf` 和 `pkg-config`

Homebrew 示例（Apple Silicon / Intel 通用）：

```bash
brew install php@8.1 autoconf pkg-config
export PATH="$(brew --prefix php@8.1)/bin:$PATH"
```

#### 一键构建

在仓库根目录执行：

```bash
chmod +x tools/mac_build.sh
tools/mac_build.sh
```

脚本依次执行 `phpize`、`./configure --enable-gene=shared --with-php-config=...` 和 `make`，不运行 Swoole 验收。可用选项：

- `--install`：执行 `sudo make install`
- `--test`：额外运行 TestRunner
- `--clean`：清理后构建
- `--php /path/to/php`：指定 PHP

成功标准：生成 `src/modules/gene.so`，且 `php --ri gene` 可加载扩展。

#### 手动构建

```bash
cd src
phpize
./configure --enable-gene=shared --with-php-config="$(command -v php-config)"
make -j"$(sysctl -n hw.ncpu)"
```

#### 免安装验证

```bash
GENE_SO="$(pwd)/src/modules/gene.so"
PHP="$(brew --prefix php@8.1)/bin/php"
"$PHP" -n -d "extension=$GENE_SO" --ri gene
```

#### 常见问题

| 现象 | 处理 |
|------|------|
| `phpize: command not found` | 将 `$(brew --prefix php@8.1)/bin` 加入 `PATH` |
| `Cannot find autoconf` | 执行 `brew install autoconf` |
| 扩展加载架构不匹配 | 确认 `file "$(command -v php)"` 与 `file src/modules/gene.so` 同为 `arm64` 或 `x86_64` |

### Windows（本机环境）

以下环境已于 2026-09-23 验证：

| 项目 | 路径/配置 |
|------|-----------|
| PHP SDK | `F:\php-sdk-2.8.4`（2.4.0 起改用 `Get-CimInstance` 探测架构，不依赖 `wmic`）；旧版已移至 `F:\php-sdk-2.3.0.bak` 停用 |
| PHP 源码树 | `F:\php_src\php-8.1.30-src`（PHP 8.1 NTS x64，VS2019/vs16） |
| Gene 源码 | `F:\php_src\php-8.1.30-src\ext\gene` 是指向本仓库 `src/` 的 Junction |
| 构建产物 | `F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll` |
| 部署目录 | `D:\wampServer-php8.1_x64_nts\php_ext\php_gene.dll` |

若 `F:\php-sdk-2.8.4` 不存在，先用 `Get-ChildItem F:\ -Directory -Filter "php-sdk*"` 确认实际 SDK 版本。各版本的 `phpsdk-vs16-x64.bat` 用法相同。

`config.nice.bat` 已包含 `--enable-gene=shared`。创建任务文件，例如 `task.bat`：

```bat
cd /d F:\php_src\php-8.1.30-src
call config.nice.bat
nmake php_gene.dll
```

再通过 x64 SDK 环境执行：

```bat
F:\php-sdk-2.8.4\phpsdk-vs16-x64.bat -t task.bat
```

#### Windows 构建注意事项

- 遇到 `Unsupported OS arch` 或 `'wmic' 不是内部或外部命令`：仅 php-sdk ≤2.3.0 用 `wmic` 探测架构，Windows 11 25H2 起已移除 `wmic`。≥2.4.0（本机为 2.8.4）无此问题；若使用旧版，调用 SDK 前设置 `PHP_SDK_OS_ARCH_NUM=9`（9 表示 x64）。
- Makefile 必须在 x64 环境生成：`BUILD_DIR=x64\Release`，且不得包含 `_USE_32BIT_TIME_T`。若曾在 x86 环境重新 configure，须在 `phpsdk-vs16-x64` 环境重跑 `config.nice.bat`。
- Windows SDK 10.0.26100.0 的 `corecrt.h` 会对 x64 构建中出现的 `_USE_32BIT_TIME_T` 报 `#error`。
- nmake 的 .dep 不跟踪 `ext/gene` 内部头文件依赖：`src/gene.h`（`zend_gene_globals` 布局）等头文件变更后，旧 .obj 不会自动重编，新旧对象混链会导致全局结构体字段错位（典型症状：路由派发报 `Gene Unknown Router Cache`，`cache_layer_memory_write_depth` 读出垃圾值）。`task_build_x64/x86.bat` 已在 nmake 前删除 `ext\gene\*.obj`，手动增量编译时若改了公共头文件，须先删 `x64\Release\ext\gene\*.obj`（x86 为 `Release\ext\gene\*.obj`）。
- Windwos下需要保持src/*.c,*.h 为 UTF-8 with BOM编码，不然会出现warnings。
- 部署前确认 WampServer 的 httpd/php-cgi 未锁定旧 DLL，再执行覆盖。

## 验证与测试

| 范围 | 入口 |
|------|------|
| 回归测试、单文件测试、免部署参数 | `test/README.md` |
| FPM/Swoole 验收及发布准入 | `tools/acceptance/README.md` |
| 审计结论一键复现 | `audit/repro/`，索引约定见 `audit/README.md` |

旧 DLL 被占用时，可直接加载新产物进行免部署验证：

```bat
D:\wampServer-php8.1_x64_nts\bin\php.exe -n -d extension_dir=D:\wampServer-php8.1_x64_nts\php_ext -d extension=pdo_sqlite -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll <script.php>
```

## 行为约定

修改相关模块时，必须保持以下语义，并补充对应回归测试。

### Cache / Worker

- `gene.cache_reserve <= gene.cache_max_items` 是矛盾配置。`workerReady()` 会将有效 reserve 向上修正为 `max_items + max(64, max_items/4)`；该修正只增加内存占用，不改变淘汰语义，并仅记录一次 warning，提示修正 `php.ini`。
- Swoole 模式下，上述诊断通过 `gene_log_diag()` 仅写入 `error_log`，不得触发用户错误处理器，以免 workerStart 中的异常导致 worker 无限重启。
- `workerReady()` 是幂等的一次性引导钩子。`worker_ready` 标记命中后直接返回：不得重复写日志，也不得在 freeze 后扩容 bucket 数组；扩容会移动 `arData`，导致读者持有的裸指针悬垂。

### Database

- Mysql、Sqlite、Pgsql、Mssql 驱动的 `insert()` 等写方法采用惰性执行：下一次读取 `lastId()`、`affectedRows()`、`row()`、`all()` 等结果时才真正执行；重复读取会重复执行。
- `history()` 返回快照。返回后引擎可以继续记录新语句，但不得修改调用方已取得的数组。

### ORM

- `fill()` 收到非空主键时，将模型视为已持久化（`exists=1`）。自然主键或 UUID 表插入时，应使用 `fill($data, false)`、`setExists(false)` 或 `Model::create()`。
- `find($id, true)` 返回模型实例。hydrate 只调用 public 且无必填参数的构造函数；跳过 private/protected 构造函数。
- hydrate 模型执行 `save()` 命中 0 行时发出 `E_NOTICE`，不得静默丢失。
- `create()` / `save()` 的 payload 自带主键时原样返回该主键；否则返回 `lastId()`，其中数字字符串归一为 int。
- `versionKeys`（版本键 => 行内列名）的失效是行级的：写入前按主键或同一 where 预读受影响行，映射列发生变更时同时失效新旧值；payload 未包含的映射列按其当前行值失效。非主键 `updateBy`/批量删除的预读受 `$versionScanLimit`（默认 1000）限制，超限告警并跳过失效而非部分失效；预读 SELECT 与 UPDATE 在事务外非原子，需要严格失效时应放进 `transaction()` 内执行。事务内 bump 按 PDO 连接分桶，仅在本连接 commit 后冲刷、rollback 丢弃（回归：`OrmTest::testVersionKeysPerConnection`）。模型静态属性（`$table`/`$fields`/`$versionKeys`/`$versionScanLimit` 等）在 C 层父类中无类型声明，子类不得加 PHP 类型，应使用 PHPDoc。
- `Model::page($where, $page, $perPage, $order = null)` 内部派发到被调类的 `paginate($where, $offset, $limit, $order)`，子类覆盖生效；`$order` 可传 `null`。`flip($id, $field, $values = [0, 1])` 单条 UPDATE 翻转，整型值内联为整数字面量、布尔按驱动输出 `TRUE/FALSE` 或 `1/0`，字符串走绑定。
- `where()/in()/having()` 的字符串片段若以 `and`/`or` 连接词开头（老版 Db 契约写法，如 `in(' and x in(?)', $ids)`，`demo` 中 `Group::delAll` 同款），`Query` 回放层会剥掉该连接词，再按 `where_started` 重发 ` AND ` 或 ` OR `（`or` 保留原语义）；仅含连接词的片段连同其 bind 一并丢弃。连接词仅在后随空白或 `(` 时识别，`android_id`/`or_id` 等列名不受影响。直连 `Db::where()/Db::in()` 不做剥离，保持 v1 verbatim 语义。

### DI / View

- `Gene\Di` 注册表存于 `ctx->di_regs`，作用域为请求/协程。Swoole 的 workerStart 回调运行在独立协程，其中的 `Di::set` 对 onRequest 协程不可见。
- worker 级共享服务应通过 `Config` 的 `class` / `params` 定义并惰性实例化（`gene_di_get` 的 config 缓存回落），或在 onRequest 内注册。
- 视图变量与 DI 是两套存储：`assign()` 写入的变量在模板中通过裸 `$name` 访问（extract 到符号表）；`View::__get/__set`（模板中的 `$this->x`）通过 `gene_di_get_class` 解析 DI 组件，无法读取 assign 变量。控制器中的 `$this->view->x` 同样不是读取视图变量的方式。

### Router / Validate

- `Router->error()` / `hook()` 的事件名接受整数等标量。`->error(404, ...)` 注册为 `error:404`；非标量回落为空事件名 `error:`。`hook(503, ...)`、`runError('404')` 语义相同。
- `through()` 组钩子在路由注册时即时组合为 `__group_*` 链，所有被引用的 `hook()` 必须先于该 through 组注册——事件表缺失或未注册名会抛 `ValueError: named hook 'x' is not registered`（旧行为是静默丢弃，等于认证旁路）。回归：`RouterTest::testGroupHookOrdering`。
- `Validate::name($f)` 同时写入 KEY 和 FIELD，因此 `name('x')->rule_email()` 等 `rule_*` 直调必须可用并返回真实校验结果。FIELD 在 `valid()` / `groupValid()` 中仍按逗号拆分并逐字段覆盖。
- `rule_int` 仅接受 `IS_LONG`；数字字符串应使用 `digit` 校验。

### Memory / Session

- `Gene\Memory` 提供 `delete`，`del` 是其别名。它满足 Session 存储句柄的 `get` / `set` / `delete` 契约，可作为 `session.driver` 的零依赖本地实现。

### Demo / Acceptance

- 设置 `GENE_DEMO_LOCAL=1` 后，`config.ini.php` 将数据库切换为 `demo/database/gene_demo.db`（由 `demo/database/init_sqlite.php` 幂等初始化），并将 session driver 和 cache hook 切换为 `localStore`。
- `Ext\LocalStore` 封装 `Gene\Memory`：数组 key 使用 `mget`，并补充 `delete` 别名；Swoole 入口仅创建 `dbPool`。
- `linux_swoole_verify.sh --demo` 使用该模式验证 `/healthz`、`/metrics` 和 wrk；web 阶段固定运行仓库内 `demo/`，不依赖外部应用。profile 说明见 `tools/acceptance/README.md`。
