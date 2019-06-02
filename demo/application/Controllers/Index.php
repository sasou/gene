<?php
namespace Controllers;
/**
 * 首页控制器
 */
class Index extends \Gene\Controller
{
    /**
     * index
     */
    public function index()
    {
        $this->title = "首页";
        $this->display('index/list', 'common');
    }
    
    /**
     * doc
     */
    public function doc($params)
    {
        $id =  $params['id'] ?? 0;
        $parsedown = new \Ext\Parsedown();
        
        // 通过模板赋值
        $this->view->title = "文档";
        $this->view->css = ['mdediter/md.min'];
        $this->view->js = ['mdediter/md.min'];
        $this->view->help = \Services\Admin\Mark::getInstance()->listAll(1);
        $this->view->doc = \Services\Admin\Mark::getInstance()->listAll(2);
        $this->view->mark = \Services\Admin\Mark::getInstance()->row($id);
        $this->view->text = $parsedown->text($this->view->mark['app_description']);
        $this->view->display('index/doc', 'common');
    }    
    
    /**
     * test
     */
    public function test()
    {
        // 控制器内变量赋值到模板
        $this->title = "性能测试";
        $this->display('index/test', 'common');
    }
}
