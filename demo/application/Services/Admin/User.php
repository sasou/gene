<?php
namespace Services\Admin;

/**
 * User Service
 * 
 * @author  sasou
 * @version  1.0
 */
class User extends \Gene\Service
{


    /**
     * 检查登录
     * 
     * @param string $username 用户名
     * @param string $password 密码
     * @return array
     */
    function checkUser($username, $password)
    {
        if (!$username || !$password) {
            return $this->error('用户名或者密码为空！');
        }
        $result = \Models\Admin\User::getInstance()->getUserInfoByName($username);
        if (!$result) {
            return $this->error('用户名不存在');
        }
        if (!$result['status']) {
            return $this->error('您的账号平台审核中，请耐心等待。。。');
        }
        $password_encode = $this->generatePasswordHash($password, $result['user_salt']);
        if ($password_encode != $result['user_pass']) {
            return $this->error('密码错误!');
        }
        //设置权限
        $result['purview'] = \Services\Admin\Purview::getInstance()->getPurviewStr($result['group_id']);
        
        //删掉一些字段
        unset($result['user_salt']);
        unset($result['user_pass']);
        $this->session->set('admin', $result);
        \Services\Admin\Log::getInstance()->log("后台登录", "用户名:" . $result['user_name'], $result['user_id']);
        return $this->success("登录成功!");
    }

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
        if($search['role'] != "") {
            $params['group_id'] = $search['role'];
        }
        if($search['name'] != "") {
            $params['user_name'] = ['%' . $search['name'] . '%', 'like'];
        }
        $start = $page > 0 ? ($page - 1) * $limit : 0;
        return \Models\Admin\User::getInstance()->lists($params, $start, $limit);
    }

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return \Models\Admin\User::getInstance()->row($id);
    }

	/**
     * getField
     * 
     * @param int    $id 
     * @param string $field 
     * @return string
     */
    public function getField($id, $field ='user_name')
    {
        return \Models\Admin\User::getInstance()->getField($id, $field);
    }
    
    /**
     * add
     *
     * @param  array $data  添加数据
     * @return int id
     */
    function add($data)
    {
        if (isset($data['user_pass']) && $data['user_pass'] != '') {
            $data['user_salt'] = substr(md5(uniqid()), 0, 16);
            $data['user_pass'] = $this->generatePasswordHash($data['user_pass'], $data['user_salt']);
        }
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Admin\User::getInstance()->add($data);
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
        if (isset($data['user_pass']) && $data['user_pass'] != '') {
            $data['user_salt'] = substr(md5(uniqid()), 0, 16);
            $data['user_pass'] = $this->generatePasswordHash($data['user_pass'], $data['user_salt']);
        } else {
            unset($data['user_pass']);
        }
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        return \Models\Admin\User::getInstance()->edit($id, $data);
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return \Models\Admin\User::getInstance()->status($id);
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return \Models\Admin\User::getInstance()->del($id);
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return \Models\Admin\User::getInstance()->delAll($id_arr);
    }
    
    /**
     * countChird
     *
     * @param  mixed $id_arr id数组或者id
     * @return int   count
     */
    function countByGroupId($id_arr)
    {
        return \Models\Admin\User::getInstance()->countByGroupId($id_arr);
    }
    
    /**
     * Generate the hashed password
     *
     * updating this method will void all password history entries
     *
     * @param string  $password The plain text password to hash
     * @param string  $salt     The salt string
     *
     * @return string
     */
    public function generatePasswordHash($password, $salt)
    {
        $hash = md5($salt . sha1($salt . $password));
        $hash = substr($hash, 0, 50);

        return $hash;
    }

}
