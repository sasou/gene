# php-gene-for-php7

    Simple, high performance,C extension framework for php7！
	
    版本：2.2.0  
    

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

框架的基础是一个高性能的进程级缓存模块，基于缓存模块，实现了一个高性能的强大路由解析以及配置缓存；
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
	
数据库支持orm操作：

    <?php
    $config = array (
            'dsn' => 'mysql:dbname=test;host=127.0.0.1;port=3306;charset=utf8',
            'username' => 'root',
            'password' => '',
            'options' => array(PDO::ATTR_PERSISTENT => true)
    );
    
	$abc = new Gene\db($config);
    // 查询全部
	var_dump($abc->select('user', ["id", "name","time"])->order("id desc")->limit(0, 1)->all());
    // 原生查询
	var_dump($abc->select('user', ["id", "name","time"])->limit(0, 1)->execute()->fetch());
    // 条件查询
	var_dump($abc->select('user', ["id", "name","time"])->where("id=:id and name=:name",[":name"=>"wuya1", ":id"=>5])->row());
    // 简单插入
	var_dump($abc->insert('user', ["name"=>"wuya","time"=>"2018-12-21"])->lastId());
    // 批量插入
	var_dump($abc->batchInsert('user', [["name"=>"wuya1","time"=>"2018-12-21"],["name"=>"wuya2","time"=>"2018-12-22"]])->affectedRows());
    // 简单更新
	var_dump($abc->update('user', ["name"=>"wuya55","time"=>"2018-12-24"])->where("id=?", [4])->affectedRows());
	// 删除操作
	var_dump($abc->delete('user')->where("id=?", 4)->affectedRows());
    // in条件更新
	var_dump($abc->update('user', ["name"=>"wuya55","time"=>"2018-12-24"])->where("id=?", [3])->in(" and id in(?)", [3,4])->affectedRows());
    // in 查询
	var_dump($abc->select('user', ["id", "name","time"])->where("id=?", [3])->in(" and id in(?)", [3,4])->row());
    // sql执行
    var_dump($abc->sql("select * from user where id=?", [3])->in(" and id in(?)", [3,4])->order("id desc")->limit(0, 1)->row());
    
    
其他类：\Gene\Controller、\Gene\Db、\Gene\View、\Gene\Request、\Gene\Response、\Gene\Session、\Gene\Reg、\Gene\Load、\Gene\Exception等，详见文档；
	
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
