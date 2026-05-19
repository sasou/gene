# Gene 框架 — AI 协作说明

本文件定义 AI 在本仓库（Gene **C 扩展** + 业务项目）中生成代码时的**硬性约定**。  
技能细节见 `gene-ai-helper/skills/gene-framework/SKILL.md`；API 见同目录 `reference.md`。

---

## 1. 框架认知

- Gene 是 **PHP 扩展**（`extension=gene`），版本线 **5.6.x**，要求 **PHP 8.0+**
- 权威 API 来源：`gene-ide-helper/Gene/**/*.php`、`demo/` 示例
- **禁止**编造类名、方法名或配置键；不确定时 grep 仓库或读 reference

---

## 2. 架构与目录

| 层 | 基类 | 职责 |
|----|------|------|
| Controller | `\Gene\Controller` | 取参、校验、调 Service、响应/视图 |
| Service | `\Gene\Service` | 业务、版本缓存、`getInstance()` |
| Model | `\Gene\Model` | `$this->db` 链式 SQL |
| Hook | `\Gene\Hook` | 路由钩子（推荐类：`Hooks\Xxx@handle`） |

命名空间与目录对齐，例如 `application/Controllers/Admin/User.php` → `Controllers\Admin\User`。

---

## 3. 控制器（标准流程）

```php
public function action()
{
    $data = $this->request->post();
    $this->validate->init($data)
        ->name('field')->required()->msg('错误提示')
        ->valid() || return $this->error($this->validate->error());

    $result = \Services\Xxx::getInstance()->method($data);
    return $this->data($result['data'] ?? [], $result['count'] ?? -1, $result['msg'] ?? 'ok');
}
```

- 取参：**`$this->request`**（已注入），勿混用未文档化的超全局
- 响应：`return $this->success/error/data(...)`；钩子内裸 JSON 用 `\Gene\Response::json()`
- 方法保持短小，**不写 SQL、不写复杂业务**

---

## 4. 路由与钩子

```php
$router->clear()
    ->lang('zh,en')
    ->get('/path', 'Controllers\Xxx@action', 'adminAuth@clearAfter')
    ->hook('adminAuth', 'Hooks\AdminAuth@handle')
    ->error(404, function () { /* ... */ });
```

- Handler 格式：`"命名空间\\类@方法"`
- 认证失败：`return false`（类钩子 `handle()`）
- 输出控制：`@clearAfter` | `@clearBefore` | `@clearAll` | `@`
- 新增路由放入**现有分组**（如 admin、rest），避免无分组散落

---

## 5. 配置注入

```php
$config->set('db', [
    'class'    => '\Gene\Db\Mysql',
    'params'   => [[ /* dsn, username, password, pool? */ ]],
    'instance' => false,  // FPM 常用 false；Swoole 见 swoole.md
]);
```

- `instance: false` — 每次新建（FPM 下 db 防链式污染）
- `instance: true` — Worker/进程单例（redis、memory、session 等）
- 访问：`$this->db`、`$this->cache` 等（Controller / Service / Hook）

---

## 6. Service / Model / 缓存

**Model 示例**

```php
return $this->db->select('sys_user', 'user_id, user_name')
    ->where(['status' => 1])
    ->order('user_id DESC')
    ->limit($start, $count)  // 偏移量, 行数
    ->all();
```

**版本缓存（写操作后必须 updateVersion）**

```php
// 读
$version = ['db.sys_user.id' => $id];
$row = $this->cache->cachedVersion(['\Models\User', 'row'], [$id], $version, 3600);

// 写
$this->cache->updateVersion(['db.sys_user.id' => $id, 'db.sys_user' => null]);
```

---

## 7. 运行环境分流

| 环境 | 入口要点 |
|------|----------|
| **FPM** | `->setMode(1,1)->run()` 无参 |
| **CLI** | `->run('get', $path)` |
| **Swoole** | `setRuntimeType` → worker 内 `Pool::create` → `workerReady()` → 请求内 `Request::init` → `run()` → **`cleanup()`** |

Swoole 细则：**必读** `skills/gene-framework/swoole.md`。

---

## 8. 安全与质量

- 所有外部输入走 **Validate**
- 权限：路由钩子 + Service 二次校验
- 不提交密钥；不改 `git config`；不 force push
- 改动**最小范围**，匹配现有代码风格
- 无把握时不重构、不抽象过度

---

## 9. 调试

```php
\Gene\Benchmark::start();
// ...
\Gene\Benchmark::end();
// Benchmark::time() / memory()
```

环境配置：`config.ini.dev.php` / `test` / `prod`。

---

## 10. 文档索引

| 文件 | 用途 |
|------|------|
| `skills/gene-framework/SKILL.md` | Cursor Skill（自动触发） |
| `skills/gene-framework/reference.md` | API 大全 |
| `skills/gene-framework/swoole.md` | Swoole / 连接池 |
| `rules/gene-project.mdc` | 编辑 `**/*.php` 时的规则 |

**原则**：沿用仓库已有写法；API 以 ide-helper 为准，不扩展框架能力边界。
