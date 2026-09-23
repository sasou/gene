<?php

/**
 * [GENE_FIX:2026-09-23 S3] Demo login business smoke — spawned by
 * DemoLoadTest with GENE_DEMO_LOCAL=1 (sqlite file + localStore, no
 * external services). Bootstraps the demo exactly like public/cli.php but
 * stops before run(), then calls checkUser('admin', <wrong>) directly.
 *
 * A correct result is the business error array {code:4000, msg:密码错误!} —
 * reaching it proves the whole chain works: config → Di cache/localStore →
 * cachedVersion → Models\Admin\User (ORM model load) → sqlite join →
 * verifyPassword. Any fatal along the way is a non-zero exit / non-JSON.
 */

define('APP_ROOT', dirname(__DIR__) . DIRECTORY_SEPARATOR . 'demo'
    . DIRECTORY_SEPARATOR . 'application');
define('CONF_DIR', dirname(__DIR__) . DIRECTORY_SEPARATOR . 'demo'
    . DIRECTORY_SEPARATOR . 'config');

$app = \Gene\Application::getInstance();
$app->bootstrap(APP_ROOT, CONF_DIR, [
    'router'     => 'router.ini.php',
    'config'     => 'config.ini.php',
    'mode'       => 1,
    'debug_envs' => ['dev', 'test', 'gray'],
]);

$r = \Services\Admin\User::getInstance()->checkUser('admin', 'definitely-wrong-password');
echo json_encode($r, JSON_UNESCAPED_UNICODE), "\n";

$ok = is_array($r) && ($r['code'] ?? 0) === 4000
    && strpos((string)($r['msg'] ?? ''), '密码错误') !== false;
exit($ok ? 0 : 1);
