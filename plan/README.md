# Gene 演进计划

本目录存放 Gene 扩展的立项/实施文档，供后续立项与实现参考。按立项依据分两类：

| 文档 | 内容 | 立项依据 |
|------|------|----------|
| [audit-backlog.md](audit-backlog.md) | 审计遗留项、profile 准入项、已 revert 功能重设计 | 源码审计、压测、ASAN |
| 其余各篇 | 产品驱动的 API 缺口与优先级 | 重复模式、热路径、生命周期覆盖 |

**维护约定**

- 新需求须附**代码证据**（本仓库缺口或可复现的重复模式 + 热路径说明）
- 审计来源的待办统一记入 `audit-backlog.md`；其余文档不重复登记审计项，交叉引用即可
- 实现约束：C 层只加「重复 ≥3 处或热路径」的 API；Db 惰性写语义不变；对应 `test/*.php` 加用例；ide-helper + `gene-ai-helper/skills/gene-framework/reference.md` 同步
- 本目录文档**只写 Gene 扩展**，不写业务仓库迁移清单

---

## 文档索引

| 文件 | 说明 | 状态 |
|------|------|------|
| [orm-v2.closed.md](orm-v2.closed.md) | Db ↔ ORM 对称性（Query ops、timestamps、批量写、行锁、IN） | 关闭（6.1.0 已落地） |
| [lifecycle-completeness.closed.md](lifecycle-completeness.closed.md) | 全生命周期原语（Http、SSE、Context、限流/锁、Json、Crypto） | 关闭（6.1.x 已落地，见文内 §八） |
| [rest-invoke.closed.md](rest-invoke.closed.md) | 框架级 REST 互调（Request 栈、Invoke 本地隔离、命名 Rest、Http multipart） | 关闭（6.1.x 已落地，见文内复盘） |
| [typical-usage-gaps.closed.md](typical-usage-gaps.closed.md) | 6.1 全面采用后的残留缺口（union、JOIN ON、increment、Context __get、Request::input、Http max_bytes、cachedHotVersion） | 关闭（里程碑 A 随 6.2.0 落地；§10.2 条件触发项另行立项） |
| [hook-runtime.closed.md](hook-runtime.closed.md) | Hook 使用驱动的请求策略、终止语义、组级组合与 request-id 收口 | 关闭（2026-09-07 全部落地，见文内 §九） |
| [application-entry-runtime.closed.md](application-entry-runtime.closed.md) | FPM/Swoole 入口收口（请求适配、派发清理、环境装载与 Pool 边界） | 关闭（6.2.3 全部落地，见文内实施记录） |
| [gene_swoole_uaf_fix.closed.md](gene_swoole_uaf_fix.closed.md) | Swoole worker signal 11 的 UAF 根因与最小改动修复 | 关闭（修复已落地；3 项观察项移交 `audit-backlog.md` §七） |
| [Performance-tuning-V1.closed.md](Performance-tuning-V1.closed.md) | 极致并发优化 V1：已完成项目的实现与验收结果 | 关闭（归档存证，后续由 V2 承接） |
| [Performance-tuning-V2.md](Performance-tuning-V2.md) | 极致并发优化 V2：自动化验收规范与尚待实现项 | 进行中 |
| [audit-backlog.md](audit-backlog.md) | 审计驱动待办（F3/F4、模块缺口、性能观测项、O6/O7 Linux 验证、文档/测试缺口） | 进行中（持续维护） |

状态约定：`候选`/`进行中` → 有待办项，文件名保持 `*.md`；`关闭` → 方案已落地或归档存证，不再维护待办，文件名加 `.closed.md` 后缀（遗留待验证项移交 `audit-backlog.md`）。

---

## ORM 对称性（已关闭）

详细规格与复盘见 [orm-v2.closed.md](orm-v2.closed.md)。

| 优先级 | 能力 | 编码效率 | 性能 |
|--------|------|----------|------|
| P0 | Query + paginate(order) | 砍掉手写 count/select 分叉 | 查询次数不变 |
| P0 | 可配置 timestamps | 砍掉每处 `time()` | 无 |
| P0 | createMany / insertIgnore / upsert | 砍掉裸 SQL | 批量插入少 round-trip |
| P0 | lockForUpdate | 砍掉裸 `FOR UPDATE` | 正确性 |
| P0 | findMany / in(数组) | 砍掉全表 + N 次 `row()` | 少行 / 少查询 |
| P1 | toggle / like escape / selectSub | 中 | 中 |

---

## 全生命周期原语（已关闭）

详细规格见 [lifecycle-completeness.closed.md](lifecycle-completeness.closed.md)。ORM 不在该文范围。

| 优先级 | 能力 | 编码效率 | 性能 |
|--------|------|----------|------|
| P0 | `Gene\Http`（curl / Swoole 协程双后端） | 砍掉每项目 curl 样板 | Swoole 下避免阻塞 worker |
| P0 | `Response::write` + SSE | 砍掉手写 flush | 流式延迟可控 |
| P0 | `Gene\Context` + Log 带 request_id | 请求隔离、排障 | 无 |
| P0 | Redis/Memory `rateLimit` / `lock` | 砍掉错误的 SQL COUNT / flock | 少 DB；多机锁可用 |
| P1 | `Request::json` + `Gene\Json` | 入站 JSON 一处语义 | 正确性 |
| P1 | `Gene\Crypto`（hmac / randomId / GCM） | 砍掉令牌与 ID 复制 | 无 |
| P1 | demo Cors / RequestId 钩子 | 约定，不改派发链 | 无 |

---

## REST 互调（已关闭）

详细规格与复盘见 [rest-invoke.closed.md](rest-invoke.closed.md)。只写扩展能力；应用网关/注册表/队列不在范围。运输层仍是已有 `Gene\Http`。

| 优先级 | 能力 | 编码效率 | 性能 / 安全 |
|--------|------|----------|-------------|
| P0 | Request 快照栈 + cleanup 排空 | 不再 `init` 覆盖入站 | FPM/Swoole 不串请求 |
| P0 | `Gene\Invoke::local` | 同进程互调一行 | 无网络；Controller 非单例 |
| P0 | `Gene\Rest` 不可变 proxy | 命名服务、本地失败才 HTTP | 协程安全；走现有 Http |
| P0 | `Http` multipart `files` | 上传不必自造 curl | 双后端一致 |
| P1 | demo Ping + 双模式测试 | 可回归 | 无环境 SKIP，禁止假绿 |

---

## 典型用法后续缺口（已关闭）

详细规格见 [typical-usage-gaps.closed.md](typical-usage-gaps.closed.md)。ORM v2 与生命周期原语落地后，应用层仍常见的 `sql()` 逃生舱与 FPM/Swoole 分叉；里程碑 A 已随 6.2.0 落地，下表为当时的候选清单存档。

| 优先级 | 能力 | 编码效率 | 性能 / 正确性 |
|--------|------|----------|----------------|
| P0 | `Query::union` / JOIN 字符串 ON | 砍掉关系列表与聚合计数裸 SQL | 绑定一致 |
| P0 | `increment` / `decrement` | 砍掉表达式 UPDATE | 原子计数 |
| P0 | `__get` → Context 回退 | 去掉 Context/Di 双写 | 少请求态 bug |
| P0 | `Request::input` + 可选 JSON→POST | 去掉三份合并逻辑 | 少重复解析 |
| P1 | `Http` `max_bytes` | 删掉应用层下载护栏 | 降峰值内存 |
| P1 | `cachedHotVersion` | 去掉运行时 if | FPM L1 / Swoole 安全分流 |
