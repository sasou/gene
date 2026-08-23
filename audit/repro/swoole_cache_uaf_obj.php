<?php
/* [AUDIT 2026-08-23 UAF-4] 子进程：验证 Memory::set 对 IS_OBJECT 直接 E_ERROR 拒绝，
 * 且拒绝发生在取写锁之前（进程可正常退出，不死锁）。 */
$m = new Gene\Memory('uaf-probe');
$m->set('biz:obj', new stdClass());
echo "BAD: object accepted\n";
