<?php
	$e = gene\exception::getEx();
    $file = $e->getFile();
    $line = $e->getLine();
    if ( file_exists( $file ) )
    {
        $lines = file( $file );
    }
?>
<!DOCTYPE html>
<head>
<meta charset='utf-8'>
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
<title><?php echo $e->getMessage(); ?></title>
<style type="text/css">
body{padding: 0;margin: 0;}
.content {width: 960px;margin: 0 auto;padding:5px 10px;}
h1, h2{font-weight: bold;color: #AF7C8C;}
ul.code {font-size: 13px;line-height: 20px;color: #68777d;background-color: #f2f9fc;border: 1px solid #c9e6f2;border-radius: 3px;}
ul.code li {width : 100%;margin:0px;white-space: pre ;list-style-type:none;font-family : monospace;}
ul.code li.line {color : red;}
table.trace {width : 100%;font-size: 13px;line-height: 20px;color: #68777d;background-color: #f2f9fc;border: 1px solid #c9e6f2;border-radius: 3px;}
table.thead tr {background : rgb(240,240,240);}
table.trace tr.odd {background : #DAEFE9;}
table.trace tr.even {background : #ECF1F0;}
table.trace td {padding : 2px 4px 2px 4px;}
.foot {line-height: 20px;color: #D2D8DA;margin:10px;}
</style>
</head>
<body>
<div class="content">
	<center><h1>Uncaught <?php echo get_class($e);?></h1></center>
	<h2><?php echo $e->getMessage(); ?></h2>
	<p>An uncaught <?php echo get_class($e);?> was thrown on line <?php echo $line;?> of file <?php echo basename( $file );?> that prevented further execution of this request.</p>
	<h2>Where it happened:</h2>
	<?php if (isset($lines)):?>
	<code>File:***<?php echo substr($file,-(ceil(strlen($file)*0.6)));?> Line:<?php echo $line;?></code>
	<ul class="code">
		<?php for($i =$line-3;$i<$line+3;$i++):?>
			<?php if($i>0 && $i<count($lines)):?>
				<?php if ( $i == $line-1 ) : ?>
					<li class="line"><?php echo htmlspecialchars(str_replace( "\n", "", $lines[$i] ), ENT_COMPAT);?></li>
				<?php else:?>
					<li><?php echo htmlspecialchars(str_replace( "\n", "", $lines[$i] ));?></li>
				<?php endif;?>
			<?php endif;?>
		<?php endfor;?>
	</ul>
	<?php endif; ?>
	<?php if ( is_array( $e->getTrace() ) ) : ?>
	<h2>Stack trace:</h2>
	<table class="trace">
		<thead><tr><td>File</td><td>Line</td><td>Class</td><td>Function</td><td>Arguments</td></tr></thead>
		<tbody>
		<?php foreach ( $e->getTrace() as $i => $trace ) : ?>
			<tr class="<?php echo $i % 2 == 0 ? 'even' : 'odd'; ?>">
				<td><?php echo isset($trace[ 'file' ]) ? basename($trace[ 'file' ]) : ''; ?></td>
				<td><?php echo isset($trace[ 'line' ]) ? $trace[ 'line' ] : ''; ?></td>
				<td><?php echo isset($trace[ 'class' ]) ? $trace[ 'class' ] : ''; ?></td>
				<td><?php echo isset($trace[ 'function' ]) ? $trace[ 'function' ] : ''; ?></td>
				<td>
					<?php if( isset($trace[ 'args' ]) ) : ?>
						<?php foreach ( $trace[ 'args' ] as $i => $arg ) : ?>
							<span title="<?php echo var_export( $arg, true ); ?>"><?php echo gettype( $arg ); ?></span>
							<?php echo $i < count( $trace['args'] ) -1 ? ',' : ''; ?> 
						<?php endforeach;?>
					<?php else:?>
					NULL
					<?php endif;?>
				</td>
			</tr>
		<?php endforeach;?>
		</tbody>
	</table>
	<?php else:?>
		<pre><?php echo $e->getTraceAsString();?></pre>
	<?php endif;?>
	<center class="foot">Gene 1.21</center>
</div>
</body>
</html>
<?php die;?>