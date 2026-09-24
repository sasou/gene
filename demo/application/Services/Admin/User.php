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
     * 用户行缓存版本键（sys_user.user_id）
     *
     * @param int $id
     * @return array
     */
    protected function userRowVersion($id)
    {
        return ['db.sys_user.user_id' => $id];
    }

    /**
     * 登录查询缓存版本键（按登录名）
     *
     * @param string $username
     * @return array
     */
    protected function userLoginVersion($username)
    {
        return ['db.sys_user.user_name' => $username];
    }

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
        $result = $this->cache->cachedVersion(
            ["\Models\Admin\User", 'getUserInfoByName'],
            [$username],
            $this->userLoginVersion($username),
            3600
        );
        if (!$result) {
            return $this->error('用户名不存在');
        }
        if (!$result['status']) {
            return $this->error('您的账号平台审核中，请耐心等待。。。');
        }
        $stored = $result['user_pass'];
        if (!$this->verifyPassword($password, $result['user_salt'], $stored)) {
            return $this->error('密码错误!');
        }
        // 命中旧 md5 摘要、91d0442 期间的 salt 拼接哈希，或算法/成本已过时
        // 时，用 password_hash($password) 原地重写。updateBy 会 bump
        // versionKeys，下次登录读到新摘要。
        if (strpos($stored, '$') !== 0
            || !password_verify($password, $stored)
            || password_needs_rehash($stored, PASSWORD_DEFAULT)) {
            \Models\Admin\User::getInstance()->edit($result['user_id'], [
                'user_pass' => $this->generatePasswordHash($password, $result['user_salt']),
            ]);
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
    function lists($page = 1, $limit = 10, $search = [])
    {
        $params = [];
        if(($search['role'] ?? '') != "") {
            $params['group_id'] = $search['role'];
        }
        if(($search['name'] ?? '') != "") {
            $params['user_name'] = ['%' . $search['name'] . '%', 'like'];
        }
        return \Models\Admin\User::getInstance()->lists($params, $page > 0 ? $page : 1, $limit);
    }

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return $this->cache->cachedVersion(
            ["\Models\Admin\User", 'row'],
            [$id],
            $this->userRowVersion($id),
            3600
        );
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
        return $this->cache->cachedVersion(
            ["\Models\Admin\User", 'getField'],
            [$id, $field],
            $this->userRowVersion($id),
            3600
        );
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
            $data['user_pass'] = $this->generatePasswordHash($data['user_pass'], '');
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
            $data['user_pass'] = $this->generatePasswordHash($data['user_pass'], '');
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
     * @param string  $salt     The salt string（保留参数兼容调用方；bcrypt
     *                          自带随机盐，不再参与哈希——16 字符 salt 会占掉
     *                          bcrypt 72 字节输入上限，吞掉长口令尾部）
     *
     * @return string
     */
    public function generatePasswordHash($password, $salt)
    {
        return password_hash($password, PASSWORD_DEFAULT);
    }

    /**
     * 三种存储格式：$ 开头的新哈希先试 password_verify($password)，失败再试
     * $salt . $password（兼容 91d0442 期间写入的拼接格式）；非 $ 开头走旧
     * md5 摘要。user_salt 只服务旧格式。
     */
    public function verifyPassword($password, $salt, $stored)
    {
        if (!is_string($stored) || $stored === '') {
            return false;
        }
        if (strpos($stored, '$') === 0) {
            return password_verify($password, $stored)
                || password_verify($salt . $password, $stored);
        }
        $legacy = substr(md5($salt . sha1($salt . $password)), 0, 50);
        return hash_equals($legacy, $stored);
    }

}
