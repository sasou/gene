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
    protected static string $table = 'sys_user';
    protected static string $primaryKey = 'user_id';
    protected static array $fields = [
        'user_id', 'user_name', 'user_realname', 'user_icon', 'group_id', 'status',
    ];

    /**
     * lists — 兼容旧 Service / 缓存回调
     */
    function lists($params, $start, $limit)
    {
        return static::paginate($params ?: [], (int) $start, (int) $limit);
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
        return $this->db
            ->sql('update sys_user set status=abs(status-1)')
            ->where('user_id=?', $id)
            ->affectedRows();
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
        return $this->db->sql("select 
                                    a.user_id,a.user_name,a.user_pass,a.user_salt,a.user_realname,a.user_icon,
                                    a.group_id,b.group_title,a.status 
                                from 
                                    sys_user a 
                                left join 
                                    sys_group b on b.group_id=a.group_id 
                                where 
                                    a.user_name=?", $username)->row();
    }
}
