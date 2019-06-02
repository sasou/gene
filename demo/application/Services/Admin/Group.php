<?php
namespace Services\Admin;

/**
 * Group Service
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Group extends \Gene\Service
{

    /**
     * run
     *
     * @param  int  $page     分页
     * @param  int  $pageSize 每页数量
     * @return array
     */
    function lists($page = 1, $pagesize = 10)
    {
        $start = $page > 0 ? ($page - 1)*$pagesize : 0;
        return \Models\Admin\Group::getInstance()->lists($start, $pagesize);
    }

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return \Models\Admin\Group::getInstance()->row($id);
    }

	/**
     * getField
     * 
     * @param int    $id 
     * @param string $field 
     * @return string
     */
    public function getField($id, $field ='group_title')
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
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Admin\Group::getInstance()->add($data);
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
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Admin\Group::getInstance()->edit($id, $data);
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return \Models\Admin\Group::getInstance()->status($id);
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return \Models\Admin\Group::getInstance()->del($id);
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return \Models\Admin\Group::getInstance()->delAll($id_arr);
    }

}
