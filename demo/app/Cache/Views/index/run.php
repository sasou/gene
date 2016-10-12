<br><br>
变量输出：<br>
<?php echo htmlspecialchars($test, ENT_COMPAT);?>

<br><br>
if:<br>
<?php if(1>0) { ?>
<?php echo htmlspecialchars($test, ENT_COMPAT);?><br>
<?php } ?>

<br><br>
loop:<br>
<?php if(is_array($arr)) { foreach($arr as $v) { ?>
<?php echo htmlspecialchars($v, ENT_COMPAT);?><br>
<?php } } ?>

<br><br>
for:<br>
<?php for($i=0;$i<2;$i++) { ?>
<?php echo htmlspecialchars($i, ENT_COMPAT);?><br>
<?php } ?>

<br><br>
常量输出：<br>
<?php echo APP_ROOT;?>

<br><br>
eval输出：<br><?php echo 111?><br>

<br><br>
:输出：<br>
<?php echo count($arr); ?><br>

<br><br>
~输出：<br>
<?php var_dump($arr); ?><br>