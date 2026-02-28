---
name: gene-framework
description: 基于 Gene PHP 扩展框架开发 Web 或常驻应用。在用户进行 Gene 框架开发、编写控制器/路由/配置、使用 Gene 的 Request/View/Controller/Application 或询问 Gene API 时使用。
---

# Gene 框架开发

## 何时使用

- 开发或修改 Gene 项目中的控制器、路由、配置、视图、模型、接口、进程等
- 查询 Gene 某类/某方法的用法、参数或返回值

## 目录与模块

| 目录 | 命名空间 | 说明 |
|------|----------|------|
| application/Controllers/ | Controllers | 不同业务创建子目录 | 方法保持简短 
| application/Api/ | Api | REST 接口（/rest/:c/:a） 
| application/Cli/ | Cli | 命令行，路由 /cli/:c/:a 
| application/Services/ | Services | 业务逻辑 | 缓存封装 | 不同业务创建子目录 
| application/Models/ | Models | 数据访问 | 不同业务创建子目录
| application/Ext/ | Ext | 扩展（第三方 SDK 等）
| application/Views/ | 视图 | 不同业务创建子目录
| config/ | - | router.ini.php、config.ini.{env}.php | 配置文件目录

## 快速约定

- 控制器继承 `\Gene\Controller`；通过 `$this->db`、`$this->cache`、`$this->validate` 使用注入组件
- 取参：`Controller::get()/post()/params()`；渲染视图：`assign()` + `display()`
- 入口链式：`Application::getInstance()->load("router.ini.php")->load("config.ini.php")->run($method, $uri)`
- 配置中通过 `'class' => '\Gene\Xxx'` 注册组件，控制器内用 `$this->xxx` 访问

## 路由与钩子（router.ini.php）

- **语言**：`->lang('zh,en,ru')`
- **分组**：`->group("/admin/")`、`->group("/rest/:c")`、`->group("/api/:app/")`，后接 `->get/post(...)`，最后 `->group()` 结束
- **钩子名**：`adminAuth`（后台登录与权限）、`webAuth`（前台登录）、`affAuth`（推广员）、`restAuth`（REST 内网或授权）；输出：`@clearAfter`、`@clearBefore`、`@clearAll`
- **通配**：Admin 用 `Controllers\Admin\:c@run` 或 `:c@:a`，Im 用 `Controllers\Im\:c@:a`
- **after 钩子**：统一 `\Gene\Response::json($params, $callback)` 输出 JSON

## 配置注入（config.ini.*.php）

常用组件名：`view`、`request`、`response`、`validate`、`websession`、`adminsession`、`db`、`memcache`、`redis`、`cache`、`rest`、`icomet`、`disk`、`geo`、`translate`、`language`。控制器/Service 内用 `$this->组件名` 访问。

## 视图路径约定

- 前台：`web/xxx` + layout `web/layout`
- 聊天/设置：`chat/xxx` + layout `chat/layout`
- 后台：`admin/xxx`（如 parent、welcome、login），无第二参数则无 layout



## 常用 API 速查

| 类 | 方法 | 说明 |
|----|------|------|
| Controller | get($key, $value), post(), params($key) | 获取 GET/POST/路由参数 |
| Controller | assign($name, $value), display($file, $parent_file) | 视图变量与渲染 |
| Controller | success(), error(), data(), json() | 统一响应输出 |
| Controller | url($path) | 带当前语言前缀的 URL |
| Controller | redirect($url, $code) | 重定向 |
| Application | config($key), params($key) | 配置与路由参数 |
| Application | getModule(), getController(), getAction() | 当前模块/控制器/动作 |

## 详细 API

完整类与方法说明见 [reference.md](reference.md)。
