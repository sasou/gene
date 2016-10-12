<?php
namespace Ext\Com\Db\Pdo;
use \PDO;
/**
 * Pdo Mysql Operation
 */
class Mysql extends baseAbstract
{

    /**
     * Create a PDO object and connects to the database.
     *
     * @param array $config
     * @return PDO
     */
    public function connect()
    {
        if ($this->ping(FALSE)) {
            return $this->conn;
        }
        if ($this->config['persistent']) {
            $this->config['options'][PDO::ATTR_PERSISTENT] = true;
            $this->config['options'][PDO::ATTR_ERRMODE] = PDO::ERRMODE_Exception;
        }
        $this->config['options'][PDO::ATTR_EMULATE_PREPARES] = false;
        $this->conn = new PDO("mysql:host={$this->config['host']};port={$this->config['port']};dbname={$this->config['database']};charset={$this->config['charset']}", $this->config['user'], $this->config['password'], $this->config['options']);
        return $this->conn;
    }

}
