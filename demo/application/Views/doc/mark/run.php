<div class="layui-row">
    <div class="layui-inline">
        <form class="layui-form" action="" lay-filter="searchForm">
            文档名称
            <div class="layui-input-inline" style="width: 150px;">
              <input type="text" name="title" autocomplete="off" class="layui-input">
            </div>
            <button class="layui-btn"  lay-submit="" lay-filter="sreach"><i class="layui-icon">&#xe615;</i></button>
        </form>
    </div>
	<div class="layui-btn-group" style="float:right;">
        <button class="layui-btn add-btn layui-btn-sm" data-url="/mark/add.html"><i class="layui-icon"></i> 增加文档</button>
        <button class="layui-btn layui-btn-sm layui-btn-danger del-all" data-url="/mark/delAll"><i class="layui-icon">&#xe640;</i> 批量删除</button>
	</div>
</div>
<div class="layui-row">
<table class="layui-table">
    <thead>
        <tr>
            <th>
                <div class="layui-unselect header layui-form-checkbox" lay-skin="primary"><i class="layui-icon"></i></div>
            </th>
            <th>类型</th>
            <th>文档名称</th>
            <th>作者</th>

            <th>状态</th>
            <th>操作</th>
        </tr>
    </thead>
    <tbody>
        <?php foreach($mark['list'] as $one): ?>
        <tr>
            <td>
                <div class="layui-unselect layui-form-checkbox" lay-skin="primary" data-id="<?php echo $one['mark_id']; ?>"><i class="layui-icon"></i></div>
            </td>

                <td><?php echo $mark_type[$one['mark_type']];?></td>
                <td><?php echo $one['mark_title']?></td>
                <td><?php echo \Services\Admin\User::getInstance()->getField($one['user_id']);?></td>
            <td>
            <div class="layui-table-cell laytable-cell-1-lock"><div class="layui-unselect layui-form-checkbox <?php if($one['status']): ?>layui-form-checked<?php endif;?> status-btn" data-url="/mark/status/<?php echo $one['mark_id']; ?>"><span>启用</span><i class="layui-icon  layui-icon-ok"></i></div> </div>
            </td>
            <td class="td-manage">
                <div class="layui-inline mine-nowrap">
                    <button class="layui-btn layui-btn-xs layui-btn-normal edit-btn" data-url="/mark/edit/<?php echo $one['mark_id']; ?>.html">
                        <i class="layui-icon">&#xe642;</i>
                    </button>
                    <button class="layui-btn layui-btn-xs layui-btn-danger del-btn" data-url="/mark/del/<?php echo $one['mark_id']; ?>">
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
iniPage(<?php echo $page;?>, <?php echo $mark['count'];?>, <?php echo $limit;?>);
initForm('searchForm', {
    "title": "<?php echo $search['title'];?>",

})
</script>