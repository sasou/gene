<div class="admin radius">
	<div class="panel">
		<div class="panel-head"><strong>查看文档</strong>
		</div>
		<div class="panel-body">
            <table class="table table-hover bg-back">
                <tr>
                    <th width="80px"></th>
                    <th></th>
                </tr>
                <form class="form-x" action="" onsubmit="return false;" method="post" id="ajaxForm" name="ajaxForm">
            
		<tr>
			<td><label>id</label></td>
			<td><?php echo $mark['mark_id'];?></td>
		</tr>

		<tr>
			<td><label>文档名称</label></td>
			<td><?php echo $mark['mark_title'];?></td>
		</tr>

		<tr>
			<td><label>文档内容</label></td>
			<td><?php echo $mark['app_description'];?></td>
		</tr>

		<tr>
			<td><label>上传者ID</label></td>
			<td><?php echo $mark['user_id'];?></td>
		</tr>

		<tr>
			<td><label>排序</label></td>
			<td><?php echo $mark['order'];?></td>
		</tr>

		<tr>
			<td><label>应用状态：0不可用、1可用</label></td>
			<td><?php echo $mark['status'];?></td>
		</tr>

		<tr>
			<td><label>添加时间</label></td>
			<td><?php echo $mark['addtime'];?></td>
		</tr>

		<tr>
			<td><label>修改时间</label></td>
			<td><?php echo $mark['updatetime'];?></td>
		</tr>

                </form>
            </table>
        </div>
	</div>
</div>