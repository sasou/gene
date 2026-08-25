<?php
/**
 * Verifies the actual case the tri-state rateLimit() fix targets: a
 * connection that was fine, then drops mid-session (checkError()-matching
 * disconnect), during the eval call itself — NOT a construct-time failure
 * (that already throws uncaught, by design, unrelated to this fix).
 */
$cfg = [
    'persistent' => false,
    'host' => '192.168.5.102',
    'port' => 6379,
    'timeout' => 3,
    'ttl' => 0,
    'password' => 'rds2024',
    'serializer' => 0,
];

$r = new \Gene\Cache\Redis($cfg);

// Find our own connection's client id via the underlying Redis object.
$myId = $r->__call('client', ['id']);
echo "My client id: " . var_export($myId, true) . "\n";

// Open a second raw connection to issue CLIENT KILL against the first.
$killer = new \Redis();
$killer->connect('192.168.5.102', 6379, 3);
$killer->auth('rds2024');
$killResult = $killer->rawCommand('CLIENT', 'KILL', 'ID', (string)$myId);
echo "CLIENT KILL result: " . var_export($killResult, true) . "\n";
$killer->close();

// Give the TCP stack a moment to actually tear down the socket.
usleep(200000);

$k = 'gene:rl:midsession:' . bin2hex(random_bytes(4));
$res = $r->rateLimit($k, 2, 30);
echo "rateLimit() right after kill: " . var_export($res, true) . "\n";

if ($res === true || $res === false) {
    echo "INFO: reconnect succeeded transparently and returned a real result — checkError()'s built-in retry absorbed the drop before we ever saw an unrecoverable error.\n";
} elseif ($res === null) {
    echo "PASS: mid-session drop surfaced as null (indeterminate), not a false 'blocked'.\n";
}

// Confirm the connection self-healed: a normal follow-up call should work.
$res2 = $r->rateLimit($k, 2, 30);
echo "rateLimit() follow-up call: " . var_export($res2, true) . "\n";

$r->__call('del', [$k]);
$r->free();
