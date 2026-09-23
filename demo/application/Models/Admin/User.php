<?php
namespace Models\Admin;

/**
 * User Model — Gene\Orm\Model ActiveRecord
 *
 * @author  sasou
 * @version  2.0
 */
class User extends \Gene\Orm\Model
{
    /** @var string C 层声明为无类型 static，子类声明不得加类型（PHP 继承规则） */
    protected static $table = 'sys_user';
    /** @var string */
    protected static $primaryKey = 'user_id';
    /** @var string[] */
    protected static $fields = [
        'user_id', 'user_name', 'user_realname', 'user_icon', 'group_id', 'status',
    ];

    /** @var array<string,string> 版本键 => 行内列名 */
    protected static $versionKeys = [
        'db.sys_user.user_id' => 'user_id',
        'db.sys_user.user_name' => 'user_name',
    ];

    /**
     * lists — 兼容旧 Service / 缓存回调
     */
    function lists($params, $page, $limit)
    {
        return static::page($params ?: [], (int) $page, (int) $limit);
    }

    /**
     * row — 兼容 cachedVersion(["\\Models\\Admin\\User", "row"], ...)
     */
    function row($id)
    {
        return static::find($id);
    }

    /**
     * getField
     */
    public function getField($id, $field = 'user_name')
    {
        $data = $this->row($id);
        return isset($data[$field]) ? $data[$field] : '';
    }

    /**
     * add
     */
    function add($data)
    {
        return static::create($data);
    }

    /**
     * edit
     */
    function edit($id, $data)
    {
        return static::updateBy($id, $data);
    }

    /**
     * status — 非常规 SQL，仍走 $this->db
     */
    function status($id)
    {
        return static::flip($id, 'status');
    }

    /**
     * del
     */
    function del($id)
    {
        return static::destroy($id);
    }

    /**
     * delAll
     */
    function delAll($id_arr)
    {
        return static::destroyAll($id_arr);
    }

    /**
     * countByGroupId
     */
    function countByGroupId($id_arr)
    {
        return static::query()
            ->in('group_id in(?)', (array) $id_arr)
            ->count();
    }

    /**
     * 按主键取登录名（直连库，供缓存失效用）
     */
    function userNameById($id)
    {
        $id = (int) $id;
        if ($id <= 0) {
            return '';
        }
        $v = $this->db
            ->select('sys_user', 'user_name')
            ->where('user_id=?', $id)
            ->limit(1)
            ->cell();
        return $v !== null && $v !== false ? (string) $v : '';
    }

    /**
     * 批量主键取登录名
     */
    function userNamesByIds(array $id_arr)
    {
        if (!$id_arr) {
            return [];
        }
        $list = $this->db
            ->select('sys_user', 'user_name')
            ->in('user_id in(?)', $id_arr)
            ->all();
        $names = [];
        foreach ($list as $row) {
            if (!empty($row['user_name'])) {
                $names[] = $row['user_name'];
            }
        }
        return array_values(array_unique($names));
    }

    /**
     * 检查登录（多表 join，逃生舱）
     */
    function getUserInfoByName($username)
    {
        return static::query()
            ->fields('sys_user.user_id,sys_user.user_name,sys_user.user_pass,sys_user.user_salt,sys_user.user_realname,sys_user.user_icon,sys_user.group_id,sys_group.group_title,sys_user.status')
            ->join('sys_group', ['sys_group.group_id' => 'sys_user.group_id'], 'left')
            ->where('sys_user.user_name=?', $username)
            ->row();
    }
}
