<?php
namespace Controllers\Admin;

/**
 * Mark Controller
 * 
 * @author sasou<admin@php-gene.com>
 * @version 1.0
 * @copyright  Copyright (c) 2018 * Co., Ltd.
 */
class Mark extends \Gene\Controller
{
    
    /**
     * run
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function run($arr)
    {
        $page = intval($this->get("page", 1));
        $limit = intval($this->get("limit", 10));
        $search['title'] = trim($this->get("title"));
        $title = '文档管理';
        $mark_type = ["", "框架使用", "框架类文档", "技术研究"];
        $mark = \Services\Admin\Mark::getInstance()->lists($page, $limit, $search);
        $this->display("admin/mark/run", "parent");
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
        $row = \Services\Admin\Mark::getInstance()->row($id);
    }
    
    /**
     * add
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function add()
    {
        $title = '文档添加';
        $this->display("admin/mark/add", "easy");
    }
    
    /**
     * addPost
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function addPost()
    {
        $data = $this->post('data');
        $id = \Services\Admin\Mark::getInstance()->add($data);
        if ($id) {
            return $this->success("添加成功");
        }
        return $this->error("添加失败");
    }
    
    /**
     * edit
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function edit($params)
    {
        $title = '文档修改';
        $id = intval($params["id"]);
        $mark = \Services\Admin\Mark::getInstance()->row($id);
        $this->display("admin/mark/edit", "easy");
    }
    
    /**
     * editPost
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function editPost()
    {
        $id = intval($this->post('id'));
        $data = $this->post('data');
        $count = \Services\Admin\Mark::getInstance()->edit($id, $data);
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
        $count = \Services\Admin\Mark::getInstance()->status($id);
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
        $count = \Services\Admin\Mark::getInstance()->del($id);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

    /**
     * delAll
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function delAll()
    {
        $id_arr = $this->post("id", 0);
        $count = \Services\Admin\Mark::getInstance()->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }
	
}
