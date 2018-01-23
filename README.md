# php-gene-for-php7

    Simple, high performance,C extension framework for php7！
    版本：2.0.1  
    

php5的版本 ：https://github.com/sasou/php-gene

windows版本：https://github.com/sasou/php-gene-for-windows

简单、高性能的php c扩展框架！
框架的核心是gene_application类，加载配置文件并启动：

    简单运行：
	<?php
	$app = new \Gene\Aplication();
	$app->load("router.ini.php")
	    ->load("config.ini.php")
	    ->run();

框架的基础是一个高性能的进程缓存模块，基于缓存模块，实现了一个高性能的强大路由解析以及配置缓存；
路由强大灵活，支持回调、rest、http请求方式（get,post,put,patch,delete,trace,connect,options,head）等：

	<?php
	$router = new \Gene\Router();
	$router->clear()
		//定义get
		->get("/",function(){
				echo "index get";
			})
		//定义post
		->post("/",function(){
				echo "index post";
			})	
		//分组模式
		->group("/admin")
			->get("/:name/",function($params){
				var_dump($params);
			})
			->get("/:name.:ext",function($params){
				var_dump($params);
			})
			->get("/:name/sasoud",function($params){
				var_dump($params);
			})
			->get("/blog/:ext/baidu",function($params){
				echo 'baidu';
				return array('return'=>1);
			},"auth@clearAll")
		->group()
		
		->get("/index",function(){
			echo 'index';
		})
		//定义404
		->error(404,function(){
			echo " 404 ";
		})
		//定义自定义钩子
		->hook("auth",function(){
			echo " auth hook ";
		})
		//全局前置钩子
		->hook("before", function(){
			echo " before hook ";
		})
		//全局后置钩子
		->hook("after", function($params){
			echo " after hook ";
			if(is_array($params))var_dump($params);
		});

配置类支持bool、int、long、string、array、常量等数据类型：

	<?php
	$config = new \Gene\Config();
	$config->clear();
	$config>set("dsfsdfsd",array('_url'=>array('sd'=>'sdfsdf222','sds'=>'sdfsf678'),'port'=>3307));
	支持快捷存与取（.分隔）：
	$config->set("dsfsdfsd.port",'test');
	$config->get("dsfsdfsd.port");
	

其他类：\Gene\Controller、\Gene\View、\Gene\Request、\Gene\Response、\Gene\Session、\Gene\Reg、\Gene\Load、\Gene\Exception等，详见文档；
	
安装：
	
	phpize
	./configure --enable-gene=shared
	make
	make install
	
DEMO：
	
	index.php 启动文件
	config.ini.php 配置文件
	router.inc.php 路由文件
	
测试：

	测试网站：http://php-gene.com/
    
	测试路由示例：
	http://php-gene.com/demo/admin
	http://php-gene.com/demo/admin.html
	http://php-gene.com/demo/admin/demo.jpg
	http://php-gene.com/demo/admin/ajax.js
	http://php-gene.com/demo/admin/blog/test/baidu

案例一：
        湖北省教育用户认证中心(全省几百万学生、教育用户的登录入口)
        http://open.e21.cn/
        
案例二：
        尚动电子商务平台
        https://www.shangsports.com/
