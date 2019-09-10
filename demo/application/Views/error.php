<?php
    $file = $e->getFile();
    $line = $e->getLine();
    if ( file_exists( $file ) )
    {
        $lines = file( $file );
    }
?>
<!DOCTYPE html>
<head>
<meta charset="utf-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
<title>Caught Exception</title>
<style type="text/css">
body{padding: 0;margin: 0;}
.content {width: 80%;margin: 0 auto;padding:5px 10px;}
h1{font-weight: bolder;color: #cc7a94;padding: 5px;}
h2{font-weight: normal;color: #AF7C8C;background-color: #e9f4f5;padding: 5px;}
ul.code {font-size: 13px;line-height: 20px;color: #68777d;background-color: #f2f9fc;border: 1px solid #c9e6f2;}
ul.code li {width : 100%;margin:0px;white-space: pre ;list-style-type:none;font-family : monospace;}
ul.code li.line {color : red;}
table.trace {width : 100%;font-size: 13px;line-height: 20px;color: #247696;background-color: #c9e6f2;}
table.trace thead tr {background-color: #90a9b3;color: white;}
table.trace tr {background-color: #f2f9fc;}
table.trace td {padding : 5px;}
.foot {line-height: 20px;color: #cc7a94;margin:10px;}
</style>
</head>
<body>
<div class="content">
    
<center><h1>Caught Exception</h1></center>

<h2>What's happened:</h2>
<code><?php echo $e->getMessage(); ?></code>

<h2>Where's happened:</h2>
<?php if(isset($lines)) { ?>
<code>File:***<?php echo substr($file,-(ceil(strlen($file)*0.6))); ?> Line:<?php echo $line; ?></code>
<ul class="code">
<?php for($i=$line-8;$i<$line+8;$i++) { ?>
<?php if($i>0 && $i<count($lines)) { ?>
<?php if($i == $line-1) { ?>
<li class="line"><?php echo $lines[$i]; ?></li>
<?php } else { ?>
<li><?php echo $lines[$i]; ?></li>
<?php } ?>
<?php } ?>
<?php } ?>
</ul>
<?php } ?>
<?php if(is_array( $e->getTrace() )) { ?>

<h2>Stack trace:</h2>
<table class="trace">
<thead><tr><td width="180px">File</td><td width="30px">Line</td><td width="250px">Class</td><td width="150px">Function</td><td>Arguments</td></tr></thead>
<tbody>
<?php if(is_array($e->getTrace())) { foreach($e->getTrace() as $i => $trace) { ?>
<?php if(isset($trace[ 'class' ]) && $trace[ 'function' ] != "doError") { ?>
<tr>
<td><?php echo isset($trace[ 'file' ]) ? basename($trace[ 'file' ]) : ''; ?></td>
<td><?php echo $trace[ 'line' ] ?? ''; ?></td>
<td><?php echo $trace[ 'class' ] ?? ''; ?></td>
<td><?php echo $trace[ 'function' ] ?? ''; ?></td>
<td>
<?php if(isset($trace['args'])) { ?>
<?php if(is_array($trace['args'])) { foreach($trace['args'] as $i => $arg) { ?>
    <span title="<?php echo var_export( $arg, true ); ?>"><?php echo gettype( $arg ); ?></span> 
<?php } } ?>
<?php } else { ?>
NULL
<?php } ?>
</td>
</tr>
<?php } ?>
<?php } } ?>
</tbody>
</table>
<?php } ?>

<center class="foot">Gene <?php echo gene_version(); ?></center>

</div>
</body>
</html>