<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title><?php echo $this->title;?> - 微服务管理平台</title>
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
<body>
<div style="padding: 15px;">
    <div class="layui-row">
        <span class="layui-breadcrumb">
            <?php $path = \Services\Admin\Module::getInstance()->path; ?>
            <a><cite>首页</cite></a>
            <?php $i = 0;foreach($path as $v): ?>
            <a><cite><?php echo $v['title']?></cite></a>
            <?php $i++;?>
            <?php endforeach;?>
        </span>
    </div>
    <div class="layui-row" style="margin-top:15px;">
    <?php require $this::contains();?>
    </div>
</div>
</body>
</html>