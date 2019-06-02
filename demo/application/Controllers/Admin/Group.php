<?php
namespace Controllers\Admin;

/**
 * Group Controller
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Group extends \gene\Controller
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
        $title = '角色管理';
        $group = \Services\Admin\Group::getInstance()->lists($page);
        $this->display("admin/group/run", "parent");
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
        $row = \Services\Admin\Module::getInstance()->row($id);
    }
    
    /**
     * add
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function add()
    {
        $title = '角色添加';
        $purviewList = \Services\Admin\Module::getInstance()->purviewList();
        $this->display("admin/group/add", "easy");
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
        $purview = $this->post('purview');
        $id = \Services\Admin\Group::getInstance()->add($data);
        if ($id) {
            \Services\Admin\Purview::getInstance()->add($id, $purview);
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
        $title = '角色修改';
        $id = intval($params["id"]);
        $purviewList = \Services\Admin\Module::getInstance()->purviewList();
        $purview = \Services\Admin\Purview::getInstance()->lists($id);
        $group = \Services\Admin\Group::getInstance()->row($id);
        $this->display("admin/group/edit", "easy");
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
        $purview = $this->post('purview');
        $count = \Services\Admin\Group::getInstance()->edit($id, $data);
        if ($count >= 0) {
            \Services\Admin\Purview::getInstance()->update($id, $purview);
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
        $count = \Services\Admin\Group::getInstance()->status($id);
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
        if (\Services\Admin\User::getInstance()->countByGroupId($id) > 0) {
            return $this->error("删除失败,当前角色已经拥有用户");
        }
        $count = \Services\Admin\Group::getInstance()->del($id);
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
        if (\Services\Admin\User::getInstance()->countByGroupId($id_arr) > 0) {
            return $this->error("删除失败,当前角色已经拥有用户");
        }
        $count = \Services\Admin\Group::getInstance()->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

}
