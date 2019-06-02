<?php
namespace Models\Admin;

/**
 * Module Model
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Module extends \Gene\Model
{

    /**
     * lists
     *
     * @param  string  $purview 权限栏目
     * @return array
     */
    function lists($purview = '')
    {
        $list = $this->db
                     ->select("sys_module", "module_id,module_pid,module_title,module_url,module_icon")
                     ->where("status=1 and module_icon!='' and find_in_set(module_id,?)", $purview)
                     ->order("`order` desc")
                     ->all();
        return $list;
    }

    /**
     * manageList
     *
     * @return array
     */
    function manageList()
    {
        $count = $this->db
                      ->count("sys_module")
                      ->cell();
        $list = $this->db
                      ->select("sys_module", "module_id,module_pid,module_title,module_url,module_icon,status")
                      ->order("module_id desc")
                      ->all();
        $tree = \Ext\Helper\Tree::init();
        $tree->config(array('parentid' => 'module_pid', 'id' => 'module_id', 'name' => 'module_title'));
        $tree->getTrees($list, $optionlist);
        return ["count" => $count, "list" => $optionlist];
    }
    
    /**
     * purviewList
     *
     * @return array
     */
    function purviewList()
    {
        $list = $this->db
                      ->select("sys_module", "module_id,module_pid,module_title,module_icon,status")
                      ->order("module_id desc")
                      ->all();
        $tree = \Ext\Helper\Tree::init();
        $tree->config(array('parentid' => 'module_pid', 'id' => 'module_id', 'name' => 'module_title'));
        $tree->getTrees($list, $optionlist);
        $data = [];
        foreach($optionlist as $v) {
            $one = [];
            $one['id'] = $v['module_id'];
            $one['name'] = $v['module_title'];
            $one['deep'] = $v['deep'];
            if ($v['deep'] == 2 ) {
                $data[$v['module_pid']]['purview'][] = $one;
            } else {
                $data[$v['module_id']] = $one;
            }
        }
        return $data;
    }
    
    /**
     * path
     *
     * @param  string $uri 访问路径
     * @return array
     */
    function path($uri)
    {
        $curMenu = $this->db
                      ->select("sys_module", "module_id,module_path,module_title,module_url")
                      ->where("status=1 and module_url=?", $uri)
                      ->row();
        if (isset($curMenu['module_path'])) {
            $menu = [];
            $parMenu = $this->db
                      ->select("sys_module", "module_id,module_title,module_url")
                      ->where("status=1 and FIND_IN_SET(module_id,?)", $curMenu['module_path'])
                      ->all();
            if (isset($parMenu[0])) {
                foreach($parMenu as $v) {
                    $menu[$v['module_id']]['title'] = $v['module_title'];
                    $menu[$v['module_id']]['url'] = $v['module_url'];
                }
            }
            $menu[$curMenu['module_id']]['title'] = $curMenu['module_title'];
            $menu[$curMenu['module_id']]['url'] = $curMenu['module_url'];
            return [$menu, $curMenu['module_id']];
        }
        return null;
    }
    
    /**
     * 取path
     * @param int $id
     * @return string
     */
    public function getPidPath($pid)
    {
        if (isset($pid) && ($pid > 0)) {
            $path = $this->getField($pid, "module_path");
            $path = ($path == '') ? '0,' : $path;
            $path = $path  . "," . $pid;
        } else {
            $path = "0,";
        }
        return $path;
    }
    
    
    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return $this->db
                    ->select("sys_module")
                    ->where("module_id=?", $id)
                    ->limit(1)
                    ->row();
    }
    
	/**
     * getField
     * 
     * @param int    $id 
     * @param string $field 
     * @return string
     */
    public function getField($id, $field ='module_title')
    {
        $data = $this->row($id);
        return isset($data[$field]) ? $data[$field] : '';
    }
    
    /**
     * add
     *
     * @param  array $data  添加数据
     * @return int id
     */
    function add($data)
    {
        return $this->db
                    ->insert("sys_module", $data)
                    ->lastId();
    }

    /**
     * edit
     *
     * @param  int   $id    更新id
     * @param  array $data  更新数据
     * @return int count
     */
    function edit($id, $data)
    {
        return $this->db
                    ->update("sys_module", $data)
                    ->where("module_id=?", $id)
                    ->affectedRows();
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return $this->db
                    ->sql("update sys_module set status=abs(status-1)")
                    ->where("module_id=?", $id)
                    ->affectedRows();
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return $this->db
                    ->delete("sys_module")
                    ->where("module_id=?", $id)
                    ->affectedRows();
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return $this->db
                       ->delete("sys_module")
                       ->in("module_id in(?)", $id_arr)
                       ->affectedRows();
    }

    /**
     * countChird
     *
     * @param  mixed $id_arr id数组或者id
     * @return int   count
     */
    function countChird($id_arr)
    {
        return $this->db
                       ->count("sys_module")
                       ->in("module_pid in(?)", $id_arr)
                       ->cell();
    }
}
