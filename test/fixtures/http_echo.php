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
if (isset($_GET['invalid'])) {
    header('Content-Type: text/plain');
    echo '{';
    return;
}
http_response_code($code);
header('X-Echo-Method: ' . $method);
header('Content-Type: application/json; charset=UTF-8');
$body = file_get_contents('php://input');
$files = [];
if (!empty($_FILES) && is_array($_FILES)) {
    foreach ($_FILES as $k => $f) {
        $files[$k] = [
            'name' => $f['name'] ?? '',
            'type' => $f['type'] ?? '',
            'size' => $f['size'] ?? 0,
        ];
    }
}
echo json_encode([
    'method' => $method,
    'uri' => $uri,
    'body' => $body,
    'post' => $_POST ?? [],
    'files' => $files,
], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
