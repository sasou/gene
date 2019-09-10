<div class="layui-main"  style="padding-top:10px">

    <div class="layui-row">
      <div class="layui-card">
        <div class="layui-card-header">
        <span class="layui-breadcrumb">
          <a href="/">首页</a>
          <a><cite>性能测试</cite></a>
        </span>
        </div>
        </div>
    </div>

<div class="layui-row" style="margin-top:10px;">
    <div class="layui-col-md12 layui-card" style="padding:10px;">
           <blockquote class="layui-elem-quote"><p style="text-align: left;">
            <strong>测试环境：</strong><br/>
            system：centos 7 CPU：4核 内存：8 GB<br/>
            nginx：1.15.0<br/>
            php：7.1.18<br/>
            gene：V<?php echo gene_version();?></blockquote>
          <div class="layui-row">
            <div class="layui-col-md6">
                  <img src="/static/images/test.png" alt="原生代码测试">
                  <div class="caption">
                    <strong>原生代码测试</strong>
                    <p>ab -n10000 -c100 127.0.0.1/test.php</p>
                    <br/>
                    <?php highlight_file(APP_ROOT . "/../public/static/test/test.php"); ?>
                  </div>
            </div>
            <div class="layui-col-md6">
                  <img src="/static/images/gene.png" alt="Gene框架代码测试">
                  <div class="caption">
                    <strong>Gene框架代码测试</strong>
                    <p>ab -n10000 -c100 127.0.0.1/test</p>
                    <br/>
                    <?php highlight_file(APP_ROOT . "/../public/static/test/gene.php"); ?>
                  </div>
            </div>
          </div>

            <div>
                <strong>测试分析：</strong>
                <br/>
                原生测试就一个文件、一个方法<br/>
                框架测试就至少4个文件：入口文件（index.php）、配置文件（config.ini.php）、路由文件（router.ini.php）、控制器文件（Controller\index.php），执行的方法数量远远超过原生测试,性能却几乎没有什么损耗。<br/><br/>
                    这里选择用最简单的一个方法来对比，充分说明，gene框架尽管封装了不少框架逻辑，但通过比较巧妙的设计，基本没有增加损耗；测试中，因为框架执行的方法数量、复杂度要远远超过简单的echo，所以性能稍有损耗；<br/>
                    <br/>
                <strong>测试代码：</strong>
                <br/>
                    真金不怕火炼，欢迎围观，测试代码下载！<a href="/static/test/demo.zip">demo.zip</a>
                </p>
            </div>
    </div>
</div>
</div>