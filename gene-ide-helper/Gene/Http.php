<?php
namespace Gene;

/**
 * Outbound HTTP client.
 *
 * FPM/CLI: PHP curl_* (requires ext-curl). Handle reused only inside the
 * current request. Swoole (runtime_type >= 2): Swoole\Coroutine\Http\Client
 * so the worker is not blocked. Does not compile-link libcurl.
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
     *   body?: string,
     *   timeout?: float|int,
     *   connect_timeout?: float|int,
     *   ssl_verify?: bool,
     *   retry?: int,
     *   stream?: callable,
     *   keep_alive?: bool
     * } $options
     *   json and body are mutually exclusive. retry is GET/HEAD only, 5xx/timeout,
     *   exponential backoff, capped at 3. ssl_verify defaults true.
     * @return array{status:int, headers:array, body:string}
     * @throws \Exception
     */
    public static function request(array $options) {}
}
