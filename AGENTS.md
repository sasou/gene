# Gene 扩展 — 构建与验证速查

## Windows 构建（本机已验证 2026-08-20）

- PHP SDK：`F:\php-sdk-2.3.0`（本机实际安装版本）；构建树：`F:\php_src\php-8.1.30-src`（PHP 8.1 NTS x64，VS2019/vs16）。
- `F:\php_src\php-8.1.30-src\ext\gene` 是指向本仓库 `src/` 的 **Junction**，改源码即改构建树。
- 构建步骤（config.nice.bat 已配好 `--enable-gene=shared`）：

```bat
rem task.bat 内容:
cd /d F:\php_src\php-8.1.30-src
call config.nice.bat
nmake php_gene.dll
F:\php-sdk-2.3.0\phpsdk-vs16-x64.bat -t <task.bat>
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

## 验证入口

- 审计复现脚本：`php audit\repro\<name>.php`（每条审计结论可一键复现）。
- 测试套件：`php test\OrmTest.php`、`php test\DatabaseTest.php`、`php test\RouterTest.php`。
- 注意：控制台重定向输出为 UTF-16 是 PowerShell 编码问题，非测试失败。

## 约定

- 缓存：`gene.cache_reserve <= gene.cache_max_items` 属矛盾配置，`workerReady()` 会**自动向上矫正**
  生效 reserve 为 `max_items + max(64, max_items/4)`（只多占内存，不改淘汰语义），并记一次
  warning 提示修正 php.ini；Swoole 模式下该诊断走 `gene_log_diag()` 只写 error_log，
  不触发用户错误处理器（避免 workerStart 内异常导致 worker 无限重启）。
  `workerReady()` 是**幂等**的一次性引导钩子（`worker_ready` 标记早返回），重复调用
  不会重写日志，也不会 post-freeze 扩容 bucket 数组（扩容会移动 arData → 读者裸指针悬垂）。
- Db 驱动（Mysql/Sqlite/Pgsql/Mssql）的 `insert()` 等写方法是**惰性执行**：下一次读调用
  （`lastId()`/`affectedRows()`/`row()`/`all()` 等）才真正执行，重复调用会重复执行。
- ORM：`fill()` 含非空主键即视为已持久化（`exists=1`），`find($id, true)` 返回模型实例
  （hydrate 会调用 **public 且**无必填参数的构造函数；private/protected 构造函数跳过）。**自然主键/UUID 表**请用 `fill($data, false)`、
  `setExists(false)` 或 `Model::create()` 插入；hydrate 模型 `save()` 命中 0 行会发
  `E_NOTICE`（不再静默丢失）。`create()`/`save()` 在 payload 自带主键时原样返回该主键，
  否则返回 `lastId()`（数字串归一为 int）。
