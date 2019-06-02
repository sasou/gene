<?php
namespace Services\Admin;

/**
 * Module Service
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Module extends \Gene\Service
{

    /**
     * lists
     *
     * @param  string  $purview 权限栏目
     * @return array
     */
    function lists($purview = '')
    {
        return $this->cache->cached(["\Models\Admin\Module",'lists'], [$purview], 10);
    }

    /**
     * manageList
     *
     * @return array
     */
    function manageList()
    {
        return $this->cache->cachedVersion(["\Models\Admin\Module",'manageList'], [], ['db.sys_module' => null], 10);
    }
    
    /**
     * purviewList
     *
     * @return array
     */
    function purviewList()
    {
        return \Models\Admin\Module::getInstance()->purviewList();
    }
    
    /**
     * initPath
     *
     * @param  string $uri 访问路径
     * @return array
     */
    function initPath($uri)
    {
        $path = $this->cache->cached(['\Models\Admin\Module','path'], [$uri], 10);
        $this->path = isset($path[0]) ? $path[0] : [];
        $this->id = isset($path[1]) ? $path[1] : 0;
        return $this->id;
    }
    
    /**
     * 取path
     * @param int $id
     * @return string
     */
    public function getPidPath($pid)
    {
        return \Models\Admin\Module::getInstance()->getPidPath($pid);
    }
    
    
    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return \Models\Admin\Module::getInstance()->row($id);
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
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        $data['module_path'] = $this->getPidPath($data['module_pid']);
        $data['addtime'] = time();
        $this->cache->updateVersion(['db.sys_module' => null]);
        return \Models\Admin\Module::getInstance()->add($data);
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
        $data['module_path'] = $this->getPidPath($data['module_pid']);
        $data['updatetime'] = time();
        $this->cache->updateVersion(['db.sys_module' => null]);
        return \Models\Admin\Module::getInstance()->edit($id, $data);
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        $this->cache->updateVersion(['db.sys_module' => null]);
        return \Models\Admin\Module::getInstance()->status($id);
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        $this->cache->updateVersion(['db.sys_module' => null]);
        return \Models\Admin\Module::getInstance()->del($id);
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        $this->cache->updateVersion(['db.sys_module' => null]);
        return \Models\Admin\Module::getInstance()->delAll($id_arr);
    }

    /**
     * countChird
     *
     * @param  mixed $id_arr id数组或者id
     * @return int   count
     */
    function countChird($id_arr)
    {
        return \Models\Admin\Module::getInstance()->countChird($id_arr);
    }
}
