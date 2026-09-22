<?php
namespace Hooks;

/**
 * Router error hook - extends Gene\Hook base class.
 * Registered for the 404 error route.
 */
class ErrorHook extends \Gene\Hook
{
    /**
     * Error hook handler.
     * Return false to abort the request.
     */
    public function handle()
    {
        return $this->respond(['code' => 404, 'msg' => 'Not Found'], 404);
    }
}
