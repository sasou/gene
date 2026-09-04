逐条拆解性能计划中的假设、收益与风险，再对照当前实现和测试/构建约束验证其可行性；只给分析结论，不修改任何文件。

## 总体结论

这份清单**适合作为静态性能审计的候选项集合，但不适合按当前内容直接执行**。大方向合理，定位多数准确，但夹杂了若干：

- 已经实现、却仍被列为待优化的项目；
- 对配置语义理解相反的项目；
- 会改变公开 API 行为的“性能优化”；
- 缺少压测数据却给出较高收益评级的项目；
- 对生命周期、动态派发和协程模型估计过于乐观的项目。

综合评价约 **6/10**。其中数据库、缓存分区和局部分配优化值得继续推进；视图配置、响应缓冲、Session、Log、Memory 深拷贝等内容必须先纠正。

---

# 一、必须纠正的问题

## 1. `view_compile` 的分析和推荐配置是反的

文档称：

> `gene.view_compile=0` 时每请求执行 28 次正则替换；生产应使用  
> `view_compile=1 + view_compile_check_mtime=0`

实际代码是：

- 只有 `isCompile || view_compile` 为真才进入编译分支；
- `view_compile_check_mtime=0` 时，`view_compile_needs_rebuild()` 直接返回 1，即**每次都重新编译**；
- 普通 `display()` 走 `gene_view_display()`，不走 `displayExt()` 的模板编译分支。

<ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="52-69" />  
<ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="445-475" />  
<ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="623-630" />  
<ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="734-759" />

因此文档第 2.1 和第 7 节推荐的组合会造成：

```ini
gene.view_compile = 1
gene.view_compile_check_mtime = 0
```

**每次请求执行 28 轮 `php_pcre_replace`**，正好与优化目标相反。

如果使用运行时编译缓存，应当是：

```ini
gene.view_compile = 1
gene.view_compile_check_mtime = 1
```

如果使用离线预编译产物，则应关闭运行时编译。

这是整份文档最严重的问题。

---

## 2. 3.2 `Memory::get()` 的现状描述错误

文档称 `Memory::get()` 调用了 `gene_memory_zval_local_copy()`，并建议：

> 标量直接 `ZVAL_COPY_VALUE`

实际情况：

- `Memory::get()` 调用的是 `gene_memory_zval_local()`；
- `Cache` 业务读取才使用 `gene_memory_zval_local_copy()`；
- `LONG/DOUBLE/NULL/BOOL` **已经使用 `ZVAL_COPY_VALUE`**；
- 两个函数目前对字符串、数组都执行深拷贝。

<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="396-431" />  
<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="458-485" />  
<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="1291-1303" />

因此 3.2 的第一项不是待优化项，而是已经实现。

字符串借用也不能简单通过“persistent/interned 标志”实现。当前代码明确说明，业务缓存覆盖或淘汰可能释放被借用字符串，之前的零拷贝路径已经因为 UAF 被撤销。`getBorrowed()` 只有在保证整个借用期间无覆盖、删除、TTL 清理和协程切换的内部路径中才可能安全，风险应定为**高**，不是“中”。

---

## 3. 1.6 响应全量缓冲会破坏 `write()` 语义

当前 `Response::write()`：

- Swoole 下调用 `$response->write()`；
- FPM 下执行 `php_write()` 并 `sapi_flush()`；
- 明确承担流式输出功能。

<ref_snippet file="F:\github_code\gene\src\http\response.c" lines="817-853" />

文档建议把所有 `write()` 累积到 `smart_str`，然后在 `end()` 或析构时输出。这会破坏：

- SSE；
- chunked streaming；
- 大文件或大响应的恒定内存特性；
- 调用者期望的及时 flush；
- 客户端首字节时间。

而且文档引用的 `response.c:863-865` 循环丢弃 output buffer，实际属于 `sseStart()`，正是为了确保 SSE 不被缓冲，不是普通响应输出的低效逻辑。

<ref_snippet file="F:\github_code\gene\src\http\response.c" lines="857-894" />

因此这一项不能作为透明优化。合理方向只能是：

- 保留 `write()` 的流式语义；
- 另增明确的 buffered API；
- 或仅减少非流式内部调用产生的小块写入；
- 不应在 Swoole 下把现有 `$response->write()` 改为全量缓冲。

风险应从“中”提高到**高，且涉及 API 语义变化**。

---

## 4. 5.5 Session“写合并”其实已经存在

当前 `Session::set()`：

- 修改内存中的 session 数组；
- 只设置 dirty；
- 不立即写后端；
- `save()` 或析构时才调用 handler 保存。

<ref_snippet file="F:\github_code\gene\src\session\session.c" lines="1034-1055" />  
<ref_snippet file="F:\github_code\gene\src\session\session.c" lines="947-986" />

Cookie 也有 `cookie_sent` 判断，不会每次 `set()` 都真正重复发送。当前尚可优化的是每次 `set/del` 都调用一次 `gene_session_auto_cookie()` 的属性检查，而不是“后端写合并”。

此外，文档建议：

> `php_random_bytes` + `gene_u64_to_hex`

如果只生成一个 64-bit 值，Session ID 熵只有 64 bit，偏低。更合理的是至少 16 字节随机数再 hex 编码为 32 字符。

---

## 5. 5.6 Log 的两项优化已经实现

文档建议：

1. 级别过滤前移到任何分配之前；
2. context 为空时跳过 JSON。

当前代码已经如此：

- 日志级别在 datetime、context 合并和 JSON 之前判断；
- 只有非空 context 数组才编码 JSON。

<ref_snippet file="F:\github_code\gene\src\tool\log.c" lines="173-190" />  
<ref_snippet file="F:\github_code\gene\src\tool\log.c" lines="226-249" />

仍然有效的优化只有：

- 秒级时间字符串缓存；
- 减少 `error_log()` PHP 调用帧；
- 可选日志缓冲。

但直接用 `write()` 替代 `error_log()` 会改变 SAPI/error_log 配置、日志轮转、多进程追加和 Windows 行为，不能称为低风险。宣称“吞吐 5–10×”也没有基准依据。

---

## 6. 1.1 的 hook 预编译实际上已经完成大部分

`gene_route_pc` 已经保存：

- `is_before` / `is_after`；
- before/after/hook 源码；
- 对应 closure；
- route action 源码；
- eval 程序。

<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="757-797" />  
<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="1005-1066" />

所以文档所说“把 hook 解析结果存入 `gene_route_pc`”属于已有实现。真正待做的只有：

- 默认是否开启；
- action `zend_function *` 是否值得缓存；
- 路由修改后的失效机制。

特别是 `route_pc` 以 leaf `HashTable*` 地址为 key，并借用路由树内的指针。开启后必须保证 `workerReady()` 后路由树绝对只读，否则可能出现 stale descriptor 或 UAF。

<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="1364-1392" />

---

# 二、方向合理，但方案需要重新设计

## 1.3 `chird` 线性扫描：瓶颈存在，提出的索引不匹配当前结构

线性递归扫描确实存在：

<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="300-374" />

但当前结构中：

- 静态 segment 已经先通过 HashTable 精确查找；
- `chird` 主要按占位符名称保存；
- 当前匹配代码没有按“正则类型/首字符”检查当前 segment；
- 多个泛型占位符真正的区别通常在后续路径分支。

所以“静态 → 正则 → 泛型分桶”“首字符索引”无法自然把复杂度降至近 O(1)。静态本来就不在 `chird` 线性扫描中。

更合理的方向是：

- 注册期检测同层等价泛型路由；
- 对后续固定 suffix 构建判别索引；
- 或重新设计成带约束的 radix tree；
- 明确定义多个可变路由冲突时的优先级。

瓶颈判断合理，但具体方案和“近 O(1)”收益明显过度乐观，风险应为**中高**。

---

## 1.2 action 双重哈希：小优化成立，但不能无条件缓存到 route_pc

当前确实先 `zend_hash_str_exists()`，进入 `gene_factory_call_1()` 后又查一次方法。

<ref_snippet file="F:\github_code\gene\src\router\router.c" lines="447-466" />

改成一次 `find_ptr` 并把 `zend_function *` 传给调用函数是合理的。

但把函数指针缓存到 `route_pc` 只适用于：

- 类和 action 都是固定值；
- class entry 已稳定；
- 没有 `:c` / `:a` 动态替换；
- 不允许运行期修改类或路由。

同一个 route leaf 如果可派发到不同 controller/action，就不能只缓存一个函数指针。建议保留为局部微优化，不应与 route_pc 绑定为通用方案。

---

## 1.4 ctx 内联缓冲：可行，但收益数字没有依据

减少 method/path/module/controller/action 的小块分配是合理方向。不过：

- 给每个 context 固定加入多个 64/256 字节缓冲，会提高所有活跃协程和 ctx pool 的常驻内存；
- 长字段仍需处理 heap fallback 及所有权标记；
- `ptr,len` 借用 URI 内容必须严格保证 URI 生命周期；
- “分配器流量下降约 50%”无法从静态分析得出。

相比为每个字段分别增加数组，更适合评估：

- 一个 ctx 级小型 arena；
- 单一 URI backing buffer；
- 字段保存 offset/length；
- reset 时整体复位。

方向合理，优先级“中”合理，收益比例应删除或等压测后填写。

---

## 1.5 Webscan per-worker 对象：不如直接去对象化

`check()` 当前主要读取对象配置，没有明显请求状态写入：

<ref_snippet file="F:\github_code\gene\src\http\webscan.c" lines="224-267" />

但把普通 PHP 对象跨请求保存在 worker 级位置仍涉及：

- 属性 zval 生命周期；
-配置热更新；
- RINIT/RSHUTDOWN；
- Swoole 与 FPM 差异。

更简单安全的方案是应用层直接调用一个接收配置的 C helper，完全省掉对象构造，而不是长期保存 PHP 对象。

另外 flatten 增加长度上限会改变安全扫描覆盖范围，属于安全策略变化，不能只按性能优化处理。

---

## 2.2 View 输出捕获：收益未证明

`render()` 使用 PHP output buffer 是正常语义，因为模板可以执行任意 PHP 并通过 `echo`、include、扩展输出。要直接捕获到 `smart_str`，最终仍需要接入 PHP output handler，并不一定能省掉 output subsystem。

<ref_snippet file="F:\github_code\gene\src\mvc\view.c" lines="813-859" />

“嵌套子视图共享一层 buffer”还需先证明当前代码确实创建了多层 buffer；普通 include/contains 通常天然写入当前 buffer。该项不应列为明确的中等收益项目。

---

## 2.3 `EG(included_files)` 只能限定用于类自动加载

`gene_load_import()` 既编译、执行文件，又把路径加入 `EG(included_files)`，但没有先判断是否已包含。

<ref_snippet file="F:\github_code\gene\src\factory\load.c" lines="76-127" />

短路要注意：

- 路径必须规范化到 `opened_path`；
- 类自动加载可近似 include_once；
- 视图文件必须允许重复执行并使用不同 symbol table；
- 不能给通用 `gene_load_import()` 无条件加入短路。

`workerReady()` 预 include 整个控制器/模型目录还可能改变加载顺序和文件副作用，风险不是“低”。

---

# 三、缓存部分

## 3.1 拆分框架缓存与业务缓存：方向正确

这一项是架构上最值得做的项目之一。

当前首次业务写会永久设置 `cache_business_dirty=1`，之后所有框架元数据读取重新加锁：

<ref_snippet file="F:\github_code\gene\src\cache\memory.h" lines="21-46" />

拆分后可以获得：

- 框架表 workerReady 后真正不可变；
- 路由/配置/DI 继续无锁；
- 业务表独立 TTL/LRU/锁；
- 不再要求两种完全不同生命周期的数据共享 bucket reserve。

但“高并发读串行点”的表述略夸张。单个 Swoole worker 通常是单线程协作调度，短小且不 yield 的 rwlock 临界区更多是固定原子/函数开销，不一定发生严重线程竞争。

RCU 没有必要作为紧随其后的方案：单 worker 协程模型中收益有限，回收 epoch 和裸指针问题反而很大。优先完成拆表即可。

### 文档遗漏的重要问题

冻结后新 key 插入依赖预留 bucket，而删除留下的 tombstone 不会降低 `nNumUsed`。长期高 churn 即使有 LRU，也会逐渐耗尽预留 bucket，最终增加 `cache_insert_refused`。

<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="833-865" />

因此拆表不仅是消除读锁，也应解决业务表必须允许正常 rehash 的问题。

---

## 3.3 `mget()` 一次加锁：合理，但方案遗漏 TTL

当前确实逐 key 调 `gene_memory_get()`：

<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="1782-1824" />

一次锁内批量查询是合理优化，但不能只是直接 `zend_symtable_str_find`，还必须保留：

- TTL 判断；
- 过期项返回 miss；
- 深拷贝必须在释放锁之前完成；
- FPM 下是否执行延迟删除；
- hit/miss 计数语义。

已有 `gene_memory_get_triple()` 可以作为正确参考。

<ref_snippet file="F:\github_code\gene\src\cache\memory.c" lines="901-938" />

包含这些约束后风险仍不高，但文档方案本身不完整。

---

## 3.4 缓存键：可以优化，但改默认哈希不是低风险

代码已经支持多种 hash mode，包括 FNV、xxHash64、FarmHash、Murmur 和 TurboHash：

<ref_snippet file="F:\github_code\gene\src\cache\cache.c" lines="563-619" />

改变默认哈希会导致：

- 现有外部/APCu/Redis/Memcached key 全部变化；
- 滚动发布期间新旧实例不能共享缓存；
- 命中率瞬间归零；
- 可能引入不同碰撞概率和 key 长度。

所以可增加推荐配置或显式版本化，而不应作为“低风险默认替换”。

---

## 3.5 TTL sweep：方向合理

写少读多时，过期 key 确实只表现为 miss，不一定回收存储。可配置 sweep 合理。

但 Swoole `Timer::tick` 回调必须：

- 只操作业务表；
- 与 worker 生命周期绑定；
- 避免对每个实例创建 timer；
- 控制单次扫描时间；
- 不在锁内 yield。

---

## 3.6 FPM 非 ZTS 跳锁：结论条件不完整

文档说：

> FPM worker 单线程，`worker_ready=0`

但 `workerReady()` 实际没有在 `runtime_type<2` 时提前返回，会照样 reserve 并将 `worker_ready=1`：

<ref_snippet file="F:\github_code\gene\src\app\application.c" lines="1317-1387" />

因此“FPM 下 worker_ready 永远为 0”不成立。虽然典型 FPM 应用不会主动调用它，但优化条件不能建立在这个未强制的假设上。

---

# 四、数据库部分

## 4.1 Pool 缺失 `FETCH_ASSOC`：成立，第一优先级合理

Pool 规范化配置只设置：

- ERRMODE；
- EMULATE_PREPARES；
- Swoole 下 PERSISTENT=false；

没有设置 `ATTR_DEFAULT_FETCH_MODE=FETCH_ASSOC`。

<ref_snippet file="F:\github_code\gene\src\db\pool.c" lines="246-285" />

非池路径则设置了 `19 => 2`：

<ref_snippet file="F:\github_code\gene\src\db\mysql.c" lines="265-279" />

这是清单中最明确的低风险高收益项。但不能只在“options 不存在”分支加一行，已有 options 数组的分支也要设置。另外应确认 Pgsql/Sqlite/Mssql 非池路径中的 `ATTR_CASE`、`ATTR_ORACLE_NULLS` 是否也需要保持一致。

---

## 4.2 FPM 默认持久连接：收益可能高，但不应该默认开启

FPM 不走 Gene Pool 的判断准确：

<ref_snippet file="F:\github_code\gene\src\db\pool.c" lines="1551-1564" />

事务回滚 hygiene 已有，但不能清除全部连接状态，例如：

- session variables；
- temporary tables；
- advisory locks；
- prepared statements；
- SQL mode/time zone；
-认证状态和断线状态。

所以可以提供显式配置或文档建议，但不应把 `ATTR_PERSISTENT=true` 作为框架默认。优先级可以高，风险应为**中高**。

---

## 4.3 PDO 方法指针缓存：成立，但优先级被高估

源码中不是 12 处，而是约 19 处方法查找。优化确实有效：

<ref_snippet file="F:\github_code\gene\src\db\pdo.c" lines="856-875" />

但相对真实 SQL 网络和数据库执行时间，一次 HashTable lookup 通常很小。除非微基准证明 CPU 已成为瓶颈，不适合作为 P0。

还要注意：

- `PDOStatement` 可以通过 `ATTR_STATEMENT_CLASS` 使用用户类；
- 用户类 method/CE 在 FPM 下可能只有请求生命周期；
- 一个进程级静态 `zend_function *` 不能缓存任意用户 statement class；
- 需要只对精确内部 CE 使用静态缓存，其他类型继续动态查找。

技术上可做，实际应为 P2/P3 微优化。

---

## 4.4 ORM `call_user_function`：成立

这里每次确实创建方法名并动态调用：

<ref_snippet file="F:\github_code\gene\src\orm\meta.c" lines="341-351" />

Gene 四个 Db 类为 final，因此按精确 CE 缓存常用函数指针相对安全。因为一次 ORM 查询会组合多个链式调用，累积收益可能比 4.3 更明显。

适合第一批，但仍应通过“纯内存 mock Db 调用微基准”测量，不能用真实远程 SQL 延迟掩盖结果。

---

## 4.5 Statement LRU：可行，但收益表述不完整

在 native prepares 下，复用 statement 可以省掉重复 SQL 的 server prepare 往返；不会省掉 execute 往返。

适用前提：

- SQL 高度重复；
- 同一物理连接；
- 正确关闭 cursor；
- 连接重连后全部失效；
- 控制服务端 prepared statement 数量；
- 处理 DDL、SET 和驱动差异。

高风险评级正确，不应在驱动抽象完成前优先实施。

---

## 4.8 四驱动抽象：合理，但不是其他优化的技术前提

抽象方言能明显减少重复和维护成本，但 4.3、4.5、4.6 都可以先在公共 `pdo.c` 或 shared helper 中实现，并非必须等待四驱动整体重构。

大重构与性能优化最好分开，否则回归时很难判断是抽象错误还是优化错误。

---

# 五、Redis、通用优化与构建

## 5.1 Pool Atomic C 化：必须先强制 per-worker 约束

同 worker、单线程、C 操作期间不 yield 时，普通 `zend_long` 足够。但源码/API 没有阻止用户在 server start 前构造 Pool。

直接替换可能改变跨 worker 使用语义。合理顺序是：

1. 明确 Pool 只能在 WorkerStart 创建；
2. 禁止 clone/serialize；
3. 检测进程 PID；
4. 然后把计数改为 worker-local C 字段。

否则风险接近中高。

---

## 5.2 RedisPool“1ms忙等”有所夸张

正常空池路径并不会简单循环 `max+2` 次：

1. 先尝试一次 1ms pop；
2. 未满则创建连接；
3. 已满则进入 `waitTimeout` 阻塞；
4. 只有无效队列项或创建失败等路径才重试。

<ref_snippet file="F:\github_code\gene\src\cache\redis_pool.c" lines="1230-1307" />

所以它更像“每次空闲队列 miss 多出约 1ms 延迟”，而不是持续忙等。优化仍有价值，但现状和收益描述应调整。

---

## 5.3 serialize C API：可行但不是中等通用收益

当前已经缓存 PHP 函数指针，并通过 `zend_call_known_function` 调用，不是最慢的 `call_user_function`：

<ref_snippet file="F:\github_code\gene\src\common\common.c" lines="743-793" />

直接调用 `php_var_serialize` 能省调用帧，但大对象序列化的主要成本仍然是递归遍历和分配。收益可能只在大量小值时明显。

igbinary C API 还会增加可选扩展 ABI、头文件和构建探测依赖。

---

## 6.1 JSON 直调：技术可行，但“高”评级不合理

源码注释已经明确记录了当前权衡：

- PHP 8.x 小版本间内部 API 签名变化；
- 当前函数指针已缓存；
- 约 0.5µs 调用开销可以接受。

<ref_snippet file="F:\github_code\gene\src\common\common.c" lines="743-763" />

因此除非压测显示每请求大量调用 JSON 编解码，否则它更像低到中优先级兼容性换性能项，不应标为“高”。

---

## 6.2 编译标志：需要先确认 PHP 构建继承值

`config.m4` 没有主动追加 `-O3/LTO` 是事实：

<ref_file file="F:\github_code\gene\src\config.m4" />  
<ref_file file="F:\github_code\gene\src\config.w32" />

但扩展编译通常继承 PHP 构建环境的优化标志，不能由“config.m4 没写”推导出“当前没有优化”。

还需要注意：

- `-O3` 未必比 `-O2` 更快；
- LTO 对 PHP 扩展收益可能很小；
- `/GL` 必须和最终链接参数配套；
- `-fvisibility=hidden` 需检查所有导出符号；
- `-march=native` 只能用于同机部署。

“中高收益”没有数据支持，应先比较实际编译命令和生成汇编/基准结果。

---

## 6.3 PCRE 缓存：描述不完整

通过 PHP `preg_match` 调用时，PHP 本身已经使用 PCRE 编译缓存。直接调用 `pcre_get_compiled_regex_cache()` 主要节省 PHP 调用帧，并不是从“完全不缓存”变成“缓存”。

如果规则字符串每次内容相同，当前已有请求内缓存收益；如果希望跨 Swoole 请求复用，还需证明 PHP 的缓存生命周期及正则字符串生命周期。

---

## 6.5 FPM Curl 复用：ctx 级复用无法跨请求

FPM 每请求都会销毁 ctx，因此“ctx 级 handle”只能在同一请求内复用，不能解决“每请求 `curl_easy_init`”。

如果要跨 FPM 请求复用，需要：

- process-level handle pool；
- 每次 `curl_easy_reset`；
- 清除 header、callback、POST body、private data；
- 处理 fork、DNS、TLS 和异常；
- 不能长期保存请求级 zval。

直接链接 libcurl 还会增加构建依赖；这应视为独立架构功能，而不是普通中风险优化。

---

# 六、落地顺序需要调整

当前“第一批”包含一些合理项目，但建议改成以下逻辑。

## 真正适合第一批

1. Pool 补 `ATTR_DEFAULT_FETCH_MODE=FETCH_ASSOC`，同时覆盖自定义 options 分支。
2. action 单次 HashTable lookup，但暂不缓存进 route_pc。
3. JSON 类等明确的函数指针缓存。
4. `mget()` 单锁实现，同时保留 TTL 和锁内深拷贝。
5. ORM final Db 类的 known-function 调用。
6. 零散 `ZEND_STRL` / `ZVAL_STRINGL`。
7. Benchmark 改用 `gene_hrtime()` 和 Zend memory C API。
8. 更正生产配置，尤其 View 配置。

## 第二批

1. 框架缓存/业务缓存拆表。
2. Webscan 去对象化。
3. ctx arena/offset 方案。
4. FPM 持久连接作为显式 opt-in。
5. Pool worker-local 计数，但先强制生命周期约束。
6. Log 时间缓存与可选缓冲。
7. 驱动公共 helper 抽取。

## 暂缓或先做专项设计

1. Response 全量缓冲。
2. route_pc 缓存 action function。
3. Statement LRU。
4. Cache RCU。
5. 全量驱动抽象与性能改造同时进行。
6. OpenSSL/libcurl 直接链接。
7. 修改默认缓存哈希算法。

---

# 七、验证方法也不够可靠

`test/BenchmarkTest.php` 主要是 `Gene\Benchmark` 的功能测试，有大量 `usleep`、打印和一般 PHP 操作，不能验证 Router、Memory、PDO 等优化前后的性能。

<ref_file file="F:\github_code\gene\test\BenchmarkTest.php" />

建议的验证至少应包括：

- 每个优化独立的 C/PHP 微基准；
- 固定 warm-up；
- 多轮运行并报告 median、p95/p99、标准差；
- CPU pinning 或至少固定 worker 数量；
- 基线/优化版使用相同 PHP、OPcache、Swoole、数据库配置；
- 路由基准区分静态、单占位、多冲突占位、深层路径；
- Memory 基准区分无业务写、首次 dirty 后、TTL、高 churn；
- 数据库函数派发使用 mock/本地无网络基准；
- 真实 SQL 另测吞吐和连接建立成本；
- 输出/SSE 必须验证到达时间，而不只是总 RPS。

文档里的 ASAN 命令也不够完整。通常需要在 `./configure`/编译和链接阶段都带 sanitizer 参数，并确保 PHP 进程能加载 ASAN runtime；`test/*.php` 也不是一个能够自动执行全部测试文件的命令。

---

## 最终判断

**值得保留的核心方向：**

- 框架/业务缓存拆分；
- Pool `FETCH_ASSOC`；
- ORM 动态调用优化；
- `mget` 批量锁；
- action 双重查找；
- ctx 分配优化；
- 驱动去重；
- 生产模式关闭 SQL history；
- 更系统的压测与 ASAN 验证。

**当前明确不合理或已经过时的内容：**

- 2.1 View 编译语义；
- 1.1 “尚未缓存 hook 结果”；
- 1.6 全量缓冲 `write()`；
- 3.2 标量尚未直接复制；
- 5.5 Session 尚未写合并；
- 5.6 Log 尚未前置级别过滤/跳过空 context；
- 5.2 “最多 max+2 次 1ms 忙等”的常态描述；
- 6.2 从 config 文件推断没有编译优化；
- 9 将 `BenchmarkTest.php` 当性能微基准。

所以结论是：**技术审计框架合理，但内容中存在足以导致性能倒退或语义回归的错误；必须先校正，再进入实施阶段。**

本次仅分析，**没有修改任何文档或源码**。