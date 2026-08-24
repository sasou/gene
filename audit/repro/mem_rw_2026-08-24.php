<?php
/**
 * Repro for MEM-RW fix: Gene\Memory writes must keep working after
 * Application::workerReady() in "Swoole mode" (gene.runtime_type=2).
 * Run with: php -n -d extension_dir=... -d extension=php_gene.dll
 *   -d gene.runtime_type=2 mem_rw_2026-08-24.php
 */
$app = \Gene\Application::getInstance();
$app->workerReady();

$m = new \Gene\Memory('repro');

$k = 'rl_' . bin2hex(random_bytes(4));
$ok1 = $m->rateLimit($k, 2, 30);
$ok2 = $m->rateLimit($k, 2, 30);
$ok3 = $m->rateLimit($k, 2, 30);
echo "rateLimit after freeze: " . var_export([$ok1, $ok2, $ok3], true) . "\n";
if ($ok1 === true && $ok2 === true && $ok3 === false) {
    echo "PASS rateLimit works post-freeze\n";
} else {
    echo "FAIL rateLimit post-freeze\n";
}

$sk = 'set_' . bin2hex(random_bytes(4));
$m->set($sk, 'value-after-freeze');
$got = $m->get($sk);
echo "set/get after freeze: " . var_export($got, true) . "\n";
echo ($got === 'value-after-freeze' ? "PASS" : "FAIL") . " set/get works post-freeze\n";

$dk = 'del_' . bin2hex(random_bytes(4));
$m->set($dk, 'x');
$delOk = $m->del($dk);
$afterDel = $m->get($dk);
echo "del after freeze: " . var_export([$delOk, $afterDel], true) . "\n";
echo ($delOk === true && $afterDel === null ? "PASS" : "FAIL") . " del works post-freeze\n";

$lk = 'lock_' . bin2hex(random_bytes(4));
$t = $m->lock($lk, 10);
$t2 = $m->lock($lk, 10);
$unlocked = is_string($t) && $m->unlock($lk, $t);
echo "lock/unlock after freeze: " . var_export([$t !== false, $t2, $unlocked], true) . "\n";
echo (is_string($t) && $t2 === false && $unlocked === true ? "PASS" : "FAIL") . " lock/unlock works post-freeze\n";

$ik = 'incr_' . bin2hex(random_bytes(4));
$i1 = $m->incr($ik);
$i2 = $m->incr($ik, 5);
echo "incr after freeze: " . var_export([$i1, $i2], true) . "\n";
echo ($i1 === 1 && $i2 === 6 ? "PASS" : "FAIL") . " incr works post-freeze\n";

// clean() must still be refused post-freeze in Swoole mode.
$cleanResult = $m->clean();
echo "clean after freeze: " . var_export($cleanResult, true) . "\n";
echo ($cleanResult === false ? "PASS" : "FAIL") . " clean still refused post-freeze\n";
