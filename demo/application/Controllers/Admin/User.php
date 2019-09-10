<?php
namespace Controllers\Admin;

/**
 * User Controller
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class User extends \Gene\Controller
{

    /**
     * run
     *
     * @return mixed
     */
    function run()
    {
        $this->page = intval($this->get("page", 1));
        $this->limit = intval($this->get("limit", 10));
        $search['role'] = $this->get("role", '');
        $search['name'] = $this->get("name", '');
        $this->title = '用户管理';
        $this->search = $search;
        $this->userlist = \Services\Admin\User::getInstance()->lists($this->page, $this->limit, $this->search);
        $this->group = \Services\Admin\Group::getInstance()->lists();
        $this->display("admin/user/run", "parent");
    }

    /**
     * info
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function info($params)
    {
        $this->id = intval($params["id"]);
    }
    
    /**
     * set
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function set($params)
    {
        $this->title = '修改资料';
        $this->row = \Services\Admin\User::getInstance()->row($this->user['user_id']);
        $this->display("admin/user/set", "dialog");
    }
    
    /**
     * save
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function save()
    {
        $data['user_pass'] = trim($this->post('user_pass'));
        $data['user_realname'] = trim($this->post('user_realname'));
        $data['status'] = 'on';
        $count = \Services\Admin\User::getInstance()->edit($this->user['user_id'], $data);
        if ($count) {
            return $this->success("修改成功");
        }
        return $this->error("修改失败");
    }
    
    /**
     * add
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function add()
    {
        $this->title = '菜单添加';
        $this->group = \Services\Admin\Group::getInstance()->lists();
        $this->display("admin/user/add", "dialog");
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
        $id = \Services\Admin\User::getInstance()->add($data);
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
        $this->title = '菜单修改';
        $id = intval($params["id"]);
        $this->group = \Services\Admin\Group::getInstance()->lists();
        $this->row = \Services\Admin\User::getInstance()->row($id);
        $this->display("admin/user/edit", "dialog");
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
        $count = \Services\Admin\User::getInstance()->edit($id, $data);
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
        $count = \Services\Admin\User::getInstance()->status($id);
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
        $count = \Services\Admin\User::getInstance()->del($id);
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
        $count = \Services\Admin\User::getInstance()->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

}
