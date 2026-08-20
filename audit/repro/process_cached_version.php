<?php
/**
 * processCachedVersion 行为 / 异常 / 泄漏探针
 *
 * 对照 localCachedVersion / processCachedVersionBatch 的实现：
 * - 数据在 Gene\Memory，版本从 hook->get(versionKeys)
 * - 命中条件：payload 为 array 且 checkVersion(payload.version, cur_version, mode)
 */

class PcvStore
{
    public $data = [];
    public $getCalls = 0;
    public $throwOnGet = false;

    public function get($key)
    {
        $this->getCalls++;
        if ($this->throwOnGet) {
            throw new RuntimeException('hook get boom');
        }
        if (is_array($key)) {
            $out = [];
            foreach ($key as $k) {
                if (is_string($k) && array_key_exists($k, $this->data)) {
                    $out[$k] = $this->data[$k];
                }
            }
            return $out === [] ? false : $out;
        }
        return array_key_exists($key, $this->data) ? $this->data[$key] : false;
    }

    public function set($key, $value, $ttl = 0)
    {
        $this->data[$key] = $value;
        return true;
    }

    public function delete($key)
    {
        unset($this->data[$key]);
        return true;
    }

    public function incr($key, $val = 1)
    {
        if (!isset($this->data[$key])) {
            $this->data[$key] = 0;
        }
        $this->data[$key] += (int) $val;
        return $this->data[$key];
    }

    public function clear()
    {
        $this->data = [];
        $this->getCalls = 0;
    }
}

class PcvHelper
{
    public static $calls = 0;
    public static $throw = false;
    public static $returnObject = false;

    public function produce($param = null)
    {
        self::$calls++;
        if (self::$throw) {
            throw new RuntimeException('produce boom');
        }
        if (self::$returnObject) {
            return (object) ['param' => $param];
        }
        return ['param' => $param, 'n' => self::$calls];
    }
}

function fail($msg)
{
    echo "FAIL: $msg\n";
    return false;
}

function ok($msg)
{
    echo "OK   $msg\n";
    return true;
}

$php = PHP_BINARY;
echo "PHP $php gene=" . phpversion('gene') . "\n\n";

$store = new PcvStore();
$helper = new PcvHelper();
$cb = [$helper, 'produce'];
\Gene\Di::set('pcvStore', $store);
$cache = new \Gene\Cache\Cache([
    'hook' => 'pcvStore',
    'sign' => 'pcv:',
    'versionSign' => 'pcvv:',
]);
$ver = ['db.user' => 1];
$args = ['a'];
$pass = 0;
$fail = 0;

function tally($ok)
{
    global $pass, $fail;
    $ok ? $pass++ : $fail++;
}

// --- 1. empty config ---
$empty = new \Gene\Cache\Cache([]);
tally($empty->processCachedVersion($cb, $args, $ver) === null
    ? ok('empty config returns null')
    : fail('empty config should return null'));

// --- 2. missing DI hook ---
$bad = new \Gene\Cache\Cache([
    'hook' => 'noSuchHook',
    'sign' => 'pcv:',
    'versionSign' => 'pcvv:',
]);
tally($bad->processCachedVersion($cb, $args, $ver) === null
    ? ok('missing hook returns null')
    : fail('missing hook should return null'));

// --- 3. miss then hit (must updateVersion first so hook get returns array) ---
$store->clear();
PcvHelper::$calls = 0;
$r1 = $cache->processCachedVersion($cb, $args, $ver);
$c1 = PcvHelper::$calls;
$r2 = $cache->processCachedVersion($cb, $args, $ver);
$c2 = PcvHelper::$calls;
echo "  no-incr: calls after 2 reads = $c2 (expect always miss if get()=false)\n";
if ($c2 === 2) {
    tally(ok('before updateVersion: get()=false => checkVersion always miss (recompute every time)'));
} elseif ($c2 === 1) {
    tally(ok('before updateVersion: second call HIT (unexpected vs checkVersion needing arrays)'));
} else {
    tally(fail("unexpected call count $c2"));
}

$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion($ver);
$r1 = $cache->processCachedVersion($cb, ['hit1'], $ver);
$r2 = $cache->processCachedVersion($cb, ['hit1'], $ver);
if (PcvHelper::$calls === 1 && $r1 === $r2 && ($r1['param'] ?? null) === 'hit1') {
    tally(ok('after updateVersion: miss then HIT (calls=1)'));
} else {
    tally(fail('after updateVersion hit failed calls=' . PcvHelper::$calls . ' r1=' . json_encode($r1) . ' r2=' . json_encode($r2)));
}

// --- 4. updateVersion invalidates ---
$cache->updateVersion($ver);
$r3 = $cache->processCachedVersion($cb, ['hit1'], $ver);
if (PcvHelper::$calls === 2 && ($r3['n'] ?? 0) === 2) {
    tally(ok('updateVersion forces recompute'));
} else {
    tally(fail('updateVersion did not invalidate calls=' . PcvHelper::$calls));
}

// --- 5. indexed versionField vs assoc (docs require assoc) ---
$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion(['user' => 1]);
$cache->processCachedVersion($cb, ['idx'], ['user', 1]);
$cache->processCachedVersion($cb, ['idx'], ['user', 1]);
echo "  indexed ['user',1] calls=" . PcvHelper::$calls . "\n";
tally(ok('indexed versionField does not crash (calls=' . PcvHelper::$calls . ')'));

$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion(['user' => 1]);
$cache->processCachedVersion($cb, ['assoc'], ['user' => 1]);
$cache->processCachedVersion($cb, ['assoc'], ['user' => 1]);
if (PcvHelper::$calls === 1) {
    tally(ok("assoc ['user'=>1] hits on second call"));
} else {
    tally(fail("assoc versionField calls=" . PcvHelper::$calls));
}

// --- 6. multi-value version field ---
$store->clear();
PcvHelper::$calls = 0;
$mv = ['db.user.id' => [10, 20]];
$cache->updateVersion($mv);
$cache->processCachedVersion($cb, ['mv'], $mv);
$cache->processCachedVersion($cb, ['mv'], $mv);
if (PcvHelper::$calls === 1) {
    tally(ok('multi-value versionField hits'));
} else {
    tally(fail('multi-value calls=' . PcvHelper::$calls));
}
$cache->updateVersion(['db.user.id' => [10]]);
$cache->processCachedVersion($cb, ['mv'], $mv);
if (PcvHelper::$calls === 2) {
    tally(ok('partial multi-value incr invalidates (mode default)'));
} else {
    tally(fail('partial incr calls=' . PcvHelper::$calls . ' expected 2'));
}

// --- 7. mode true: element count must match ---
$store->clear();
PcvHelper::$calls = 0;
$mv2 = ['a' => 1, 'b' => 2];
$cache->updateVersion($mv2);
$cache->processCachedVersion($cb, ['mode'], $mv2, null, true);
$cache->processCachedVersion($cb, ['mode'], $mv2, null, true);
if (PcvHelper::$calls === 1) {
    tally(ok('mode=true hit when counts match'));
} else {
    tally(fail('mode=true first pair calls=' . PcvHelper::$calls));
}

// --- 8. producer throw: should not crash, exception surfaces ---
PcvHelper::$throw = true;
PcvHelper::$calls = 0;
$store->clear();
$cache->updateVersion($ver);
$threw = false;
try {
    $cache->processCachedVersion($cb, ['ex'], $ver);
} catch (RuntimeException $e) {
    $threw = ($e->getMessage() === 'produce boom');
}
PcvHelper::$throw = false;
tally($threw ? ok('producer exception surfaces') : fail('producer exception not surfaced'));

// --- 9. hook get throw ---
$store->throwOnGet = true;
$threw = false;
try {
    $cache->processCachedVersion($cb, ['gex'], $ver);
} catch (RuntimeException $e) {
    $threw = ($e->getMessage() === 'hook get boom');
}
$store->throwOnGet = false;
tally($threw ? ok('hook get exception surfaces') : fail('hook get exception not surfaced'));

$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion($ver);
$store->throwOnGet = true;
try {
    $cache->processCachedVersion($cb, ['pollute'], $ver);
} catch (RuntimeException $e) {
}
$store->throwOnGet = false;
PcvHelper::$calls = 0;
$after = $cache->processCachedVersion($cb, ['pollute'], $ver);
if (PcvHelper::$calls === 1 && is_array($after) && ($after['param'] ?? null) === 'pollute') {
    tally(ok('after hook-get throw: next call recomputes (no stale hit)'));
} else {
    tally(fail('after hook-get throw unexpected calls=' . PcvHelper::$calls . ' ret=' . json_encode($after)));
}

// --- 10. return object → Memory persist E_ERROR (fatal) skipped via subprocess? ---
// 同进程 E_ERROR 会杀掉脚本，用单独尝试 + error handler 不够。记录为已知约束。
echo "SKIP return-object: Gene\\Memory 不支持 object，会 E_ERROR（与 processCached 相同）\n";

// --- 11. processCached vs processCachedVersion 互不污染 ---
$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion($ver);
$cache->processCached($cb, ['iso'], 3600);
$nAfterProc = PcvHelper::$calls;
$cache->processCachedVersion($cb, ['iso'], $ver);
if (PcvHelper::$calls === $nAfterProc + 1) {
    tally(ok('processCached and processCachedVersion use different keys (type 1 vs 0)'));
} else {
    tally(fail('key isolation failed calls=' . PcvHelper::$calls));
}

// --- 12. unsetProcessCached 清不掉 Version 条目 ---
$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion($ver);
$cache->processCachedVersion($cb, ['unset'], $ver);
$cache->unsetProcessCached($cb, ['unset']);
$cache->processCachedVersion($cb, ['unset'], $ver);
if (PcvHelper::$calls === 1) {
    tally(ok('unsetProcessCached does NOT drop processCachedVersion entry (type mismatch)'));
} else {
    tally(fail('unsetProcessCached unexpectedly dropped version cache calls=' . PcvHelper::$calls));
}

// --- 13. batch vs single ---
$store->clear();
PcvHelper::$calls = 0;
$cache->updateVersion($ver);
$items = [[$cb, ['b1']], [$cb, ['b2']]];
$batch = $cache->processCachedVersionBatch($items, $ver);
$s1 = $cache->processCachedVersion($cb, ['b1'], $ver);
$s2 = $cache->processCachedVersion($cb, ['b2'], $ver);
if (PcvHelper::$calls === 2 && $batch[0] === $s1 && $batch[1] === $s2) {
    tally(ok('processCachedVersionBatch then single hits same payload'));
} else {
    tally(fail('batch/single mismatch calls=' . PcvHelper::$calls . ' batch=' . json_encode($batch)));
}

// --- 14. leak probe ---
$store->clear();
$cache->updateVersion($ver);
$fn = function () use ($cache, $cb, $ver) {
    $cache->processCachedVersion($cb, ['leak'], $ver);
};
$fn(); $fn(); $fn();
gc_collect_cycles();
$m0 = memory_get_usage();
for ($i = 0; $i < 3000; $i++) {
    $fn();
}
gc_collect_cycles();
$delta = memory_get_usage() - $m0;
echo "  leak probe 3000 hits: used delta=$delta bytes\n";
tally($delta < 256000 ? ok("hit-path leak probe delta=$delta") : fail("hit-path leak delta=$delta"));

$store->clear();
$fnMiss = function () use ($cache, $cb, $ver) {
    // 不 incr：每次 miss
    $cache->processCachedVersion($cb, ['leakmiss'], $ver);
};
$fnMiss(); $fnMiss();
gc_collect_cycles();
$m0 = memory_get_usage();
for ($i = 0; $i < 1500; $i++) {
    $fnMiss();
}
gc_collect_cycles();
$delta = memory_get_usage() - $m0;
echo "  leak probe 1500 misses (get=false): used delta=$delta bytes\n";
tally($delta < 2 * 1024 * 1024 ? ok("miss-path leak probe delta=$delta") : fail("miss-path leak delta=$delta"));

// --- 15. nested list like Module::purviewTrees + hit + mutate ---
class PcvNested
{
    public static $n = 0;
    public function purviewTrees()
    {
        self::$n++;
        $list = [];
        for ($i = 1; $i <= 40; $i++) {
            $list[] = [
                'id' => $i,
                'pid' => ($i > 1) ? (int) floor($i / 2) : 0,
                'title' => "mod-$i",
                'type' => ($i % 3 === 0) ? 1 : 0,
                'disabled' => 0,
            ];
        }
        return ['count' => 40, 'list' => $list];
    }
}
$store->clear();
PcvNested::$n = 0;
$ncb = [new PcvNested(), 'purviewTrees'];
$cache->updateVersion(['db.sys_module' => null]);
$a = $cache->processCachedVersion($ncb, [], ['db.sys_module' => null], 3600);
$b = $cache->processCachedVersion($ncb, [], ['db.sys_module' => null], 3600);
if (PcvNested::$n !== 1 || !isset($b['list'][0]['title'])) {
    tally(fail('nested hit failed n=' . PcvNested::$n));
} else {
    $tree = [];
    foreach ($b['list'] as $v) {
        if (($v['pid'] ?? -1) === 0) {
            $tree[] = $v;
        }
    }
    tally(ok('nested purviewTrees-shaped payload hit+foreach n=' . PcvNested::$n . ' roots=' . count($tree)));
}

echo "\n=== result: pass=$pass fail=$fail ===\n";
exit($fail ? 1 : 0);
