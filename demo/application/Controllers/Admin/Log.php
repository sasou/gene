<?php
namespace Controllers\Admin;

/**
 * Log Controller
 * 
 * @author  sasou
 * @version  1.0
 */
class Log extends \Gene\Controller
{

    /**
     * run
     */
    function run()
    {
        $this->title = '操作日志';
        $this->page = intval($this->get("page", 1));
        $this->limit = intval($this->get("limit", 10));
        $search['url'] = $this->get("url", "");
        $search['ip'] = $this->get("ip", "");
        if ($this->user['group_id'] != 1) {
            $search['user_id'] = $this->user['user_id'];
        }
        $this->search = $search;
        $this->log = \Services\Admin\Log::getInstance()->lists($this->page, $this->limit, $this->search);
        $this->display("admin/log/run", "parent");
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
        $count = \Services\Admin\Log::getInstance()->status($id);
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
        $count = \Services\Admin\Log::getInstance()->del($id);
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
        $count = \Services\Admin\Log::getInstance()->delAll($id_arr);
        if ($count) {
            return $this->success("删除成功");
        }
        return $this->error("删除失败");
    }

}
