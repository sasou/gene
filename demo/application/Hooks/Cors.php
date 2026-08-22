<?php
namespace Hooks;

/**
 * CORS hook — OPTIONS short-circuit; Origin whitelist only (never reflect).
 *
 * Wire as a named hook, or call from BeforeHook:
 *   ->hook('cors', 'Hooks\Cors@handle')
 *
 * Override $allowOrigins in a subclass, or set config key `cors.origins`.
 */
class Cors extends \Gene\Hook
{
    /** @var string[] */
    protected $allowOrigins = [
        'http://localhost',
        'http://127.0.0.1',
    ];

    public function handle()
    {
        $origin = $this->request->header('origin')
            ?? $this->request->header('Origin');
        $allow = $this->allowOrigins;
        $cfg = \Gene\Application::config('cors');
        if (is_array($cfg) && !empty($cfg['origins']) && is_array($cfg['origins'])) {
            $allow = $cfg['origins'];
        }

        $ok = is_string($origin) && $origin !== '' && in_array($origin, $allow, true);
        if ($ok) {
            \Gene\Response::header('Access-Control-Allow-Origin', $origin);
            \Gene\Response::header('Vary', 'Origin');
            \Gene\Response::header('Access-Control-Allow-Methods', 'GET, POST, PUT, PATCH, DELETE, OPTIONS');
            \Gene\Response::header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Request-Id');
            \Gene\Response::header('Access-Control-Max-Age', '86400');
        }

        if ($this->request->isOptions()) {
            \Gene\Response::end('');
            return false;
        }
        return true;
    }
}
