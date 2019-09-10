<?php
namespace Services\Admin;

/**
 * Module Service
 * 
 * @author  sasou
 * @version  1.0
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
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'lists'], [$purview], ['db.sys_module' => null], 3600);
    }

    /**
     * manageList
     *
     * @return array
     */
    function manageList()
    {
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'manageList'], [], ['db.sys_module' => null], 3600);
    }
    
    /**
     * purviewList
     *
     * @return array
     */
    function purviewList()
    {
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'purviewList'], [], ['db.sys_module' => null], 3600);
    }
    
    /**
     * initPath
     *
     * @param  string $uri 访问路径
     * @return array
     */
    function initPath($uri)
    {
        // 方法级实时版本缓存
        $path = $this->cache->cachedVersion(["\Models\Admin\Module",'path'], [$uri], ['db.sys_module' => null], 3600);
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
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'getPidPath'], [$pid], ['db.sys_module' => null], 3600);
    }
    
    
    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'row'], [$id], ['db.sys_module' => null], 3600);
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
        // 方法级实时缓存版本key更新
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
        // 方法级实时缓存版本key更新
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
        // 方法级实时缓存版本key更新
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
        // 方法级实时缓存版本key更新
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
        // 方法级实时缓存版本key更新
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
        // 方法级实时版本缓存
        return $this->cache->cachedVersion(["\Models\Admin\Module",'countChird'], [$id_arr], ['db.sys_module' => null], 3600);
    }
}
