<?php
/**
 * Gene\Memory + 依赖 gene_memory_zval_local 的调用面
 * （processCached / Config / Di 工厂参数拷贝）
 */

function fail($m)
{
    echo "FAIL $m\n";
    return false;
}
function ok($m)
{
    echo "OK   $m\n";
    return true;
}

$pass = 0;
$fail = 0;
function tally($ok)
{
    global $pass, $fail;
    $ok ? $pass++ : $fail++;
}

echo "gene=" . phpversion('gene') . "\n\n";

$mem = new \Gene\Memory('memtest');

echo "== Memory scalars / strings ==\n";
tally($mem->set('s', 'hello') ? ok('set string') : fail('set string'));
tally($mem->get('s') === 'hello' ? ok('get string') : fail('get string=' . var_export($mem->get('s'), true)));
tally($mem->set('n', 42) ? ok('set long') : fail('set long'));
tally($mem->get('n') === 42 ? ok('get long') : fail('get long'));
tally($mem->set('d', 1.5) ? ok('set double') : fail('set double'));
tally(abs($mem->get('d') - 1.5) < 1e-9 ? ok('get double') : fail('get double'));
tally($mem->set('b', true) ? ok('set true') : fail('set true'));
tally($mem->get('b') === true ? ok('get true') : fail('get true'));
tally($mem->set('z', null) ? ok('set null') : fail('set null'));
tally($mem->get('z') === null ? ok('get null stored') : fail('get null stored'));
tally($mem->get('missing_key_xyz') === null ? ok('get miss => null') : fail('get miss'));

echo "\n== Memory nested array + isolation ==\n";
$src = [
    'count' => 2,
    'list' => [
        ['id' => 1, 'pid' => 0, 'title' => 'root', 'type' => 0],
        ['id' => 2, 'pid' => 1, 'title' => 'child', 'type' => 1],
    ],
    'meta' => ['k' => 'v', '10' => 'numeric-string-key'],
];
tally($mem->set('tree', $src) ? ok('set nested') : fail('set nested'));
$g1 = $mem->get('tree');
tally(($g1['list'][1]['title'] ?? '') === 'child' ? ok('get nested title') : fail('nested title'));
$g1['list'][0]['title'] = 'MUTATED';
$g1['count'] = 999;
$g2 = $mem->get('tree');
tally(($g2['list'][0]['title'] ?? '') === 'root' && $g2['count'] === 2
    ? ok('mutate returned array does not dirty Memory')
    : fail('isolation broken ' . json_encode($g2)));

echo "\n== overwrite / exists / del / incr ==\n";
$mem->set('s', 'world');
tally($mem->get('s') === 'world' ? ok('overwrite string') : fail('overwrite'));
tally($mem->exists('s') ? ok('exists true') : fail('exists'));
tally($mem->del('s') ? ok('del') : fail('del'));
tally(!$mem->exists('s') && $mem->get('s') === null ? ok('after del miss') : fail('after del'));
$mem->set('ctr', 10);
$inc = $mem->incr('ctr', 3);
tally($inc === 13 && $mem->get('ctr') === 13 ? ok('incr') : fail('incr ret=' . var_export($inc, true)));

echo "\n== mset / mget ==\n";
tally($mem->mset(['a' => 'A', 'b' => ['x' => 1], 'c' => 3]) ? ok('mset') : fail('mset'));
$mg = $mem->mget(['a', 'b', 'c', 'nope']);
tally(is_array($mg) && ($mg['a'] ?? null) === 'A' && ($mg['b']['x'] ?? null) === 1
    ? ok('mget values')
    : fail('mget ' . json_encode($mg)));

echo "\n== processCached (same Memory copy path as hit) ==\n";
class MemProcHelper
{
    public static $n = 0;
    public function rows($tag)
    {
        self::$n++;
        $list = [];
        for ($i = 0; $i < 20; $i++) {
            $list[] = ['id' => $i, 'title' => "$tag-$i"];
        }
        return ['tag' => $tag, 'list' => $list];
    }
}
$store = new class {
    public $d = [];
    public function get($k)
    {
        if (is_array($k)) {
            $o = [];
            foreach ($k as $x) {
                if (is_string($x) && array_key_exists($x, $this->d)) {
                    $o[$x] = $this->d[$x];
                }
            }
            return $o ?: false;
        }
        return $this->d[$k] ?? false;
    }
    public function set($k, $v, $t = 0)
    {
        $this->d[$k] = $v;
        return true;
    }
    public function delete($k)
    {
        unset($this->d[$k]);
        return true;
    }
    public function incr($k, $v = 1)
    {
        $this->d[$k] = ($this->d[$k] ?? 0) + $v;
        return $this->d[$k];
    }
};
\Gene\Di::set('memHook', $store);
$cache = new \Gene\Cache\Cache(['hook' => 'memHook', 'sign' => 'pc:', 'versionSign' => 'pv:']);
$h = new MemProcHelper();
$cb = [$h, 'rows'];
MemProcHelper::$n = 0;
$r1 = $cache->processCached($cb, ['p1']);
$r2 = $cache->processCached($cb, ['p1']);
tally(MemProcHelper::$n === 1 && $r2['list'][5]['title'] === 'p1-5'
    ? ok('processCached miss then hit')
    : fail('processCached n=' . MemProcHelper::$n));
$r2['list'][5]['title'] = 'X';
$r3 = $cache->processCached($cb, ['p1']);
tally($r3['list'][5]['title'] === 'p1-5' ? ok('processCached hit isolation') : fail('processCached isolation'));
$cache->unsetProcessCached($cb, ['p1']);
$cache->processCached($cb, ['p1']);
tally(MemProcHelper::$n === 2 ? ok('unsetProcessCached then recompute') : fail('unset n=' . MemProcHelper::$n));

echo "\n== cachedVersion (hook, not Memory) ==\n";
MemProcHelper::$n = 0;
$store->d = [];
$cache->updateVersion(['db.t' => null]);
$c1 = $cache->cachedVersion($cb, ['cv'], ['db.t' => null], 60);
$c2 = $cache->cachedVersion($cb, ['cv'], ['db.t' => null], 60);
tally(MemProcHelper::$n === 1 && $c1 === $c2 ? ok('cachedVersion miss then hit') : fail('cachedVersion n=' . MemProcHelper::$n));
$cache->updateVersion(['db.t' => null]);
$c3 = $cache->cachedVersion($cb, ['cv'], ['db.t' => null], 60);
tally(MemProcHelper::$n === 2 ? ok('cachedVersion invalidate') : fail('cachedVersion inv n=' . MemProcHelper::$n));

echo "\n== Config (Memory-backed) ==\n";
try {
    $conf = new \Gene\Config('cfgt');
    $conf->set('db', ['host' => '127.0.0.1', 'port' => 3306]);
    $got = $conf->get('db');
    tally(($got['host'] ?? '') === '127.0.0.1' && ($got['port'] ?? 0) === 3306
        ? ok('Config set/get array')
        : fail('Config get ' . json_encode($got)));
    $got['host'] = 'evil';
    $got2 = $conf->get('db');
    tally($got2['host'] === '127.0.0.1' ? ok('Config get isolation') : fail('Config isolation'));
} catch (Throwable $e) {
    tally(fail('Config: ' . $e->getMessage()));
}

echo "\n== Di factory params copy (gene_memory_zval_local) ==\n";
\Gene\Di::set('di_arr', ['k' => 'v', 'n' => [1, 2, 3]]);
$da = \Gene\Di::get('di_arr');
tally(($da['k'] ?? '') === 'v' && $da['n'][2] === 3 ? ok('Di array get') : fail('Di get'));
$da['k'] = 'changed';
$da2 = \Gene\Di::get('di_arr');
tally($da2['k'] === 'v' ? ok('Di array isolation') : fail('Di isolation k=' . ($da2['k'] ?? '')));

echo "\n== leak probe Memory get ==\n";
$mem->set('leak', ['list' => array_fill(0, 10, ['t' => 'x'])]);
$fn = function () use ($mem) {
    $x = $mem->get('leak');
    $x['list'][0]['t'] = 'y';
};
$fn();
$fn();
gc_collect_cycles();
$m0 = memory_get_usage();
for ($i = 0; $i < 2000; $i++) {
    $fn();
}
gc_collect_cycles();
$delta = memory_get_usage() - $m0;
echo "  2000 Memory::get+mutate delta=$delta\n";
tally($delta < 512000 ? ok("Memory get leak probe delta=$delta") : fail("leak $delta"));

echo "\n=== result pass=$pass fail=$fail ===\n";
exit($fail ? 1 : 0);
