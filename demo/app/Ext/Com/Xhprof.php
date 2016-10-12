<?php
namespace Ext\Com;
/**
 * Xhprof
 */
class Xhprof
{

    /**
     * xhprof config
     * @var string $_logDir xhprof log save directory  default NULL
     * @var string $namespace
     */
    protected $_config;

    public function __construct()
    {
        // start profiling
        xhprof_enable(XHPROF_FLAGS_CPU + XHPROF_FLAGS_MEMORY);
    }

    /**
     * save raw data for this profiler run using default
     * @param string $namespace performance analysis class or method name
     * @param string $logDir performance analysis log file save directory
     * @return string
     */
    public function save($namespace = NULL, $logDir = NULL)
    {
        $xhprofData = xhprof_disable();

        $this->_config = Cola::getConfig('_xhprof') + array('namespace' => 'xhprof_cola', 'logDir' => '');

        if (NULL === $namespace) {
            $namespace = $this->_config['namespace'];
        }

        if (NULL === $logDir) {
            $logDir = $this->_config['logDir'];
        }

        require_once COLA_DIR . '/Com/Xhprof/xhprof_lib/utils/xhprof_lib.php';
        require_once COLA_DIR . '/Com/Xhprof/xhprof_lib/utils/xhprof_runs.php';

        // implementation of iXHProfRuns.
        $xhprof_runs = new XHProfRuns_Default($logDir);

        // save the run under a namespace "xhprof_foo"
        $run_id = $xhprof_runs->save_run($xhprofData, $namespace);
        if (empty($this->_config['viewUrl'])) {
            return "?run={$run_id}&source={$namespace}";
        } else {
            return '<a target="_blank" href="' . $this->_config['viewUrl'] . "?run={$run_id}&source={$namespace}" . '">view xhprof</a>';
        }
    }

    /**
     *
     * @return array
     */
    public function get()
    {
        return xhprof_disable();
    }

}
