<div class="layui-row">
    <div class="layui-inline">
        <form class="layui-form" action="" lay-filter="searchForm">
            URL
            <div class="layui-input-inline" style="width: 150px;">
              <input type="text" name="url" autocomplete="off" class="layui-input">
            </div>
            IP
            <div class="layui-input-inline" style="width: 150px;">
              <input type="text" name="ip" autocomplete="off" class="layui-input">
            </div>
            <button class="layui-btn"  lay-submit="" lay-filter="sreach"><i class="layui-icon">&#xe615;</i></button>
        </form>
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
            <th>操作</th>
            <th>操作数据</th>
            <th>URL</th>
            <th>IP</th>
            <th>区域</th>
            <th>用户</th>
        </tr>
    </thead>
    <tbody>
        <?php foreach($log['list'] as $one): ?>
        <tr>
            <td>
                <div class="layui-unselect layui-form-checkbox" lay-skin="primary" data-id="<?php echo $one['log_id']; ?>"><i class="layui-icon"></i></div>
            </td>
            <td><?php echo $one['log_id']; ?></td>
            <td><?php echo $one['log_title']; ?></td>
            <td><?php echo $one['log_data']?></td>
            <td><?php echo $one['log_url']?></td>
            <td><?php echo $one['log_ip']?></td>
            <td><?php echo $one['log_ip_area']?></td>
            <td><?php echo \Services\Admin\User::getInstance()->getField($one['user_id']);?></td>
        </tr>
        <?php endforeach;?>
    </tbody>
</table>
</div>
<div id="page"></div>
<script>
iniPage(<?php echo $page;?>, <?php echo $log['count'];?>, <?php echo $limit;?>);
initForm('searchForm', {
    "url": "<?php echo $search['url'];?>",
    "ip": "<?php echo $search['ip'];?>",
})
</script>