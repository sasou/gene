<?php
namespace Services\Admin;

/**
 * Log Service
 * 
 * @author  sasou
 * @version  1.0
 */
class Log extends \Gene\Service
{

    /**
     * run
     *
     * @param  int    $page      分页
     * @param  int    $limit     每页数量
     * @param  array  $search    查询条件
     * @return array
     */
    function lists($page = 1, $limit = 10, $search)
    {
        $params = [];
        if($search['url'] != "") {
            $params['log_url'] = trim($search['url']);
        }
        if($search['ip'] != "") {
            $params['log_ip'] = trim($search['ip']);
        }
        if(isset($search['user_id'])) {
            $params['user_id'] = $search['user_id'];
        }
        $start = $page > 0 ? ($page - 1) * $limit : 0;
        return \Models\Admin\Log::getInstance()->lists($params, $start, $limit);
    }
    
    /**
     * getUserLastLoginInfo
     *
     * @param  int   $user_id  user_id
     * @return array
     */
    function getUserLastLoginInfo($user_id)
    {
        return $this->cache->cached(["\Models\Admin\Log", "getUserLastLoginInfo"], [$user_id], 3600);
    } 

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return \Models\Admin\Log::getInstance()->row($id);
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return \Models\Admin\Log::getInstance()->status($id);
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return \Models\Admin\Log::getInstance()->del($id);
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return \Models\Admin\Log::getInstance()->delAll($id_arr);
    }
    
    /**
     * log 记录日志
     * 
     * @param string $title
     * @param string $log_data
     * @param int    $user_id
     * @return bool
     */
    public function log($title, $log_data, $user_id)
    {
	$ip = new \Ext\Ip\File();
	$log_ip_area = $ip->btreeSearch($this->request->server["REMOTE_ADDR"]);
        $data = array(
            'log_title' => $title,
            'log_data' => $log_data,
            'log_url' => $this->request->server["REQUEST_URI"],
            'log_ip' => $this->request->server["REMOTE_ADDR"],
            'log_ip_area' => $log_ip_area['region'],
            'user_id' => $user_id,
            'addtime' => time());
        return \Models\Admin\Log::getInstance()->add($data);
    }

}
