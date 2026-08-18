# apistore 使用驱动的 Gene 增强需求

> 来源：apistore 生产代码用法分析（FPM + `\Gene\Model` / `$this->db` 为主）。  
> Gene 版本基线：**6.0.0**。  
> 审计遗留项见 [`audit/plan/PLAN.md`](../audit/plan/PLAN.md) §7.2.3，本文在其基础上按 apistore 证据具体化。

---

## 一、apistore 用法画像

### 1.1 Model 分层

| 基类 | 规模 | 角色 |
|------|------|------|
| `\Gene\Orm\Model` | **1 个** | 仅 `application/Models/Agent/BaseCrud.php` |
| `\Gene\Model` | **约 49 个** | Admin/Shop/Learning/Erp 等旧业务 + `CapabilityConfig` |
| Agent `BaseCrud` 子类 | **约 38 个** | 后台 CRUD；热路径查询仍混用 `$this->db` |

ORM API 采用率极低：`fill()` **0 次**；`::query()` 几乎只在 `BaseCrud`；`Validate` 几乎闲置（主要见于 `Api/Webrtc.php`）。

### 1.2 重复模式（应进扩展）

```text
旧 Model lists()     ≥28 个文件   count + select 双查询，order/limit 手写
Agent BaseCrud       38+ 实体      ORM count + Db select 混搭
Service add/edit     每处写       addtime/updatetime = time()
RAG 切块             Document.php  逐条 Chunk::add()
INSERT IGNORE        3+ Model      TaskLog / EventIdempotency / IncomingNonce
FOR UPDATE           Task.php      调度器裸 SQL
全表再过滤           Skill 注入     allEnabled() 拉全表
会话记忆             MemoryManager  allBySession() 拉全量再 PHP 切 epoch
```

### 1.3 Gene 已有但 apistore 用不了

| Gene 能力 | 缺口 |
|-----------|------|
| `Model::paginate($where, $offset, $limit)` | 无 `order`、无字段投影；`BaseCrud::lists` 仍拆 count + select |
| `$timestamps` | 写死 `created_at`/`updated_at` 为 `Y-m-d H:i:s`；apistore 用 `addtime`/`updatetime` unix int |
| `Db::batchInsert()` | ORM 未透出；RAG 切块逐条 insert |
| `Query` | 无 join/update/lock；JOIN 列表全裸 SQL |
| `Db::in()` | 无数组形式 `whereIn`；Skill/DM-API 全表或 N 次 row |

### 1.4 配置与运行形态

- 生产：**FPM**，入口 `public/index.php`
- `config/config.dev.php`：`db.instance => true` + `PDO::ATTR_PERSISTENT`（为复用连接，与框架文档「FPM 用 false」相反）
- `public/swoole.php` 为旧样板，非当前运行形态

---

## 二、架构关系

```mermaid
flowchart LR
  subgraph apistore [apistore 现状]
    BaseCrud["BaseCrud lists/count+select"]
    Timestamps["Service 手填 addtime/updatetime"]
    Batch["Document 逐条 Chunk::add"]
    Ignore["INSERT IGNORE 裸 SQL"]
    Join["listsViaKbTenant 手写 JOIN"]
  end
  subgraph geneNow [Gene 已有但用不了]
    Paginate["Model::paginate 无 order"]
    Ts["timestamps 固定 created_at 字符串"]
    Bi["Db::batchInsert ORM 未透出"]
    Q["Query 无 join/update/lock"]
  end
  subgraph geneNext [计划落地]
    Q2["Query v2 + paginate order"]
    Ts2["可配置时间戳列名与 unix/datetime"]
    Write["createMany / insertIgnore / upsert / lockForUpdate"]
    Find["findMany / whereIn / after"]
  end
  BaseCrud --> Paginate
  Timestamps --> Ts
  Batch --> Bi
  Ignore --> Q
  Join --> Q
  Paginate --> Q2
  Ts --> Ts2
  Bi --> Write
  Q --> Write
  Q --> Find
```

---

## 三、P0 — 不补则 ORM 继续被绕开

### 3.1 Query 透出 Db 能力 + paginate 可排序

**证据**

- `application/Models/Agent/BaseCrud.php`：`static::query()->where()->count()` + `$this->db->select()->order('id desc')->limit()`
- 旧 Model `lists()` ≥28 个，如 `application/Models/Admin/User.php`
- JOIN：`BaseCrud::listsViaKbTenant`、`RunLog::paginateFiltered`、`Session::paginateByAgent` 全部裸 SQL

**规格**

- `Query` 增加：`join` / `leftJoin` / `rightJoin` / `group` / `having` / `union` / `first` / `exists` / `pluck` / `update` / `delete`
- `Query::paginate($offset, $limit)` 与 `Model::paginate($where, $offset, $limit, $order = null)` 返回 `{count, list}`
- 支持模型级 `$listFields` / `$fields` 投影（对齐 `BaseCrud::resolveListSelectFields`）

**apistore 替换点**：`BaseCrud::lists`、旧 `lists()` 模板、RunLog/Session 分页 JOIN

---

### 3.2 可配置 timestamps

**证据**

- `application/Services/Agent/BaseCrud.php`：`add` 写 `addtime`，`edit` 写 `updatetime`（unix int）
- `BaseCrud::status()` 手写 `updatetime=time()`
- Gene 现状：`src/orm/meta.c` `gene_orm_apply_timestamps()` 写死 `created_at`/`updated_at` 为 `Y-m-d H:i:s`

**规格**

```php
protected static $timestamps = true;
protected static $createdAt = 'addtime';
protected static $updatedAt = 'updatetime';
protected static $timestampFormat = 'unix'; // unix | datetime
```

- `create` / `save` / `updateBy` 自动填充；业务已传入则不覆盖
- `toggle` / 手写 status SQL 可顺带更新 `$updatedAt`（见 P1）

**apistore 替换点**：`Services/Agent/BaseCrud::add/edit`、各 Model `status()`

---

### 3.3 ORM 批量写 + Db 幂等写

**证据**

- `application/Services/Agent/Document.php`：RAG 切块 `foreach` 逐条 `Chunk::add()`（数百次 insert）
- Db 层已有 `batchInsert()`，ORM 未透出
- `INSERT IGNORE`：`TaskLog.php`、`EventIdempotency.php`、`IncomingNonce.php`

**规格**

| API | 说明 |
|-----|------|
| `Model::createMany(array $rows): int` | 走 `batchInsert`，一次 round-trip |
| `Db::insertIgnore($table, $fields)` | MySQL 先落地 |
| `Db::upsert($table, $fields, $updateCols)` | `ON DUPLICATE KEY UPDATE`；其它驱动文档标明降级 |
| `Model::insertIgnore` / `Model::updateOrCreate` | ORM 薄封装 |

- 惰性执行约定保持：终端方法才真正执行
- 文档对比「循环 `create()`」与 `createMany` 的性能差

**apistore 替换点**：`Document` 切块索引、`TaskLog` / 幂等表写入

---

### 3.4 lockForUpdate + 事务文档化

**证据**

- `application/Models/Agent/Task.php`：`SELECT ... FOR UPDATE` 裸 SQL
- `application/Services/Agent/Task/TaskScheduler.php`：调度器唯一认真用事务的路径

**规格**

- `Query::lockForUpdate()` / `Query::sharedLock()`
- 事务仍用现有 `beginTransaction` / `commit` / `rollBack`
- 文档提供「调度器式」样例
- **不做** ORM 自动包 `create()` 事务（审计 L4：仅文档化 `create()` = insert + lastId 无事务）

**apistore 替换点**：`Task::rowForUpdate`、`TaskScheduler`

---

### 3.5 whereIn / findMany / 游标 after

**证据**

- Skill 注入：`allEnabled()` 全表再 PHP 按 id 过滤
- DM-API：对每个 tool id 调 `Tool::row($id)`（N 次查找）
- `MemoryManager::prepareLlmMemory`：`Message::allBySession()` 拉全量再 `selectEpochMessages`

**规格**

| API | 说明 |
|-----|------|
| `Model::findMany(array $ids): array` | 按主键批量取，保持 id 顺序可选 |
| `Query::in('id', $ids)` | 数组形式；现有 `in($sql, $bind)` 保留 |
| `Query::after($column, $id)` | 等价 `where($col, '>=', $id)` + order，支撑 `id >= anchor LIMIT n` |
| `Query::where($col, $op, $val)` | 比较运算符：`>`, `>=`, `<`, `<=`, `!=` |

**apistore 替换点**：`MemoryManager`、`SkillInjector`、DM-API tool 批量加载

---

## 四、P1 — 明显减样板，复杂度可控

### 4.1 状态翻转与 LIKE 转义

**证据**

- `BaseCrud::status()`：`status=abs(status-1)`
- 旧后台同类模式；`Chunk::escapeLikePattern` 手写 LIKE 转义

**规格**

- `Model::toggle($id, 'status', $values = [0, 1])` 或 `toggle($id, 'status')` 默认 0/1 翻转
- where 数组 `['%keyword%', 'like']` 自动 escape `%`、`_`

---

### 4.2 相关计数（withCount 级）

**证据**

- `Session::paginateByAgent` / `paginateForChatSidebar`：每行相关子查询 `(SELECT COUNT(*) FROM agent_message ...)`

**规格**

- `Query::selectSub($sql, $alias)` 或 `withCount('agent_message', 'session_id', 'message_count')`
- **不做** hasMany / belongsTo 完整关联

---

### 4.3 JSON 表达式（MySQL dialect）

**证据**

- `RunLog.php`：`JSON_VALID` / `JSON_EXTRACT` / `JSON_UNQUOTE` 静态方法拼 SQL

**规格**

- `Query::whereJson($col, $path, $op, $val)` / `jsonUnquote($col, $path)`
- 标为 MySQL dialect；Sqlite 测试用简化或 skip

---

### 4.4 FPM 下 instance=true 链式隔离

**证据**

- `config/config.dev.php`：`db.instance => true` + `PDO::ATTR_PERSISTENT`
- 框架文档建议 FPM 用 `instance: false` 防链式污染；apistore 为性能选 true

**规格**

- 请求结束自动 `Db::reset()`（FPM RSHUTDOWN / `Application::cleanup`）
- 持久连接与链式状态脱钩，不改业务配置习惯

---

### 4.5 Validate 快捷绑定（不做中间件）

**证据**

- `$this->validate` 几乎只在 `Api/Webrtc.php`；Admin/Agent 全是 `intval($this->get/post)`

**规格**

- 文档 + 可选 `Controller` 示例：`$this->validate->init($data)->name('x')->required()->valid()`
- **不做**路由中间件（属 audit F4；apistore 钩子全是闭包）

---

## 五、P2 — 需求真实但不要先做

| 项 | 原因 |
|----|------|
| 全局 Scope（tenant_id） | `TenantScope` 有超管 `tenant_id=0` 跳过过滤；若做，只做 `static $globalScope` 回调 |
| casts / 模型事件 | json_encode 遍地但运行时自控；apistore 0 使用 |
| 软删除 | `status` 同时表示启用/禁用与 Message 软删，语义冲突 |
| 关联 `with()` | JOIN 点少，Query::join 足够 |
| Schema ensure / 热路径 ALTER | `BaseCrud::ensureColumn` 是项目债；Gene **不应**提供请求内 ALTER API |
| 树查询 | 仅 `Module` 依赖外部 `\Ext\Helper\Tree` |
| 路由中间件 / `Controller::init` | 已在 audit PLAN；apistore 未形成痛点 |

---

## 六、明确不进扩展

| 项 | 说明 |
|----|------|
| 三层 BaseCrud | Controller / Service / Model 重复 + `call_user_func([$model,'getInstance'])` — 项目结构债 |
| FULLTEXT + 中文 LIKE 回退 | `Chunk::searchFulltext` / `matchContentKeywords` — 业务检索策略 |
| Pay 事务内调第三方 HTTP | `Controllers/Admin/Pay.php` — 业务反模式 |
| 请求内 schema 迁移 | `ensureColumn` / `information_schema` — 应 CLI 迁移，不固化进框架 |

---

## 七、实现约束

1. **保持精简**：C 层只加「apistore 重复 ≥3 处或热路径」的 API
2. **惰性写语义不变**：`insert`/`batchInsert`/`update`/`delete` 构建 SQL；`all`/`row`/`lastId`/`affectedRows` 执行
3. **测试**：新 API 必须在 `test/OrmTest.php`、`test/DatabaseTest.php` 加用例
4. **文档同步**：`gene-ide-helper/`、`gene-ai-helper/skills/gene-framework/reference.md`
5. **与 audit 分工**：性能项（route_pc 预热、连接池泄漏等）留在 `audit/plan/PLAN.md`，不在此重复立项

---

## 八、证据文件速查

| 主题 | 路径 |
|------|------|
| ORM 唯一入口 | `apistore/application/Models/Agent/BaseCrud.php` |
| Service 时间戳 | `apistore/application/Services/Agent/BaseCrud.php` |
| 旧 CRUD 样板 | `apistore/application/Models/Admin/User.php` |
| RAG 逐条 insert | `apistore/application/Services/Agent/Document.php` |
| INSERT IGNORE | `apistore/application/Models/Agent/TaskLog.php` 等 |
| FOR UPDATE | `apistore/application/Models/Agent/Task.php` |
| 会话记忆全量拉取 | `apistore/application/Services/Agent/Runtime/MemoryManager.php` |
| JSON SQL | `apistore/application/Models/Agent/RunLog.php` |
| 租户 Scope | `apistore/application/Services/Agent/TenantScope.php` |
| Gene timestamps 实现 | `gene/src/orm/meta.c` |
| Gene paginate 实现 | `gene/src/orm/model.c` |
| 审计 ORM 缺口 | `gene/audit/plan/PLAN.md` §7.2.3 |
