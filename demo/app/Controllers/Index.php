<?php
namespace Controllers;
class Index extends \gene\Controller{
	function run(){
		echo ' index run ';
		$test = ' hello ';
		$arr = array("third");
		$this->displayExt("index/run","common");
	}
	
	public static function error(gene_exception $e){
		\gene\router::display("error"); 
	}
}