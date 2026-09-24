<?php
/**
 * 控制器属性遮蔽 DI 探针（AUDIT 2026-09-23 P3）
 *
 * 前提：已加载 gene 扩展。CLI 即可。
 * 用法：php audit/repro/controller_di_shadow.php
 *
 * 说明属性写入路径：Controller::__set 把值写进请求级 DI，键为「类名_属性名」，
 * __get 先查这个键，再回落组件名。因此 $this->user = ... 会盖住
 * BeforeHook 里 Di::set('user', $sessionUser)。
 *
 * 这不是要改掉的 C 行为。模板数据用 $this->view->assign()，写入 view_vars，
 * 不经过 __set，也不会盖住 DI。本探针只记录属性路径的结果。
 *
 * 观察：
 *   shadowed=1 → 属性写入盖住了 DI（当前实现，保持不变）
 *
 * 无扩展时 exit 2。
 */

if (!extension_loaded('gene')) {
    fwrite(STDERR, "SKIP: gene extension is not loaded\n");
    exit(2);
}

class AuditShadowController extends \Gene\Controller
{
}

\Gene\Di::set('user', ['user_id' => 1, 'from' => 'di']);

$controller = new AuditShadowController();
$before = $controller->user;
$controller->user = ['user_id' => 9, 'from' => 'assign'];
$after = $controller->user;

$beforeId = is_array($before) ? ($before['user_id'] ?? null) : null;
$afterId = is_array($after) ? ($after['user_id'] ?? null) : null;
$shadowed = ($beforeId === 1 && $afterId === 9) ? 1 : 0;

echo 'before_user_id=' . var_export($beforeId, true) . "\n";
echo 'after_user_id=' . var_export($afterId, true) . "\n";
echo 'shadowed=' . $shadowed . "\n";

if ($shadowed === 1) {
    echo "PROPERTY: \$this->user = shadowed DI; template data belongs in View::assign()\n";
    exit(0);
}
if ($beforeId === 1 && $afterId === 1) {
    echo "UNEXPECTED: property write no longer shadows DI\n";
    exit(1);
}
echo "UNEXPECTED\n";
exit(1);
