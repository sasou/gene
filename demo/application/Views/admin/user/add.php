<form class="layui-form" action="" lay-filter="geneForm">
  <div class="layui-form-item">
    <label class="layui-form-label">用户名</label>
    <div class="layui-input-inline">
      <input type="text" name="data[user_name]" required  lay-verify="required" placeholder="请输入名称" autocomplete="off" class="layui-input">
    </div>
  </div>
  <div class="layui-form-item">
    <label class="layui-form-label">所属角色</label>
    <div class="layui-input-inline">
      <select name="data[group_id]" lay-verify="required">
        <option value=""></option>
        <?php foreach($this->group['list'] as $one): ?>
        <option value="<?php echo $one['group_id']?>"><?php echo $one['group_title']?></option>
        <?php endforeach;?>
      </select>
    </div>
  </div>
    <div class="layui-form-item">
      <label for="one_pass" class="layui-form-label">
          <span class="x-red">*</span>密码
      </label>
      <div class="layui-input-inline">
          <input type="password" id="one_pass" name="data[user_pass]" required="" lay-verify="pass"
          autocomplete="off" class="layui-input">
      </div>
      <div class="layui-form-mid layui-word-aux">
          6到16个字符
      </div>
    </div>
    <div class="layui-form-item">
      <label for="L_repass" class="layui-form-label">
          <span class="x-red">*</span>确认密码
      </label>
      <div class="layui-input-inline">
          <input type="password" id="repass" name="repass" required="" lay-verify="repass"
          autocomplete="off" class="layui-input">
      </div>
    </div>
  <div class="layui-form-item layui-form-text">
    <label class="layui-form-label">真实姓名</label>
    <div class="layui-input-inline">
      <input name="data[user_realname]" placeholder="请输入真实姓名" class="layui-input">
    </div>
  </div>
  <div class="layui-form-item">
    <label class="layui-form-label">状态</label>
    <div class="layui-input-block">
      <input type="checkbox" name="data[status]" lay-skin="switch">
    </div>
  </div>
  <div class="layui-form-item">
    <div class="layui-input-block">
      <button class="layui-btn" lay-submit lay-filter="geneForm" data-url="/user/addPost">立即提交</button>
      <button type="reset" class="layui-btn layui-btn-primary">重置</button>
    </div>
  </div>
</form>
<script>
initForm('geneForm', {
    "data[status]": "1",
});

layui.use(['form','layer'], function(){
    $ = layui.jquery;
    var form = layui.form
    ,layer = layui.layer;

    //自定义验证规则
    form.verify({
        nikename: function(value){
            if(value.length < 5){
                return '昵称至少得5个字符啊';
            }
        }
        ,pass: [/(.+){6,12}$/, '密码必须6到12位']
        ,repass: function(value){
            if($('#one_pass').val()!=$('#repass').val()){
                return '两次密码不一致';
            }
        }
    });
});
</script>