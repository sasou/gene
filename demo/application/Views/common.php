<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>Gene <?php echo $this->title;?></title>
  <meta name="renderer" content="webkit">
  <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <meta name="apple-mobile-web-app-status-bar-style" content="black"> 
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="format-detection" content="telephone=no">
  
  <link rel="stylesheet" href="/static/layui/css/layui.css"  media="all">
  <link rel="stylesheet" href="/static/web/css/global.css"  media="all">
  <script src="/static/layui/layui.js" charset="utf-8"></script>
  <script src="/static/js/admin.js" charset="utf-8"></script>
</head>
<body class="fly-full">
<div class="fly-header layui-bg-black">
  <div class="layui-container">
    <a class="fly-logo" href="/">
      <img src="/static/web/images/logo.png" alt="layui">
    </a>
    <ul class="layui-nav fly-nav layui-hide-xs">
      <li class="layui-nav-item">
        <a href="/"><i class="layui-icon layui-icon-home"></i>首页</a>
      </li>
      <li class="layui-nav-item layui-this">
        <a href="/doc.html"><i class="layui-icon layui-icon-list"></i>文档</a>
      </li>
      <li class="layui-nav-item">
        <a href="/test.html"><i class="layui-icon layui-icon-engine"></i>性能测试</a>
      </li>
    </ul>
    
    <ul class="layui-nav fly-nav-user">
      <li class="layui-nav-item">
        <a href=""><i class="layui-icon layui-icon-friends"></i></a>
      </li>
      <li class="layui-nav-item">
        <a href="/admin.html">后台管理(开发者账号:develop 演示账号:guest 密码:123456)</a>
      </li>
    </ul>
  </div>
</div>

<?php require $this->contains();?>

<div class="fly-footer">
  <p>Author：Sasou Email：admin#php-gene.com</p>
  <p><a href="#" target="_blank">@2018 Php Gene V<?php echo gene_version();?></a></p>
</div>
<script>
layui.use(['util', 'laydate', 'layer'], function(){
  var util = layui.util
  ,laydate = layui.laydate
  ,layer = layui.layer;
  //固定块
  util.fixbar({
    bar1: false
    ,bar2: true
    ,css: {right: 10, bottom: 20}
    ,bgcolor: '#393D49'
    ,click: function(type){
      if(type === 'bar2') {
        window.location.href = "/doc.html";
      }
    }
  });
 });
</script>
</body>
</html>