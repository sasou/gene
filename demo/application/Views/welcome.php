<div class="gene-welcome-banner">
    <div class="wb-title">欢迎使用微服务管理后台</div>
    <div class="wb-info">
        <span><i class="layui-icon layui-icon-log"></i> 登录次数：<?php echo $this->loginInfo['count']?></span>
        <span><i class="layui-icon layui-icon-location"></i> 上次登录IP：<?php echo $this->loginInfo['top']['log_ip'];?></span>
        <span><i class="layui-icon layui-icon-date"></i> 上次登录时间：<?php echo date("Y-m-d H:i:s", $this->loginInfo['top']['addtime']);?></span>
    </div>
</div>
<fieldset class="layui-elem-field layui-field-title site-title">
  <legend><a name="default">信息统计</a></legend>
</fieldset>
<div class="layui-row layui-col-space15 gene-stat-row">
    <div class="layui-col-md4 layui-col-sm4">
        <div class="gene-stat-card">
            <i class="layui-icon layui-icon-app stat-icon"></i>
            <div class="stat-num">0</div>
            <div class="stat-name">项目</div>
            <div class="stat-sub">今日 0 · 本周 0 · 本月 0</div>
        </div>
    </div>
    <div class="layui-col-md4 layui-col-sm4">
        <div class="gene-stat-card">
            <i class="layui-icon layui-icon-file stat-icon" style="color:#06b6d4;"></i>
            <div class="stat-num">0</div>
            <div class="stat-name">文档</div>
            <div class="stat-sub">今日 0 · 本周 0 · 本月 0</div>
        </div>
    </div>
    <div class="layui-col-md4 layui-col-sm4">
        <div class="gene-stat-card">
            <i class="layui-icon layui-icon-user stat-icon" style="color:#10b981;"></i>
            <div class="stat-num">0</div>
            <div class="stat-name">管理员</div>
            <div class="stat-sub">今日 0 · 本周 0 · 本月 0</div>
        </div>
    </div>
</div>
<div class="gene-info-card">
    <div class="info-head"><i class="layui-icon layui-icon-set-sm"></i>服务器信息</div>
    <table class="layui-table">
    <tbody>
        <tr>
            <th>服务器软件</th>
            <td><span id="lbServerName"><?php echo $this->request->server('SERVER_SOFTWARE')?></span></td>
        </tr>
        <tr>
            <td>服务器IP地址</td>
            <td><?php echo $this->request->server('SERVER_ADDR')?></td>
        </tr>
        <tr>
            <td>服务器域名</td>
            <td><?php echo $this->request->server('HTTP_HOST')?></td>
        </tr>
        <tr>
            <td>服务器端口 </td>
            <td><?php echo $this->request->server('SERVER_PORT')?></td>
        </tr>
        <tr>
            <td>服务器当前时间 </td>
            <td><?php echo date("Y-m-d H:i:s", time());?></td>
        </tr>
        <tr>
            <td>开发框架</td>
            <td>Gene V<?php echo gene_version();?></td>
        </tr>
    </tbody>
    </table>
</div>
