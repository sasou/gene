<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title><?php echo $this->title;?> - 文档管理平台</title>
  <meta name="renderer" content="webkit">
  <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <meta name="apple-mobile-web-app-status-bar-style" content="black"> 
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="format-detection" content="telephone=no">
  
  <link rel="stylesheet" href="/static/layui/css/layui.css"  media="all">
  <link rel="stylesheet" href="/static/css/admin.css"  media="all">
  <script src="/static/layui/layui.js" charset="utf-8"></script>
  <script src="/static/js/admin.js" charset="utf-8"></script>
</head>
<body class="layui-layout-body">
<div class="layui-layout layui-layout-admin">
  <div class="layui-header">
    <div class="layui-logo">
        <i class="layui-icon layui-icon-template-1 layui-icon-logo"></i> 文档管理平台
    </div>
    <ul class="layui-nav layui-layout-left">
      <li class="layui-nav-item"><a href="/admin.html"><i class="layui-icon layui-icon-console" style="top: 3px;"> 控制台</i></a></li>
    </ul>
    <ul class="layui-nav layui-layout-right">
      <li class="layui-nav-item">
        <a href="javascript:;">
          <img src="http://t.cn/RCzsdCq" class="layui-nav-img">
          <?php echo $this->user['user_name'];?> [<?php echo $this->user['group_title'];?>]
        </a>
        <dl class="layui-nav-child">
          <dd><a href="javascript:void(0);" class="edit-btn" data-url="/set.html">基本资料</a></dd>
        </dl>
      </li>
      <li class="layui-nav-item"><a href="javascript:void();" class="exit-btn" data-url="/exit.action"><i class="layui-icon layui-icon-close" style="color: red;"></i> 退出</a></li>
    </ul>
  </div>
  
  <div class="layui-side layui-bg-black">
    <div class="layui-side-scroll">
		<ul class="layui-nav layui-nav-tree" lay-filter="">
		<?php $path = \Services\Admin\Module::getInstance()->path; ?>
		<?php $menu = \Services\Admin\Module::getInstance()->lists($this->user['purview']); ?>
		<?php foreach($menu as $one): ?>
		<?php if($one['module_pid'] == 0): ?>
			<li class="layui-nav-item <?php if(isset($path[$one['module_id']])):?>layui-nav-itemed<?php endif;?>">
				<a class="" href="javascript:;">
					<i class="layui-icon <?php echo $one['module_icon']?>" style="top: 3px;"> </i><cite><?php echo $one['module_title']?></cite>
				</a>
				<dl class="layui-nav-child">
					<?php foreach($menu as $two): ?>
					<?php if($two['module_pid'] == $one['module_id']): ?>
					<dd class="<?php if(isset($path[$two['module_id']])):?>layui-this<?php endif;?>">
						<a href="/<?php echo $two['module_url']?>">
							&nbsp;&nbsp;&nbsp;&nbsp;<i class="layui-icon <?php echo $two['module_icon']?>"> </i><cite><?php echo $two['module_title']?></cite>
						</a>
					</dd>
					<?php endif;?>
					<?php endforeach;?>
				</dl>
			</li>
		<?php endif;?>
		<?php endforeach;?>
		</ul>
    </div>
  </div>
  
  <div class="layui-body">
    <div style="padding: 15px;">
        <div class="layui-row">
            <span class="layui-breadcrumb">
                <a href="/"><cite>首页</cite></a>
                <?php $i = 0;foreach($path as $v): ?>
                <a <?php if($i == 1): ?>href="<?php echo $v['url']?>"<?php endif;?>><cite><?php echo $v['title']?></cite></a>
                <?php $i++;?>
                <?php endforeach;?>
            </span>
            <a class="layui-btn layui-btn-xs reload-btn" style="float:right"  href="javascript:;" title="刷新"><i class="layui-icon layui-icon-refresh-1"> </i></a>
        </div>
		<div class="layui-row" style="margin-top:15px;">
        <?php require $this->contains();?>
		</div>
    </div>
  </div>
  
  <div class="layui-footer">
    © Gene V<?php echo gene_version();?>
  </div>
</div>
</body>
</html>