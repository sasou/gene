<?php
$router = new \gene\router();
$router->clear()
	->get("/",function (){
            (new \Controllers\Index)->run();
		})
	->group("/admin")
		->get("/",function(){
			echo 'admin';
		},"auth")
		->get("/:name/",function($abc){
			var_dump($abc);
		},"auth@")
		->get("/:name.:ext",function($abc){
			var_dump($abc);
		})
		->get("/:name/sasoud",function(){
			echo 'dd';
		},"name")
		->get("/blog/:ext/baidu",function(){
			echo 'baidu';
			return array('sdfasd'=>'baidu.edu.com');
		},"auth@clearAll")
	->group()
	->get("/index",function(){
		echo 'index';
	},"@clearAll")
	->error(404,function(){echo " 404 "; })
	->error("exception",function($e){
		var_dump($e);
		gene_router::display("error");
	})
	->hook("auth",function(){echo " auth ";})
	->hook("before", function(){
		echo " before ";
		$bbc = ' bbc ';
	})
	->hook("after", function($params){
		echo " after ";
		if(is_array($params))var_dump($params);
	});
	