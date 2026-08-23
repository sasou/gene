<?php
/* [AUDIT 2026-08-23 P2-1] 子进程：验证 Memory::set 对 object/resource（含任意嵌套
 * 深度、自引用数组）在取写锁之前以 E_WARNING 拒写（旧实现仅在顶层 E_ERROR；嵌套对象
 * 会带着写锁 bailout，永久泄漏写锁）。拒写后进程必须仍然健康：后续 set/get 正常。 */
$warnings = [];
set_error_handler(function ($no, $str) use (&$warnings) {
    $warnings[] = $str;
    return true;
});

$m = new Gene\Memory('uaf-probe');
$m->set('biz:obj', new stdClass());
$m->set('biz:obj-nested', ['data' => ['when' => new DateTimeImmutable()], 'version' => ['u' => 1]]);
$rec = [];
$rec['self'] = &$rec; /* 自引用数组：无法持久化，必须被拒而非无限递归 */
$m->set('biz:rec', $rec);

$refused = count($warnings) === 3
    && $m->get('biz:obj') === null
    && $m->get('biz:obj-nested') === null
    && $m->get('biz:rec') === null;

/* 写锁若被 bailout 泄漏，这里会死锁或崩溃。 */
$m->set('biz:after', 'ok');
$alive = $m->get('biz:after') === 'ok';

echo 'refused=' . ($refused ? 'yes' : 'no') . ' alive=' . ($alive ? 'yes' : 'no') . "\n";
exit($refused && $alive ? 0 : 1);
