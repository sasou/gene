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
        $this->assign('title', "首页");
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
        $this->assign('id', $id);
        $this->assign('title', "文档");
        $this->assign('css', ['mdediter/md.min']);
        $this->assign('js', ['mdediter/md.min']);
        $this->assign('help', \Services\Doc\Mark::getInstance()->listAll(1));
        $this->assign('doc', \Services\Doc\Mark::getInstance()->listAll(2));
        
        $mark = \Services\Doc\Mark::getInstance()->row($id);
        if ($mark) {
            $this->assign('mark', $mark);
            $this->assign('title', $parsedown->text($this->view->mark['app_description']));
        }
        $this->view->display('index/doc', 'common');
    }    
    
    /**
     * test
     */
    public function test()
    {
        // 控制器内变量赋值到模板,注意与上述方法的区别
        $title = "性能测试";
        $this->assign('title', $title);
        $this->display('index/test', 'common');
    }
    
}
