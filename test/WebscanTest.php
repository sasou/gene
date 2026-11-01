<?php

/**
 * Gene\Webscan regression test.
 *
 * Covers: baseline GET/POST/Cookie/Referer hits, path-only whitelist
 * (query cannot switch the scanner off), SQL comment normalization
 * (#, --, /*!...*\/), raw+normalized double match, JSON body scanning,
 * request path / User-Agent / X-Forwarded-For scanning, PCRE fail-closed.
 *
 * Request bags go through Request::init(get, post, cookie, server, env,
 * files, request, header, raw).
 */

use Gene\Request;
use Gene\Webscan;

echo "=== Webscan Test ===\n\n";

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

function scan(array $opt = [], array $ctor = []) {
    $server = ($opt['server'] ?? []) + [
        'REQUEST_METHOD' => isset($opt['post']) || isset($opt['raw']) ? 'POST' : 'GET',
        'REQUEST_URI' => $opt['uri'] ?? '/',
        'QUERY_STRING' => (string) parse_url($opt['uri'] ?? '/', PHP_URL_QUERY),
    ];
    Request::init(
        $opt['get'] ?? [],
        $opt['post'] ?? [],
        $opt['cookie'] ?? [],
        $server,
        null,
        [],
        null,
        $opt['header'] ?? [],
        $opt['raw'] ?? null
    );
    $args = $ctor + [1, '', [], 1, 1, 1, 1, 1];
    ksort($args);
    return (new Webscan(...$args))->check();
}

/* ---------- baseline ---------- */
check("clean GET passes", scan(['get' => ['id' => '42', 'q' => 'hello world']]) === false);
check("GET union select", scan(['get' => ['id' => '1 union select password from users']]) === true);
check("GET script tag", scan(['get' => ['q' => '<script>alert(1)</script>']]) === true);
check("POST nested array", scan(['post' => ['a' => ['b' => "1' or 1=1"]]]) === true);
check("Cookie sleep()", scan(['cookie' => ['sid' => 'x and sleep(5)']]) === true);
check("Referer XSS", scan(['server' => ['HTTP_REFERER' => 'http://x/<script>']]) === true);
check("switch off", scan(['get' => ['id' => '1 union select 1']], [0 => 0]) === false);

/* ---------- whitelist: path only, leading segment ---------- */
$attack = ['id' => '1 union select password from users'];
check("white dir hit on /admin/...", scan(['uri' => '/admin/user/list', 'get' => $attack], [1 => 'admin']) === false);
check("white dir exact /admin", scan(['uri' => '/admin', 'get' => $attack], [1 => '/admin/']) === false);
check("query ?x=admin does not whitelist", scan(['uri' => '/api/x?x=admin', 'get' => $attack + ['x' => 'admin']], [1 => 'admin']) === true);
check("/adminfoo is not /admin", scan(['uri' => '/adminfoo/x', 'get' => $attack], [1 => 'admin']) === true);
check("nested /x/admin not whitelisted", scan(['uri' => '/x/admin/y', 'get' => $attack], [1 => 'admin']) === true);
check("dot-dot escape not whitelisted", scan(['uri' => '/admin/../api/x', 'get' => $attack], [1 => 'admin']) === true);
check("white url + query value", scan(['uri' => '/cms/save?m=edit', 'get' => $attack + ['m' => 'edit']], [2 => ['/cms/save' => 'm=edit']]) === false);
check("white url value missing", scan(['uri' => '/cms/save?m=del', 'get' => $attack + ['m' => 'del']], [2 => ['/cms/save' => 'm=edit']]) === true);
check("white url empty value needs empty query", scan(['uri' => '/cms/save?id=1', 'get' => $attack], [2 => ['/cms/save' => '']]) === true);
check("white url key in query ignored", scan(['uri' => '/api/x?r=/cms/save', 'get' => $attack + ['r' => '/cms/save']], [2 => ['/cms/save' => '']]) === true);

/* ---------- comment normalization ---------- */
check("INSERT#\\nINTO", scan(['post' => ['s' => "INSERT#x\nINTO t VALUES(1)"]]) === true);
check("DROP-- \\nTABLE", scan(['post' => ['s' => "DROP-- c\nTABLE users"]]) === true);
check("and#\\n1=1", scan(['get' => ['id' => "1 and#\n1=1"]]) === true);
check("or a=--b double minus", scan(['get' => ['id' => "x' or a=--a"]]) === true);
check("/*!50000 exec comment */", scan(['post' => ['s' => "1 /*!50000UNION*/ /*!50000SELECT*/ 1,2"]]) === true);
check("sibling # cannot swallow next leaf", scan(['post' => ['a' => ['#', "1' or 1=1"]]]) === true);
check("raw still matched when # hides payload", scan(['get' => ['id' => "1' union select 1,'#',password from users"]]) === true);
check("plain text with # passes", scan(['post' => ['c' => "color: #fff;\nsize -- big"]]) === false);

/* ---------- JSON body ---------- */
$jsonHdr = ['Content-Type' => 'application/json'];
check("JSON clean passes", scan(['header' => $jsonHdr, 'raw' => '{"name":"bob","n":1}']) === false);
check("JSON union select", scan(['header' => $jsonHdr, 'raw' => '{"id":"1 union select password from users"}']) === true);
check("JSON nested XSS", scan(['header' => $jsonHdr, 'raw' => '{"a":{"b":["<script>x"]}}']) === true);
check("JSON unicode-escaped quote", scan(['header' => $jsonHdr, 'raw' => '{"id":"1\u0027 or 1=1"}']) === true);
check("JSON body without content-type", scan(['raw' => '{"id":"1 union select 1 from t"}']) === true);
check("invalid JSON scanned raw", scan(['header' => $jsonHdr, 'raw' => '{"id":"1 union select 1 from t"']) === true);
check("json() still usable after scan", (function () {
    scan(['header' => ['Content-Type' => 'application/json'], 'raw' => '{"k":"v"}']);
    return Request::json() === ['k' => 'v'];
})());
check("post off skips JSON", scan(['header' => $jsonHdr, 'raw' => '{"id":"1 union select 1 from t"}'], [4 => 0]) === false);

/* ---------- path & headers ---------- */
check("path payload", scan(['uri' => '/item/1%20union%20select%20pass%20from%20u']) === true);
check("normal path passes", scan(['uri' => '/article/union-select-guide']) === false);
check("User-Agent XSS", scan(['server' => ['HTTP_USER_AGENT' => '<script>alert(1)</script>']]) === true);
check("X-Forwarded-For sqli", scan(['server' => ['HTTP_X_FORWARDED_FOR' => "1' or 1=1"]]) === true);
check("normal browser UA passes", scan(['server' => ['HTTP_USER_AGENT' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36']]) === false);

/* ---------- PCRE failure ---------- */
$old = ini_get('pcre.backtrack_limit');
$oldJit = ini_get('pcre.jit');
ini_set('pcre.jit', '0');
ini_set('pcre.backtrack_limit', '10');
$pad = '<' . str_repeat('a', 5000) . ' union select';
check("PCRE error fail-closed", scan(['post' => ['c' => $pad]]) === true);
check("PCRE error fail-open when disabled", scan(['post' => ['c' => $pad]], [7 => 0]) === false);
ini_set('pcre.backtrack_limit', $old);
ini_set('pcre.jit', $oldJit);

echo "\n" . ($fail ? "FAILED: $fail\n" : "ALL PASSED\n");
exit($fail ? 1 : 0);
