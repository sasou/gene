<?php
namespace Services\Doc;

/**
 * Mark Service
 * 
 * @author sasou<admin@php-gene.com>
 * @version 1.0
 */
class Mark extends \Gene\Service
{

    /**
     * run
     *
     * @param  int    $page      分页
     * @param  int    $limit     每页数量
     * @param  array  $search    查询条件
     * @return array
     */
    function lists($page = 1, $limit = 10, $search = [])
    {
        $params = [];
        if ($search['title'] != '') {
            $params['mark_title'] = ["%{$search['title']}%", 'like'];
        }
        $start = $page > 0 ? ($page - 1) * $limit : 0;
        return \Models\Doc\Mark::getInstance()->lists($params, $start, $limit);
    }
    
    /**
     * listAll
     *
     * @param  int   $type  type
     * @return array
     */
    function listAll($type)
    {
        // 方法级定时缓存一分钟
        return $this->cache->cached(["\Models\Doc\Mark", "listAll"], [$type], 60);
    }

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        // 方法级定时缓存一分钟
        return $this->cache->cached(["\Models\Doc\Mark", "row"], [$id], 3);
    }
    
    /**
     * add
     *
     * @param  array $data  添加数据
     * @return int id
     */
    function add($data)
    {
        $data['user_id'] = isset($this->session->get('admin')['user_id']) ? $this->session->get('admin')['user_id'] : 0;
        $data['addtime'] = time();
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Doc\Mark::getInstance()->add($data);
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
        $data['updatetime'] = time();
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Doc\Mark::getInstance()->edit($id, $data);
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return \Models\Doc\Mark::getInstance()->status($id);
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return \Models\Doc\Mark::getInstance()->del($id);
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return \Models\Doc\Mark::getInstance()->delAll($id_arr);
    }
    
}
