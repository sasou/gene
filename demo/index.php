<?php
header("Content-type: text/html; charset=UTF-8");
echo 'memory usage:',memory_get_peak_usage()/1024,'kb.<br/>';
$stime=microtime(true); 
define('APP_ROOT', __dir__ . '/app/');

// xhprof_enable(XHPROF_FLAGS_CPU | XHPROF_FLAGS_MEMORY);
for($i=0;$i<1;$i++){
$app = new \gene\application();
$app
->load("router.ini.php")
->load("config.ini.php")
->setMode(1,1)
->run()
;
}

$etime=microtime(true);
$total=$etime-$stime;
echo "<br/><br/>[页面执行时间：{$total} ]秒 ";
echo 'memory usage:',memory_get_peak_usage()/1024,'kb.';
/*
$xhprof_data = xhprof_disable();   //返回运行数据
 $XHPROF_ROOT = __dir__;
include_once $XHPROF_ROOT . "/xhprof_lib/utils/xhprof_lib.php";
include_once $XHPROF_ROOT . "/xhprof_lib/utils/xhprof_runs.php";

$xhprof_runs = new XHProfRuns_Default();
$run_id = $xhprof_runs->save_run($xhprof_data, "xhprof_testing");

echo "http://localhost/xhprof_html/index.php?run={$run_id}&source=xhprof_testing\n"; //*/