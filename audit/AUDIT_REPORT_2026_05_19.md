# Gene 扩展审计记录 — 2026-05-19

## 主题
回滚 2026-05-19 上一轮在 `src/http/response.c` 中加入的 "Swoole 方法解析失败 → SAPI 回退" 分支。

## 背景
2026-05-19 早些时候，为 `gene_response_set_redirect` / `gene_response_set_header_ex` /
`PHP_METHOD(gene_response, end)` 三处增加了以下逻辑：

```
if (swoole_resp) {
    fn = GENE_SWOOLE_RESP_METHOD(...);
    if (EXPECTED(fn)) { call; return; }
    /* fall through to sapi_header_op / php_write */
}
```

初衷是兜底 — 如果 Swoole 的 `Response::redirect/header/end` 方法解析失败，仍能通过
PHP SAPI 输出。

## 问题分析

### 1. 运行环境已经在上游分流
`gene_response_context_obj()` (`src/http/response.c:91-115`) 已严格按
`GENE_G(runtime_type) >= 2` 判断：

- `runtime_type < 2` (FPM/CGI)：返回 `NULL`，调用方走 `sapi_header_op`。
- `runtime_type >= 2` (Swoole/协程)：返回 Swoole `Response` 对象，调用 Swoole 方法。

也就是说，调用方进入 `if (swoole_resp)` 分支 **等价于** 当前处于 Swoole 模式。

### 2. Swoole 模式下 SAPI 输出无效
Swoole worker 接管了 HTTP 响应通道：
- `sapi_header_op(SAPI_HEADER_REPLACE, ...)` 写入的是 `SG(sapi_headers)`，但 Swoole
  不会把它发送给客户端。
- `php_write` 同理，输出落到 PHP 标准输出缓冲，Swoole 不会回传。

因此，在 `swoole_resp != NULL` 分支里向 SAPI 兜底，**实际是把响应丢进黑洞**，比直接
不做事更糟（消耗 CPU + 误导调试）。

### 3. fn 解析失败几乎不会发生
- `swoole_resp` 非 NULL ⟹ DI 中已注入 `Swoole\Http\Response` 实例 ⟹ Swoole 扩展已
  加载 ⟹ `redirect/header/end/cookie` 方法 100% 存在。
- 唯一能触发 `fn == NULL` 的场景是测试用 mock 对象，而那种场景下 SAPI 兜底同样无效。

### 4. 与 `cookie` 路径的不一致
`gene_response_cookie` 在 `cookie_fn == NULL` 时直接 `ZVAL_FALSE(retval); return;`，
不做 SAPI 兜底。本次清理让三处保持一致：Swoole 路径里方法缺失就早退。

## 改动
文件：`src/http/response.c`

1. `gene_response_set_redirect`：去掉 sapi fallback；`if (UNEXPECTED(!fn)) return;`
2. `gene_response_set_header_ex`：去掉 sapi fallback；`if (UNEXPECTED(!fn)) return;`
3. `PHP_METHOD(gene_response, end)`：去掉 `php_write` fallback；
   `if (UNEXPECTED(!end_fn)) RETURN_TRUE;`

FPM 路径（`swoole_resp == NULL` 时的 `sapi_header_op` / `php_write` 块）完全保留，
未做任何改动。

## 收益
- 删除三段死代码（约 30 行），二进制略小。
- 分支预测改善：`UNEXPECTED(!fn)` 让编译器把成功路径作为直线代码生成。
- 语义清晰化：环境二分由 `gene_response_context_obj()` 一处决定；调用方不再有
  "Swoole 但方法缺失" 的伪三分支。
- 避免在 Swoole 下静默丢响应到 SAPI 黑洞。

## 风险
- 若有人在 Swoole 模式下用非 Swoole stub 对象注入 `response` DI 键，旧代码会尝试
  SAPI（仍然无效），新代码直接早退。结果都是响应不发出，但新代码不再浪费 CPU。
- 生产代码路径无行为变化。

## 验证
- 未在 Windows IDE 环境编译（无 PHP SDK 工具链）。
- 需用户在 Linux 下 `phpize && ./configure && make` 验证。
- 建议压测对比改动前后 Swoole 模式下 `Response::header/end` 的 QPS。
