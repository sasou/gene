# Gene 产品驱动需求计划

本目录存放**由业务项目真实用法推导**的 Gene 扩展增强需求，供后续立项与实现参考。

与 [`audit/plan/PLAN.md`](../audit/plan/PLAN.md) 的分工：

| 目录 | 内容 | 立项依据 |
|------|------|----------|
| `audit/plan/` | 审计遗留项、profile 准入项、已 revert 功能重设计 | 源码审计、压测、ASAN |
| `plan/`（本目录） | 产品/业务驱动的 API 缺口与优先级 | 生产项目（如 apistore）重复模式、热路径 |

**维护约定**

- 新需求须附**代码证据**（文件路径 + 重复次数或热路径说明）
- 不与 `audit/plan` 抢审计项；交叉引用即可
- 实现约束：C 层只加「重复 ≥3 处或热路径」的 API；惰性写语义不变；`test/OrmTest.php` / `test/DatabaseTest.php` 加用例；ide-helper + `gene-ai-helper/skills/gene-framework/reference.md` 同步

---

## 文档索引

| 文件 | 说明 |
|------|------|
| [apistore-usage-driven.md](apistore-usage-driven.md) | 基于 apistore 用法分析的需求规格（P0/P1/P2） |

---

## 优先级总表

| 优先级 | 能力 | 编码效率 | 性能 | apistore 可立刻替换 |
|--------|------|----------|------|---------------------|
| P0 | Query + paginate(order) | 砍掉 28+ `lists()` | 少一次手写 count/select 分叉 | `BaseCrud.lists` |
| P0 | 可配置 timestamps | 砍掉 Service 每处 `time()` | 无 | `BaseCrud.add/edit` |
| P0 | createMany / insertIgnore / upsert | 砍掉裸 SQL | RAG 切块 1 次插入 | `Document` / `TaskLog` |
| P0 | lockForUpdate | 砍掉裸 `FOR UPDATE` | 正确性 | `TaskScheduler` |
| P0 | findMany / after | 砍掉全表 + N 次 `row()` | 聊天记忆、Skill 注入 | `MemoryManager` / `Skill` |
| P1 | toggle / like escape / withCount / json / Db auto-reset | 中 | 中 | 按模块 |

详细规格见 [apistore-usage-driven.md](apistore-usage-driven.md)。

---

## 背景摘要

Gene 6.0.0 已有 `Model::paginate()`、`$timestamps`、`Db::batchInsert()`，但 **API 形状对不上 apistore 存量表约定**（`addtime`/`updatetime` unix、`lists()` 需 order/投影、Query 未透出 join 等），导致业务继续手写 Db 链或裸 SQL。本目录目标不是再造一套完整 ORM，而是让现有 `lists/add/edit` 与热路径（RAG 切块、调度幂等、会话记忆）能少写 PHP、少打 SQL。
