# Gene 框架规则/技能提炼指南

基于 Gene PHP 扩展框架的类方法说明（gene-ide-helper）与 Cursor 规则/技能最佳实践，推荐如下格式与用法。

---

## 一、规则 vs 技能：如何选

| 维度 | Cursor 规则 (Rules) | Cursor 技能 (Skills) |
|------|---------------------|----------------------|
| **用途** | 写代码时自动遵循的约定、规范 | 需要时调用的「如何用 Gene 做某事」工作流 |
| **触发** | 打开匹配文件即生效（或 alwaysApply） | 描述中含触发词时 AI 会选用 |
| **内容** | 简短、可执行（推荐 <50 行/条） | 可含步骤、示例、引用详细文档 |
| **适合从「类方法说明」提炼** | 常用 API 用法约定、禁止/推荐写法 | 完整开发流程、API 速查、疑难场景 |

**建议**：两者配合使用——用**规则**约束日常写法，用**技能**提供按需的框架用法与 API 参考。

---

## 二、推荐格式 1：Cursor 规则（.mdc）

**适用**：控制器/请求/视图/配置等「怎么写」的约定。

**位置**：`.cursor/rules/`，单文件 `.mdc`。

**结构**：

```markdown
---
description: 简短说明（会出现在规则选择器）
globs: "**/*.php"   # 或 "application/**/*.php"
alwaysApply: false
---

# 规则标题

- 一条约定一行，或 简短 Do/Don't 示例
- 只写 AI 需要遵守的、与 Gene 相关的要点
```

**从类方法说明提炼时**：
- 不复制整份 API，只写「在本项目中应如何用」：例如用 `Controller::get/post/params` 取参，用 `assign` + `display` 渲染视图。
- 每条规则一个关注点（如：请求与参数、视图渲染、响应输出、配置与依赖注入）。

**示例**：见 `.cursor/rules/gene-controller-usage.mdc`（若已创建）。

---

## 三、推荐格式 2：Cursor 技能（SKILL.md + reference）

**适用**：基于 Gene 的完整开发流程、API 速查、多步操作。

**位置**：`.cursor/skills/gene-framework/`（项目技能，与仓库共享）。

**目录建议**：

```
.cursor/skills/gene-framework/
├── SKILL.md          # 何时用、快速步骤、常用 API 速查、链接到 reference
└── reference.md      # 从 gene-ide-helper 提炼的「类与方法说明」结构化文档
```

**SKILL.md 结构**：

```markdown
---
name: gene-framework
description: 基于 Gene PHP 扩展框架开发 Web/常驻应用。使用当用户进行 Gene 框架开发、写控制器/路由/配置、使用 Gene 的 Request/View/Controller/Application 或询问 Gene API 时。
---

# Gene 框架开发

## 何时使用
- 开发/修改 Gene 项目中的控制器、路由、配置、视图
- 查询 Gene 某类/方法的用法或参数

## 快速约定
- 控制器继承 `\Gene\Controller`，通过 `$this->db`、`$this->cache` 等使用注入组件
- 取参：`Controller::get()/post()/params()`；渲染：`assign()` + `display()`
- 入口链式：`Application::getInstance()->load("router.ini.php")->load("config.ini.php")->run(...)`

## 常用 API 速查
（此处只列最常用方法名与一行说明，详细见 reference.md）

| 类 | 方法 | 说明 |
|----|------|------|
| Controller | get/post/params | 获取 GET/POST/路由参数 |
| Controller | assign, display | 视图变量与渲染 |
| Controller | success/error/data/json | 统一响应输出 |
| Application | config, params, getModule/getController/getAction | 配置与路由信息 |

## 详细 API
完整类与方法说明见 [reference.md](reference.md)。
```

**reference.md 结构**（从 gene-ide-helper 提炼）：

- 按**类**分节：Controller、Application、Request、Response、View、Router、Config、Db\Mysql、Cache\* 等。
- 每类下：
  - 类职责一句话
  - `@property` 列表（若有）
  - 方法列表：**方法名(参数)** → 返回值/行为简述（可从 PHPDoc 转写，去掉实现细节）。

这样 AI 在需要时读取 reference.md 即可查完整方法说明，SKILL.md 保持简短。

---

## 四、从 PHPDoc 到 reference 的提炼要点

- **保留**：方法名、参数名、返回值类型、中文注释（如 `getLang`「获取当前语言」）、`url()` 等行为说明。
- **省略**：空方法体、重复的 `@param mixed`、与 IDE 桩实现相关的细节。
- **统一**：同一术语全文一致（如「路由参数」不混用「URL 参数」）。

---

## 五、总结：最佳格式建议

1. **规则**：多条 `.mdc`，按主题分（控制器、视图、请求、配置等），`globs: "**/*.php"`，内容简短、可执行。
2. **技能**：一个 `gene-framework` 技能，`SKILL.md` 写触发条件、快速约定、常用 API 表；`reference.md` 放从「类方法说明」整理出的完整 API 文档。
3. **维护**：类方法说明有更新时，优先更新 `reference.md`，再视情况增删/调整规则与 SKILL 中的速查表。

按上述格式，既能从现有 PHP 类方法说明中提炼出可用的规则与技能，又符合 Cursor 对规则（简洁、按文件触发）和技能（描述、渐进式披露）的最佳实践。
