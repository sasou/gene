<?php
namespace Hooks;

/**
 * ReadonlyGuard — 动态路由 GET 只读守卫参考实现（S12）。
 *
 * 背景：`:c/:a` 动态路由让"控制器.动作"直接暴露在 URL 上，
 * 一个写动作 `User/delAll` 用 GET 就能触发——浏览器预取、爬虫、
 * 甚至 <img src> 都能替用户"点击"它。
 *
 * C 层等价物：`Application::readonly([...])` 已实现同样语义；
 * 这个 Hook 是不需要升级框架版本时的 PHP 参考实现，也便于
 * 理解/调整白名单结构。
 *
 * 使用（router.ini.php）：
 *   $router->hook('ro', 'Hooks\ReadonlyGuard@handle')
 *       ->group('/admin/')->through(['ro'])
 *       ->get('/:c/:a', 'Controllers\Admin\:c@:a');
 *
 * 白名单结构：{ 前缀 => { 控制器 => [动作...] , '*' => [动作...] } }
 * 命中前缀 + 两段路径的 GET 请求，动作不在白名单 → 405。
 */
class ReadonlyGuard
{
    /**
     * 与 Application::readonly() 相同的表结构。
     * 真实项目从配置读，这里写成常量便于 demo。
     */
    private const ALLOW = [
        '/admin' => [
            '*'      => ['lists', 'index', 'run'],     // 所有控制器默认放行这些只读动作
            'module' => ['lists', 'index', 'run', 'tree'],
        ],
    ];

    public function handle()
    {
        if (strtoupper(\Gene\Request::getMethod() ?: 'GET') !== 'GET') {
            return true;
        }
        $path = '/' . ltrim(\Gene\Application::getPath() ?: '/', '/');
        foreach (self::ALLOW as $prefix => $map) {
            if (strncmp($path, $prefix . '/', strlen($prefix) + 1) !== 0) {
                continue;
            }
            $rest = substr($path, strlen($prefix) + 1);
            $seg = explode('/', $rest);
            if (count($seg) < 2) {
                continue;
            }
            [$c, $a] = [$seg[0], strtok($seg[1], '.?') ?: $seg[1]];
            $actions = $map[$c] ?? $map['*'] ?? null;
            if (is_array($actions) && !in_array(strtolower($a), array_map('strtolower', $actions), true)) {
                http_response_code(405);
                return $this->respond(['code' => 4005, 'msg' => 'method not allowed']);
            }
        }
        return true;
    }

    private function respond($payload)
    {
        \Gene\Response::setJsonHeader();
        \Gene\Response::json($payload);
        return false;
    }
}
