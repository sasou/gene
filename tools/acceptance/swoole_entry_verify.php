<?php
/**
 * swoole_entry_verify.php — Linux + Swoole 环境对三种请求入口的验收脚本：
 *
 *   --entry=manual|init|handle  onRequest 入口路径（默认 handle）：
 *       manual = waitWorkerReady + 九参 Request::init + setResponse + ob + run
 *                + catch(500+缓冲输出) + isWritable + end + cleanup（旧手写样板）
 *       init   = 同上，但请求袋由 Request::initSwoole 提取
 *       handle = Application::handleSwoole($request, $response) 一行收口
 *   --soak=N                    追加 N 次 /echo 请求 soak，随后断言
 *                               co_contexts_items=0（worker_num=1 时准确）
 *   --bench                     对 /empty /echo /json /di 各打一波微基准，
 *                               输出 req/s、p50/p99、peak RSS、co_contexts_items
 *   --host=H --port=N --workers=N
 *
 * 设计要点：
 *   1. 三种入口对同一组路由产生**完全一致的响应体与状态码**（manual/init
 *      复刻 handleSwoole 的可观测语义：异常→500+已缓冲输出），因此
 *      RESULT-DIGEST 可跨入口方式直接比较 —— digest 一致即语义等价。
 *   2. 脚本自带服务端 + 协程客户端，无外部依赖；workerStart 里走
 *      pools()/startPools()（env 提供 DSN 时真实建池）、workerExit/
 *      workerStop 里走 stopPoolTimers()/closePools()，顺带验证编排 API
 *      在真实 Swoole 生命周期下的 no-op 安全与真实路径。
 *   3. 闭环判据：输出 ALL-PASS + RESULT-DIGEST；soak 后 co_contexts_items=0。
 *
 * 用法（Linux + swoole + gene）：
 *   php tools/acceptance/swoole_entry_verify.php --entry=handle
 *   php tools/acceptance/swoole_entry_verify.php --entry=handle --soak=100000
 *   php tools/acceptance/swoole_entry_verify.php --entry=manual --bench
 */

namespace App {
    /* 直派 MCA 控制器（方法名避开 Controller 的静态同名方法）；
     * actGuarded 用于证明 hook abort/respond 后控制器不执行。 */
    class Ctl extends \Gene\Controller
    {
        public function actEmpty()    { /* 空控制器：一行都不输出 */ }
        public function actEcho()     { echo "R:echo"; }
        public function actJson()     { \Gene\Response::json(['ok' => true]); }
        public function actBoom()     { echo "partial-"; throw new \RuntimeException('entry-boom'); }
        public function actFile()     { \Gene\Response::sendFile(ENTRY_VERIFY_FILE, 0, 64); }
        public function actGuarded()  { echo "R:GUARDED-RAN"; }
        public function actRedir()    { \Gene\Response::redirect('/to'); }
        public function actWrite()    { \Gene\Response::write('chunk1'); echo 'tail'; }
        public function actDi()       { echo \Gene\Di::get('entry_verify_svc'); }
    }
}

namespace {

    $options = getopt('', ['entry:', 'soak::', 'bench', 'host:', 'port:', 'workers:']);
    $ENTRY   = $options['entry'] ?? 'handle';
    if (!in_array($ENTRY, ['manual', 'init', 'handle'], true)) {
        fwrite(STDERR, "[FATAL] --entry 仅接受 manual|init|handle。\n");
        exit(64);
    }
    $SOAK    = isset($options['soak']) ? max(0, (int) $options['soak']) : 0;
    $BENCH   = isset($options['bench']);
    define('VHOST', $options['host'] ?? (getenv('GENE_VERIFY_HOST') ?: '127.0.0.1'));
    define('VPORT', max(1, (int) ($options['port'] ?? (getenv('GENE_VERIFY_PORT') ?: 9538))));
    define('VWORKERS', max(1, (int) ($options['workers'] ?? 1)));
    define('ENTRY_VERIFY_FILE', __FILE__);

    if (!extension_loaded('swoole')) {
        fwrite(STDERR, "[FATAL] swoole 扩展未加载，本脚本必须在 Linux + Swoole 环境运行。\n");
        exit(2);
    }
    if (!extension_loaded('gene')) {
        fwrite(STDERR, "[FATAL] gene 扩展未加载。\n");
        exit(2);
    }

    /** 协程 HTTP 客户端：返回 [body, statusCode]。 */
    function http_get(string $path): array
    {
        $cli = new \Swoole\Coroutine\Http\Client(VHOST, VPORT);
        $cli->set(['timeout' => 10]);
        $cli->get($path);
        $out = [(string) $cli->body, (int) $cli->statusCode];
        $cli->close();
        return $out;
    }

    function run_self_test($server, string $entry, int $soak, bool $bench): void
    {
        $failures = 0; /* also mirrored into $GLOBALS['entry_verify_failures'] */
        $idx = 0;
        $check = function (string $label, bool $cond, string $detail = '') use (&$failures, &$idx) {
            $idx++;
            printf("  [%s] %2d. %s%s\n", $cond ? 'PASS' : 'FAIL', $idx, $label, $detail !== '' ? "  ($detail)" : '');
            if (!$cond) $failures++;
        };

        echo "[A] 三种入口响应等价性（entry={$entry}）\n";
        $expectFile = substr((string) file_get_contents(ENTRY_VERIFY_FILE), 0, 64);
        $cases = [
            // path => [expectedBody, expectedStatus]
            '/echo'    => ['R:echo', 200],
            '/empty'   => ['', 200],
            '/json'    => ['{"ok":true}', 200],
            '/boom'    => ['partial-', 500],
            '/file'    => [$expectFile, 200],
            '/denied'  => ['{"error":"denied"}', 401],
            '/vetoed'  => ['', 200],
            '/redir'   => ['', 302],
            '/write'   => ['chunk1tail', 200],
            '/di'      => ['R:di', 200],
        ];
        $digestParts = [];
        foreach ($cases as $path => [$expect, $expectStatus]) {
            [$got, $status] = http_get($path);
            $check("GET {$path} => body+status", $got === $expect && $status === $expectStatus,
                "status={$status} got='" . substr($got, 0, 60) . "'");
            $digestParts[] = $path . '=' . $status . ':' . $got;
        }
        [$got404, $status404] = http_get('/no/such/route');
        $check('GET /no/such/route 被优雅处理（非空确定性响应）', $got404 !== '', "status={$status404}");
        $digestParts[] = '/no/such/route=' . $status404 . ':' . $got404;

        echo "\n[B] 协程上下文隔离（并发 /cid/N 各回各路径）\n";
        $N = 100;
        $results = [];
        $wg = new \Swoole\Coroutine\WaitGroup();
        for ($i = 0; $i < $N; $i++) {
            $wg->add();
            go(function () use ($i, &$results, $wg) {
                [$body] = http_get("/cid/{$i}");
                $results[$i] = $body;
                $wg->done();
            });
        }
        $wg->wait();
        $isoOk = true;
        $badSample = '';
        for ($i = 0; $i < $N; $i++) {
            if (($results[$i] ?? null) !== "/cid/{$i}") {
                $isoOk = false;
                $badSample = "i={$i} got='" . ($results[$i] ?? 'NULL') . "'";
                break;
            }
        }
        $check("{$N} 并发协程上下文零串扰", $isoOk, $badSample);

        if ($soak > 0) {
            echo "\n[C] handleSwoole 请求级 soak（{$soak} 次 /echo）\n";
            $conc = 200;
            $remaining = $soak;
            while ($remaining > 0) {
                $batch = min($conc, $remaining);
                $wg = new \Swoole\Coroutine\WaitGroup();
                for ($i = 0; $i < $batch; $i++) {
                    $wg->add();
                    go(function () use ($wg) {
                        http_get('/echo');
                        $wg->done();
                    });
                }
                $wg->wait();
                $remaining -= $batch;
            }
            /* 本协程自身可能占一个 ctx —— 先自清理再读 stats。worker_num=1
             * 时全部请求与本协程同属一个 worker，计数准确。 */
            \Gene\Application::cleanup();
            $stats = (new \Gene\Memory())->stats();
            $check('soak 后 co_contexts_items=0', ($stats['co_contexts_items'] ?? -1) === 0,
                'co_contexts_items=' . var_export($stats['co_contexts_items'] ?? null, true));
            $check('ctx_pool 未超限', ($stats['ctx_pool_size'] ?? -1) <= ($stats['ctx_pool_max'] ?? -1),
                'ctx_pool_size=' . var_export($stats['ctx_pool_size'] ?? null, true));
            $digestParts[] = 'soak=' . ($stats['co_contexts_items'] ?? 'x');
        }

        if ($bench) {
            echo "\n[D] 微基准（entry={$entry}；空控制器/echo/JSON/DI 四类路径）\n";
            $benchOne = function (string $path, int $conc, int $total): array {
                $lat = [];
                $t0 = microtime(true);
                $remaining = $total;
                while ($remaining > 0) {
                    $batch = min($conc, $remaining);
                    $wg = new \Swoole\Coroutine\WaitGroup();
                    for ($i = 0; $i < $batch; $i++) {
                        $wg->add();
                        go(function () use ($path, $wg, &$lat) {
                            $s = microtime(true);
                            http_get($path);
                            $lat[] = (microtime(true) - $s) * 1000.0;
                            $wg->done();
                        });
                    }
                    $wg->wait();
                    $remaining -= $batch;
                }
                $dt = microtime(true) - $t0;
                sort($lat);
                $n = count($lat);
                return [
                    'rps' => $dt > 0 ? $n / $dt : 0.0,
                    'p50' => $n ? $lat[(int) ($n * 0.50)] : 0.0,
                    'p99' => $n ? $lat[(int) ($n * 0.99)] : 0.0,
                    'n'   => $n,
                ];
            };
            $benchTotal = max(500, min(20000, (int) (getenv('ENTRY_BENCH_TOTAL') ?: 5000)));
            foreach (['/empty', '/echo', '/json', '/di'] as $path) {
                $r = $benchOne($path, 50, $benchTotal);
                printf("  BENCH path=%-6s n=%d rps=%.0f p50=%.2fms p99=%.2fms\n",
                    $path, $r['n'], $r['rps'], $r['p50'], $r['p99']);
            }
            \Gene\Application::cleanup();
            $stats = (new \Gene\Memory())->stats();
            printf("  BENCH-META entry=%s peak_mem=%d co_ctx=%d watermark=%d\n",
                $entry, memory_get_peak_usage(true),
                $stats['co_contexts_items'] ?? -1, $stats['co_contexts_watermark'] ?? -1);
        }

        sort($digestParts);
        $digest = substr(hash('sha256', implode('|', $digestParts)), 0, 16);
        echo "\nRESULT-DIGEST={$digest}\n";
        echo "RUN-CONFIG entry={$entry} capi=" . ini_get('gene.swoole_getcid_capi')
            . " precompile=" . ini_get('gene.route_precompile') . "\n";
        echo ($failures === 0 ? "ALL-PASS \xE2\x9C\x85\n" : "{$failures} FAILED \xE2\x9D\x8C\n");
        $GLOBALS['entry_verify_failures'] = $failures;

        /* exit() 在协程/Timer 回调中不可靠 —— 关停 server 后在主流程统一退出。 */
        $server->shutdown();
    }

    \Gene\Application::setRuntimeType('swoole');
    \Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL);

    echo "=== gene Swoole 入口验证（manual/init/handle 三模式 + soak + bench）===\n";
    echo "扩展版本: " . phpversion('gene') . "  swoole: " . phpversion('swoole') . "\n";
    echo "entry={$ENTRY} soak={$SOAK} bench=" . ($BENCH ? 'on' : 'off')
        . " workers=" . VWORKERS . "\n\n";

    $server = new \Swoole\Http\Server(VHOST, VPORT, SWOOLE_PROCESS);
    $server->set([
        'reactor_num' => 1,
        'worker_num'  => VWORKERS,
        'log_level'   => SWOOLE_LOG_ERROR,
    ]);

    $app = \Gene\Application::getInstance();

    $server->on('workerStart', function ($server, $workerId) use ($app, $ENTRY, $SOAK, $BENCH) {
        $router = new \Gene\Router();
        $router->clear()
            ->get('/empty',   '\\App\\Ctl@actEmpty')
            ->get('/echo',    '\\App\\Ctl@actEcho')
            ->get('/json',    '\\App\\Ctl@actJson')
            ->get('/boom',    '\\App\\Ctl@actBoom')
            ->get('/file',    '\\App\\Ctl@actFile')
            ->get('/denied',  '\\App\\Ctl@actGuarded', 'deny')
            ->get('/vetoed',  '\\App\\Ctl@actGuarded', 'veto')
            ->get('/redir',   '\\App\\Ctl@actRedir')
            ->get('/write',   '\\App\\Ctl@actWrite')
            ->get('/di',      '\\App\\Ctl@actDi')
            ->get('/cid/:id', function () { echo \Gene\Application::getPath(); })
            ->error(404, function () { echo 'R:404'; })
            ->hook('deny', function () { \Gene\Hook::respond(['error' => 'denied'], 401); })
            ->hook('veto', function () { \Gene\Hook::abort(); });

        $app->setMode(1, 0);

        /* 编排 API 走线：env 提供 DSN/Redis 时真实建池，否则 pools([]) no-op。
         * workerExit/workerStop 的 stopPoolTimers/closePools 与之配套。 */
        $decls = [];
        if (getenv('GENE_MYSQL_DSN') && getenv('GENE_MYSQL_USER')) {
            (new \Gene\Config())->set('entry_verify_db', [
                'class'  => \Gene\Db\Mysql::class,
                'params' => [[getenv('GENE_MYSQL_DSN'), getenv('GENE_MYSQL_USER'), getenv('GENE_MYSQL_PASS') ?: '']],
            ]);
            $decls['dbPool'] = ['driver' => 'db', 'component' => 'entry_verify_db'];
        }
        if (getenv('GENE_REDIS_HOST')) {
            (new \Gene\Config())->set('entry_verify_redis', [
                'class'  => \Gene\Cache\Redis::class,
                'params' => [[
                    'host'    => getenv('GENE_REDIS_HOST'),
                    'port'    => (int) (getenv('GENE_REDIS_PORT') ?: 6379),
                    'timeout' => (float) (getenv('GENE_REDIS_TIMEOUT') ?: 3),
                ] + (getenv('GENE_REDIS_PASS') ? ['password' => getenv('GENE_REDIS_PASS')] : [])],
            ]);
            $decls['redisPool'] = ['driver' => 'redis', 'component' => 'entry_verify_redis'];
        }
        $app->pools($decls);
        $app->startPools();
        $app->workerReady();

        if ($workerId === 0) {
            \Swoole\Timer::after(600, function () use ($server, $ENTRY, $SOAK, $BENCH) {
                run_self_test($server, $ENTRY, $SOAK, $BENCH);
            });
        }
    });

    /* 编排 API 在真实 Swoole 生命周期下的走线（无池时安全空转）。 */
    $server->on('workerExit', function () use ($app) { $app->stopPoolTimers(); });
    $server->on('workerStop', function () use ($app) { $app->closePools(); });

    $server->on('request', function ($request, $response) use ($app, $ENTRY) {
        /* Di 注册表是请求/协程级（ctx->di_regs）：必须在请求协程内注册，
         * workerStart 里的 set 写在另一个协程的 ctx 中，对请求不可见。 */
        \Gene\Di::set('entry_verify_svc', 'R:di');
        if ($ENTRY === 'handle') {
            $app->handleSwoole($request, $response);
            return;
        }
        /* manual / init 入口：复刻 handleSwoole 的可观测语义——异常→500+
         * 已缓冲输出、未结束则 end(输出)、异常与正常路径都 cleanup。 */
        \Gene\Application::waitWorkerReady();
        if ($ENTRY === 'init') {
            \Gene\Request::initSwoole($request);
        } else {
            \Gene\Request::init($request->get, $request->post, $request->cookie,
                $request->server, null, $request->files, null, $request->header, $request->rawContent());
        }
        \Gene\Application::setResponse($response);
        ob_start();
        $failed = false;
        try {
            $app->run();
        } catch (\Throwable $e) {
            $failed = true;
        }
        $out = ob_get_clean();
        \Gene\Application::cleanup();
        if (!$response->isWritable()) {
            return;
        }
        if ($failed) {
            $response->status(500);
        }
        $response->end($out);
    });

    echo "Swoole server starting on " . VHOST . ":" . VPORT . " (entry={$ENTRY}) ... (自测后自动退出)\n\n";
    $server->start();
    echo "server stopped.\n";
    exit(($GLOBALS['entry_verify_failures'] ?? 0) > 0 ? 1 : 0);
}
