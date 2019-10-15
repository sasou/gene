# gene for php7

    Grace, fastest, flexibility, simple PHP extension framework！优雅、极速、灵活、简单的PHP扩展框架！     

>   最新版本：3.0.5    
>   官方网站：Gene框架 http://www.php-gene.com/

框架核心特性：
* 优雅：优雅微架构，提供松耦合的、有一定的有界上下文的面向服务架构，按需组合，适应DDD领域驱动设计；
* 简单：一分钟demo入门，优雅而简单；
* 极速：PHP-FPM模式下运行速度最快的框架；
* 场景：胜任WEB应用、微服务、常驻进程等场景；
* 灵活：支持传统MVC模式；同时支持MVCS模式：瘦MC模式，通过增加service层更好的实现模块封装；
* 路由：完整支持HTTP REST请求；底层采用二叉树查找算法，性能强劲；
* 钩子：路由支持全局前置、后置钩子；支持自定义前置钩子；
* 依赖注入：参考 Java Spring 的 Bean 设计思想，实现了简易好用的IoC；支持全局注入，局部控制反转等；
* 中间件：AOP (面向切面编程)，配置文件注册对象，调用方便且解耦；
* 工厂：提供工厂单例支持；
* 数据库：封装高性能PDO ORM类库；
* 配置：配置缓存到进程，修改自动更新；
* 视图：方式一、使用编译模板引擎，支持模板标签，支持模板缓存；方式二、使用原生PHP做模板引擎；两种方式均支持布局、属性赋值、对象引用等；
* 长连接：按进程保持的长连接，支持Mysql/Redis/Memcached；持久连接断开自动检测；
* 命令行：封装了命令行开发基础设施，可快速开发控制台程序、守护进程；
* 缓存：支持两种缓存，一是方法级定时缓存；二是实时版本缓存（创新功能：高效的实时缓存方案，轻松解决复杂缓存的更新，比如分页数据的缓存）； 
* 自动加载：基于 PSR-4；
* 完美支持swoole（低内存占用，无内存泄露）；
* 其他：redis、memcached类库二次封装；

## 简单应用（一分钟入门）  
### 第一步：应用入口index.php

   加载配置文件并启动：
   
    <?php
    $app = \Gene\Application::getInstance();
    $app
      ->load("router.ini.php")
      ->load("config.ini.php")
      ->run();  

### 第二步：路由文件router.ini.php   

   配置REST路由：支持指定类方法或者回调闭包函数；   
   
    <?php
    $router = new \Gene\Router();
    $router->clear()
    
    //定义get
    ->get("/", "\Controllers\Index@run")
    
    ->get("/test", "\Controllers\Index@test", "@clearAll")
    
    //定义post
    ->post("/",function(){
            echo "index post";
    })    
        
    //分组模式
    ->group("/admin")
        ->get("/:name/",function($params){
            var_dump($params);
        })
        ->get("/blog/:ext",function($params){
            var_dump($params);
        },"auth@clearAll")
    ->group()
    
    //定义静态页面
    ->get("/index.html",function(){
        echo 'index';
    }, "@clearAfter")
    
    //定义404
    ->error(404,function(){
        echo " 404 ";
    })
    
    //定义自定义钩子
    ->hook("auth",function(){
        echo " auth hook ";
        return true; // 返回false中断请求
    })
    
    //全局前置钩子
    ->hook("before", function(){
        echo " before hook ";
    })
    
    //全局后置钩子
    ->hook("after", function($params){
        echo " after hook ";
    });

### 第三步：配置文件config.ini.php

    配置应用变量或者对象；
    <?php
    $config = new \Gene\Config();
    $config->clear();
    
    //视图类注入配置
    $config->set("view", [
        'class' => '\Gene\View'
    ]);

    //http请求类注入配置
    $config->set("request", [
        'class' => '\Gene\Request'
    ]);

    //http响应类注入配置
    $config->set("response", [
        'class' => '\Gene\Response'
    ]);
    
    //数据库类注入配置
    $config->set("db", [
        'class' => '\Gene\Db\Mysql',
        'params' => [[
        'dsn' => 'mysql:dbname=gene_web;host=127.0.0.1;port=3306;charset=utf8',
        'username' => 'root',
        'password' => '',
        'options' => [PDO::ATTR_PERSISTENT => true]
            ]],
        'instance' => true
    ]);

    //缓存类注入配置
    $config->set("memcache", [
        'class' => '\Gene\Cache\Memcached',
        'params' => [[
        'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
        'persistent' => true,
            ]],
        'instance' => true
    ]);
    
### 第四步：控制器文件\Controllers\Index:  

    namespace Controllers;
    class Index extends \Gene\Controller
    {
        /**
         * run
         */
        public function run()
        {
            echo 'hello world!';
        }
        
        /**
         * test
         */
        public function test()
        {
            // 模板赋值：配置文件里面配置view，调用$this->view自动注入到当前类空间 
            $this->view->title = "文档";
            // 调用父子模板
            $this->view->display('index', 'common');
        }    
    }

### 第五步：运行：在浏览器输入项目地址，比如：http://localhost/
    
## 快速安装
    
    phpize
    ./configure --enable-gene=shared
    make
    make install
    

## 案例 
    一：湖北省教育用户认证中心(全省几百万学生、教育用户的登录入口) ：http://open.e21.cn/
            
    二：尚动电子商务平台

    三：生材网 https://www.materialw.com/


php5的版本 ：https://github.com/sasou/php-gene （最新版本：2.1.0）

windows版本：https://github.com/sasou/php-gene-for-windows

<a href="https://info.flagcounter.com/AEYx"><img src="https://s11.flagcounter.com/count2/AEYx/bg_FFFFFF/txt_000000/border_CCCCCC/columns_2/maxflags_10/viewers_0/labels_1/pageviews_1/flags_0/percent_0/" alt="Flag Counter" border="0"></a>
