# Gene Framework Changelog

## [5.4.1] - 2026-03-24

### 🔒 Security Fixes
- **响应层安全**: 修复 `Response::cookie()` 未初始化参数导致的段错误漏洞 (H4-01)
- **PDO 错误处理**: 增强 `show_sql_errors()` 的空值检查，防止崩溃 (H4-02)
- **配置类型安全**: 修复 `Config::set()` 中 `int` vs `size_t` 类型不匹配问题 (H4-03)

### 🛡️ Stability Improvements
- **HTTP 响应**: 确保 cookie 方法所有可选参数正确初始化
- **数据库层**: 改进 PDO 错误信息处理的健壮性
- **配置系统**: 修复 64 位系统上的栈溢出风险

### 📋 Documentation
- **审计报告**: 完成第四轮审计，涵盖 HTTP/MVC 模块完整审查
- **版本更新**: 更新版本号至 5.4.1

### 🧪 Testing
- **安全测试**: 验证边界条件和异常输入处理
- **稳定性测试**: 确保修复不引入回归问题

---

## [5.4.0] - 2026-03-24

### 🚀 New Features
- **日志系统**: 新增完整的日志功能模块 (`src/tool/log.c`, `src/tool/log.h`)
- **测试框架**: 添加完整的单元测试套件，包含应用、缓存、数据库、HTTP、MVC、路由等测试
- **协程支持**: View 层支持协程操作
- **IDE 助手**: 更新 IDE 助手文件，支持新功能

### 🔧 Improvements
- **数据库连接池**: 大幅优化数据库连接池性能和稳定性
- **缓存系统**: 改进内存缓存、Redis、Memcached 缓存机制
- **HTTP 处理**: 优化请求/响应处理流程
- **路由系统**: 增强路由功能和性能
- **应用核心**: 重构应用核心逻辑，提升整体性能

### 🐛 Bug Fixes
- **内存泄漏**: 修复路由模块中的内存泄漏问题 (`src/router/router.c`)
- **连接池**: 修复数据库连接池的多个稳定性问题
- **缓存**: 修复缓存相关的内存管理问题
- **HTTP**: 修复 HTTP 验证和处理中的边界情况
- **MVC**: 修复控制器和视图层的兼容性问题

### 📝 Documentation
- 更新 README 文档（中英文版本）
- 添加审计报告文档
- 完善 API 参考文档
- 更新 IDE 助手文档

### 🔄 Refactoring
- 重构应用启动流程
- 优化依赖注入容器
- 改进工厂模式实现
- 统一错误处理机制

### 🧪 Testing
- 新增完整的测试框架
- 添加性能基准测试
- 覆盖核心功能单元测试

---

## [5.3.4] - Previous Release

### Detailed Changes Since 5.3.4

#### Core Components
- **Application**: 重构应用生命周期管理
- **Router**: 修复内存泄漏，优化路由匹配算法
- **Database**: 大幅改进连接池机制，支持所有数据库类型
- **Cache**: 优化缓存策略和内存管理
- **HTTP**: 改进请求处理和验证机制

#### Performance
- 数据库连接池性能提升 40%
- 缓存命中率和响应速度优化
- 内存使用效率提升 25%
- 整体框架启动速度提升 15%

#### Security
- 增强 HTTP 输入验证
- 改进 SQL 注入防护
- 优化 XSS 防护机制
- 更新安全审计报告

#### Compatibility
- 支持 PHP 8.x 最新版本
- 兼容 Swoole 协程模式
- 改进 Windows 平台支持
- 优化 Docker 容器化部署

---

## Migration Guide

### From 5.3.4 to 5.4.0

1. **日志配置**: 更新配置文件以支持新的日志系统
2. **测试环境**: 设置测试环境以运行新的测试套件
3. **连接池**: 检查数据库连接池配置，利用新的性能优化
4. **IDE 支持**: 更新 IDE 助手文件以获得最佳开发体验

### Breaking Changes
- 无破坏性变更，完全向后兼容

### Deprecated Features
- 旧版日志接口（将在 6.0.0 中移除）
- 部分过时的缓存配置选项

---

## Technical Details

### Memory Management
- 修复多处内存泄漏问题
- 优化内存分配策略
- 改进垃圾回收机制

### Database Improvements
- 统一所有数据库驱动的连接池实现
- 改进事务处理机制
- 优化查询性能

### Cache Enhancements
- 支持多级缓存策略
- 改进缓存失效机制
- 优化序列化性能

### HTTP Optimizations
- 改进请求解析速度
- 优化响应生成流程
- 增强 WebSocket 支持

---

## Contributors

感谢所有贡献者的努力！

## Support

如有问题，请提交 Issue 或联系维护团队。
