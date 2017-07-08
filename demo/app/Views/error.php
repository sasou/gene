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
<meta charset='UTF-8'>
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
	<center><h1>Uncaught {:get_class($e)}</h1></center>
	<h2>{:$e->getMessage()}</h2>
	<p>An uncaught {:get_class($e)} was thrown on line {$line} of file {:basename( $file )} that prevented further execution of this request.</p>
	<h2>Where it happened:</h2>
	{if isset($lines)}
	<code>File:***{:substr($file,-(ceil(strlen($file)*0.6)))} Line:{:$line}</code>
	<ul class="code">
		{for $i=$line-3;$i<$line+3;$i++}
			{if $i>0 && $i<count($lines)}
				{if $i == $line-1}
					<li class="line">{:htmlspecialchars(str_replace( "\n", "", $lines[$i] ), ENT_COMPAT)}</li>
				{else}
					<li>{:htmlspecialchars(str_replace( "\n", "", $lines[$i] ))}</li>
				{/if}
			{/if}
		{/for}
	</ul>
	{/if}
	{if is_array( $e->getTrace() )}
	<h2>Stack trace:</h2>
	<table class="trace">
		<thead><tr><td>File</td><td>Line</td><td>Class</td><td>Function</td><td>Arguments</td></tr></thead>
		<tbody>
		{loop $e->getTrace() $i $trace}
			{if isset($trace[ 'class' ])}
			<tr class="{:$i % 2 == 0 ? 'even' : 'odd'}">
				<td>{:isset($trace[ 'file' ]) ? basename($trace[ 'file' ]) : ''}</td>
				<td>{:isset($trace[ 'line' ]) ? $trace[ 'line' ] : ''}</td>
				<td>{:isset($trace[ 'class' ]) ? $trace[ 'class' ] : ''}</td>
				<td>{:isset($trace[ 'function' ]) ? $trace[ 'function' ] : ''}</td>
				<td>
					{if isset($trace['args'])}
						{loop $trace['args'] $i $arg}
							<span title="{:var_export( $arg, true )}">{:gettype( $arg )}</span>
							{:$i < count( $trace['args'] ) -1 ? ',' : ''}
						{/loop}
					{else}
					NULL
					{/if}
				</td>
			</tr>
			{/if}
		{/loop}
		</tbody>
	</table>
	{else}
		<pre>{:$e->getTraceAsString()}</pre>
	{/if}
	<center class="foot">Gene {:gene_version()}</center>
</div>
</body>
</html>