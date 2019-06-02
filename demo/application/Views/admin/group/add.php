<form class="layui-form" action="">
    <div class="layui-tab">
      <ul class="layui-tab-title">
        <li class="layui-this">基本设置</li>
        <li>权限分配</li>
      </ul>
      <div class="layui-tab-content">
        <div class="layui-tab-item layui-show">
  <div class="layui-form-item">
    <label class="layui-form-label">角色名称</label>
    <div class="layui-input-block">
      <input type="text" name="data[group_title]" required  lay-verify="required" placeholder="请输入名称" autocomplete="off" class="layui-input">
    </div>
  </div>
  <div class="layui-form-item layui-form-text">
    <label class="layui-form-label">角色描述</label>
    <div class="layui-input-block">
      <textarea name="data[group_description]" placeholder="请输入角色描述" class="layui-textarea"></textarea>
    </div>
  </div>
  <div class="layui-form-item">
    <label class="layui-form-label">状态</label>
    <div class="layui-input-block">
      <input type="checkbox" name="data[status]" lay-skin="switch">
    </div>
  </div>
        </div>
        <div class="layui-tab-item">
            <table class="layui-table">
                <tbody>
                <?php if(is_array($purviewList)):?>
                <?php foreach($purviewList as $v):?>
                    <tr>
                        <td>
                            <?php echo str_repeat("&nbsp;", $v['deep'] * 8);?><input type="checkbox" name="purview[<?php echo $v['id'];?>]" lay-skin="switch" lay-text="<?php echo $v['name'];?>|<?php echo $v['name'];?>" value="<?php echo $v['id'];?>">
                        </td>
                        <td>
                            <?php if(isset($v['purview'][0])):?>
                            <?php foreach($v['purview'] as $p):?>
                              <input type="checkbox" name="purview[<?php echo $p['id'];?>]" lay-skin="switch" lay-text="<?php echo $p['name'];?>|<?php echo $p['name'];?>" value="<?php echo $p['id'];?>">
                            <?php endforeach;?>
                            <?php endif;?>
                        </td>
                    </tr>
                <?php endforeach;?>
                <?php endif;?>
                </tbody>
            </table>
        </div>
      </div>
    </div>
 
  <div class="layui-form-item">
    <div class="layui-input-block">
      <button class="layui-btn" lay-submit lay-filter="geneForm" data-url="/group/addPost">立即提交</button>
      <button type="reset" class="layui-btn layui-btn-primary">重置</button>
    </div>
  </div>
</form>