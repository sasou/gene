<?php
/**
 * Lifecycle primitives (Context / Json / Crypto / SSE / Memory lock).
 */

class LifecycleTest
{
    public function __construct()
    {
        echo "=== Gene Lifecycle Primitives Test Suite ===\n\n";
    }

    public function testContext()
    {
        echo "Testing Gene\\Context:\n";
        \Gene\Context::set('request_id', 'abc123');
        \Gene\Context::set('user', ['id' => 1]);
        $id = \Gene\Context::get('request_id');
        $missing = \Gene\Context::get('nope', 'dflt');
        $all = \Gene\Context::all();
        if ($id === 'abc123' && $missing === 'dflt' && isset($all['user']['id'])) {
            echo "✓ set/get/all works\n";
        } else {
            echo "✗ Context round-trip failed\n";
        }
        echo "\n";
    }

    public function testLogMergesRequestId()
    {
        echo "Testing Log merges request_id:\n";
        \Gene\Context::set('request_id', 'rid-log');
        $tmp = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'gene_log_rid.log';
        @unlink($tmp);
        \Gene\Log::setFile($tmp);
        \Gene\Log::info('hello', ['ip' => '1.1.1.1']);
        $line = @file_get_contents($tmp);
        if (is_string($line) && strpos($line, 'rid-log') !== false && strpos($line, '1.1.1.1') !== false) {
            echo "✓ Log context includes request_id\n";
        } else {
            echo "✗ Log did not merge request_id: " . var_export($line, true) . "\n";
        }
        @unlink($tmp);
        echo "\n";
    }

    public function testJson()
    {
        echo "Testing Gene\\Json:\n";
        $s = \Gene\Json::encode(['zh' => '你好', 'p' => '/a/b']);
        if (strpos($s, '你好') !== false && strpos($s, '/a/b') !== false && strpos($s, '\\/') === false) {
            echo "✓ encode UNESCAPED_UNICODE|UNESCAPED_SLASHES\n";
        } else {
            echo "✗ encode flags: $s\n";
        }
        $d = \Gene\Json::decode($s);
        if (is_array($d) && ($d['zh'] ?? '') === '你好') {
            echo "✓ decode object → array\n";
        } else {
            echo "✗ decode failed\n";
        }
        try {
            \Gene\Json::decode('{bad');
            echo "✗ invalid JSON did not throw\n";
        } catch (\Throwable $e) {
            echo "✓ invalid JSON throws\n";
        }
        echo "\n";
    }

    public function testRequestJson()
    {
        echo "Testing Request::json():\n";
        \Gene\Request::init([], [], [], [], null, [], null, [], '');
        $empty = \Gene\Request::json();
        if ($empty === null) {
            echo "✓ empty body → null\n";
        } else {
            echo "✗ empty body expected null\n";
        }
        \Gene\Request::init([], [], [], [], null, [], null, [], '{"ok":true,"n":1}');
        $j = \Gene\Request::json();
        if (is_array($j) && ($j['ok'] ?? false) === true) {
            echo "✓ object body → array\n";
        } else {
            echo "✗ object body failed\n";
        }
        try {
            \Gene\Request::init([], [], [], [], null, [], null, [], '{');
            \Gene\Request::json();
            echo "✗ invalid body did not throw\n";
        } catch (\Throwable $e) {
            echo "✓ invalid body throws\n";
        }
        try {
            \Gene\Request::init([], [], [], [], null, [], null, [], 'null');
            \Gene\Request::json();
            echo "✗ JSON null did not throw\n";
        } catch (\Throwable $e) {
            echo "✓ JSON null (non-empty) throws\n";
        }
        echo "\n";
    }

    public function testSseWrite()
    {
        echo "Testing Response::write / SSE:\n";
        ob_start();
        \Gene\Response::write('chunk-a');
        \Gene\Response::sseStart();
        \Gene\Response::sseEvent('ping', 'hi');
        \Gene\Response::sseEvent('msg', ['k' => 'v']);
        $out = ob_get_clean();
        if (strpos($out, 'chunk-a') !== false
            && strpos($out, "event: ping\n") !== false
            && strpos($out, "data: hi\n") !== false
            && strpos($out, 'event: msg') !== false) {
            echo "✓ write + sseEvent frames\n";
        } else {
            echo "✗ SSE output: " . var_export($out, true) . "\n";
        }
        echo "\n";
    }

    public function testCrypto()
    {
        echo "Testing Gene\\Crypto:\n";
        $enc = \Gene\Crypto::base64UrlEncode("\xfb\xff");
        $dec = \Gene\Crypto::base64UrlDecode($enc);
        if ($dec === "\xfb\xff" && strpos($enc, '+') === false && strpos($enc, '/') === false) {
            echo "✓ base64url round-trip\n";
        } else {
            echo "✗ base64url failed\n";
        }
        $tok = \Gene\Crypto::hmacToken(['uid' => 7, 'purpose' => 'login'], 's3cret', 60);
        $payload = \Gene\Crypto::hmacVerify($tok, 's3cret');
        if (($payload['uid'] ?? 0) === 7 && isset($payload['exp'])) {
            echo "✓ hmacToken / hmacVerify\n";
        } else {
            echo "✗ hmac verify payload\n";
        }
        try {
            \Gene\Crypto::hmacVerify($tok, 'wrong');
            echo "✗ bad secret did not throw\n";
        } catch (\Throwable $e) {
            echo "✓ bad secret throws\n";
        }
        $rid = \Gene\Crypto::randomId('usr_', 8);
        if (strpos($rid, 'usr_') === 0 && strlen($rid) === 4 + 16) {
            echo "✓ randomId prefix+hex\n";
        } else {
            echo "✗ randomId: $rid\n";
        }
        $key = str_repeat('k', 32);
        $c = \Gene\Crypto::encrypt('plain-text', $key);
        $p = \Gene\Crypto::decrypt($c, $key);
        if ($p === 'plain-text') {
            echo "✓ AES-256-GCM round-trip\n";
        } else {
            echo "✗ GCM decrypt mismatch\n";
        }
        echo "\n";
    }

    public function testMemoryRateLimitLock()
    {
        echo "Testing Memory::rateLimit / lock:\n";
        $m = new \Gene\Memory('lifecycle');
        $k = 'rl_' . bin2hex(random_bytes(4));
        $ok1 = $m->rateLimit($k, 2, 30);
        $ok2 = $m->rateLimit($k, 2, 30);
        $ok3 = $m->rateLimit($k, 2, 30);
        if ($ok1 && $ok2 && $ok3 === false) {
            echo "✓ rateLimit allows 2 then false\n";
        } else {
            echo "✗ rateLimit: " . var_export([$ok1, $ok2, $ok3], true) . "\n";
        }
        $lk = 'lock_' . bin2hex(random_bytes(4));
        $t = $m->lock($lk, 10);
        $t2 = $m->lock($lk, 10);
        $unlocked = is_string($t) && $m->unlock($lk, $t);
        $bad = $m->unlock($lk, 'nope');
        if (is_string($t) && $t2 === false && $unlocked && $bad === false) {
            echo "✓ lock NX + compare-and-del unlock\n";
        } else {
            echo "✗ lock/unlock: " . var_export([$t, $t2, $unlocked, $bad], true) . "\n";
        }
        echo "\n";
    }

    public function testRedisRateLimitLock()
    {
        echo "Testing Redis::rateLimit / lock:\n";
        if (!extension_loaded('redis')) {
            echo "SKIP Redis (ext-redis not loaded)\n\n";
            return;
        }
        try {
            $r = new \Gene\Cache\Redis([
                'host' => '127.0.0.1',
                'port' => 6379,
                'timeout' => 0.3,
                'serializer' => 0,
            ]);
            $k = 'gene:rl:' . bin2hex(random_bytes(4));
            $ok1 = $r->rateLimit($k, 2, 30);
            $ok2 = $r->rateLimit($k, 2, 30);
            $ok3 = $r->rateLimit($k, 2, 30);
            if ($ok1 && $ok2 && $ok3 === false) {
                echo "✓ Redis rateLimit allows 2 then false\n";
            } else {
                echo "✗ Redis rateLimit: " . var_export([$ok1, $ok2, $ok3], true) . "\n";
            }
            $lk = 'gene:lock:' . bin2hex(random_bytes(4));
            $t = $r->lock($lk, 10);
            $t2 = $r->lock($lk, 10);
            $unlocked = is_string($t) && $r->unlock($lk, $t);
            if (is_string($t) && $t2 === false && $unlocked) {
                echo "✓ Redis lock NX + Lua unlock\n";
            } else {
                echo "✗ Redis lock/unlock: " . var_export([$t, $t2, $unlocked], true) . "\n";
            }
            $r->free();
        } catch (\Throwable $e) {
            echo "SKIP Redis (" . $e->getMessage() . ")\n";
        }
        echo "\n";
    }

    public function runAllTests()
    {
        $this->testContext();
        $this->testLogMergesRequestId();
        $this->testJson();
        $this->testRequestJson();
        $this->testSseWrite();
        $this->testCrypto();
        $this->testMemoryRateLimitLock();
        $this->testRedisRateLimitLock();
        echo "=== Lifecycle Primitives Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new LifecycleTest();
    $test->runAllTests();
}
