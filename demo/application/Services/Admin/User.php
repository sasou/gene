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
     * 写库后失效：该行 user_id + 当前库中 user_name，及可选曾用登录名（如改名）
     *
     * @param int   $id
     * @param array $extraLoginNames
     */
    protected function bumpUserCacheForUser($id, array $extraLoginNames = [])
    {
        $name = \Models\Admin\User::getInstance()->userNameById($id);
        $names = $extraLoginNames;
        if ($name !== '') {
            $names[] = $name;
        }
        $names = array_values(array_unique(array_filter($names, function ($n) {
            return $n !== null && $n !== '';
        })));
        $ver = $this->userRowVersion($id);
        if ($names) {
            $ver['db.sys_user.user_name'] = count($names) === 1 ? $names[0] : $names;
        }
        $this->cache->updateVersion($ver);
    }

    /**
     * 删除后失效（库中已无该行，登录名须调用方预先取出）
     *
     * @param int $id
     * @param string|null $loginName
     */
    protected function bumpUserCacheDeleted($id, $loginName)
    {
        $ver = $this->userRowVersion($id);
        if ($loginName !== null && $loginName !== '') {
            $ver['db.sys_user.user_name'] = $loginName;
        }
        $this->cache->updateVersion($ver);
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
            $data['user_salt'] = substr(md5(uniqid()), 0, 16);
            $data['user_pass'] = $this->generatePasswordHash($data['user_pass'], $data['user_salt']);
        }
        $data['status'] = isset($data['status']) && $data['status'] == 'on' ? 1 : 0;
        $id = \Models\Admin\User::getInstance()->add($data);
        if ($id) {
            $this->bumpUserCacheForUser($id);
        }
        return $id;
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
        $oldLoginName = null;
        if (isset($data['user_name'])) {
            $oldLoginName = \Models\Admin\User::getInstance()->userNameById($id);
        }
        $count = \Models\Admin\User::getInstance()->edit($id, $data);
        if ($count) {
            $extra = ($oldLoginName !== null && $oldLoginName !== '') ? [$oldLoginName] : [];
            $this->bumpUserCacheForUser($id, $extra);
        }
        return $count;
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        $count = \Models\Admin\User::getInstance()->status($id);
        if ($count) {
            $this->bumpUserCacheForUser($id);
        }
        return $count;
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        $model = \Models\Admin\User::getInstance();
        $loginName = $model->userNameById($id);
        $count = $model->del($id);
        if ($count) {
            $this->bumpUserCacheDeleted($id, $loginName);
        }
        return $count;
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        $model = \Models\Admin\User::getInstance();
        $loginNames = $model->userNamesByIds($id_arr);
        $count = $model->delAll($id_arr);
        if ($count) {
            $ver = ['db.sys_user.user_id' => $id_arr];
            if ($loginNames) {
                $ver['db.sys_user.user_name'] = count($loginNames) === 1 ? $loginNames[0] : $loginNames;
            }
            $this->cache->updateVersion($ver);
        }
        return $count;
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
