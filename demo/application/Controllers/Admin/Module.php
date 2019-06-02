<?php
namespace Controllers\Admin;

/**
 * Module Controller
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Module extends \Gene\Controller
{

    /**
     * run
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function run($arr)
    {
        $title = '权限分类';
        $module = \Services\Admin\Module::getInstance()->manageList();
        $this->display("admin/module/run", "parent");
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
        $title = '菜单添加';
        $moduleList = \Services\Admin\Module::getInstance()->manageList();
        $this->display("admin/module/add", "easy");
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
        $id = \Services\Admin\Module::getInstance()->add($data);
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
        $title = '菜单修改';
        $id = intval($params["id"]);
        $moduleList = \Services\Admin\Module::getInstance()->manageList();
        $module = \Services\Admin\Module::getInstance()->row($id);
        $this->display("admin/module/edit", "easy");
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
        $count = \Services\Admin\Module::getInstance()->edit($id, $data);
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
        $count = \Services\Admin\Module::getInstance()->status($id);
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
        $module = \Services\Admin\Module::getInstance();
        if ($module->countChird($id) > 0) {
            return $this->error("删除失败,当前栏目拥有下级栏目");
        }
        $count = $module->del($id);
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
        $module = \Services\Admin\Module::getInstance();
        if ($module->countChird($id_arr) > 0) {
            return $this->error("删除失败,当前栏目拥有下级栏目");
        }
        $count = $module->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

}
