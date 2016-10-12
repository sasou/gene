<?php
namespace Ext\Com\Db;
/**
 * oracle operate
 */
class Oci extends baseAbstract
{

    private $_beginTransaction = FALSE;

    /**
     * Connect to Oracle
     *
     * @return resource connection
     */
    public function connect()
    {
        if (!extension_loaded('oci8')) {
            throw new Cola_Com_Db_\Exception('Can not find oci extension.');
        }

        $func = ($this->config['persistent']) ? 'oci_pconnect' : 'oci_connect';

        $this->conn = $func($this->config['user'], $this->config['password'], "(DESCRIPTION = (ADDRESS = (PROTOCOL = TCP)(HOST ={$this->config['host']})(PORT ={$this->config['port']})) (CONNECT_DATA = (SERVER = DEDICATED) (SERVICE_NAME ={$this->config['database']})))", $this->config['charset']);
        if (is_resource($this->conn)) {
            return $this->conn;
        }
        $this->_throw\Exception();
    }

    /**
     * Close connection
     *
     */
    public function close()
    {
        if ($this->query) {
            oci_free_statement($this->query);
        }
        if (is_resource($this->conn)) {
            oci_close($this->conn);
        }
    }

    /**
     * Free result
     *
     */
    public function free()
    {
        return oci_free_statement($this->query);
    }

    /**
     * _query sql
     *
     * @param string $sql
     * @return Cola_Com_Db_Oci
     */
    protected function _query($sql)
    {
        $this->ping();
        $this->_writeLog($sql);
        $this->query = NULL;
        $this->query = oci_parse($this->conn, $sql);
        if ($this->query) {

            if (FALSE === $this->_beginTransaction && FALSE === oci_execute($this->query, OCI_COMMIT_ON_SUCCESS)) {
                $this->_throw\Exception();
            }
            if (TRUE === $this->_beginTransaction && FALSE === oci_execute($this->query, OCI_DEFAULT)) {
                $this->rollBack();
                $this->_throw\Exception();
            }
            return $this->query;
        }
        $this->_throw\Exception();
    }

    /**
     * Return the rows affected of the last sql
     *
     * @return int
     */
    public function affectedRows()
    {
        return oci_num_rows($this->query);
    }

    /**
     * Fetch one row result
     *
     * @param string $type
     * @return mixd
     */
    public function fetch($type = 'ASSOC')
    {
        $type = strtoupper($type);

        switch ($type) {
            case 'ASSOC':
                $func = 'oci_fetch_assoc';
                break;
            case 'NUM':
                $func = 'oci_fetch_row';
                break;
            case 'OBJECT':
                $func = 'oci_fetch_object';
                break;
            default:
                $func = 'oci_fetch_array';
        }

        return $func($this->query);
    }

    /**
     * Fetch All result
     *
     * @param string $type
     * @return array
     */
    public function fetchAll($type = 'ASSOC')
    {
        switch ($type) {
            case 'ASSOC':
                $func = 'oci_fetch_assoc';
                break;
            case 'NUM':
                $func = 'oci_fetch_row';
                break;
            case 'OBJECT':
                $func = 'oci_fetch_object';
                break;
            default:
                $func = 'oci_fetch_array';
        }
        $result = array();
        while ($row = $func($this->query)) {
            $result[] = $row;
        }
        oci_free_statement($this->query);
        return $result;
    }

    /**
     * 该函数执行一个SQL语句（例如创建表或者修改添加字段等）
     * @param string $sql 要执行的SQL语句；
     * @return boolean execute result or error.
     */
    public function execute($sql)
    {
        $this->_writeLog($sql);
        $this->lastSql = $sql . '@' . date('Y-m-d H:i:s');
        if ($this->debug) {
            $this->log[] = $this->lastSql;
        }
        $this->query = NULL;
        $this->query = oci_parse($this->conn, $sql);
        if ($this->query) {
            if (FALSE === $this->_beginTransaction && FALSE === oci_execute($this->query, OCI_COMMIT_ON_SUCCESS)) {
                $this->_throw\Exception();
            }
            if (TRUE === $this->_beginTransaction && FALSE === oci_execute($this->query, OCI_DEFAULT)) {
                $this->rollBack();
                $this->_throw\Exception();
            }
            return $this->query;
        }

        $this->_throw\Exception();
    }

    /**
     * 执行SQL并返回数组——分页；注意排序字段如果不是唯一的请在后面加一个唯一的字段，否则数据可能有问题
     * @param string $sql 要执行的SQL语句；
     * @param int $page 第几页
     * @param int $pageNum 每页多少条
     * @return array
     */
    public function pageQuery($sql, $page, $pageNum = 10)
    {
        $this->lastSql = $sql . '@' . date('Y-m-d H:i:s');
        if ($this->debug) {
            $this->log[] = $this->lastSql;
        }
        $page = intval($page);
        if ($page < 1) {
            $page = 1;
        }
        $pageNum = intval($pageNum);
        $start = ($page - 1) * $pageNum;
        $sql = "SELECT z2.*
            FROM (
                SELECT z1.*, ROWNUM AS \"db_rownum\"
                FROM (
                    " . $sql . "
                ) z1
            ) z2
            WHERE z2.\"db_rownum\" BETWEEN " . ($start + 1) . " AND " . ($start + $pageNum);

        if ($this->_query($sql)) {
            return $this->fetchAll();
        }

        $this->_throw\Exception();
    }

    /**
     * Find data
     *
     * @param array $opts
     * @return array
     */
    public function find($opts)
    {
        if (is_string($opts)) {
            $opts = array('where' => $opts);
        }

        $opts = $opts + array(
            'fileds' => '*',
            'where' => 1,
            'order' => null,
            'start' => -1,
            'limit' => -1
        );

        $sql = "select {$opts['fileds']} from {$opts['table']} where {$opts['where']}";

        if ($opts['order']) {
            $sql .= " order by {$opts['order']}";
        }

        if (0 <= $opts['start'] && 0 <= $opts['limit']) {
            $sql = "SELECT z2.*
            FROM (
                SELECT z1.*, ROWNUM AS \"db_rownum\"
                FROM (
                    " . $sql . "
                ) z1
            ) z2
            WHERE z2.\"db_rownum\" BETWEEN " . ($opts['start'] + 1) . " AND " . ($opts['start'] + $opts['limit']);
        }

        return $this->sql($sql);
    }

    /**
     * 绑定变量基本方法
     * @param string $sql 要执行的SQL语句
     * @param array $bindData 绑定数据
     * @return int 返回影响的记录
     */
    public function dbQueryBind($sql, array $bindData)
    {
        $this->ping();
        $this->lastSql = $sql . '@' . date('Y-m-d H:i:s');
        if ($this->debug) {
            $this->log[] = $this->lastSql;
        }
        $this->_writeLog($sql, $bindData);
        $this->query = oci_parse($this->conn, $sql);
        foreach ($bindData as $key => $val) {
            oci_bind_by_name($this->query, ":{$key}", $val);
        }

        $result = $this->_beginTransaction ? oci_execute($this->query, OCI_DEFAULT) : oci_execute($this->query, OCI_COMMIT_ON_SUCCESS);
        if ($result) {
            return $this->query;
        }
        $this->_throw\Exception();
    }

    /**
     * 绑定变量，返回多行
     * @param string $query 要执行的SQL语句
     * @param array $bindData  变量
     * @return array
     */
    public function queryBind($query, $bindData)
    {
        $this->ping();
        if ($this->dbQueryBind($query, $bindData)) {
            $tags = explode(' ', $query, 2);
            switch (strtoupper(trim($tags[0]))) {
                case 'SELECT':
                    $result = array();
                    oci_fetch_all($this->query, $result, 0, -1, OCI_FETCHSTATEMENT_BY_ROW);
                    break;
                case 'INSERT':
                    $result = $this->lastInsertId();
                    break;
                case 'UPDATE':
                case 'DELETE':
                    $result = $this->affectedRows();
                    break;
                default:
                    $result = $this->query;
            }
            return $result;
        }

        $this->_throw\Exception();
    }

    /**
     * Insert
     *
     * @param array $data
     * @param string $table
     * @return boolean
     */
    public function insert($data, $table)
    {
        $keys = '';
        $values = '';
        foreach ($data as $key => $value) {
            $keys .= "$key,";
            $values .= "'" . $this->escape($value) . "',";
        }
        $sql = "insert into $table (" . substr($keys, 0, -1) . ") values (" . substr($values, 0, -1) . ")";
        return $this->sql($sql);
    }

    /**
     * Update table
     *
     * @param array $data
     * @param string $where
     * @param string $table
     * @return int
     */
    public function update($data, $where, $table)
    {
        $tmp = array();

        foreach ($data as $key => $value) {
            $tmp[] = "$key=" . "'" . $this->escape($value) . "'";
        }

        $str = implode(',', $tmp);

        $sql = "update {$table} set {$str} where {$where}";

        return $this->sql($sql);
    }

    /**
     * Get last insert id
     *
     * @return int
     */
    public function lastInsertId()
    {
        return oci_num_rows($this->query);
    }

    /**
     * Beging transaction
     *
     */
    public function beginTransaction()
    {
        $this->_beginTransaction = TRUE;
    }

    /**
     * Commit transaction
     *
     * @return boolean
     */
    public function commit()
    {
        $this->_beginTransaction = FALSE;
        return oci_commit($this->conn);
    }

    /**
     * Roll back transaction
     *
     * @return boolean
     */
    public function rollBack()
    {
        $this->_beginTransaction = FALSE;
        return oci_rollback($this->conn);
    }

    /**
     * Escape string
     *
     * @param string $str
     * @return string
     */
    public function escape($str)
    {
        return str_replace("'", "''", $str);
    }

    /**
     *  Ping oci server
     *
     * @param boolean $reconnect
     * @return boolean
     */
    public function ping($reconnect = TRUE)
    {
        if ($this->conn) {
            return true;
        }
        if ($reconnect) {
            $this->close();
            $this->connect();
            return $this->ping(FALSE);
        }

        return false;
    }

    /**
     * Get error
     *
     * @return array
     */
    public function error()
    {
        if ($error = oci_error($this->conn)) {
            return array('code' => intval($error['code']), 'msg' => $error['message']);
        }
        if ($error = oci_error($this->query)) {
            return array('code' => intval($error['code']), 'msg' => $error['message']);
        }
    }

    /**
     * 写日志函数
     * @param string $query
     * @param array $data 绑定类型数据
     */
    private function _writeLog($query, $data = array())
    {
        $this->ping();
        if (!empty($data)) {
            foreach ($data as $key => $val) {
                $query = str_replace($key, $val, $query);
            }
        }
        //排除查询记录
        $tags = explode(' ', $query, 2);
        if ('SELECT' == strtoupper(trim($tags[0]))) {
            return;
        }
        $query = addslashes(str_replace(array("'", '"'), '', $query));
        $opUrl = $_SERVER['PHP_SELF'] . '?' . $_SERVER['QUERY_STRING'];
        $logQuery = "insert into e_operate_log(FILEPATH,MSG,USERS,IP,TIME)values('" . $opUrl . "','" . $query . "','" . $_SESSION['dzdauserid'] . "','" . $_SERVER['REMOTE_ADDR'] . "',sysdate)";

        $this->query = oci_parse($this->conn, $logQuery);
        if (oci_execute($this->query)) {
            return oci_num_rows($this->query);
        }
        $this->_throw\Exception();
    }

}
