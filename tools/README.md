# tools/ — 构建与验收工具

## 构建脚本

| 脚本 | 平台 | 用途 |
|------|------|------|
| `mac_build.sh` | macOS | `phpize` → `configure --enable-gene=shared` → `make` 一键构建；`--install` / `--test` / `--clean` / `--php PATH` |
| `build_all.bat` | Windows | 经 PHP SDK 批量构建 x64/x86 × PHP 8.1–8.5 矩阵；用法 `build_all.bat [all|x64|x86] [all|8.1..8.5]`，环境变量 `PHP_SDK_ROOT` / `PHP_SRC_ROOT` |
| `task_build_x64.bat` / `task_build_x86.bat` | Windows | `build_all.bat` 调用的单架构构建任务（由 `phpsdk-*.bat -t` 启动） |
| `task_gene_build.bat` | Windows | 单源码树构建任务：`buildconf` → `configure` → `nmake`，全部可用 `GENE_*` 环境变量覆盖 |
| `build_gene_task.bat` | Windows | 最小化本机任务（`config.nice.bat` + `nmake php_gene.dll`），对应根 `AGENTS.md` 的 Windows 构建流程 |

## 验收工具

`acceptance/` 为 FPM/Swoole 验收套件（含 `verify_5_6_6*.php`、`swoole_entry_verify.php`、`linux_swoole_verify.sh` 等），用法与判定规则见 `acceptance/README.md`。
