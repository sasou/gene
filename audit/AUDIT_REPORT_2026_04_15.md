# Gene PHP扩展 — 审计报告：cache.c, validate.c, factory.c, exception.c
**日期：** 2026-04-15
**范围：** 内存安全、性能（缓存版本控制、验证、工厂调度）
**审查文件：** cache/cache.c, http/validate.c, factory/factory.c, exception/exception.c, common/common.c, common/common.h, gene.c, gene.h

---

## 1. 内存泄漏发现（高优先级）

### L1 — `cachedVersion()`：缓存未命中/版本不匹配时 `cur_version` 泄漏
**位置：** `cache/cache.c:1204-1240`
**影响：** 每次缓存版本过期或数据缺失的调用都会泄漏 `cur_version` zval。
- **路径A（1206-1218行）：** 版本不匹配 → `cur_version` 在1205行创建，在 `gene_cache_build_version_payload` 中使用，但在1218行 `RETURN_ZVAL` 前未释放。
- **路径B（1228-1240行）：** 缓存中数据缺失 → `cur_version` 在1230行创建，但在1240行 `RETURN_ZVAL` 前未释放。
- **路径C（1220-1226行）：** 缓存命中 → `cur_version` 在1224行正确释放。✓

**修复：** 在路径A和B的每个提前返回前添加 `zval_ptr_dtor(&cur_version)`。

### L2 — `localCachedVersion()`：缓存未命中/版本不匹配时 `cur_version` 泄漏
**位置：** `cache/cache.c:1275-1323`
**影响：** 与L1相同的模式。`cur_version` 在1286行填充。
- **路径A（1292-1303行）：** 版本不匹配 → `cur_version` 未释放。缺少 `zval_ptr_dtor(&cur_version)`。
- **路径B（1312-1323行）：** 无缓存 → `cur_version` 未释放。缺少 `zval_ptr_dtor(&cur_version)`。
- **缓存命中（1305-1311行）：** 在1310行正确释放。✓

**修复：** 在路径A和B的每个提前返回前添加 `zval_ptr_dtor(&cur_version)`。

### L3 — `processCachedVersion()`：缓存未命中/版本不匹配时 `cur_version` 泄漏
**位置：** `cache/cache.c:1495-1535`
**影响：** 相同模式。`cur_version` 在1505行填充。
- **路径A（1511-1519行）：** 版本不匹配 → `cur_version` 未释放。
- **路径B（1528-1535行）：** 无缓存 → `cur_version` 未释放。
- **缓存命中（1521-1525行）：** 在1524行正确释放。✓

**修复：** 在路径A和B的每个提前返回前添加 `zval_ptr_dtor(&cur_version)`。

### 严重性评估
这些泄漏在版本化缓存操作的**每次缓存未命中**时都会发生。在使用 `cachedVersion`/`localCachedVersion`/`processCachedVersion` 的高流量应用中，每次未命中都会泄漏一个包含版本数据（通常是版本字符串数组）的 zval。在持续负载下，这会在 FPM 模式下按请求累积，在 Swoole 模式下按协程累积。

---

## 2. 性能发现（中/低优先级）

### P1（低） — `gene_mb_strlen()` 每次调用都为编码创建 ZVAL_STRING
**位置：** `http/validate.c:195-210`
**影响：** `ZVAL_STRING(&bm_z, bm)` 在每次检查字符串长度的验证调用中为编码参数（总是 "UTF8"）分配 zend_string。
**修复：** 对编码使用静态内部字符串。由于验证通常不是每请求的热路径，因此不关键。

### P2（低） — `gene_preg_match_str()` 每次调用都为正则表达式创建 ZVAL_STRING
**位置：** `http/validate.c:222-233`
**影响：** 正则表达式模式字符串每次调用都在堆上分配。对于内置模式（mobile、date），这些是常量。
**修复：** 调用者可以预先构建 zval 正则表达式字符串。低优先级。

### P3（信息） — `exception.c` 冷路径字符串分配
**位置：** `exception/exception.c:91-106` (`gene_html_escape`)
**影响：** 为 "UTF-8" 编码创建临时 ZVAL_STRING。仅在错误显示期间使用 — 无性能问题。

---

## 3. 代码质量观察

### Q1 — 批量方法（cachedVersionBatch, localCachedVersionBatch, processCachedVersionBatch）是干净的
所有批量方法都在函数结束时、循环外部正确释放 `cur_version`。未检测到泄漏。批量模式在循环内正确使用 `Z_TRY_ADDREF(cur_version)`，之后使用单个 `zval_ptr_dtor(&cur_version)`。

### Q2 — `factory.c` 函数调度已充分优化
`gene_factory_call`、`gene_factory_call_1`、`gene_factory_call_2` 都在可用时使用缓存的 `zend_function` 指针调用 `zend_call_known_function`，仅对未知方法回退到 `call_user_function`。无问题。

### Q3 — `gene.c` 上下文生命周期是正确的
RINIT/RSHUTDOWN 正确管理 `default_ctx`、`resident_ctx` 和 `co_contexts`。`gene_co_context_dtor` 析构函数正确处理 `current_ctx` 指向正在销毁的上下文的情况。

### Q4 — `common.c` 哈希实现是正确的
FNV-1a、xxHash64、FarmHash64、MurmurHash3-32、TurboHash32 都使用适当的展开和最终化。common.h 中的 `gene_hex_pair` 表设计良好，用于快速十六进制转换。

---

## 4. 修复摘要

| # | 严重性 | 修复 | 文件 | 行号 |
|---|----------|-----|------|-------|
| L1 | **高** | 在 cachedVersion 未命中路径添加 `zval_ptr_dtor(&cur_version)` | cache.c | 1214, 1236 |
| L2 | **高** | 在 localCachedVersion 未命中路径添加 `zval_ptr_dtor(&cur_version)` | cache.c | 1299, 1319 |
| L3 | **高** | 在 processCachedVersion 未命中路径添加 `zval_ptr_dtor(&cur_version)` | cache.c | 1516, 1532 |

---

## 5. 不需要更改的文件

- `exception/exception.c` — 仅冷路径，无热路径问题
- `factory/factory.c` — 已使用 `zend_call_known_function` 优化
- `gene.c` / `gene.h` — 上下文生命周期正确
- `common/common.c` / `common/common.h` — 哈希算法和工具函数是干净的
- `http/validate.c` — 仅低优先级字符串分配优化
