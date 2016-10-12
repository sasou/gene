<?php
namespace Ext\Com\Db\Pdo;
use \PDO;
/**
 * Pdo Db Abstract
 */
abstract class baseAbstract extends \Ext\Com\Db\baseAbstract
{

    /**
     * PDOStatement
     * @var PDOStatement
     */
    public $stmt = null;
    public $operator = array('=', '!=', '>', '<', '>=', '<=', 'BETWEEN', 'LIKE');

    /**
     * Select Database
     *
     * @param string $database
     * @return boolean
     */
    public function selectDb($database)
    {
        return $this->query("use $database;");
    }

    /**
     * Close mysql connection
     *
     */
    public function close()
    {
        $this->conn = null;
    }

    /**
     * Free result
     *
     */
    public function free()
    {
        $this->query = null;
    }

    /**
     * Free stmt
     *
     */
    public function freeStmt()
    {
        $this->stmt = null;
    }

    /**
     * Load data
     *
     * @param int $id colunm value
     * @param string $col column key
     * @return array
     */
    public function load($id, $col, $table)
    {
        $sql = "select * from {$table} where {$col} = ? limit 1;";
        return $this->getRow($sql, array($id));
    }

    /**
     * _update
     *
     * @param int $id column value
     * @param array $data update data
     * @param string $pk column name, not column key defalut primary key.
     * @param string $table table name.
     * @return mixed
     */
    public function _update($id, $data, $pk, $table)
    {
        $where = $pk . '=' . (is_int($id) ? $id : "'$id'");
        try {
            return $this->doUpdate($table, $data, $where);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
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
        return $this->doInsert($table, $data);
    }

    /**
     * _delete
     *
     * @param int $id column value
     * @param string $col column name, not column key defalut primary key.
     * @param string $table table name.
     * @return boolean
     */
    public function _delete($id, $col, $table)
    {
        $sql = "DELETE FROM {$table} WHERE {$col}= ?";
        return $this->sql($sql, array($id));
    }

    /**
     * Find data
     *
     * @param array $opts
     * @return array
     * @deprecated 2.1
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
        $wheres = $this->parseWhere($opts['where']);
        $sql = "select {$opts['fileds']} from {$opts['table']} where {$wheres['sql']}";

        if ($opts['order']) {
            $sql .= " order by {$opts['order']}";
        }

        if (0 <= $opts['start'] && 0 <= $opts['limit']) {
            $sql .= " limit {$opts['start']}, {$opts['limit']}";
        }
        return $this->sql($sql, $wheres['value']);
    }

    /**
     * Get a result row
     *
     * @param string $sql
     * @param array $data
     * @return array
     */
    public function row($sql, $data = array())
    {
        return $this->getRow($sql, $data);
    }

    /**
     * Get first column of result
     *
     * @param string $sql
     * @return string
     */
    public function col($sql, $data = array())
    {
        return $this->getCol($sql, $data);
    }

    /**
     * Count num rows
     *
     * @param string $where
     * @param string $table
     * @return int
     * @deprecated 2.1
     */
    public function count($where, $table)
    {
        return $this->getCount($table, $where);
    }

    /**
     * Query sql
     *
     * @param string $sql
     * @param array $data
     * @return resource
     */
    public function query($sql, $data = array())
    {
        return $this->doQuery($sql, $data);
    }

    /**
     * parseWhere
     *
     * @param string $where
     * @return array
     */
    public function parseWhere($where)
    {
        $values = array();
        $strWhere = '';
        if (!empty($where) && is_string($where)) {
            $where = str_ireplace(array(' and ', ' or '), array(' AND ', ' OR '), trim($where));
            if (false === strpos($where, ' OR ')) {
                if (false !== strpos($where, ' AND ')) {
                    foreach (explode('AND', $where) as $andlist) {
                        $data = explode('=', $andlist);
                        $strWhere .= "{$data[0]}=? AND";
                        $values[] = $this->stripQuotes($data[1]);
                    }
                } else {
                    $data = explode('=', $where);
                    $strWhere .= "{$data[0]}=? AND";
                    $values[] = $this->stripQuotes($data[1]);
                }
            }
        }
        $strWhere = ($strWhere == '') ? $where : rtrim($strWhere, 'AND');
        $result = array('sql' => $strWhere, 'value' => $values);
        return $result;
    }

    /**
     * Get SQL result
     *
     * @param string $sql
     * @param array $data
     * @param string $type
     * @return mixed
     */
    public function sql($sql, $data = array(), $type = 'ASSOC')
    {
        return $this->doSql($sql, $data);
    }

    /**
     * Query sql
     *
     * @param string $sql
     * @return Cola_Com_Db_Mysql
     * @deprecated 2.1
     */
    protected function _query($sql)
    {
        try {
            $tags = explode(' ', trim($sql), 2);
            switch (strtoupper(trim($tags[0]))) {
                case 'SELECT':
                    return $this->conn->query($sql);
                case 'INSERT':
                case 'UPDATE':
                case 'DELETE':
                    $this->exec($sql);
                    return $this->query;
                default:
                    return $this->conn->query($sql);
            }
        } catch (\Exception $ex) {
            $this->_throwException();
        }
    }

    /**
     * do query
     * @param type $sql
     * @param type $data
     * @return boolean
     * @deprecated 2.1
     */
    public function exec($sql, $data = array())
    {
        $this->query = $this->conn->prepare($sql);
        if (FALSE !== $this->query->execute($data)) {
            return TRUE;
        }
        $this->_throwException();
    }

    /**
     * Return the rows affected of the last sql
     *
     * @return int
     * @deprecated 2.1
     */
    public function affectedRows()
    {
        return $this->query->rowCount();
    }

    /**
     * Get pdo fetch style
     *
     * @param string $style
     * @return int
     */
    protected static function _getFetchStyle($style)
    {
        switch ($style) {
            case 'ASSOC':
                $style = PDO::FETCH_ASSOC;
                break;
            case 'BOTH':
                $style = PDO::FETCH_BOTH;
                break;
            case 'NUM':
                $style = PDO::FETCH_NUM;
                break;
            case 'OBJECT':
                $style = PDO::FETCH_OBJECT;
                break;
            default:
                $style = PDO::FETCH_ASSOC;
        }

        return $style;
    }

    /**
     * Fetch one row result
     *
     * @param string $type
     * @return mixd
     * @deprecated 2.1
     */
    public function fetch($type = 'ASSOC')
    {
        return $this->query->fetch(self::_getFetchStyle(strtoupper($type)));
    }

    /**
     * Fetch All result
     *
     * @param string $type
     * @return array
     * @deprecated 2.1
     */
    public function fetchAll($type = 'ASSOC')
    {
        $result = $this->query->fetchAll(self::_getFetchStyle(strtoupper($type)));
        $this->free();
        return $result;
    }

    /**
     * Initiate a transaction
     *
     * @return boolean
     */
    public function beginTransaction()
    {
        $this->ping();
        return $this->conn->beginTransaction();
    }

    /**
     * Commit a transaction
     *
     * @return boolean
     */
    public function commit()
    {
        return $this->conn->commit();
    }

    /**
     * Roll back a transaction
     *
     * @return boolean
     */
    public function rollBack()
    {
        return $this->conn->rollBack();
    }

    /**
     * Get the last inserted ID.
     *
     * @return integer
     */
    public function lastInsertId($name = null)
    {
        $last = $this->conn->lastInsertId($name);
        if (false === $last) {
            return false;
        } else if ('0' === $last) {
            return true;
        } else {
            return intval($last);
        }
    }

    /**
     * Escape string
     *
     * @param string $str
     * @return string
     * @deprecated 2.1
     */
    public function escape($str)
    {
        $this->ping();
        return $this->conn->quote($str);
    }

    /**
     * Get error
     *
     * @return array
     */
    public function error()
    {
        if (is_object($this->query) && $this->query->errorCode()) {
            $errno = $this->query->errorCode();
            $error = $this->query->errorInfo();
        } else {
            $errno = $this->conn->errorCode();
            $error = $this->conn->errorInfo();
        }
        return array('code' => $errno, 'msg' => $error[2]);
    }

    /**
     * Ping mysql server
     *
     * @param boolean $reconnect
     * @return boolean
     */
    public function ping($reconnect = true)
    {
        if ($this->conn && $this->conn->query('select 1')) {
            return true;
        }

        if ($reconnect) {
            $this->close();
            $this->connect();
            return $this->ping(false);
        }

        return false;
    }

    /**
     * bing value
     * @param string $sql prepare sql
     * @param array $array bind array data
     * @param array $typeArray bind data type
     */
    public function bindSql($sql, $array, $typeArray = false)
    {
        $this->lastSql = $sql;
        $this->ping();
        $this->query = $this->conn->prepare($sql);

        if (is_object($this->query) && ($this->query instanceof PDOStatement)) {
            foreach ($array as $key => $value) {
                if ($typeArray) {
                    $this->query->bindValue(":{$key}", $value, $typeArray[$key]);
                } else {
                    if (is_int($value)) {
                        $param = PDO::PARAM_INT;
                    } elseif (is_bool($value)) {
                        $param = PDO::PARAM_BOOL;
                    } elseif (is_null($value)) {
                        $param = PDO::PARAM_NULL;
                    } elseif (is_string($value)) {
                        $param = PDO::PARAM_STR;
                    } else {
                        $param = false;
                    }
                    (FALSE !== $param) && $this->query->bindValue(":{$key}", $value, $param);
                }
            }

            if ($this->query->execute()) {
                $tags = explode(' ', $sql, 2);
                switch (strtoupper(trim($tags[0]))) {
                    case 'SELECT':
                        ($result = $this->fetchAll()) || ($result = array());
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
                $this->free();
                return $result;
            } else {
                $this->free();
                $this->_throwException();
            }
        }

        return FALSE;
    }

    /**
     * get a row data
     * @param string $sql
     * @param maxed $data
     * @return false|array
     */
    public function getRow($sql, $data)
    {
        if (!is_array($data)) {
            $data = array($data);
        }
        $result = $this->doQuery($sql, $data)->fetch(PDO::FETCH_ASSOC);
        $this->freeStmt();
        return $result ? : false;
    }

    /**
     * get a row data
     * @param string $sql
     * @param maxed $data
     * @return false|array
     */
    public function getCol($sql, $data)
    {
        if (!is_array($data)) {
            $data = array($data);
        }
        $result = $this->doQuery($sql, $data)->fetch(PDO::FETCH_ASSOC);
        $this->freeStmt();
        return empty($result) ? NULL : current($result);
    }

    /**
     * Count num rows
     *
     * @param string $table
     * @param string $where
     * @return int
     */
    public function getCount($table, $where)
    {
        $values = array();
        $bWhere = $where;
        $strWhere = '';
        $wheres = array();
        if (is_string($where)) {
            $where = str_ireplace(array(' and ', ' or '), array(' AND ', ' OR '), trim($where));
            if (false === strpos($where, ' OR ')) {
                if (false !== strpos($where, ' AND ')) {
                    $andWhere = array();
                    foreach (explode('AND', $where) as $andlist) {
                        list($key, $value) = explode('=', $andlist);
                        $andWhere[$key] = $value;
                    }
                    $wheres = $andWhere;
                } else {
                    list($key, $value) = explode('=', $where);
                    $wheres[$key] = $value;
                }
            }
        }
        if (!empty($wheres)) {
            foreach ($wheres as $key => $list) {
                $strWhere .= "{$key}=? AND";
                $values[] = $this->stripQuotes($list);
            }
        }

        $bWhere = ($strWhere == '') ? $where : rtrim($strWhere, 'AND');

        $sql = "select count(1) as CNT from {$table} where {$bWhere};";
        $result = $this->doQuery($sql, $values)->fetchColumn();
        $this->freeStmt();
        return intval($result);
    }

    /**
     * Insert
     *
     * @param string $table
     * @param array $data
     * @return boolean
     */
    public function doInsert($table, $data)
    {
        $keys = array();
        $marks = array();
        $values = array();
        foreach ($data as $key => $val) {
            is_array($val) && ($val = json_encode($val, JSON_UNESCAPED_UNICODE));
            $keys[] = "`{$key}`";
            $marks[] = '?';
            $values[] = $val;
        }

        $keys = implode(',', $keys);
        $marks = implode(',', $marks);
        $sql = "insert into {$table} ({$keys}) values ({$marks});";
        return $this->doSql($sql, $values);
    }

    /**
     * Multiple insert
     *
     * @param string $table
     * @param array $rows
     * @return boolean
     */
    public function minsert($table, $rows)
    {
        if (empty($rows)) {
            return true;
        }
        $bindOne = array_fill(0, count($rows[0]), '?');
        $bindAll = array_fill(0, count($rows), implode(',', $bindOne));
        $bind = '(' . implode('),(', $bindAll) . ')';
        $keys = array_keys($rows[0]);
        $values = array();
        foreach ($rows as $row) {
            foreach ($keys as $key) {
                $value = is_array($row[$key]) ? json_encode($row[$key], JSON_UNESCAPED_UNICODE) : $row[$key];
                $values[] = $value;
            }
        }
        if (is_int($keys[0])) {
            $fields = '';
        } else {
            $fields = ' (`' . implode('`,`', $keys) . '`) ';
        }

        $sql = "insert into {$table}{$fields} values {$bind}";
        return $this->doSql($sql, $values);
    }

    /**
     * Update table
     *
     * @param string $table
     * @param array $data
     * @param string $where
     * @return int
     */
    public function doUpdate($table, $data, $where = '0')
    {
        $keys = array();
        $values = array();
        foreach ($data as $key => $val) {
            is_array($val) && ($val = json_encode($val, JSON_UNESCAPED_UNICODE));
            $keys[] = "`{$key}`=?";
            $values[] = $val;
        }
        $keys = implode(',', $keys);
        if (is_string($where)) {
            $where = array($where, array());
        }
        $values = array_merge($values, $where[1]);
        $sql = "update {$table} set {$keys} where {$where[0]}";
        return $this->doSql($sql, $values);
    }

    /**
     * Fetch All result
     *
     * @param string $style
     * @return array
     */
    public function getfetchAll($style = PDO::FETCH_ASSOC)
    {
        $result = $this->stmt->fetchAll($style);
        $this->free();
        return $result;
    }

    public function doSql($sql, $data = array())
    {
        $this->doQuery($sql, $data);
        $tags = explode(' ', $sql, 2);
        switch (strtoupper($tags[0])) {
            case 'SELECT':
                $result = $this->getfetchAll();
                break;
            case 'INSERT':
                $result = $this->lastInsertId();
                break;
            case 'UPDATE':
            case 'DELETE':
                $result = (0 <= $this->stmt->rowCount());
                break;
            default:
                $result = $this->stmt;
        }
        return $result;
    }

    /**
     * prepare PDOStatement
     * @param string $sql
     * @param array $data
     * @return PDOStatement
     */
    public function doQuery($sql, $data)
    {
        $this->log[] = array('time' => date('Y-m-d H:i:s'), 'sql' => $sql, 'data' => $data);
        $this->stmt = $this->getPdo()->prepare($sql);
        $this->stmt->execute($data);
        return $this->stmt;
    }

    /**
     * get pdo
     * @return PDO
     */
    public function getPdo()
    {
        if (null === $this->conn) {
            $this->connect();
        }
        return $this->conn;
    }

    /**
     * strip quotes
     * @param string $str
     * @return string
     */
    public function stripQuotes($str)
    {
        $str = ltrim($str, '"');
        $str = rtrim($str, '"');
        $str = ltrim($str, "'");
        $str = rtrim($str, "'");
        return $str;
    }

}
