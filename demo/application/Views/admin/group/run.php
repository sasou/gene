<div class="layui-row">
	<div class="layui-btn-group">
        <button class="layui-btn add-btn layui-btn-sm" data-url="/group/add.html"><i class="layui-icon"></i> 增加角色</button>
        <button class="layui-btn layui-btn-sm layui-btn-danger del-all" data-url="/group/delAll"><i class="layui-icon">&#xe640;</i> 批量删除</button>
	</div>
</div>

<div class="layui-row">
<table class="layui-table">
    <thead>
        <tr>
            <th>
                <div class="layui-unselect header layui-form-checkbox" lay-skin="primary"><i class="layui-icon"></i></div>
            </th>
            <th>ID</th>
            <th>角色名称</th>
            <th>角色描述</th>
            <th>状态</th>
            <th>操作</th>
        </tr>
    </thead>
    <tbody>
        <?php foreach($group['list'] as $one): ?>
        <tr>
            <td>
                <div class="layui-unselect layui-form-checkbox" lay-skin="primary" data-id="<?php echo $one['group_id']; ?>"><i class="layui-icon"></i></div>
            </td>
            <td><?php echo $one['group_id']; ?></td>
            <td><?php echo $one['group_title']; ?></td>
            <td><i class="layui-icon" style="top: 3px;"><?php echo $one['group_description']?></i></td>
            <td>
            <div class="layui-table-cell laytable-cell-1-lock"><div class="layui-unselect layui-form-checkbox <?php if($one['status']): ?>layui-form-checked<?php endif;?> status-btn" data-url="/group/status/<?php echo $one['group_id']; ?>"><span>启用</span><i class="layui-icon  layui-icon-ok"></i></div> </div>
            </td>
            <td class="td-manage">
                <div class="layui-inline mine-nowrap">
                    <button class="layui-btn layui-btn-xs layui-btn-normal edit-btn" data-url="/group/edit/<?php echo $one['group_id']; ?>.html">
                        <i class="layui-icon">&#xe642;</i>
                    </button>
                    <button class="layui-btn layui-btn-xs layui-btn-danger del-btn" data-url="/group/del/<?php echo $one['group_id']; ?>">
                        <i class="layui-icon">&#xe640;</i>
                    </button>
                </div>
            </td>
        </tr>
        <?php endforeach;?>
    </tbody>
</table>
</div>
<div id="page"></div>
<script>
iniPage(<?php echo $page;?>, <?php echo $group['count'];?>);
</script>