# audit/ — 审计档案

本目录存放**源码审计产生的文档与证据**，与 `plan/`（产品驱动计划）分工不同：这里的条目一律来自审计、压测、ASAN 等实证来源。

## 目录结构

| 路径 | 职责 | 维护规则 |
|------|------|----------|
| `AUDIT_REPORT_*.md` | 时点审计报告（**只读归档**） | 按日期命名，发布后不改写；遗留项由 `plan/audit-backlog.md` 承接 |
| `repro/` | 审计结论的一键复现/探针脚本 | 每条报告结论对应可执行脚本，头部注释写明前提与用法 |
| `results/` | 验收输出目录（运行期生成，不入库） | `tools/acceptance/run_acceptance.php --output` 的默认落点 |

审计驱动的**未落地/待验证**待办统一维护在 [`plan/audit-backlog.md`](../plan/audit-backlog.md)（2026-09-13 由 `audit/plan/PLAN.md` 迁入，与本目录产品计划同址管理）；历史报告中出现的 `audit/plan/PLAN.md` 路径即指该文件。

## 使用方式

- 复现某条审计结论：`php audit/repro/<name>.php`（个别脚本需 `-d` 开关或 Swoole 环境，以脚本头注释为准；无环境时应显式 SKIP，禁止假通过）。
- `.py` 脚本为静态分析工具（如 arginfo/zpp 全量比对），与 `.php` 复现脚本同属证据。
- 2026-07-30 之前的报告已按 `plan/audit-backlog.md` 文末「报告关闭台账」关闭并从工作区移除，原文可在 git 历史（`1e44950` 之前）中查阅。

## 维护约定

- 每轮新审计：新增 `AUDIT_REPORT_<date>.md` 与对应 `repro/` 脚本；随后把**仍未实现/未验证**的项迁移进 `plan/audit-backlog.md`，已在报告内闭环的项不入 backlog。
- 报告之间允许相互引用文件名；被引用的已删除历史报告视为 git 历史出处，不需要恢复文件。
- `repro/` 脚本服务于「结论可复现」，与 `test/` 的常驻回归套件互补：稳定的行为回归应沉淀为 `test/*Test.php`，本目录保留一次性证据与探针。
