<?php
namespace Hooks;

/**
 * Legacy request-id hook retained for older applications.
 *
 * New FPM/Swoole entrypoints should use Application::requestId(), which applies
 * trust and length policies and writes both Gene\Context and the response header.
 */
class RequestId extends \Gene\Hook
{
    public function handle()
    {
        $id = $this->request->header('x-request-id')
            ?? $this->request->header('X-Request-Id');
        if (!is_string($id) || $id === '') {
            $id = bin2hex(random_bytes(8));
        }
        \Gene\Context::set('request_id', $id);
        \Gene\Response::header('X-Request-Id', $id);
        return true;
    }
}
