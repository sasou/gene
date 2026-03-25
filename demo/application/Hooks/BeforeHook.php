<?php
namespace Hooks;

/**
 * Global before hook - extends Gene\Hook base class.
 * Executed before every route dispatch (unless cleared).
 */
class BeforeHook extends \Gene\Hook
{
    /**
     * Before hook handler.
     * Return false to abort the request.
     */
    public function handle()
    {
        $user = $this->session->get('admin');
        \Gene\Di::set('user', $user);
        return true;
    }
}
