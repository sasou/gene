<?php
namespace Ext\Com\Db;
/**
 * Master and slave db Operation
 */
class Masterslave extends baseAbstract
{

    /**
     * Master MySQL Adapter
     *
     * @var Cola_Com_Db_Abstract
     */
    protected $_master = null;

    /**
     * Slave MySQL Adapter
     *
     * @var Cola_Com_Db_Abstract
     */
    protected $_slave = null;

    /**
     * Current MySQL Adapter
     *
     * @var Cola_Com_Db_Abstract
     */
    protected $_mysql = null;

    /**
     * Configuration
     *
     * @var array
     */
    public $config = array(
        'adapter' => 'Pdo_Mysql',
        'master' => array(
            'host' => '127.0.0.1',
            'port' => 3306,
            'user' => 'test',
            'password' => '',
            'database' => 'test',
            'charset' => 'utf8',
            'persistent' => false,
            'options' => array()
        ),
        'slave' => array(
            'host' => '127.0.0.1',
            'port' => 3306,
            'user' => 'test',
            'password' => '',
            'database' => 'test',
            'charset' => 'utf8',
            'persistent' => false,
            'options' => array()
        )
    );

    /**
     * Constructor.
     *
     * $config is an array of key/value pairs
     * containing configuration options.  These options are common to most adapters:
     *
     * database       => (string) The name of the database to user
     * user           => (string) Connect to the database as this username.
     * password       => (string) Password associated with the username.
     * host           => (string) What host to connect to, defaults to localhost
     *
     * Some options are used on a case-by-case basis by adapters:
     *
     * port           => (string) The port of the database
     * persistent     => (boolean) Whether to use a persistent connection or not, defaults to false
     * charset        => (string) The charset of the database
     *
     * @param  array $config
     */
    public function __construct($config)
    {
        $this->config = $config + $this->config;
    }

    /**
     * Master MySQL Adapter
     *
     * @return Cola_Com_Db_Abstract
     */
    public function master()
    {
        if ($this->_master) {
            return $this->_master;
        }

        $config = array(
            'adapter' => $this->config['adapter'],
            'params' => $this->config['master']
        );

        $this->_master = Cola_Com_Db::factory($config);
        $this->_slave = $this->_master;
        $this->_mysql = $this->_master;

        return $this->_master;
    }

    /**
     * Slave MySQL Adapter
     *
     * @param string $name
     * @return Cola_Com_Db_Abstract
     */
    public function slave($name = null)
    {
        if ($this->_slave) {
            return $this->_slave;
        }

        if (NULL === $name || empty($this->config['slave'][$name])) {
            $name = array_rand($this->config['slave']);
        }
        $params = $this->config['slave'][$name];

        $config = array(
            'adapter' => $this->config['adapter'],
            'params' => $params
        );

        $this->_slave = Cola_Com_Db::factory($config);
        $this->_mysql = $this->_slave;

        return $this->_slave;
    }

    /**
     * Returns the underlying database connection object or resource.
     * If not presently connected, this initiates the connection.
     *
     * @return object|resource|null
     */
    public function connect()
    {
        NULL === $this->_master && $this->master()->connect();
        NULL === $this->_slave && $this->slave()->connect();
        return $this->_mysql->conn;
    }

    /**
     * Set Debug or not
     *
     * @param boolean $flag
     * @return void
     */
    public function debug($flag = true)
    {
        if ($this->_master) {
            $this->_master->debug = $flag;
        }
        if ($this->_slave) {
            $this->_slave->debug = $flag;
        }
    }

    /**
     * Get or set log
     *
     * if $msg is null, then will return log
     *
     * @param string $msg
     * @return array|Cola_Com_Db_Abstract
     */
    public function log($msg = null)
    {
        if (NULL !== $msg) {
            $this->_mysql ? $this->_mysql->log = $msg : $this->log = $msg;
            return $this;
        }

        $log = array('default' => $this->log);
        if ($this->_master) {
            $log['master'] = $this->_master->log;
        }
        if ($this->_slave) {
            $log['master'] = $this->_slave->log;
        }
        return $log;
    }

    /**
     * Get SQL result
     *
     * @param string $sql
     * @param string $type
     * @return mixed
     */
    public function sql($sql, $type = 'ASSOC')
    {
        $tags = explode(' ', $sql, 2);
        switch (strtoupper(trim($tags[0]))) {
            case 'SELECT':
                return $this->slave()->sql($sql, $type);
            default:
                return $this->master()->sql($sql, $type);
        }
    }

    /**
     * Get a result row
     *
     * @param string $sql
     * @param string $type
     * @return array
     */
    public function row($sql, $type = 'ASSOC')
    {
        return $this->slave()->row($sql, $type);
    }

    /**
     * Get first column of result
     *
     * @param string $sql
     * @return string
     */
    public function col($sql)
    {
        return $this->slave()->col($sql);
    }

    /**
     * Find data
     *
     * @param array $conditions
     * @return array
     */
    public function find($conditions)
    {
        return $this->slave()->find($conditions);
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
        return $this->master()->insert($data, $table);
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
        return $this->master()->update($data, $where, $table);
    }

    /**
     * Delete from table
     *
     * @param string $where
     * @param string $table
     * @return int
     */
    public function delete($where, $table)
    {
        return $this->master()->delete($where, $table);
    }

    /**
     * Count num rows
     *
     * @param string $where
     * @param string $table
     * @return int
     */
    public function count($where, $table)
    {
        return $this->slave()->count($where, $table);
    }

    /**
     * Get error
     *
     * @return string|array
     */
    public function error()
    {
        return $this->_mysql->error();
    }

    /**
     * Close mysql connection
     *
     */
    public function close()
    {
        if ($this->_master) {
            $this->_master->close();
        }
        if ($this->_slave) {
            $this->_slave->close();
        }
    }

    /**
     * Query sql
     *
     * @param string $sql
     */
    public function _query($sql)
    {
        $this->_lastSql = $sql;

        $tags = explode(' ', $sql, 2);
        switch (strtoupper($tags[0])) {
            case 'SELECT':
                $this->slave()->_query($sql);
                break;
            default:
                $this->master()->_query($sql);
        }
    }

    /**
     * Return the rows affected of the last sql
     *
     * @return int
     */
    public function affectedRows()
    {
        return $this->_mysql->affectedRows();
    }

    /**
     * Fetch one row result
     *
     * @param string $type
     * @return mixd
     */
    public function fetch($type = 'ASSOC')
    {
        return $this->_mysql->fetch($type = 'ASSOC');
    }

    /**
     * Fetch All result
     *
     * @param string $type
     * @return array
     */
    public function fetchAll($type = 'ASSOC')
    {
        return $this->_mysql->fetchAll($type = 'ASSOC');
    }

    /**
     * Get last insert id
     *
     * @return int
     */
    public function lastInsertId()
    {
        return $this->master()->lastInsertId();
    }

    /**
     * Beging transaction
     *
     */
    public function beginTransaction()
    {
        return $this->master()->beginTransaction();
    }

    /**
     * Commit transaction
     *
     * @return boolean
     */
    public function commit()
    {
        return $this->master()->commit();
    }

    /**
     * Roll back transaction
     *
     * @return boolean
     */
    public function rollBack()
    {
        return $this->master()->rollBack();
    }

    /**
     * Free result
     *
     */
    public function free()
    {
        $this->_mysql->free();
    }

    /**
     * Escape string
     *
     * @param string $str
     * @return string
     */
    public function escape($str)
    {
        foreach (array($this->_master, $this->_slave) as $mysql) {
            if ($mysql) {
                return $mysql->escape($str);
            }
        }

        return addslashes($str);
    }

    /**
     * Magic get value
     *
     * @param string $key
     * @return mixed
     */
    public function __get($key)
    {
        switch ($key) {
            case 'master':
                return $this->master = $this->master();
            case 'slave':
                return $this->slave = $this->slave();
            default:
                throw new Cola_Com_Db_\Exception('Undefined property: ' . get_class($this) . '::' . $key);
        }
    }

}
