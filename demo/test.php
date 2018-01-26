<?php
for($i=0;$i<100;$i++) {
	spl_autoload_register('Gene\Load::autoload');
}


function auto($class){
    echo 'auto';
}
function aa($class){
    echo 'aa';
}

spl_autoload_register("auto");
spl_autoload_register("aa");

$cc= spl_autoload_functions();
var_dump($cc);
