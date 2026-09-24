<?php
namespace Controllers\Admin;

/**
 * Group Controller
 * 
 * @author  sasou
 * @version  1.0
 */
class Group extends \gene\Controller
{

    /**
     * run
     */
    function run()
    {
        $page = intval($this->get("page", 1));
        $this->view->assign('page', $page);
        $this->view->assign('title', '角色管理');
        $this->view->assign('group', \Services\Admin\Group::getInstance()->lists($page));
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
        // $this->row 是 DI 写入；视图数据用 assign()
        $this->view->assign('row', \Services\Admin\Module::getInstance()->row($id));
    }
    
    /**
     * add
     */
    function add()
    {
        $this->view->assign('title', '角色添加');
        $this->view->assign('purviewList', \Services\Admin\Module::getInstance()->purviewList());
        $this->display("admin/group/add", "dialog");
    }
    
    /**
     * addPost
     */
    function addPost()
    {
        $data = $this->post('data');
        $purview = $this->post('purview');
        if (!$this->validate->init(is_array($data) ? $data : [])
            ->name('group_title')->required()->msg('角色名不能为空')
            ->valid()) {
            return $this->error($this->validate->error());
        }
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
        $this->view->assign('title', '角色修改');
        $id = intval($params["id"]);
        $this->view->assign('purviewList', \Services\Admin\Module::getInstance()->purviewList());
        $this->view->assign('purview', \Services\Admin\Purview::getInstance()->lists($id));
        $this->view->assign('group', \Services\Admin\Group::getInstance()->row($id));
        $this->display("admin/group/edit", "dialog");
    }
    
    /**
     * editPost
     */
    function editPost()
    {
        $id = intval($this->post('id'));
        $data = $this->post('data');
        $purview = $this->post('purview');
        if (!$this->validate->init(is_array($data) ? $data : [])
            ->name('group_title')->required()->msg('角色名不能为空')
            ->valid()) {
            return $this->error($this->validate->error());
        }
        $count_g = \Services\Admin\Group::getInstance()->edit($id, $data);
        $count_p = \Services\Admin\Purview::getInstance()->update($id, $purview);
        if ($count_g || $count_p) {
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
