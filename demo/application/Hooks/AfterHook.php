<?php
namespace Hooks;

/**
 * Global after hook - extends Gene\Hook base class.
 * Executed after every route dispatch (unless cleared).
 * Receives the dispatch result as parameter.
 */
class AfterHook extends \Gene\Hook
{
    /**
     * After hook handler.
     * @param mixed $params The dispatch result from the controller action.
     */
    public function handle($params = null)
    {
        $callback = $this->request->get("callback") ?? '';
        \Gene\Response::json($params, $callback);
    }
}
