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
 * multi(): curl_multi when Native CURL hook is available; otherwise sequential
 * Client with E_NOTICE. Does not compile-link libcurl.
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
     *   query?: array,
     *   json?: mixed,
     *   form?: array,
     *   files?: array<string, string|array>,
     *   body?: string|array,
     *   timeout?: float|int,
     *   connect_timeout?: float|int,
     *   ssl_verify?: bool,
     *   retry?: int,
     *   stream?: callable,
     *   sse?: callable,
     *   sse_forward?: bool,
     *   discard_body?: bool,
     *   keep_alive?: bool
     * } $options
     *   query and urlencoded form use RFC3986 encoding. json, string body and
     *   file-less form are mutually exclusive. files + form is multipart;
     *   files + array body remains compatible, but body + form throws. Automatic
     *   Content-Type never replaces a case-insensitive caller header. Unknown
     *   options emit E_NOTICE. retry is GET/HEAD only, 5xx/timeout, exponential
     *   backoff, capped at 3. ssl_verify defaults true. stream and sse are mutually
     *   exclusive. Swoole stream/sse is post-execute 8KB slices (not TTFB).
     *   Nested request() throws.
     * @return array{status:int, headers:array, body:string}
     * @throws \Exception
     */
    public static function request(array $options) {}

    /**
     * Parallel batch via curl_multi (FPM/CLI, or Swoole with Native CURL hook).
     * No stream/sse. Item failure: status=0 + error, no throw.
     * concurrency clamped 1..16 (CURLMOPT_MAX_TOTAL_CONNECTIONS). Max 64 items.
     * Swoole without Native CURL: sequential Coroutine\\Http\\Client + E_NOTICE.
     *
     * @param list<array> $requests
     * @param array{concurrency?:int} $options
     * @return list<array{status:int, headers:array, body:string, error?:string}>
     */
    public static function multi(array $requests, array $options = []) {}
}
