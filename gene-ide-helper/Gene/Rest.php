<?php
namespace Gene;

class Rest
{
    /**
     * @param array|null $config
     */
    public function __construct($config = null) {}

    /**
     * Immutable proxy for a named service. Does not mutate the shared instance.
     *
     * @param string $name
     * @return Rest
     */
    public function use($name) {}

    /**
     * @param string $class
     * @param string $action
     * @param array $params
     * @param array $files
     * @return mixed
     */
    public function local($class, $action, array $params = [], array $files = []) {}

    /**
     * @param string $method
     * @param string $path path must start with /
     * @param array $options same as Http::request plus decode
     * @return array{status:int, headers:array, body:mixed}
     */
    public function http($method, $path, array $options = []) {}

    /**
     * Local if class/action can dispatch; otherwise options['path'] + base_url.
     *
     * @param string $class
     * @param string $action
     * @param array $params
     * @param array $options
     * @return mixed
     */
    public function call($class, $action, array $params = [], array $options = []) {}
}
