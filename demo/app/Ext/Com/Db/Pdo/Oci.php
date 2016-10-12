<?php
namespace Ext\Com\Db\Pdo;
use \PDO;
/**
 * Pdo Oracle Operation
 */
class Oci extends baseAbstract
{

    /**
     * Create a PDO object and connects to the database.
     *
     * @param array $config
     * @return resource
     */
    public function connect()
    {
        if ($this->ping(false)) {
            return $this->conn;
        }

        if ($this->config['persistent']) {
            $this->config['options'][PDO::ATTR_PERSISTENT] = true;
        }

        $dns = "(DESCRIPTION =
        (ADDRESS_LIST =
          (ADDRESS = (PROTOCOL = TCP)(HOST = {$this->config['host']})(PORT = {$this->config['port']}))
        )
        (CONNECT_DATA =
          (SERVICE_NAME = {$this->config['database']})
        )
        (CHARSET={$this->config['charset']})
      )";
        $this->conn = new PDO("oci:dbname={$dns};", $this->config['user'], $this->config['password'], $this->config['options']);
        return $this->conn;
    }

    /**
     * Ping oracle server
     *
     * @param boolean $reconnect
     * @return boolean
     */
    public function ping($reconnect = true)
    {
        if ($this->conn && $this->conn->query('select 1 from dual')) {
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
            $keys .= "{$key},";
            $values .= $this->escape($value) . ',';
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
            $tmp[] = "{$key}=" . $this->escape($value);
        }

        $str = implode(',', $tmp);

        $sql = "update {$table} set {$str} where {$where}";

        return $this->sql($sql);
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
     * Get the last inserted ID.
     *
     * @return integer
     */
    public function lastInsertId()
    {
        return $this->query->rowCount();
    }

}
