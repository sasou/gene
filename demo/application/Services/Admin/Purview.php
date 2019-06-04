<?php
namespace Services\Admin;

/**
 * Purview Service
 * 
 * @author  sasou
 * @version  1.0
 */
class Purview extends \Gene\Service
{

    /**
     * lists
     *
     * @param  int $group_id  group_id
     * @return array
     */
    function lists($group_id)
    {
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Purview",'lists'], [$group_id], ['db.sys_purview' => null], 3600);
    }
    
    
    /**
     * lists
     *
     * @param  int $group_id  group_id
     * @return array
     */
    function getPurviewStr($group_id)
    {
        $ret =  $this->lists($group_id);
        $str = '';
        if (isset($ret[0])) {
            foreach($ret as $k=>$v) {
                if ($k == 0) {
                    $str = $v['obj_id'];
                } else {
                    $str .= "," . $v['obj_id'];
                }
            }
        }
        return $str;
    }
    
    /**
     * add
     *
     * @param  int   $group_id  group_id
     * @param  array $purview   权限节点
     * @return int id
     */
    function add($group_id, $purview)
    {
        $data = [];
        if (is_array($purview)) {
            foreach($purview as $v) {
                $row = [];
                $row['group_id'] = $group_id;
                $row['obj_id'] = $v;
                $row['addtime'] = time();
                $data[] = $row;
            }
        }
        // 方法级实时缓存版本key更新
        $this->cache->updateVersion(['db.sys_purview' => null]);
        return \Models\Admin\Purview::getInstance()->batchInsert($data);
    }

    /**
     * edit
     *
     * @param  int   $group_id    group_id
     * @param  array $purview     purview
     * @return int count
     */
    function update($group_id, $purview)
    {
        \Models\Admin\Purview::getInstance()->delAll($group_id);
        // 方法级实时缓存版本key更新
        $this->cache->updateVersion(['db.sys_purview' => null]);
        return $this->add($group_id, $purview);
    }

}
