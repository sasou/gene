<form class="layui-form" action="" lay-filter="geneForm">
    <input type="hidden" name="id" value="<?php echo $this->mark['mark_id'];?>" >
  <div class="layui-form-item">
    <label class="layui-form-label">复选框</label>
    <div class="layui-input-block">
      <input type="radio" name="data[mark_type]" value="1" title="框架使用">
      <input type="radio" name="data[mark_type]" value="2"title="框架类文档">
      <input type="radio" name="data[mark_type]" value="3" title="技术研究">
    </div>
  </div>
  <div class="layui-form-item">
    <label class="layui-form-label">文档名称</label>
    <div class="layui-input-block">
      <input type="text" name="data[mark_title]" value="<?php echo $this->mark['mark_title'];?>" lay-verify="required" placeholder="请输入文档名称" autocomplete="off" class="layui-input">
    </div>
  </div>

        <div class="layui-form-item layui-form-text">
        <label class="layui-form-label">文档内容</label>
        <div class="layui-input-block">
          <textarea name="data[app_description]" style="height:300px;" placeholder="请输入文档内容" lay-verify="required" class="layui-textarea"><?php echo $this->mark['app_description'];?></textarea>
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
      <button class="layui-btn" lay-submit lay-filter="geneForm" data-url="/mark/editPost">立即提交</button>
      <button type="reset" class="layui-btn layui-btn-primary">重置</button>
    </div>
  </div>
</form>
<script>
initForm('geneForm', {
    "data[mark_type]": "<?php echo $this->mark['mark_type'];?>",
    "data[status]": <?php echo $this->mark['status'];?>
});
</script>