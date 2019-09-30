<div class="fly-case-header">
  <p class="fly-case-year">Gene</p>
  <a href="#">
    <p class="fly-case-banner layui-text"> Simple, high performance,C extension framework for php！ </p>
  </a>
  <p class="fly-case-version">最新版本：V<?php echo gene_version();?> （May 25, 2019 更新）</p>
  <div class="fly-case-btn">
    <a href="https://github.com/sasou/php-gene-for-php7" target="_blank" class="layui-btn layui-btn-big fly-case-active">获取Linux版本</a>
    <a href="https://github.com/sasou/php-gene-for-windows" target="_blank" class="layui-btn layui-btn-primary layui-btn-big">获取windows版本</a>
    
    <a href="http://pan.baidu.com/s/1nv2tTS9" target="_blank" style="padding: 0 15px;">下载windows集成环境 （提取码：xaqk）</a>
  </div>
</div>
<div class="layui-main">
    <fieldset class="layui-elem-field layui-field-title" style="margin-top: 30px;">
      <legend><span class="layui-badge-lg layui-bg-cyan">灵活</span>
  <span class="layui-badge-lg layui-bg-blue">强大</span>
  <span class="layui-badge-lg layui-bg-black">简单</span>
  <span class="layui-badge-lg layui-bg-green">高效</span>
  的 C 扩展框架。框架核心能力如下：</legend>
    </fieldset>
  <blockquote class="layui-elem-quote">
* 高性能：极简架构，超过yaf、Phalcon等同类型C扩展框架的功能及性能；</br>
* MVCS：瘦MC模式，业务逻辑封装到service；</br>
* 路由：完整支持HTTP REST请求；底层采用二叉树查找算法，性能强劲；</br>
* 钩子：路由支持全局、局部钩子支持；</br>
* 依赖注入：参考 Java Spring 的 Bean 设计思想，实现了简易好用的 IoC；全局注入，局部控制反转等；</br>
* 中间件：AOP (面向切面编程)，配置文件注册对象，调用方便且解耦；</br>
* 工厂：提供全局工厂单例支持；</br>
* 数据库：封装高性能orm类库；</br>
* 配置：配置缓存到进程，修改自动更新；</br>
* 长连接：按进程保持的长连接，支持Mysql/Redis/Memcached；持久连接断开自动检测；</br>
* 视图：方式一、使用编译模板引擎，支持模板标签，支持模板缓存；方式二、使用原生PHP做模板引擎；两种方式均支持布局、属性赋值、对象引用等；</br>
* 命令行：封装了命令行开发基础设施，可快速开发控制台程序、守护进程；</br>
* 缓存：支持两种缓存，一是方法级定时缓存；二是实时版本缓存（创新功能：高效的实时缓存方案，轻松解决复杂缓存的更新，比如分页数据的缓存）； </br>
* 自动加载：基于 PSR-4；</br>
* 完美支持swoole（低内存占用，无内存泄露）；</br>
* 其他：redis、memcached类库二次封装；</br>
  </blockquote>
  <ul class="site-idea">
    <li>
      <fieldset class="layui-elem-field layui-field-title">
        <legend>返璞归真-高性能</legend>
        <p>身处在框架社区的繁荣之下，我们都在有意或无意地追逐。而 Gene 偏偏回望当初，奔赴在返璞归真的漫漫征途，自信并勇敢着，追寻于原生态的书写指令，试图以最底层的方式诠释高效。常驻内存的运行方式，具有传统 Web 框架无法比拟的性能优势，轻松超过 Lavaral、Yii、Ci等。</p>
      </fieldset>
    </li>
    <li>
      <fieldset class="layui-elem-field layui-field-title">
        <legend>双面体验-简单而不简单</legend>
        <p>极简而具有扩展性的架构设计，没有多余的封装，执行更加高效。极简是视觉所见的外在，是开发所念的简易。丰盈是倾情雕琢的内在，是信手拈来的承诺。以极低的学习成本享受到Gene带来的高性能与全新的编程体验，一切本应如此，简而全，双重体验。</p>
      </fieldset>
    </li>
    <li>
      <fieldset class="layui-elem-field layui-field-title">
        <legend>星辰大海-不限应用场景</legend>
        <p>围绕常驻内存的方式而设计，提供了命令行、容器注入、钩子、路由、缓存等开发所需的众多开箱即用的组件特性，加上高性能的优势，特别适合开发高性能 Web 服务、移动互联网 API 服务。命令行开发封装完善，能快速开发各种数据处理类需求，如：统计数据、数据转换等。</p>
      </fieldset>
    </li>
  </ul>
    <div class="layui-bg-cyan" style="padding: 5px 10px;">
<fieldset class="layui-elem-field layui-field-title">
  <legend>版本历史</legend>
</fieldset> 
<ul class="layui-timeline">
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text">
      <div class="layui-timeline-title">2019年9月，Gene 3.0.5 发布。完美支持swoole。。。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-sm">
      <div class="layui-timeline-title">2019年6月，Gene 3.0.2 发布。实时版本缓存，性能提升。。。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text-sm">
      <div class="layui-timeline-title">2018年7月，Gene 2.2.0 发布。陆续被多个大型商业项目使用。。。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text-sm">
      <div class="layui-timeline-title">2017年6月，Gene 里程碑版本 1.2.3 发布，完善了框架的基本元素。。。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text-sm">
      <div class="layui-timeline-title">2016年5月，Gene 首个版本 1.0 发布。应用到线上重要的项目。。。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text-sm">
      <div class="layui-timeline-title">2016年3月，Gene 孵化。</div>
    </div>
  </li>
  <li class="layui-timeline-item">
    <i class="layui-icon layui-anim layui-anim-rotate layui-anim-loop layui-timeline-axis"></i>
    <div class="layui-timeline-content layui-text-sm">
      <div class="layui-timeline-title">许久前的轮子时代，使用了Colaphp、Ci、Thinkphp、Lavaral、Yaf等框架。</div>
    </div>
  </li>
</ul>
        </div>
</div>