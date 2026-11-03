<?php

/**
 * View escaping / Response script helpers / Db orderBy / Language negotiate /
 * Monitor::security regression (plan/framework-review-2026-11.md).
 */

use Gene\View;

echo "=== View Escape & Hardening Test ===\n\n";

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

// View::e — quotes, invalid UTF-8, non-string inputs
check('e() escapes single quote', View::e("a'b") === 'a&#039;b');
check('e() escapes double quote/tags', View::e('<a href="x">') === '&lt;a href=&quot;x&quot;&gt;');
check('e() substitutes invalid UTF-8', View::e("ok\xC3") !== '' && strpos(View::e("ok\xC3"), 'ok') === 0);
check('e() null -> empty', View::e(null) === '');
check('e() int', View::e(42) === '42');
check('e() array -> empty', View::e([1]) === '');

// View::js — </script> cannot terminate the script block
$js = View::js("</script><script>alert(1)</script>");
check('js() hex-escapes tags', strpos($js, '<') === false && strpos($js, '>') === false);
check('js() hex-escapes quotes', strpos(View::js("a'\"b"), "'") === false);
check('js() arrays', View::js(['a' => 1]) === '{"a":1}');

// View::safeUrl — scheme whitelist with whitespace/control obfuscation
check('safeUrl() http kept', View::safeUrl('https://x.com/?a=1&b=2') === 'https://x.com/?a=1&amp;b=2');
check('safeUrl() relative kept', View::safeUrl('/doc/a.html') === '/doc/a.html');
check('safeUrl() javascript blocked', View::safeUrl('javascript:alert(1)') === '#');
check('safeUrl() obfuscated scheme blocked', View::safeUrl(" java\tscript:alert(1)") === '#');
check('safeUrl() data blocked', View::safeUrl('data:text/html,x') === '#');
check('safeUrl() attribute breakout escaped', strpos(View::safeUrl('/a"onmouseover="x'), '"') === false);

// View::clean — escapes without purifier, delegates with one
check('clean() escapes without purifier', View::clean('<b>x</b>') === '&lt;b&gt;x&lt;/b&gt;');
View::setPurifier(fn($h) => strip_tags($h, '<b>'));
check('clean() uses purifier', View::clean('<b>x</b><script>y</script>') === '<b>x</b>y');
View::setPurifier(null);

// Template compile: {$a.b} escaped, {$v} ENT_QUOTES, {!! !!} raw
$tmp = sys_get_temp_dir() . '/gene_view_' . getmypid();
@mkdir("$tmp/app/Views/sub", 0777, true);
file_put_contents("$tmp/app/Views/sub/t.php", "{\$v}|{\$a.b}|{\$a.b.c}|{!! \$raw !!}|{:\$expr}");
$cwd = getcwd();
chdir($tmp);
$n = View::compileAll();
check('compileAll() compiled one template', $n === 1);
$compiled = @file_get_contents("$tmp/app/Cache/Views/sub/t.php");
check('{$v} -> View::e', $compiled !== false && strpos($compiled, '\Gene\View::e($v)') !== false);
check('{$a.b} -> View::e', strpos((string)$compiled, "\\Gene\\View::e(\$a['b'])") !== false);
check('{$a.b.c} -> View::e', strpos((string)$compiled, "\\Gene\\View::e(\$a['b']['c'])") !== false);
check('{!! !!} raw', strpos((string)$compiled, 'echo $raw;') !== false);
$v = "x'y"; $a = ['b' => ['c' => '<i>']]; $raw = '<b>'; $expr = 1;
ob_start();
include "$tmp/app/Cache/Views/sub/t.php";
$out = ob_get_clean();
check('rendered output escaped', strpos($out, "x&#039;y|") === 0 && strpos($out, '&lt;i&gt;') !== false && strpos($out, '|<b>|') !== false);
chdir($cwd);
@unlink("$tmp/app/Cache/Views/sub/t.php");
@unlink("$tmp/app/Views/sub/t.php");

// Db / Query orderBy whitelist (no connection needed for builder errors)
if (class_exists('PDO') && in_array('sqlite', PDO::getAvailableDrivers(), true)) {
    $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
    $db->sql('CREATE TABLE t (id INTEGER, name TEXT)')->execute();
    $db->sql("INSERT INTO t VALUES (1,'b'),(2,'a')")->execute();
    $rows = $db->select('t')->orderBy('n', 'desc', ['n' => 'name'])->all();
    check('orderBy() alias map', $rows[0]['name'] === 'b');
    $rows = $db->select('t')->orderBy('id', 'DESC', ['id', 'name'])->all();
    check('orderBy() list allow', (int)$rows[0]['id'] === 2);
    foreach ([['id; DROP TABLE t', 'asc', null], ['id', 'asc; x', null], ['email', 'asc', ['id']]] as $bad) {
        try {
            $db->select('t')->orderBy(...$bad);
            check('orderBy() rejects ' . $bad[0] . '/' . $bad[1], false);
        } catch (\ValueError $e) {
            check('orderBy() rejects ' . $bad[0] . '/' . $bad[1], true);
        }
    }
} else {
    echo "SKIP orderBy (pdo_sqlite missing)\n";
}
check('Query::orderBy exists', method_exists('Gene\Orm\Query', 'orderBy'));

// Language::negotiate
$lang = new \Gene\Language(sys_get_temp_dir(), 'zh');
check('negotiate() q ordering', $lang->negotiate(['zh', 'en'], 'fr;q=1,en-US;q=0.8,zh;q=0.5') === 'en');
check('negotiate() primary subtag', $lang->negotiate(['en', 'zh-CN'], 'zh') === 'zh-CN');
check('negotiate() fallback first', $lang->negotiate(['zh', 'en'], 'de') === 'zh');
check('negotiate() q=0 ignored', $lang->negotiate(['zh', 'en'], 'en;q=0') === 'zh');

// Response::alert / redirectJs — </script> breakout and javascript: URLs
ob_start();
\Gene\Response::alert('</script><script>alert(1)</script>', 'javascript:alert(2)');
$out = ob_get_clean();
check('alert() text cannot close script', substr_count($out, '</script>') === 1);
check('alert() javascript: url replaced', strpos($out, 'javascript:') === false && strpos($out, 'href="/";') !== false);
ob_start();
\Gene\Response::redirectJs('/a?b="c"');
$out = ob_get_clean();
check('redirectJs() JSON-encodes url', strpos($out, 'href="\/a?b=\u0022c\u0022"') !== false);

// gene.redirect_hosts (PHP_INI_SYSTEM; run with -d gene.redirect_hosts=example.com,*.trusted.org)
if (ini_get('gene.redirect_hosts') !== '' && ini_get('gene.redirect_hosts') !== false) {
    $cases = ['https://example.com/x' => true, 'https://a.trusted.org/' => true, '//evil.com/' => false,
              'https://example.com@evil.com/' => false, '/\\evil.com' => false, '/local' => true];
    foreach ($cases as $url => $kept) {
        ob_start();
        \Gene\Response::redirectJs($url);
        $o = ob_get_clean();
        check("redirect_hosts $url", $kept ? strpos($o, 'href="/";') === false : strpos($o, 'href="/";') !== false);
    }
} else {
    echo "SKIP redirect_hosts (ini not set)\n";
}

// View::__get falls back to assign() vars when DI misses
$view = new View();
$view->assign('page_title', 'T1');
check('view->x reads assign var', $view->page_title === 'T1' && isset($view->page_title));
check('view->x missing is null', $view->no_such_var === null && !isset($view->no_such_var));

// Orm $incrementing = false keeps fill() as a new row
class UuidModel extends \Gene\Orm\Model {
    protected static $table = 'u';
    protected static $primaryKey = 'id';
    protected static $incrementing = false;
}
class AutoModel extends \Gene\Orm\Model {
    protected static $table = 'a';
    protected static $primaryKey = 'id';
}
$u = (new UuidModel())->fill(['id' => 'abc']);
$a = (new AutoModel())->fill(['id' => 5]);
$exists = fn($m) => (fn() => $this->exists)->call($m);
check('$incrementing=false fill() not exists', !$exists($u));
check('default fill() with pk exists', (bool)$exists($a));

// View path safety: traversal / absolute / drive paths rejected before any I/O
foreach (['../x', 'a/../../x', '/etc/passwd', '\\x', 'C:/x'] as $bad) {
    $r = @(new View())->render($bad);
    check("render() rejects $bad", $r === false || $r === null || $r === '');
}

// Monitor::security
$sec = \Gene\Monitor::security();
check('security() shape', isset($sec['gene.view_autoescape']['ok'], $sec['display_errors']['recommended']));

echo "\n" . ($fail ? "FAILED: $fail\n" : "ALL PASSED\n");
exit($fail ? 1 : 0);
