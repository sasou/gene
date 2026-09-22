# Gene IDE Helper

`gene-ide-helper/Gene/` 是 Gene PHP 扩展公开类的 IDE stub，用于代码补全、参数提示和静态分析；文件中的空方法不会提供运行时实现，项目运行时仍须加载 `extension=gene`。

## 使用

将 `gene-ide-helper/` 标记为 IDE 的外部类库或 include path，**不要**在业务代码中 `require` 这些文件，也不要把它注册为 Composer 运行时自动加载目录。

常见 IDE：

1. PhpStorm：`Settings | PHP | Include Path` 添加本目录。
2. VS Code/Intelephense：将本目录加入 `intelephense.environment.includePaths`。
3. 其他 PHP IDE：将本目录配置为只读 external library。

## 能力边界与维护

- 扩展 C 源码 `src/`（类注册、方法表与 arginfo）是公开 API 的最终权威来源。
- stub 应与当前 6.2.x 扩展保持类名、命名空间、方法名、静态性、必填参数和公开属性一致。
- `gene-ai-helper/skills/gene-framework/reference.md` 面向开发者解释 API 语义；两者都不得扩展 C 实现不存在的能力。
- `Gene\Execute` 会执行可信 PHP 源码，不得接收外部输入；应用层 Web 扫描优先通过 `Gene\Application::webscan()` 配置。

更新扩展公开 API 时，应同步修改对应 stub 和 AI reference，并至少对全部 stub 执行 `php -l`。
