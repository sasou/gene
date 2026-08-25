<?php
namespace Gene;

class Invoke
{
    /**
     * Isolated in-process dispatch. Snapshots Request, scopes params, news
     * a Controller, restores even on exception. Depth max 8.
     *
     * @param string $class
     * @param string $action
     * @param array $params
     * @param array $files
     * @return mixed
     */
    public static function local($class, $action, array $params = [], array $files = []) {}
}
