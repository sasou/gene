<?php
namespace Hooks;

/**
 * Request-id hook — write Gene\Context + X-Request-Id response header.
 *
 * Incoming X-Request-Id is reused; otherwise bin2hex(random_bytes(8)).
 * Gene\Log automatically merges Context.request_id into $context.
 *
 *   ->hook('requestId', 'Hooks\RequestId@handle')
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
