<?php

/**
 * Security baseline regression test (S1-S10, S12).
 *
 * Covers: trusted-proxy IP resolution + CIDR, isSecure() XFP gating,
 * JSONP callback validation, Password new/legacy verify + rehash,
 * Throttle atomic counters, keyring seal/unseal + derive + signRequest,
 * TOTP + randomCode, Csrf token/check, cspNonce, readonly/secureHeaders
 * config registration.
 *
 * CLI notes: request bags go through Request::init() (same path as FPM);
 * seal/unseal sections are skipped when openssl is unavailable in the
 * running build (the throw is asserted instead).
 */

use Gene\Application;
use Gene\Crypto;
use Gene\Csrf;
use Gene\Di;
use Gene\Ip;
use Gene\Memory;
use Gene\Password;
use Gene\Request;
use Gene\Response;
use Gene\Throttle;

echo "=== Security Baseline Test ===\n\n";

$fail = 0;
function check($name, $cond) {
    global $fail;
    if ($cond) {
        echo "✓ $name\n";
    } else {
        $fail++;
        echo "✗ $name\n";
    }
}

/* ---------- S1: Gene\Ip ---------- */
check("Ip::isValid v4", Ip::isValid('203.0.113.9') === true);
check("Ip::isValid v6", Ip::isValid('2001:db8::1') === true);
check("Ip::isValid embedded-v4", Ip::isValid('::ffff:10.0.0.1') === true);
check("Ip::isValid rejects 999", Ip::isValid('999.1.1.1') === false);
check("Ip::inCidr v4 hit", Ip::inCidr('192.168.1.5', '192.168.0.0/16') === true);
check("Ip::inCidr v4 miss", Ip::inCidr('192.169.1.5', '192.168.0.0/16') === false);
check("Ip::inCidr v6 hit", Ip::inCidr('2001:db8::99', '2001:db8::/32') === true);
check("Ip::inCidr v6 miss", Ip::inCidr('2001:db9::99', '2001:db8::/32') === false);
check("Ip::inCidr list form", Ip::inCidr('10.2.3.4', ['192.168.0.0/16', '10.0.0.0/8']) === true);
check("Ip::isInternal 10/8", Ip::isInternal('10.1.2.3') === true);
check("Ip::isInternal 172.16/12", Ip::isInternal('172.16.5.5') === true);
check("Ip::isInternal 192.168/16", Ip::isInternal('192.168.1.1') === true);
check("Ip::isInternal ULA fd00::/7", Ip::isInternal('fd00::1') === true);
check("Ip::isInternal public", Ip::isInternal('8.8.8.8') === false);

/* strict parser: malformed forms must be rejected, not silently parsed */
check("Ip::isValid dup ::", Ip::isValid('2001:db8::1::2') === false);
check("Ip::isValid trailing garbage", Ip::isValid('2001:db8::1x') === false);
check("Ip::isValid bad embedded v4", Ip::isValid('::ffff:999.0.0.1') === false);
check("Ip::isValid 5 octets", Ip::isValid('1.2.3.4.5') === false);
check("Ip::isValid v4-mapped form", Ip::isValid('::ffff:192.168.0.1') === true);
check("Ip::inCidr v4 bad prefix", @Ip::inCidr('10.0.0.1', '10.0.0.0/33') === false);
check("Ip::inCidr v6 bad prefix", @Ip::inCidr('2001:db8::1', '2001:db8::/129') === false);
check("Ip::inCidr junk cidr", @Ip::inCidr('10.0.0.1', '10.0.0.0/x') === false);
check("Ip::inCidr v4-mapped vs v4 cidr", Ip::inCidr('::ffff:10.1.2.3', '10.0.0.0/8') === true);

/* ---------- S1: Request ip methods ---------- */
$server = [
    'REQUEST_METHOD' => 'GET',
    'REQUEST_URI' => '/',
    'REMOTE_ADDR' => '172.18.0.1',
    'HTTP_X_FORWARDED_FOR' => '203.0.113.7, 172.18.0.1',
];
Request::init(null, null, null, $server);
check("Request::directIp peer", Request::directIp() === '172.18.0.1');
check("Request::ip ignores untrusted XFF", Request::ip() === '172.18.0.1');

Application::getInstance()->trustProxies(['proxies' => '172.18.0.0/16', 'proto' => true]);
/* idempotent: a second call with the same table must not break or double-compile */
Application::getInstance()->trustProxies(['proxies' => '172.18.0.0/16', 'proto' => true]);
check("Request::ip rightmost-untrusted", Request::ip() === '203.0.113.7');
check("Request::ips chain", is_array(Request::ips()) && count(Request::ips()) >= 1);

$server['HTTP_X_FORWARDED_FOR'] = '10.0.0.99, 203.0.113.7, 172.18.0.1';
Request::init(null, null, null, $server);
check("Request::ip skips spoofed left entries", Request::ip() === '203.0.113.7');
check("Request::isInternal peer", Request::isInternal() === true);
check("Request::isInternal arg", Request::isInternal('8.8.8.8') === false);

/* isSecure: XFP believed only from a trusted peer */
$server['HTTP_X_FORWARDED_PROTO'] = 'https';
$server['REMOTE_ADDR'] = '8.8.8.8';
Request::init(null, null, null, $server);
check("isSecure: XFP from untrusted peer ignored", Request::isSecure() === false);
$server['REMOTE_ADDR'] = '172.18.0.1';
Request::init(null, null, null, $server);
check("isSecure: XFP from trusted peer honored", Request::isSecure() === true);
$server['HTTPS'] = 'on';
Request::init(null, null, null, $server);
check("isSecure: real HTTPS", Request::isSecure() === true);
unset($server['HTTPS'], $server['HTTP_X_FORWARDED_PROTO']);

/* Swoole-style explicit header bag: XFP arrives via `header` (lowercase),
 * not the server bag — must still be honored for a trusted peer */
$server2 = ['REQUEST_METHOD' => 'GET', 'REQUEST_URI' => '/', 'REMOTE_ADDR' => '172.18.0.1'];
Request::init(null, null, null, $server2, null, null, null, ['x-forwarded-proto' => 'https']);
check("isSecure: XFP via header bag (trusted)", Request::isSecure() === true);
$server2['REMOTE_ADDR'] = '8.8.8.8';
Request::init(null, null, null, $server2, null, null, null, ['x-forwarded-proto' => 'https']);
check("isSecure: XFP via header bag (untrusted)", Request::isSecure() === false);

/* ---------- S4: JSONP callback ---------- */
ob_start();
Response::json(['a' => 1], 'app.cb_1', 0);
$out = ob_get_clean();
check("jsonp valid callback wraps", strpos($out, 'app.cb_1(') !== false);
ob_start();
Response::json(['a' => 1], 'alert(1)//', 0);
$out2 = ob_get_clean();
check("jsonp invalid callback dropped", strpos($out2, 'alert(') === false && strpos($out2, '"a"') !== false);
ob_start();
Response::json(['a' => 1], 'x..y', 0);
$out3 = ob_get_clean();
check("jsonp double-dot dropped", strpos($out3, 'x..y') === false);

/* ---------- S6: Gene\Password ---------- */
$pw = 'S3cret!pass';
$h = Password::hash($pw);
check("Password::hash format", is_string($h) && strlen($h) > 20 && $h[0] === '$');
$vr = Password::verify($pw, $h);
check("Password::verify ok", is_array($vr) && $vr['ok'] === true && $vr['needs_rehash'] === false);
$vr2 = Password::verify('wrong', $h);
check("Password::verify bad", is_array($vr2) && $vr2['ok'] === false);
check("Password::dummy false", Password::dummy($pw) === false);

$salt = 'ab12';
$legacy = md5($salt . sha1($salt . $pw));
check("Password::verifyLegacy hit", Password::verifyLegacy($pw, $legacy, $salt) === true);
check("Password::verifyLegacy miss", Password::verifyLegacy('x', $legacy, $salt) === false);
$va = Password::verifyAny($pw, $h);
check("verifyAny modern", $va['ok'] === true && $va['algo'] === 'bcrypt' && $va['needs_rehash'] === false);
$va2 = Password::verifyAny($pw, $legacy, ['salt' => $salt]);
check("verifyAny legacy + rehash flag", $va2['ok'] === true && $va2['algo'] === 'legacy' && $va2['needs_rehash'] === true);
check("needsRehash modern", Password::needsRehash($h) === false);

/* ---------- S9: keyring / seal / derive / signRequest ---------- */
check("Crypto::keyring", Crypto::keyring([
    'k1' => str_repeat('a', 32),
    'k2' => str_repeat('b', 32),
], 'k2') === true);

if (function_exists('openssl_encrypt')) {
    $sealed = Crypto::seal('hello-sec', 'cookie.sess');
    check("Crypto::seal wire form", is_string($sealed) && substr($sealed, 0, 3) === 'g1.');
    check("Crypto::unseal round-trip", Crypto::unseal($sealed, 'cookie.sess') === 'hello-sec');
    check("Crypto::unseal wrong ctx", Crypto::unseal($sealed, 'cookie.other') === null);
    check("Crypto::unseal tamper", Crypto::unseal(substr($sealed, 0, -4) . 'AAAA', 'cookie.sess') === null);
    /* rotation: re-register with k2 removed — old ciphertext still decodes via kid */
    Crypto::keyring(['k2' => str_repeat('b', 32)]);
    check("Crypto::unseal after rotate", Crypto::unseal($sealed, 'cookie.sess') === 'hello-sec');
    Crypto::keyring(['k1' => str_repeat('a', 32), 'k2' => str_repeat('b', 32)], 'k2');

    /* sealed cookie: "<exp>\n<value>" embedded expiry — a cookie the browser
     * still sends past its lifetime must be rejected server-side */
    $ck = [
        'myc' => Crypto::seal("0\nsess-val", 'cookie.myc'),
        'fut' => Crypto::seal((time() + 60) . "\nfresh", 'cookie.fut'),
        'old' => Crypto::seal((time() - 10) . "\nstale", 'cookie.old'),
        'bad' => Crypto::seal('no-expiry-header', 'cookie.bad'),
    ];
    Request::init(null, null, $ck, $server);
    check("sealedCookie plain value", Request::sealedCookie('myc') === 'sess-val');
    check("sealedCookie future expiry", Request::sealedCookie('fut') === 'fresh');
    check("sealedCookie expired rejected", Request::sealedCookie('old') === null);
    check("sealedCookie malformed rejected", Request::sealedCookie('bad') === null);
    check("sealedCookie absent", Request::sealedCookie('none') === null);
} else {
    try {
        Crypto::seal('x');
        check("Crypto::seal no-openssl should throw", false);
    } catch (\Exception $e) {
        check("Crypto::seal no-openssl throws", true);
    }
}
check("Crypto::derive 32B", strlen(Crypto::derive('rest')) === 32);
$sig = Crypto::signRequest('post', '/rest/X/y', [
    'secret' => str_repeat('s', 32), 'body' => 'b', 'ts' => 1700000000, 'nonce' => 'n1',
]);
check("signRequest shape", isset($sig['ts'], $sig['nonce'], $sig['sign']) && strlen($sig['sign']) === 43);
$sig2 = Crypto::signRequest('post', '/rest/X/y', [
    'secret' => str_repeat('s', 32), 'body' => 'b', 'ts' => 1700000000, 'nonce' => 'n1',
]);
check("signRequest deterministic", $sig['sign'] === $sig2['sign']);
$sig3 = Crypto::signRequest('post', '/rest/X/y', [
    'secret' => str_repeat('s', 32), 'body' => 'other', 'ts' => 1700000000, 'nonce' => 'n1',
]);
check("signRequest body affects sign", $sig['sign'] !== $sig3['sign']);

/* ---------- S10: TOTP + randomCode ---------- */
$sec = Crypto::totpSecret();
check("totpSecret base32", preg_match('/^[A-Z2-7]+$/', $sec) === 1);
$code = Crypto::totp($sec, 1700000000);
check("totp 6-digit", preg_match('/^\d{6}$/', $code) === 1);
check("totp deterministic", Crypto::totp($sec, 1700000000) === $code);
check("totpVerify now", Crypto::totpVerify($sec, Crypto::totp($sec), 1) === true);
check("totpVerify garbage", Crypto::totpVerify($sec, 'x1y2z3', 1) === false);
/* strict digits: wrong length / padded / signed forms must all fail */
check("totpVerify short code", Crypto::totpVerify($sec, '12345', 1) === false);
check("totpVerify long code", Crypto::totpVerify($sec, '1234567', 1) === false);
check("totpVerify whitespace code", Crypto::totpVerify($sec, ' 23456', 1) === false);
/* replay store: the same code in the same window is single-use */
$totpStore = new Memory('sec_totp');
$nowCode = Crypto::totp($sec);
check("totpVerify first use", Crypto::totpVerify($sec, $nowCode, 1, 30, 6, ['used' => $totpStore]) === true);
check("totpVerify replay rejected", Crypto::totpVerify($sec, $nowCode, 1, 30, 6, ['used' => $totpStore]) === false);
check("randomCode digits", preg_match('/^\d{6}$/', Crypto::randomCode(6)) === 1);
$rc2 = Crypto::randomCode(8, 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789');
check("randomCode alphabet", strlen($rc2) === 8 && strpbrk($rc2, '01IO') === false);

/* ---------- S7: Gene\Throttle ---------- */
$mem = new Memory('sec_regress');
check("Throttle::hit 1", Throttle::hit($mem, 'login:u1', 60) === 1);
check("Throttle::hit 2", Throttle::hit($mem, 'login:u1', 60) === 2);
check("Throttle::tooMany under", Throttle::tooMany($mem, 'login:u1', 5) === false);
check("Throttle::count", Throttle::count($mem, 'login:u1') === 2); /* tooMany is read-only now */
check("Throttle::attempt over", Throttle::attempt($mem, 'login:u1', 2, 60) === false);
check("Throttle::clear", Throttle::clear($mem, 'login:u1') === true);
check("Throttle::count after clear", Throttle::count($mem, 'login:u1') === 0);
check("Throttle::count missing key", Throttle::count($mem, 'login:ghost') === 0);

/* fixed-window TTL: a counter created with ttl=1 must expire (the earlier
 * bare-INCR path left the key permanent → throttle never released) */
$memT = new Memory('sec_thr_ttl');
check("Throttle::hit ttl window opens", Throttle::hit($memT, 'w', 1) === 1);
check("Memory::incr ttl arg", $memT->incr('ic', 1, 1) === 1);
sleep(2);
check("Throttle key expired after ttl", Throttle::count($memT, 'w') === 0);
check("Memory::incr key expired after ttl", !$memT->get('ic'));
/* read-only gate: tooMany must not consume budget */
check("Throttle::tooMany read-only", Throttle::tooMany($memT, 'ro', 1) === false && Throttle::count($memT, 'ro') === 0);

/* ---------- S2: Gene\Csrf ---------- */
class SecFakeSess {
    public $d = [];
    public function get($k) { return $this->d[$k] ?? null; }
    public function set($k, $v) { $this->d[$k] = $v; return true; }
    public function save() { return true; }
}
$secSess = new SecFakeSess();
Di::set('session', $secSess);
$tok = Csrf::token();
check("Csrf::token string", is_string($tok) && strlen($tok) >= 20);
check("Csrf::check own token", Csrf::check($tok) === true);
check("Csrf::check foreign", Csrf::check('bogus') === false);
/* a failed check must not rotate or create a token (no side effects) */
check("Csrf::check no side effect", ($secSess->d['_csrf'] ?? null) === $tok);
check("Csrf::field html", strpos(Csrf::field(), 'name="_token"') !== false);

/* ---------- S3: cspNonce + headers config ---------- */
$n1 = Response::cspNonce();
$n2 = Response::cspNonce();
check("cspNonce stable per request", $n1 === $n2 && strlen($n1) >= 20);

/* ---------- S12/S3 policy config registration ---------- */
$app = Application::getInstance();
$app->readonly(['/admin' => ['*' => ['list', 'index']]]);
$app->secureHeaders(['enabled' => true]);
$app->csrf(['methods' => ['post', 'put', 'delete']]);
$app->restGuard(['prefixes' => ['/rest/'], 'internal' => true, 'secret' => str_repeat('x', 32)]);
check("policy config registration", true);

/* ---------- session class surface ---------- */
$ref = new ReflectionClass('Gene\Session');
check("Session idle_ttl prop", $ref->hasProperty('idle_ttl'));
check("Session absolute_ttl prop", $ref->hasProperty('absolute_ttl'));
check("Session bind_flags prop", $ref->hasProperty('bind_flags'));

echo "\n=== Results: " . ($fail ? "FAILED $fail" : "ALL PASS") . " ===\n";
exit($fail ? 1 : 0);
