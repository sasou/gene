# Gene 产品驱动需求计划

本目录存放**由典型应用用法推导**的 Gene 扩展增强需求，供后续立项与实现参考。

与 [`audit/plan/PLAN.md`](../audit/plan/PLAN.md) 的分工：

| 目录 | 内容 | 立项依据 |
|------|------|----------|
| `audit/plan/` | 审计遗留项、profile 准入项、已 revert 功能重设计 | 源码审计、压测、ASAN |
| `plan/`（本目录） | 产品驱动的 API 缺口与优先级 | 重复模式、热路径、生命周期覆盖 |

**维护约定**

- 新需求须附**代码证据**（本仓库缺口或可复现的重复模式 + 热路径说明）
- 不与 `audit/plan` 抢审计项；交叉引用即可
- 实现约束：C 层只加「重复 ≥3 处或热路径」的 API；Db 惰性写语义不变；对应 `test/*.php` 加用例；ide-helper + `gene-ai-helper/skills/gene-framework/reference.md` 同步
- 本目录文档**只写 Gene 扩展**，不写业务仓库迁移清单

---

## 文档索引

| 文件 | 说明 | 状态 |
|------|------|------|
| [orm-v2.md](orm-v2.md) | Db ↔ ORM 对称性（Query ops、timestamps、批量写、行锁、IN） | 6.1.0 已落地 |
| [lifecycle-completeness.md](lifecycle-completeness.md) | 全生命周期原语（Http、SSE、Context、限流/锁、Json、Crypto） | 方案，待立项 |
| [rest-invoke.md](rest-invoke.md) | 框架级 REST 互调（Request 栈、Invoke 本地隔离、命名 Rest、Http multipart） | 方案，待立项 |

---

## ORM 对称性（已落地）

详细规格与复盘见 [orm-v2.md](orm-v2.md)。

| 优先级 | 能力 | 编码效率 | 性能 |
|--------|------|----------|------|
| P0 | Query + paginate(order) | 砍掉手写 count/select 分叉 | 查询次数不变 |
| P0 | 可配置 timestamps | 砍掉每处 `time()` | 无 |
| P0 | createMany / insertIgnore / upsert | 砍掉裸 SQL | 批量插入少 round-trip |
| P0 | lockForUpdate | 砍掉裸 `FOR UPDATE` | 正确性 |
| P0 | findMany / in(数组) | 砍掉全表 + N 次 `row()` | 少行 / 少查询 |
| P1 | toggle / like escape / selectSub | 中 | 中 |

---

## 全生命周期原语（待立项）

详细规格见 [lifecycle-completeness.md](lifecycle-completeness.md)。ORM 不在该文范围。

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

## REST 互调（待立项）

详细规格见 [rest-invoke.md](rest-invoke.md)。只写扩展能力；应用网关/注册表/队列不在范围。`Gene\Http` 已落地，本文补隔离本地调用与命名客户端。

| 优先级 | 能力 | 编码效率 | 性能 / 安全 |
|--------|------|----------|-------------|
| P0 | Request 快照栈 + cleanup 排空 | 不再 `init` 覆盖入站 | FPM/Swoole 不串请求 |
| P0 | `Gene\Invoke::local` | 同进程互调一行 | 无网络；Controller 非单例 |
| P0 | `Gene\Rest` 不可变 proxy | 命名服务、本地失败才 HTTP | 协程安全；走现有 Http |
| P0 | `Http` multipart `files` | 上传不必自造 curl | 双后端一致 |
| P1 | demo Ping + 双模式测试 | 可回归 | 无环境 SKIP，禁止假绿 |
