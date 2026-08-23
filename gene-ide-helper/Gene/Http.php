<?php
namespace Gene;

/**
 * Outbound HTTP client.
 *
 * FPM/CLI: PHP curl_* (requires ext-curl). Handle reused only inside the
 * current request. Swoole (runtime_type >= 2): Swoole\Coroutine\Http\Client
 * so the worker is not blocked. keep_alive=true reuses Client by host:port:ssl
 * inside the current request/coroutine (destroyed on cleanup). stream on
 * Swoole is post-execute 8KB slices — Client has no write-function.
 * Does not compile-link libcurl.
 *
 * @return array{status:int, headers:array, body:string}
 */
class Http
{
    /**
     * @param array{
     *   method?: string,
     *   url: string,
     *   headers?: array<string,string>,
     *   json?: mixed,
     *   files?: array<string, string|array>,
     *   body?: string|array,
     *   timeout?: float|int,
     *   connect_timeout?: float|int,
     *   ssl_verify?: bool,
     *   retry?: int,
     *   stream?: callable,
     *   keep_alive?: bool
     * } $options
     *   json, body (string) and files are mutually exclusive. files + array body
     *   is multipart form fields. retry is GET/HEAD only, 5xx/timeout,
     *   exponential backoff, capped at 3. ssl_verify defaults true.
     * @return array{status:int, headers:array, body:string}
     * @throws \Exception
     */
    public static function request(array $options) {}
}
