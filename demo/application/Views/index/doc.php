<div class="layui-main"  style="padding-top:10px">

<div class="layui-row">
    <div class="layui-card">
        <div class="layui-card-header">
        <span class="layui-breadcrumb">
          <a href="/">首页</a>
          <a><cite>文档</cite></a>
        </span>
        </div>

    </div> 
</div>
<div class="layui-row" style="margin-top:10px;">
<div class="layui-col-md3">
  <div class="layui-card">
    <div class="layui-card-header">使用帮助</div>
    <div class="layui-card-body">
    <ul>
    <?php if(isset($this->help[0])):?>
    <?php foreach($this->help as $v):?>
      <li role="presentation" <?php if($id==$v['mark_id']):?>class="active"<?php endif;?>><a href="/doc/<?php echo $v['mark_id'];?>.html"><?php echo $v['mark_title'];?></a></li>
    <?php endforeach;?>
    <?php endif;?>
    </ul>
    </div>
  </div>
  
  <div class="layui-card">
    <div class="layui-card-header">框架类文档</div>
    <div class="layui-card-body">
    <ul>
    <?php if(isset($this->doc[0])):?>
    <?php foreach($this->doc as $v):?>
      <li role="presentation" <?php if($id==$v['mark_id']):?>class="active"<?php endif;?>><a href="/doc/<?php echo $v['mark_id'];?>.html"><?php echo $v['mark_title'];?></a></li>
    <?php endforeach;?>
    <?php endif;?>
    </ul>
    </div>
  </div>
</div>
<div class="layui-col-md9">
  <div class="layui-card" style="margin-left:10px;">
    <div class="layui-card-header"><?php if(!$id):?>正文<?php else:?><?php echo $this->mark['mark_title'];?><?php endif;?></div>
    <div class="layui-card-body">
<?php if(!$id):?>
<p style="text-align: left;">
<strong>安装：</strong><br>
<br>
phpize<br>
./configure --enable-gene=shared<br>
make<br>
make install<br>
<br>
<strong>DEMO：</strong><br>
<br>
index.php 启动文件<br>
config.ini.php 配置文件<br>
router.inc.php 路由文件<br>
<br>
<strong>测试：</strong><br>
<br>
官方网站：<br>
    http://php-gene.com/<br>
可测试路由实例：<br>
http://php-gene.com/demo/admin<br>
http://php-gene.com/demo/admin.html<br>
http://php-gene.com/demo/admin/demo.jpg<br>
http://php-gene.com/demo/admin/ajax.js<br>
http://php-gene.com/demo/admin/blog/test/baidu<br>
</p>
<?php endif;?>
<?php echo $this->text;?>
    </div>
  </div>
</div>
</div>
</div>