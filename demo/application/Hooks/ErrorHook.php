<?php
namespace Hooks;

/**
 * Global Error hook - extends Gene\Hook base class.
 * Executed before every route dispatch (unless cleared).
 */
class ErrorHook extends \Gene\Hook
{
    /**
     * Error hook handler.
     * Return false to abort the request.
     */
    public function handle()
    {
        $this->redirect('/404.html');
    }
}
