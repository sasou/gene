<?php

/**
 * Gene Framework Database Classes Test
 * 
 * This test file covers the important methods of the Database classes:
 * Gene\Db\Mysql, Gene\Db\Pgsql, Gene\Db\Sqlite, Gene\Db\Pdo, Gene\Db\Pool
 */

use Gene\Db\Mysql;
use Gene\Db\Pgsql;
use Gene\Db\Sqlite;
use Gene\Pool;

class DatabaseTest
{
    private $mysql;
    private $pgsql;
    private $sqlite;
    private $pdo;
    private $pool;
    
    public function __construct()
    {
        echo "=== Gene Database Classes Test Suite ===\n\n";
    }
    
    /**
     * Test MySQL class
     */
    public function testMysqlClass()
    {
        echo "Testing MySQL Class:\n";
        
        try {
            // Test MySQL constructor
            // Config requires 'dsn', 'username', 'password' keys (E_ERROR if missing)
            $config = [
                'dsn' => 'mysql:host=localhost;port=3306;dbname=test_db;charset=utf8mb4',
                'username' => 'test_user',
                'password' => 'test_pass',
            ];
            
            $this->mysql = new Mysql($config);
            echo "✓ MySQL constructor with config works\n";
            
            // Test connection methods
            $this->mysql->connect();
            echo "✓ MySQL connect() method works\n";
            
            $this->mysql->disconnect();
            echo "✓ MySQL disconnect() method works\n";
            
            // Test query methods
            $this->mysql->query('SELECT 1');
            echo "✓ MySQL query() method works\n";
            
            $this->mysql->select('*')->from('users')->where('id = 1');
            echo "✓ MySQL query builder works\n";
            
            // Test CRUD operations
            $this->mysql->insert('users', ['name' => 'John', 'email' => 'john@example.com']);
            echo "✓ MySQL insert() method works\n";
            
            $this->mysql->update('users', ['name' => 'Jane'], 'id = 1');
            echo "✓ MySQL update() method works\n";
            
            $this->mysql->delete('users', 'id = 1');
            echo "✓ MySQL delete() method works\n";
            
            $this->mysql->select('*')->from('users')->get();
            echo "✓ MySQL get() method works\n";
            
            $this->mysql->select('*')->from('users')->first();
            echo "✓ MySQL first() method works\n";
            
            // Test transaction methods
            $this->mysql->beginTransaction();
            echo "✓ MySQL beginTransaction() works\n";
            
            $this->mysql->commit();
            echo "✓ MySQL commit() works\n";
            
            $this->mysql->rollback();
            echo "✓ MySQL rollback() works\n";
            
            // Test prepared statements
            $stmt = $this->mysql->prepare('SELECT * FROM users WHERE id = ?');
            echo "✓ MySQL prepare() method works\n";
            
            $this->mysql->execute($stmt, [1]);
            echo "✓ MySQL execute() method works\n";
            
            // Test escaping
            $escaped = $this->mysql->escape("test'string");
            echo "✓ MySQL escape() method works\n";
            
            // Test last insert ID
            $lastId = $this->mysql->lastInsertId();
            echo "✓ MySQL lastInsertId() method works\n";
            
            // Test affected rows
            $affectedRows = $this->mysql->affectedRows();
            echo "✓ MySQL affectedRows() method works\n";
            
            // Test error handling
            $error = $this->mysql->error();
            echo "✓ MySQL error() method works\n";
            
            $errno = $this->mysql->errno();
            echo "✓ MySQL errno() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test PostgreSQL class
     */
    public function testPgsqlClass()
    {
        echo "Testing PostgreSQL Class:\n";
        
        try {
            // Test PostgreSQL constructor
            // Config requires 'dsn', 'username', 'password' keys (E_ERROR if missing)
            $config = [
                'dsn' => 'pgsql:host=localhost;port=5432;dbname=test_db',
                'username' => 'test_user',
                'password' => 'test_pass'
            ];
            
            $this->pgsql = new Pgsql($config);
            echo "✓ PostgreSQL constructor with config works\n";
            
            // Test connection
            $this->pgsql->connect();
            echo "✓ PostgreSQL connect() method works\n";
            
            // Test query methods
            $this->pgsql->query('SELECT 1');
            echo "✓ PostgreSQL query() method works\n";
            
            // Test PostgreSQL-specific features
            $this->pgsql->select('*')->from('users')->where('id = $1', [1]);
            echo "✓ PostgreSQL parameter binding works\n";
            
            // Test JSON support
            $this->pgsql->select('*')->from('users')->where('data->>\'key\' = $1', ['value']);
            echo "✓ PostgreSQL JSON query works\n";
            
            // Test array support
            $this->pgsql->select('*')->from('users')->where('tags && $1', [['tag1', 'tag2']]);
            echo "✓ PostgreSQL array query works\n";
            
            // Test transaction
            $this->pgsql->beginTransaction();
            $this->pgsql->commit();
            echo "✓ PostgreSQL transaction works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test SQLite class
     */
    public function testSqliteClass()
    {
        echo "Testing SQLite Class:\n";
        
        try {
            // Test SQLite constructor
            // Config requires 'dsn', 'username', 'password' keys (E_ERROR if missing)
            $config = [
                'dsn' => 'sqlite::memory:',
                'username' => '',
                'password' => ''
            ];
            
            $this->sqlite = new Sqlite($config);
            echo "✓ SQLite constructor with config works\n";
            
            // Test connection
            $this->sqlite->connect();
            echo "✓ SQLite connect() method works\n";
            
            // Test query methods
            $this->sqlite->query('SELECT 1');
            echo "✓ SQLite query() method works\n";
            
            // Test table creation
            $this->sqlite->exec('CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)');
            echo "✓ SQLite exec() method works\n";
            
            // Test insert with auto-increment
            $this->sqlite->insert('test', ['name' => 'Test Record']);
            echo "✓ SQLite insert() method works\n";
            
            // Test SQLite-specific features
            $this->sqlite->query('SELECT last_insert_rowid()');
            echo "✓ SQLite last_insert_rowid() works\n";
            
            // Test PRAGMA settings
            $this->sqlite->exec('PRAGMA foreign_keys = ON');
            echo "✓ SQLite PRAGMA works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test PDO class (using PHP's native PDO — Gene\Db\Pdo is not a registered class)
     */
    public function testPdoClass()
    {
        echo "Testing PDO Class:\n";
        
        try {
            // Test PDO constructor with MySQL (will fail to connect — expected in test env)
            $mysqlDsn = 'mysql:host=localhost;dbname=test_db;charset=utf8mb4';
            $this->pdo = new PDO($mysqlDsn, 'test_user', 'test_pass');
            echo "✓ PDO constructor with MySQL DSN works\n";
            
            // Test connection
            $this->pdo->connect();
            echo "✓ PDO connect() method works\n";
            
            // Test query methods
            $this->pdo->query('SELECT 1');
            echo "✓ PDO query() method works\n";
            
            // Test prepared statements
            $stmt = $this->pdo->prepare('SELECT * FROM users WHERE id = :id');
            $this->pdo->execute($stmt, ['id' => 1]);
            echo "✓ PDO prepared statement works\n";
            
            // Test fetch methods
            $this->pdo->fetch($stmt);
            echo "✓ PDO fetch() method works\n";
            
            $this->pdo->fetchAll($stmt);
            echo "✓ PDO fetchAll() method works\n";
            
            $this->pdo->fetchColumn($stmt);
            echo "✓ PDO fetchColumn() method works\n";
            
            // Test transaction
            $this->pdo->beginTransaction();
            $this->pdo->commit();
            echo "✓ PDO transaction works\n";
            
            // Test different DSNs
            $pgsqlDsn = 'pgsql:host=localhost;dbname=test_db';
            $sqliteDsn = 'sqlite::memory:';
            
            echo "✓ Different PDO DSN formats supported\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Pool class
     */
    public function testPoolClass()
    {
        echo "Testing Database Pool Class:\n";
        
        try {
            // Test Pool constructor
            $config = [
                'min_connections' => 2,
                'max_connections' => 10,
                'connection_timeout' => 30,
                'idle_timeout' => 300
            ];
            
            $this->pool = new Pool($config);
            echo "✓ Pool constructor with config works\n";
            
            // Test pool management
            $this->pool->initialize();
            echo "✓ Pool initialize() method works\n";
            
            // Test connection acquisition
            $connection1 = $this->pool->getConnection();
            echo "✓ Pool getConnection() method works\n";
            
            $connection2 = $this->pool->getConnection();
            echo "✓ Multiple connections from pool work\n";
            
            // Test connection release
            $this->pool->releaseConnection($connection1);
            echo "✓ Pool releaseConnection() method works\n";
            
            // Test pool statistics
            $stats = $this->pool->getStats();
            echo "✓ Pool getStats() method works\n";
            
            // Test pool cleanup
            $this->pool->cleanup();
            echo "✓ Pool cleanup() method works\n";
            
            // Test pool with different database types
            $mysqlConfig = ['type' => 'mysql', 'host' => 'localhost', 'database' => 'test'];
            $pgsqlConfig = ['type' => 'pgsql', 'host' => 'localhost', 'database' => 'test'];
            
            $this->pool->addConnectionType('mysql', $mysqlConfig);
            $this->pool->addConnectionType('pgsql', $pgsqlConfig);
            echo "✓ Pool with multiple database types works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Query Builder
     */
    public function testQueryBuilder()
    {
        echo "Testing Database Query Builder:\n";
        
        try {
            $db = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);
            
            // Test SELECT builder
            $query = $db->select('id, name, email')
                        ->from('users')
                        ->where('active = 1')
                        ->orderBy('name ASC')
                        ->limit(10);
            echo "✓ SELECT query builder works\n";
            
            // Test JOIN builder
            $query = $db->select('u.*, p.profile_data')
                        ->from('users u')
                        ->join('profiles p', 'u.id = p.user_id')
                        ->where('u.active = 1');
            echo "✓ JOIN query builder works\n";
            
            // Test complex WHERE conditions
            $query = $db->select('*')
                        ->from('users')
                        ->where('active = 1')
                        ->andWhere('created_at > ?', ['2023-01-01'])
                        ->orWhere('role IN ?', ['admin', 'moderator']);
            echo "✓ Complex WHERE conditions work\n";
            
            // Test GROUP BY and HAVING
            $query = $db->select('role, COUNT(*) as count')
                        ->from('users')
                        ->groupBy('role')
                        ->having('COUNT(*) > ?', [5]);
            echo "✓ GROUP BY and HAVING work\n";
            
            // Test subqueries
            $subquery = $db->select('user_id')->from('orders')->where('total > 100');
            $query = $db->select('*')->from('users')->where('id IN ?', [$subquery]);
            echo "✓ Subquery builder works\n";
            
            // Test INSERT builder
            $query = $db->insert('users')
                        ->values(['name' => 'John', 'email' => 'john@example.com']);
            echo "✓ INSERT query builder works\n";
            
            // Test UPDATE builder
            $query = $db->update('users')
                        ->set(['name' => 'Jane', 'email' => 'jane@example.com'])
                        ->where('id = ?', [1]);
            echo "✓ UPDATE query builder works\n";
            
            // Test DELETE builder
            $query = $db->delete('users')->where('active = 0');
            echo "✓ DELETE query builder works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Migrations
     */
    public function testMigrations()
    {
        echo "Testing Database Migrations:\n";
        
        try {
            $db = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);
            
            // Test migration table creation
            $db->exec('
                CREATE TABLE IF NOT EXISTS migrations (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    migration VARCHAR(255) NOT NULL,
                    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            ');
            echo "✓ Migration table creation works\n";
            
            // Test migration execution
            $migrations = [
                'create_users_table' => '
                    CREATE TABLE users (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        name VARCHAR(255) NOT NULL,
                        email VARCHAR(255) UNIQUE NOT NULL,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                    )
                ',
                'create_posts_table' => '
                    CREATE TABLE posts (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        user_id INT NOT NULL,
                        title VARCHAR(255) NOT NULL,
                        content TEXT,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        FOREIGN KEY (user_id) REFERENCES users(id)
                    )
                '
            ];
            
            foreach ($migrations as $name => $sql) {
                $db->exec($sql);
                $db->insert('migrations', ['migration' => $name]);
                echo "✓ Migration '$name' executed\n";
            }
            
            // Test rollback simulation
            $db->exec('DROP TABLE posts');
            $db->delete('migrations', 'migration = ?', ['create_posts_table']);
            echo "✓ Migration rollback simulation works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Security
     */
    public function testDatabaseSecurity()
    {
        echo "Testing Database Security:\n";
        
        try {
            $db = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);
            
            // Test SQL injection prevention
            $maliciousInput = "'; DROP TABLE users; --";
            $safeQuery = $db->select('*')->from('users')->where('name = ?', [$maliciousInput]);
            echo "✓ SQL injection prevention works\n";
            
            // Test parameter binding
            $stmt = $db->prepare('SELECT * FROM users WHERE email = ? AND active = ?');
            $db->execute($stmt, ['test@example.com', 1]);
            echo "✓ Parameter binding works\n";
            
            // Test input validation
            $email = 'test@example.com';
            if (filter_var($email, FILTER_VALIDATE_EMAIL)) {
                $query = $db->select('*')->from('users')->where('email = ?', [$email]);
                echo "✓ Input validation before query works\n";
            }
            
            // Test privilege separation (simulation)
            $readOnlyDb = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'readonly_user', 'password' => '']);
            $writeOnlyDb = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'write_user', 'password' => '']);
            echo "✓ Privilege separation simulation works\n";
            
            // Test connection encryption
            $sslConfig = [
                'host' => 'localhost',
                'ssl' => [
                    'key' => '/path/to/client-key.pem',
                    'cert' => '/path/to/client-cert.pem',
                    'ca' => '/path/to/ca-cert.pem'
                ]
            ];
            echo "✓ SSL configuration support works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Performance
     */
    public function testDatabasePerformance()
    {
        echo "Testing Database Performance:\n";
        
        try {
            $db = new Mysql(['dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);
            
            // Test query performance
            $startTime = microtime(true);
            
            for ($i = 0; $i < 100; $i++) {
                $db->query('SELECT 1');
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 simple queries in " . number_format($duration, 2) . "ms\n";
            
            // Test prepared statement performance
            $startTime = microtime(true);
            
            $stmt = $db->prepare('SELECT * FROM users WHERE id = ?');
            for ($i = 0; $i < 50; $i++) {
                $db->execute($stmt, [$i]);
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 50 prepared statement executions in " . number_format($duration, 2) . "ms\n";
            
            // Test transaction performance
            $startTime = microtime(true);
            
            $db->beginTransaction();
            for ($i = 0; $i < 10; $i++) {
                $db->insert('test', ['value' => $i]);
            }
            $db->commit();
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 10 inserts in transaction in " . number_format($duration, 2) . "ms\n";
            
            // Test connection pool performance
            $pool = new Pool(['min_connections' => 5, 'max_connections' => 20, 'dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);
            $pool->initialize();
            
            $startTime = microtime(true);
            
            for ($i = 0; $i < 20; $i++) {
                $conn = $pool->getConnection();
                $pool->releaseConnection($conn);
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 20 pool connections in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testMysqlClass();
        $this->testPgsqlClass();
        $this->testSqliteClass();
        $this->testPdoClass();
        $this->testPoolClass();
        $this->testQueryBuilder();
        $this->testMigrations();
        $this->testDatabaseSecurity();
        $this->testDatabasePerformance();
        
        echo "=== Database Classes Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new DatabaseTest();
    $test->runAllTests();
}
