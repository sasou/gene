<div class="layui-row">
    <div class="layui-inline">
        <form class="layui-form" action="" lay-filter="searchForm">
            角色分类
            <div class="layui-input-inline" style="width: 150px;">
              <select name="role" lay-verify="">
                <option value=""></option>
                <?php foreach($this->group['list'] as $one): ?>
                <option value="<?php echo $one['group_id']?>"><?php echo $one['group_title']?></option>
                <?php endforeach;?>
              </select>
            </div>
            用户名
            <div class="layui-input-inline" style="width: 150px;">
              <input type="text" name="name" autocomplete="off" class="layui-input">
            </div>
            <button class="layui-btn"  lay-submit="" lay-filter="sreach"><i class="layui-icon">&#xe615;</i></button>
        </form>
    </div>
	<div class="layui-btn-group" style="float:right;">
        <button class="layui-btn add-btn layui-btn-sm" data-url="/user/add.html"><i class="layui-icon"></i> 增加用户</button>
        <button class="layui-btn layui-btn-sm layui-btn-danger del-all" data-url="/user/delAll"><i class="layui-icon">&#xe640;</i> 批量删除</button>
	</div>
</div>
<div class="layui-row">
<table class="layui-table">
    <thead>
        <tr>
            <th>
                <div class="layui-unselect header layui-form-checkbox" lay-skin="primary"><i class="layui-icon"></i></div>
            </th>
            <th>用户组</th>
            <th>用户名</th>
            <th>真实姓名</th>
            <th>状态</th>
            <th>操作</th>
        </tr>
    </thead>
    <tbody>
        <?php foreach($this->userlist['list'] as $one): ?>
        <tr>
            <td>
                <div class="layui-unselect layui-form-checkbox" lay-skin="primary" data-id="<?php echo $one['user_id']; ?>"><i class="layui-icon"></i></div>
            </td>
            <td><?php echo \Services\Admin\Group::getInstance()->getField($one['group_id']); ?></td>
            <td><?php echo $one['user_name']; ?></td>
            <td><?php echo $one['user_realname']?></td>
            <td>
            <div class="layui-table-cell laytable-cell-1-lock"><div class="layui-unselect layui-form-checkbox <?php if($one['status']): ?>layui-form-checked<?php endif;?> status-btn" data-url="/user/status/<?php echo $one['user_id']; ?>"><span>启用</span><i class="layui-icon  layui-icon-ok"></i></div> </div>
            </td>
            <td class="td-manage">
                <div class="layui-inline mine-nowrap">
                    <button class="layui-btn layui-btn-xs layui-btn-normal edit-btn" data-url="/user/edit/<?php echo $one['user_id']; ?>.html">
                        <i class="layui-icon">&#xe642;</i>
                    </button>
                    <button class="layui-btn layui-btn-xs layui-btn-danger del-btn" data-url="/user/del/<?php echo $one['user_id']; ?>">
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
iniPage(<?php echo $this->page;?>, <?php echo $this->userlist['count'];?>, <?php echo $this->limit;?>);
initForm('searchForm', {
    "role": "<?php echo $this->search['role'];?>",
    "name": "<?php echo $this->search['name'];?>",
})
</script>