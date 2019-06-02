<blockquote class="layui-elem-quote">
    <p>欢迎使用微服务管理后台！</p>
    <p>登录次数：<?php echo $loginInfo['count']?> 上次登录IP：<?php echo $loginInfo['top']['log_ip'];?>  上次登录时间： <?php echo date("Y-m-d H:i:s", $loginInfo['top']['addtime']);?></p>
</blockquote>
<fieldset class="layui-elem-field layui-field-title site-title">
  <legend><a name="default">信息统计</a></legend>
</fieldset>
<table class="layui-table">
	<thead>
		<tr>
			<th></th>
			<th>项目</th>
			<th>文档</th>
			<th>管理员</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>总数</td>
			<td>0</td>
			<td>0</td>
			<td>0</td>
		</tr>
		<tr>
			<td>今日</td>
			<td>0</td>
			<td>0</td>
			<td>0</td>
		</tr>
		<tr>
			<td>本周</td>
			<td>0</td>
			<td>0</td>
			<td>0</td>
		</tr>
		<tr>
			<td>本月</td>
			<td>0</td>
			<td>0</td>
			<td>0</td>
		</tr>
	</tbody>
</table>
<table class="layui-table">
	<thead>
		<tr>
			<th colspan="2" scope="col">服务器信息</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<th width="30%">服务器软件</th>
			<td><span id="lbServerName"><?php echo $_SERVER['SERVER_SOFTWARE']?></span></td>
		</tr>
		<tr>
			<td>服务器IP地址</td>
			<td><?php echo $_SERVER['SERVER_ADDR']?></td>
		</tr>
		<tr>
			<td>服务器域名</td>
			<td><?php echo $_SERVER['HTTP_HOST']?></td>
		</tr>
		<tr>
			<td>服务器端口 </td>
			<td><?php echo $_SERVER['SERVER_PORT']?></td>
		</tr>
		<tr>
			<td>服务器当前时间 </td>
			<td><?php echo date("Y-m-d H:i:s", time());?></td>
		</tr>
		<tr>
			<td>开发框架</td>
			<td>Gene V<?php echo gene_version();?></td>
		</tr>
	</tbody>
</table>