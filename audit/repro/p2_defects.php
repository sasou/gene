<?php
/* [AUDIT 2026-08-23 P2] 第二轮三处缺陷的复现/验证脚本：
 *
 *   P2-1 类型预检只覆盖顶层 → 嵌套 object/resource 在持锁状态下 E_ERROR bailout，
 *        泄漏写锁。修复后：递归全量预检在取锁前以 E_WARNING 拒写（子进程验证，
 *        见 swoole_cache_uaf_obj.php，由 swoole_cache_uaf.php STEP E 驱动）。
 *
 *   P2-2 "半截条目"（有 version、无 data）在 6 处版本化缓存读路径被直接解引用：
 *        cachedVersion / localCachedVersion / processCachedVersion /
 *        cachedVersionBatch / localCachedVersionBatch / processCachedVersionBatch。
 *        修复后：cacheData == NULL 一律按 miss 走重算分支。本脚本用用户态 hook
 *        store 构造半截条目，验证 cachedVersion / cachedVersionBatch 两条代表性
 *        路径（其余四处代码形态相同）。
 *
 *   P2-3 cache_reserve <= cache_max_items 时 LRU 淘汰永不触发、冻结后新键被静默
 *        拒写。修复后：workerReady() 发现配置矛盾即 E_WARNING（子进程验证）。
 *
 * 运行：php audit\repro\p2_defects.php
 */

use Gene\Cache\Cache;
use Gene\Di;

error_reporting(E_ALL);
ini_set('display_errors', '1');

$fail = 0;
function check($label, $cond) {
    global $fail;
    echo ($cond ? "ok" : "FAIL"), "  {$label}\n";
    if (!$cond) {
        $fail++;
    }
}

class P2Store
{
    public $data = [];
    public function get($key) {
        if (is_array($key)) {
            /* 多 key 取必须返回数组（可为空）；返回 false 会让 cachedVersion 冷启动
             * 直接 RETURN_NULL（与真实 redis 类 hook 语义一致）。 */
            $out = [];
            foreach ($key as $k) {
                if (is_string($k) && array_key_exists($k, $this->data)) {
                    $out[$k] = $this->data[$k];
                }
            }
            return $out;
        }
        return array_key_exists($key, $this->data) ? $this->data[$key] : false;
    }
    public function set($key, $value, $ttl = 0) { $this->data[$key] = $value; return true; }
    public function delete($key) { unset($this->data[$key]); return true; }
    public function incr($key, $val = 1) {
        if (!isset($this->data[$key])) { $this->data[$key] = 0; }
        $this->data[$key] += (int) $val;
        return $this->data[$key];
    }
    /* 模拟"半截条目"：所有 {data,version} payload 只留 version。 */
    public function stripData() {
        foreach ($this->data as $k => $v) {
            if (is_array($v) && array_key_exists('version', $v) && array_key_exists('data', $v)) {
                unset($this->data[$k]['data']);
            }
        }
    }
}

class P2Producer
{
    public $calls = 0;
    public function produce($p = null) {
        $this->calls++;
        return ['p' => $p, 'n' => $this->calls];
    }
}

echo "== P2-1: 嵌套 object / 自引用数组 取锁前拒写（子进程） ==\n";
$child = sprintf('"%s" -n -d extension_dir=%s -d extension=pdo_sqlite -d extension=%s %s',
    PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
    'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
    escapeshellarg(__DIR__ . '\\swoole_cache_uaf_obj.php'));
exec($child, $out, $code);
echo "  child exit={$code} output=", implode(' ', $out), "\n";
check('nested object refused, no fatal, no lock leak', $code === 0
    && strpos(implode(' ', $out), 'refused=yes alive=yes') !== false);

echo "== P2-2a: cachedVersion 命中半截条目按 miss 重算 ==\n";
$store = new P2Store();
$producer = new P2Producer();
Di::set('p2store', $store);
$cache = new Cache(['hook' => 'p2store', 'sign' => 'p2a:', 'versionSign' => 'p2ver:']);

$r1 = $cache->cachedVersion([$producer, 'produce'], ['x'], ['user', 1], 3600);
check('first call populates cache', is_array($r1) && $producer->calls === 1);

$store->stripData(); /* 制造 {version} 无 {data} 的半截条目 */
$r2 = $cache->cachedVersion([$producer, 'produce'], ['x'], ['user', 1], 3600);
check('half entry treated as miss -> recompute (no NULL deref)',
    is_array($r2) && $producer->calls === 2 && $r2['n'] === 2);

$r3 = $cache->cachedVersion([$producer, 'produce'], ['x'], ['user', 1], 3600);
check('recomputed entry fully written -> hit again',
    is_array($r3) && $producer->calls === 2);

echo "== P2-2b: cachedVersionBatch 命中半截条目按 miss 重算 ==\n";
$store->data = [];
$producer->calls = 0;
$items = [
    [[$producer, 'produce'], ['b1']],
    [[$producer, 'produce'], ['b2']],
];
$rb1 = $cache->cachedVersionBatch($items, ['user', 1], 3600);
check('batch first call populates cache',
    is_array($rb1) && count($rb1) === 2 && $producer->calls === 2);

$store->stripData();
$rb2 = $cache->cachedVersionBatch($items, ['user', 1], 3600);
check('batch half entries -> recompute all (no NULL deref)',
    is_array($rb2) && count($rb2) === 2 && $producer->calls === 4
    && $rb2[0]['n'] === 3 && $rb2[1]['n'] === 4);

echo "== P2-3: workerReady 校验 cache_reserve > cache_max_items（子进程） ==\n";
$childScript = sys_get_temp_dir() . '\\p2_workerready.php';
file_put_contents($childScript, "<?php\n\\Gene\\Application::workerReady();\necho \"workerReady done\\n\";\n");
$runChild = function ($reserve, $maxItems) use ($childScript) {
    $cmd = sprintf('"%s" -n -d extension_dir=%s -d extension=pdo_sqlite -d extension=%s'
        . ' -d gene.cache_reserve=%d -d gene.cache_max_items=%d %s 2>&1',
        PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
        'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
        $reserve, $maxItems, escapeshellarg($childScript));
    exec($cmd, $o, $c);
    return implode("\n", $o);
};
$bad = $runChild(4096, 10000);
check('contradictory config (4096 <= 10000) triggers E_WARNING',
    strpos($bad, 'gene.cache_reserve') !== false && strpos($bad, 'gene.cache_max_items') !== false);
$good = $runChild(65536, 10000);
check('sane config (65536 > 10000) stays silent', strpos($good, 'cache_reserve') === false);
$off = $runChild(0, 0);
check('defaults (max_items=0) stay silent', strpos($off, 'cache_reserve') === false);
@unlink($childScript);

echo $fail === 0 ? "\nPASS\n" : "\nFAIL ({$fail})\n";
exit($fail === 0 ? 0 : 1);
