<?php
namespace Ext\Com\Db\Pdo;
use \PDO;
/**
 * Pdo sqlite Operation
 */
class Sqlite extends baseAbstract
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
        $this->conn = new PDO("sqlite:{$this->config['database']}", $this->config['user'], $this->config['password'], $this->config['options']);
        return $this->conn;
    }

}
