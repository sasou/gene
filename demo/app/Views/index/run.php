<br><br>
变量输出：<br>
{$test}

<br><br>
if:<br>
{if 1>0}
{$test}<br>
{/if}

<br><br>
loop:<br>
{loop $arr $v}
{$v}<br>
{/loop}

<br><br>
for:<br>
{for $i=0;$i<2;$i++}
{$i}<br>
{/for}

<br><br>
常量输出：<br>
{APP_ROOT}

<br><br>
eval输出：<br>
{eval echo 111}<br>

<br><br>
:输出：<br>
{:count($arr)}<br>

<br><br>
~输出：<br>
{~var_dump($arr)}<br>