<?php
/** P1-4: user error handler converting E_WARNING to exception during tx hygiene. */
$file = sys_get_temp_dir() . '/gene_tx_hygiene_handler.db';
@unlink($file);
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE IF NOT EXISTS t (a int)')->execute();
\Gene\Di::set('p14_db', $db);
$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (1)')->execute();
echo "inTransaction=", var_export($db->inTransaction(), true), "\n";
set_error_handler(function ($no, $str) {
    echo "[handler] converting to ErrorException: $str\n";
    throw new ErrorException($str, 0, $no);
});
echo "-- end of script, RSHUTDOWN hygiene follows --\n";
