<?php
namespace Controllers;
class Index extends \gene\Controller{
	function run(){
		echo ' index run ';
		$test = ' hello ';
		$arr = array("third");
		$this->displayExt("index/run","common");
	}
}
