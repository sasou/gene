<?php
namespace Hooks;

/**
 * Admin authentication hook - extends Gene\Hook base class.
 * Replaces the closure-based hook with a class-based one,
 * avoiding eval() and improving performance.
 */
class AdminAuth extends \Gene\Hook
{
    /**
     * Named hook handler - called for routes with "adminAuth" hook.
     * Return false to abort the request (e.g. unauthorized).
     */
    public function handle()
    {
        if (!isset($this->user['user_id'])) {
            $this->redirect('/login.html');
            return false;
        }
        $id = \Services\Admin\Module::getInstance()->initPath(\Gene\Application::getRouterUri());
        if (!$id || strpos(",{$this->user['purview']},", ",{$id},") === false) {
            \Gene\Response::json(\Gene\Response::error("操作未授权"));
            return false;
        }
        return true;
    }
}
