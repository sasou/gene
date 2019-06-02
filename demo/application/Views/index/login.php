<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title><?php echo $title;?> - 微服务管理平台</title>
  <meta name="renderer" content="webkit">
  <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <meta name="apple-mobile-web-app-status-bar-style" content="black"> 
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="format-detection" content="telephone=no">
  
  <link rel="stylesheet" href="/static/layui/css/layui.css"  media="all">
  <link rel="stylesheet" href="/static/css/sign.css"  media="all">
  <script src="/static/layui/layui.js" charset="utf-8"></script>
  <script src="/static/js/login.js" charset="utf-8"></script>
</head>
<body class="layui-unselect lau-sign-body">

<div class="layui-form layui-form-pane lau-sign-form">
    <h1 class="lau-sign-title">SIGN IN SYSTEM</h1>
    <p class="lau-sign-subtitle">微服务管理平台</p>
    <div class="layui-form-item">
        <label class="layui-form-label"><i class="layui-icon layui-icon-username"></i> 账　号</label>
        <div class="layui-input-block">
            <input type="text" name="username" placeholder="请输入用户名" autocomplete="off" class="layui-input">
        </div>
    </div>
    <div class="layui-form-item">
        <label class="layui-form-label"><i class="layui-icon layui-icon-password"></i> 密　码</label>
        <div class="layui-input-block">
            <input type="password" name="password" placeholder="请输入密码" autocomplete="off" class="layui-input">
        </div>
    </div>
    <div class="layui-form-item">
        <label class="layui-form-label"><i class="layui-icon layui-icon-vercode"></i> 验证码</label>
        <div class="layui-input-block lau-sign-code">
            <input type="text" name="captcha" placeholder="请输入图形验证码" autocomplete="off" class="layui-input">
            <img src="/captcha.action" alt="图形验证码" class="lau-sign-captcha">
        </div>
    </div>
    <div class="layui-form-item">
        <input type="checkbox" name="remember" lay-skin="primary" title="记住密码">
        <a class="lau-sign-forgot lau-sign-link" href="/forgot.html">忘记密码？</a>
    </div>
    <div class="layui-form-item">
        <button type="button" class="layui-btn layui-btn-fluid" lay-submit lay-filter="login">登 入</button>
    </div>
    <div class="layui-form-item lau-sign-other">

    </div>
</div>
<div class="layui-trans lau-sign-footer">
    <p>@ 2018 <a href="http://www.php-gene.com/" target="_blank">Gene</a> V2.2</p>
</div>
</body>
</html>