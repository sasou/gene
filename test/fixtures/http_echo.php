<?php
/**
 * Echo server for Gene\Http tests. Started via `php -S`.
 */
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
$uri = $_SERVER['REQUEST_URI'] ?? '/';
$code = isset($_GET['code']) ? (int)$_GET['code'] : 200;
$sleep = isset($_GET['sleep']) ? (int)$_GET['sleep'] : 0;
if ($sleep > 0) {
    usleep($sleep * 1000);
}
http_response_code($code);
header('X-Echo-Method: ' . $method);
header('Content-Type: application/json; charset=UTF-8');
$body = file_get_contents('php://input');
echo json_encode([
    'method' => $method,
    'uri' => $uri,
    'body' => $body,
], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
