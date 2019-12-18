<?php
namespace Controllers;
/**
 * Index Controller
 * 
 * @author  sasou
 * @version  1.0
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
        // 获取url路由定义的参数
        $id =  $params['id'] ?? 0;
        // 使用自定义类库
        $parsedown = new \Ext\Parsedown();
        
        // 通过模板赋值
        $this->view->title = "文档";
        $this->view->css = ['mdediter/md.min'];
        $this->view->js = ['mdediter/md.min'];
        $this->view->help = \Services\Doc\Mark::getInstance()->listAll(1);
        $this->view->doc = \Services\Doc\Mark::getInstance()->listAll(2);
        $mark = \Services\Doc\Mark::getInstance()->row($id);
        if ($mark) {
            $this->view->mark = $mark;
            $this->view->text = $parsedown->text($this->view->mark['app_description']);
        }
        $this->view->display('index/doc', 'common');
    }    
    
    /**
     * test
     */
    public function test()
    {
        // 控制器内变量赋值到模板,注意与上述方法的区别
        $this->title = "性能测试";
        $this->display('index/test', 'common');
    }
    
}
