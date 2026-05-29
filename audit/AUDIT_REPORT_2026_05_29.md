# Gene PHP Extension 审计报告 — FPM/Swoole 高并发性能与内存安全

## 版本信息

| 项 | 值 |
|---|---|
| **版本** | 5.6.4 |
| **日期** | 2026-05-29 |
| **审计类型** | 静态代码审计 + 针对性修复（非压测） |
| **审计目标** | FPM / Swoole 双模式下极致并发性能 + 极致内存占用 / 杜绝泄漏与悬垂 |
| **主要源码** | `src/router/router.c`、`src/mvc/view.c`、`src/gene.c`、`src/factory/factory.c`、`src/di/di.c`、`src/http/request.c` |
| **关联报告** | [AUDIT_REPORT_2026_05_25.md](./AUDIT_REPORT_2026_05_25.md)、[AUDIT_REPORT_2026_05_24.md](./AUDIT_REPORT_2026_05_24.md)（悬垂指针专项） |

## 执行摘要

本轮在已高度优化（前 21 轮）的基础上做了一次聚焦扫描。结论：

1. **内存安全**：核心生命周期（`gene_request_context_*`）、`factory.c`、`di.c`、`request.c` 的分配/释放配对完整，错误路径清理齐全，**未发现新增泄漏**。
2. **发现并修复 1 个跨请求 UAF（中高危）**：`src/mvc/view.c` 的 `parser_templates()` 在非 permanent interned 环境（`opcache.file_cache_only=1` / CLI / 无 opcache）下会把请求作用域字符串写入 `static` 槽，下一请求复用悬垂指针喂给 `php_pcre_replace`。
3. **实施 1 个低风险热路径微优化**：`get_router_info()` 缓存 `seg` 长度，消除每个带 hook 路由最多 5 次冗余 `strlen`。
4. **记录 1 个高收益、需确认的优化项**：`view_compile` 模式下模板每请求重编译（28 次 PCRE），可加 mtime 跳过，但属行为变更，未自动实施。

---

## 1. 已修复项

### 1.1 [中高危] view.c 模板编译器静态缓存跨请求 UAF

**文件**：`src/mvc/view.c` · `parser_templates()`

**根因**：辅助函数 `gene_interned_str_persistent(slot, s, l)`（`src/gene.c:129`）的契约是——**仅当** `zend_string_init_interned()` 返回带 `IS_STR_PERMANENT` 标记的字符串时才写入 `*slot`；否则返回一个**请求作用域**字符串并**故意保持 `*slot == NULL`**，使下一请求重新解析，避免悬垂。这是 2026-05-24 悬垂专项确立的安全模式。

原代码却把返回值回写进 `static` 数组：

```c
regex_strs[i] = gene_interned_str_persistent(&regex_strs[i], regex_raw[i], strlen(regex_raw[i]));
```

在非 permanent 环境下：

- 请求 1：`regex_strs[0]==NULL` → 进入初始化 → 把**请求 1 作用域**的 interned 字符串写入 statics。
- 请求 1 结束：per-request interned 存储被释放，statics 变悬垂。
- 请求 2：`if (!regex_strs[0])` 见到非 NULL（悬垂）→ 跳过初始化 → 在替换循环用悬垂 `regex_strs[i]` 调 `php_pcre_replace` → **use-after-free / 崩溃**。

normal opcache（permanent）下不触发，但 `file_cache_only=1`、CLI、无 opcache 环境会命中——与 2026-05-24 修复的 bug 同类，本调用点此前被遗漏。

**修复**：保留 statics 仅用于 permanent 快路径；新增每次调用的本地工作数组 `regex_use[]/replace_use[]` 驱动替换循环。

- permanent 命中：`memcpy` statics → use 数组。
- 非 permanent：`gene_interned_str_persistent` 返回值写入 use 数组（statics 保持 NULL，下一请求重解析），替换循环只读 use 数组——**当前请求始终持有效指针，statics 永不悬垂**。

请求作用域 interned 字符串带 `IS_STR_INTERNED`，`zend_string_release` 对其为 no-op，故无泄漏、无 double-free。性能：permanent 环境零额外开销；非 permanent 环境每次编译 ~56 次 interned 解析（一次哈希探测，约 25ns/次），远小于一次磁盘写。

### 1.2 [低风险/性能] get_router_info 冗余 strlen

**文件**：`src/router/router.c` · `get_router_info()`

`seg` 由 `php_strtok_r` 产出后在函数生命周期内不再变化，但原代码在直接派发 / 闭包 / eval 三条路径上重复调用 `strlen(seg)` 最多 5 次。改为 strtok 后缓存一次 `seg_len`，全程复用。每个带 hook 的路由省 4–5 次字符串扫描。**零行为变化。**

---

## 2. 已核查、确认健康（无需改动）

| 区域 | 文件/函数 | 结论 |
|---|---|---|
| 请求上下文生命周期 | `gene.c` `gene_request_context_free_fields` / `_reset` / `_destroy` / ctx 池 acquire/release/drain | 字段级整表销毁，链表复用 `path_params.value.ptr` 槽，全路径无泄漏 |
| 工厂 | `factory.c` `gene_factory*` / pack_array/call_params | `*_heap` 栈/堆分支均配对 `efree`；构造函数 `ret`/`tmp` 均 dtor |
| DI 容器 | `di.c` `gene_di_get` / `gene_di_get_class` / `gene_class_instance` | `cache_key_heap`/`key_buf`/`ctor_params_heap` 配对释放；singleton 注册 refcount 正确 |
| 超全局访问 | `request.c` `request_query` / `getVal` | JIT auto-globals、`ZVAL_DEREF`、SERVER/HEADER 小写回退处理到位 |
| 视图字符串路径 | `view.c` `gene_view_display(_ext)` / `contains(_ext)` / `url` | `path`/`compile_path`/`cpath`/`out_ptr` 所有堆分支均 `efree`；编译结果 `result` 全路径 `zend_string_release` |
| 视图符号表 | `view.c` `gene_view_build_symbol_table` | 浅拷贝 + 调用方销毁；已有注释说明不可进一步共享 HashTable |

---

## 3. 记录但未实施的优化项（需确认 / 行为变更）

### 3.1 [高收益·需确认] view_compile 每请求重编译

**现状**：`gene_view_display_ext()` / `gene_view_contains_ext()` 在 `isCompile || GENE_G(view_compile)` 为真时，**每次渲染**都重新打开源模板、执行 28 次 PCRE 替换、写出编译文件。

**优化**：在重编译前比较源模板与编译产物的 mtime（`php_stream_stat_path`），仅当源较新时才重编译。生产高并发下可消除每请求 28 次 PCRE 扫描 + 文件写，是 view 层最大单点收益。

**未实施原因**：属**行为变更**，存在「源模板更新后未及时反映」的语义风险（取决于部署是否依赖每请求强制重编译）。建议由维护者确认后实施，并提供 INI 开关（如 `gene.view_compile_check_mtime`）。

### 3.2 历史审计已覆盖、不建议重复改动

连接池 `pool.c` / `redis_pool.c`（TOCTOU、yield 穿透、两阶段 closeAll）、DB retry 路径、Swoole `co_contexts` sweep —— 已在 2026-05-04~05-25 多轮加固，回归风险 > 边际收益。

### 3.3 并发能力现实边界（沿用历史结论）

- **FPM**：框架 C 层已无显著优化空间，整体 QPS 受 PHP-FPM 进程模型 + TCP/认证 RTT 约束。
- **Swoole**：上下文获取已亚微秒级；进一步收益主要来自连接池参数调优与**业务侧 static/global** 控制，而非框架 C 层。

---

## 4. 验证

| 验证项 | 方式 | 状态 |
|---|---|---|
| 改动点静态复核 | 逐行复读 `view.c` 三处编辑、`router.c` 六处编辑 | 完成；`regex_use/replace_use` 在 if/else 两分支均全量初始化，`seg_len` 仅缓存只读 |
| 编译验证 | 需 Windows PHP build SDK（`phpize`/`nmake`，见 `src/config.w32`） | **未在本环境执行**（无 PHP-SRC SDK）。建议维护者执行：`nmake` 后跑现有 demo（`demo/public`）FPM 与 `demo/public/swoole.php` 冒烟测试 |
| 回归重点 | `file_cache_only=1` / CLI 下连续 ≥2 次模板编译渲染（验证 §1.1 不再 UAF） | 待维护者本地压测 |

---

## 5. 改动文件清单

| 文件 | 改动 |
|---|---|
| `src/mvc/view.c` | 修复 `parser_templates()` 跨请求 UAF：引入 `regex_use[]/replace_use[]` 本地数组 |
| `src/router/router.c` | `get_router_info()` 缓存 `seg_len`，消除冗余 `strlen` |
| `audit/AUDIT_REPORT_2026_05_29.md` | 本报告 |

## 变更记录

| 日期 | 说明 |
|---|---|
| 2026-05-29 | 初版：view.c 跨请求 UAF 修复、router.c strlen 微优化、view_compile mtime 优化建议 |
