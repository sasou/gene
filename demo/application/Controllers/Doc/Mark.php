<?php
namespace Controllers\Doc;

/**
 * Mark Controller
 * 
 * @author sasou<admin@php-gene.com>
 * @version 1.0
 */
class Mark extends \Gene\Controller
{
    
    /**
     * run
     */
    function run()
    {
        $this->page = intval($this->get("page", 1));
        $this->limit = intval($this->get("limit", 10));
        $search['title'] = trim($this->get("title"));
        $this->title = '文档管理';
        $this->mark_type = ["", "框架使用", "框架类文档", "技术研究"];
        $this->search = $search;
        $this->mark = \Services\Doc\Mark::getInstance()->lists($this->page, $this->limit, $this->search);
        $this->display("doc/mark/run", "parent");
    }
    
	
    /**
     * info
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function info($params)
    {
        $id = intval($params["id"]);
        $this->row = \Services\Doc\Mark::getInstance()->row($id);
    }
    
    /**
     * add
     */
    function add()
    {
        $this->title = '文档添加';
        $this->display("doc/mark/add", "dialog");
    }
    
    /**
     * addPost
     */
    function addPost()
    {
        $data = $this->post('data');
        $id = \Services\Doc\Mark::getInstance()->add($data);
        if ($id) {
            return $this->success("添加成功");
        }
        return $this->error("添加失败");
    }
    
    /**
     * edit
     */
    function edit($params)
    {
        $this->title = '文档修改';
        $id = intval($params["id"]);
        $this->mark = \Services\Doc\Mark::getInstance()->row($id);
        $this->display("doc/mark/edit", "dialog");
    }
    
    /**
     * editPost
     */
    function editPost()
    {
        $id = intval($this->post('id'));
        $data = $this->post('data');
        $count = \Services\Doc\Mark::getInstance()->edit($id, $data);
        if ($count) {
            return $this->success("修改成功");
        }
        return $this->error("修改失败");
    }
    
    /**
     * status
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function status($params)
    {
        $id = intval($params["id"]);
        $count = \Services\Doc\Mark::getInstance()->status($id);
        if ($count) {
            return $this->success("更改成功");
        }
        return $this->error("更改失败");
    }
    
    /**
     * del
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function del($params)
    {
        $id = intval($params["id"]);
        $count = \Services\Doc\Mark::getInstance()->del($id);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

    /**
     * delAll
     */
    function delAll()
    {
        $id_arr = $this->post("id", 0);
        $count = \Services\Doc\Mark::getInstance()->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }
	
}
