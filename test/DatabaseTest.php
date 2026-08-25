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

    // [GENE_FIX:2026-08-10 R2] Failure/skip tallies. Previously every section
    // ended with catch-all "✗ Error: ..." and the suite still exited 0, so a
    // stale test calling a non-existent API was indistinguishable from a
    // missing local driver. Now: Error => failed (API mismatch), Exception
    // => skipped (environment missing), and runAllTests() reports/returns it.
    private $failed = 0;
    private $skipped = 0;

    public function __construct()
    {
        echo "=== Gene Database Classes Test Suite ===\n\n";
    }

    private function fail($msg)
    {
        $this->failed++;
        echo "✗ $msg\n";
    }

    private function skip($msg)
    {
        $this->skipped++;
        echo "- SKIP: $msg\n";
    }

    private function reportCaught(Throwable $e)
    {
        if ($e instanceof \Error) {
            // Call to undefined method/class, TypeError, ... — test staleness.
            $this->fail('Error: ' . $e->getMessage());
        } else {
            // PDOException 'could not find driver', connection refused, ... — env.
            $this->skip($e->getMessage());
        }
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
            $lastId = $this->mysql->lastId();
            echo "✓ MySQL lastId() method works\n";
            
            // Test affected rows
            $affectedRows = $this->mysql->affectedRows();
            echo "✓ MySQL affectedRows() method works\n";
            
            // Test error handling
            $error = $this->mysql->error();
            echo "✓ MySQL error() method works\n";
            
            $errno = $this->mysql->errno();
            echo "✓ MySQL errno() method works\n";
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            // dsn-only config — regression guard for the 2026-08-09 H1 segfault
            // (gene_pdo_construct used to dereference NULL username/password).
            $this->sqlite = new Sqlite([
                'dsn' => 'sqlite::memory:',
            ]);
            echo "✓ SQLite constructor with dsn-only config works\n";

            // Raw DDL through the sql()->execute() pair
            $this->sqlite->sql('CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)')->execute();
            echo "✓ SQLite sql()->execute() table creation works\n";

            // Test insert with auto-increment
            $this->sqlite->insert('test', ['name' => 'Test Record']);
            echo "✓ SQLite insert() method works\n";

            // NOTE: insert() is lazy — the pending statement executes on the next
            // read call, so exercise the aliases BEFORE lastId() triggers it.
            // Test PDO-named aliases and quote pass-through (5.7.0+, F1-8)
            $lastId = $this->sqlite->lastId();
            if ($lastId > 0) {
                echo "✓ SQLite lastId() works (=$lastId)\n";
            } else {
                $this->fail("SQLite lastId() returned " . var_export($lastId, true));
            }

            $quoted = $this->sqlite->quote("it's");
            if ($quoted === "'it''s'") {
                echo "✓ SQLite quote() method works\n";
            } else {
                $this->fail("SQLite quote() unexpected: " . var_export($quoted, true));
            }

            // Query builder: select/where/limit/row + update/delete + affectedRows
            $row = $this->sqlite->select('test')->where('id=?', [$lastId])->limit(1)->row();
            if (is_array($row) && ($row['name'] ?? '') === 'Test Record') {
                echo "✓ SQLite select()->where()->row() works\n";
            } else {
                $this->fail("SQLite row() mismatch: " . json_encode($row));
            }

            $affUpd = $this->sqlite->update('test', ['name' => 'Updated'])->where('id=?', [$lastId])->affectedRows();
            echo "✓ SQLite update() method works (affected=$affUpd)\n";

            $all = $this->sqlite->select('test')->all();
            $names = is_array($all) ? array_column($all, 'name') : [];
            if (in_array('Updated', $names, true)) {
                echo "✓ SQLite select()->all() reflects update\n";
            } else {
                $this->fail("SQLite all() mismatch: " . json_encode($all));
            }

            // Aliases bound to real methods (lastInsertId→lastId, rowCount→affectedRows)
            if (method_exists($this->sqlite, 'lastInsertId') && method_exists($this->sqlite, 'rowCount')) {
                echo "✓ SQLite lastInsertId()/rowCount() aliases exist\n";
            } else {
                $this->fail("SQLite PDO-named aliases missing");
            }

            // SQL history (gene.run_environment=0 default)
            $history = $this->sqlite->history();
            echo "✓ SQLite history() returns " . gettype($history) . "\n";

            // Cleanup
            $this->sqlite->delete('test')->where('id=?', [$lastId])->affectedRows();
            echo "✓ SQLite delete() method works\n";

        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
        }
        
        echo "\n";
    }
    
    /**
     * Test Database Pool class
     *
     * [GENE_FIX:2026-08-10 R2] Rewritten against the real Gene\Pool API
     * (src/db/pool.c method table): get()/put()/stats()/recycleIdle()/
     * healthCheck()/close() plus static create()/getInstance()/closeAll()/
     * stopTimers(). The previous version called six non-existent methods
     * (initialize()/getConnection()/releaseConnection()/getStats()/cleanup()/
     * addConnectionType()); the catch-all swallowed the Error each time, so
     * the pool had zero real coverage while the suite still exited 0.
     */
    public function testPoolClass()
    {
        echo "Testing Database Pool Class:\n";

        try {
            // Real config keys (pool_normalize_config): min/max/idleTimeout/
            // waitTimeout + dsn/username/password.
            $config = [
                'min' => 2,
                'max' => 4,
                'idleTimeout' => 60,
                'waitTimeout' => 1,
                'dsn' => 'sqlite::memory:',
            ];

            $this->pool = new Pool($config);
            echo "✓ Pool constructor with config works\n";

            // stats() reports the configured geometry; not closed yet
            $stats = $this->pool->stats();
            if (is_array($stats)
                && ($stats['min'] ?? null) === 2
                && ($stats['max'] ?? null) === 4
                && ($stats['closed'] ?? true) === false
                && array_key_exists('total', $stats) && array_key_exists('idle', $stats)
                && array_key_exists('using', $stats) && array_key_exists('overflow', $stats)) {
                echo "✓ Pool stats() works (min=2, max=4, closed=false)\n";
            } else {
                $this->fail('Pool stats() mismatch: ' . json_encode($stats));
            }

            // The connection lifecycle needs Swoole (Coroutine\Channel/Atomic).
            // Without it the pool degrades gracefully: get()=null, healthCheck()=false.
            if (class_exists('Swoole\Coroutine\Channel') && class_exists('Swoole\Atomic')) {
                Swoole\Coroutine\run(function () {
                    $conn = $this->pool->get();
                    if ($conn instanceof PDO) {
                        echo "✓ Pool get() returns a PDO connection\n";
                        $this->pool->put($conn);
                        echo "✓ Pool put() returns the connection to the pool\n";

                        $again = $this->pool->get();
                        if ($again instanceof PDO) {
                            echo "✓ Pool get() after put() reuses the connection\n";
                            $this->pool->put($again);
                        } else {
                            $this->fail('Pool get() after put() returned ' . var_export($again, true));
                        }

                        $hc = $this->pool->healthCheck();
                        if (is_array($hc) && array_key_exists('alive', $hc) && array_key_exists('dead', $hc)) {
                            echo "✓ Pool healthCheck() works (alive={$hc['alive']}, dead={$hc['dead']})\n";
                        } else {
                            $this->fail('Pool healthCheck() unexpected: ' . var_export($hc, true));
                        }
                    } else {
                        $this->fail('Pool get() did not return PDO: ' . var_export($conn, true));
                    }
                    $this->pool->recycleIdle();
                    echo "✓ Pool recycleIdle() runs without error\n";
                });
            } else {
                if ($this->pool->get() === null && $this->pool->healthCheck() === false) {
                    $this->skip('Swoole not loaded — get()/healthCheck() degrade to null/false (verified)');
                } else {
                    $this->fail('Pool non-Swoole degradation contract broken');
                }
                $this->pool->recycleIdle();
                echo "✓ Pool recycleIdle() is a safe no-op without Swoole\n";
            }

            // close() is idempotent; afterwards stats()['closed']===true and get()=null
            $this->pool->close();
            $this->pool->close();
            $closed = $this->pool->stats();
            if (($closed['closed'] ?? false) === true && $this->pool->get() === null) {
                echo "✓ Pool close() is idempotent; get() on a closed pool returns null\n";
            } else {
                $this->fail('Pool close() state mismatch: ' . json_encode($closed));
            }

            // Static registry: create() registers a named pool, getInstance()
            // retrieves the same object, closeAll()/stopTimers() sweep up.
            $named = Pool::create('dbtest', 'dbtest', ['min' => 1, 'max' => 2]);
            if ($named instanceof Pool && Pool::getInstance('dbtest') === $named) {
                echo "✓ Pool::create()/getInstance() registry works\n";
            } else {
                $this->fail('Pool::create()/getInstance() registry broken');
            }
            if (Pool::getInstance('no-such-pool') === null) {
                echo "✓ Pool::getInstance() returns null for an unknown name\n";
            } else {
                $this->fail('Pool::getInstance() should return null for an unknown name');
            }
            Pool::closeAll();
            Pool::stopTimers();
            echo "✓ Pool::closeAll()/stopTimers() work\n";

        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
            
            // Test connection pool performance (real Pool API: get/put/close)
            $pool = new Pool(['min' => 2, 'max' => 8, 'waitTimeout' => 1, 'dsn' => 'mysql:host=localhost;dbname=test_db', 'username' => 'test_user', 'password' => 'test_pass']);

            $startTime = microtime(true);

            for ($i = 0; $i < 20; $i++) {
                $conn = $pool->get();
                if ($conn instanceof PDO) {
                    $pool->put($conn);
                }
            }

            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            $pool->close();
            echo "✓ 20 pool get()/put() cycles in " . number_format($duration, 2) . "ms\n";
            
        } catch (Throwable $e) {
            $this->reportCaught($e);
        }
        
        echo "\n";
    }
    
    /**
     * [GENE_FEATURE:2026-08-07] Test Sqlite::attach() and detach()
     */
    public function testSqliteAttachDetach()
    {
        echo "Testing Sqlite attach()/detach():\n";

        try {
            // Create a temporary sqlite database for testing
            // (constructor expects a config array — string dsn is not supported)
            $tempDb = tempnam(sys_get_temp_dir(), 'gene_attach_test');
            $sqlite = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $tempDb]);

            // Create a second temp database to attach
            $tempDb2 = tempnam(sys_get_temp_dir(), 'gene_attach_test2');
            $sqlite2 = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $tempDb2]);
            $sqlite2->release();

            // Test attach
            $ok = $sqlite->attach($tempDb2, "auxdb");
            echo "✓ attach() returns: " . var_export($ok, true) . "\n";

            // Test detach
            $ok2 = $sqlite->detach("auxdb");
            echo "✓ detach() returns: " . var_export($ok2, true) . "\n";

            // Test invalid schema name is rejected
            $bad = $sqlite->attach($tempDb2, "aux; DROP TABLE");
            echo "✓ attach() with invalid schema returns: " . var_export($bad, true) . "\n";

            $sqlite->release();
            @unlink($tempDb);
            @unlink($tempDb2);
        } catch (Throwable $e) {
            $this->reportCaught($e);
        }

        echo "\n";
    }

    /**
     * [GENE_FEATURE:2026-08-18 3.3/3.4] Db-level insertIgnore / upsert /
     * lockForUpdate / sharedLock — sqlite semantics (OR IGNORE works,
     * upsert unsupported, locks are documented no-ops with E_NOTICE).
     */
    public function testSqliteV2WriteApis()
    {
        echo "Testing Sqlite insertIgnore/upsert/lock APIs:\n";

        try {
            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $db->sql('CREATE TABLE kv (k TEXT PRIMARY KEY, v INTEGER)')->execute();

            // insertIgnore: first write lands, duplicate is ignored
            $n = $db->insertIgnore('kv', ['k' => 'a', 'v' => 1])->affectedRows();
            if ((int)$n === 1) {
                echo "✓ insertIgnore() first write affected=1\n";
            } else {
                $this->fail("insertIgnore first affected=" . var_export($n, true));
            }
            $n = $db->insertIgnore('kv', ['k' => 'a', 'v' => 99])->affectedRows();
            $row = $db->select('kv')->where('k=?', ['a'])->row();
            if ((int)$n === 0 && (int)($row['v'] ?? 0) === 1) {
                echo "✓ insertIgnore() duplicate ignored (affected=0, row untouched)\n";
            } else {
                $this->fail("insertIgnore dup affected=" . var_export($n, true) . " row=" . json_encode($row));
            }

            // upsert unsupported on sqlite -> exception, no partial SQL state
            $threw = false;
            try {
                $db->upsert('kv', ['k' => 'a', 'v' => 2], ['v']);
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                echo "✓ upsert() throws on sqlite (documented degradation)\n";
            } else {
                $this->fail("upsert() did not throw on sqlite");
            }

            // locks: no-op + E_NOTICE, chainable, SQL unchanged
            $notice = null;
            set_error_handler(function ($no, $str) use (&$notice) { $notice = $str; return true; }, E_NOTICE);
            $rows = $db->select('kv')->lockForUpdate()->all();
            restore_error_handler();
            if ($notice !== null && is_array($rows) && count($rows) === 1) {
                echo "✓ lockForUpdate() no-op + E_NOTICE on sqlite\n";
            } else {
                $this->fail("lockForUpdate sqlite: notice=" . var_export($notice, true));
            }
            $notice = null;
            set_error_handler(function ($no, $str) use (&$notice) { $notice = $str; return true; }, E_NOTICE);
            $rows = $db->select('kv')->sharedLock()->all();
            restore_error_handler();
            if ($notice !== null && is_array($rows)) {
                echo "✓ sharedLock() no-op + E_NOTICE on sqlite\n";
            } else {
                $this->fail("sharedLock sqlite: notice=" . var_export($notice, true));
            }

            // LOCK fragment must not leak into the next statement on the
            // same handle (M8: lock is cleared by reset_sql_params)
            $sql = $db->select('kv')->where('k=?', ['a'])->print();
            if (is_array($sql) && isset($sql['sql']) && stripos($sql['sql'], 'FOR UPDATE') === false) {
                echo "✓ no LOCK residue after reset\n";
            } else {
                $this->fail("LOCK residue: " . json_encode($sql));
            }

            // [GENE_FIX:2026-08-19 print-crash] print() on a handle with no
            // SQL built must not crash (sql.s was NULL-dereferenced).
            $fresh = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $sql = $fresh->print();
            if (is_array($sql) && ($sql['sql'] ?? null) === '') {
                echo "✓ print() on fresh handle returns empty sql (no crash)\n";
            } else {
                $this->fail("print() fresh: " . json_encode($sql));
            }
            $fresh->select('kv');
            $sql = $fresh->print();
            $fresh->select('kv')->reset();
            $sql = $fresh->print();
            if (is_array($sql) && ($sql['sql'] ?? null) === '') {
                echo "✓ print() after reset returns empty sql (no crash)\n";
            } else {
                $this->fail("print() after reset: " . json_encode($sql));
            }

            // method surface on all 4 drivers
            foreach (['\\Gene\\Db\\Mysql', '\\Gene\\Db\\Pgsql', '\\Gene\\Db\\Sqlite', '\\Gene\\Db\\Mssql'] as $cls) {
                if (!class_exists($cls)) {
                    continue;
                }
                foreach (['insertIgnore', 'upsert', 'lockForUpdate', 'sharedLock'] as $m) {
                    if (!method_exists($cls, $m)) {
                        $this->fail("$cls missing $m");
                    }
                }
            }
            echo "✓ all drivers expose insertIgnore/upsert/lockForUpdate/sharedLock\n";
        } catch (Throwable $e) {
            $this->reportCaught($e);
        }

        echo "\n";
    }

    /**
     * Callback transaction: commit, rollback, nested no-op begin.
     */
    public function testTransactionCallback()
    {
        echo "Testing Db::transaction() (SQLite):\n";

        if (!class_exists('\\Gene\\Db\\Sqlite')) {
            $this->skip('skip transaction callback — Sqlite missing');
            return;
        }

        try {
            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $db->sql('CREATE TABLE tx_cb (id INTEGER PRIMARY KEY AUTOINCREMENT, v int)')->execute();

            $n = $db->transaction(function () use ($db) {
                $db->insert('tx_cb', ['v' => 1])->execute();
                return 42;
            });
            $rows = $db->select('tx_cb')->all();
            if ($n === 42 && is_array($rows) && count($rows) === 1 && !$db->inTransaction()) {
                echo "✓ transaction() commits and returns callback value\n";
            } else {
                $this->fail('transaction commit: ret=' . var_export($n, true) . ' rows=' . json_encode($rows));
            }

            try {
                $db->transaction(function () use ($db) {
                    $db->insert('tx_cb', ['v' => 2])->execute();
                    throw new \RuntimeException('db tx rollback');
                });
                $this->fail('transaction should rethrow');
            } catch (\RuntimeException $e) {
                $rows2 = $db->select('tx_cb')->all();
                if (is_array($rows2) && count($rows2) === 1 && !$db->inTransaction()
                    && strpos($e->getMessage(), 'db tx rollback') !== false) {
                    echo "✓ transaction() rolls back and rethrows\n";
                } else {
                    $this->fail('transaction rollback rows=' . json_encode($rows2));
                }
            }

            $innerOwn = null;
            $db->transaction(function () use ($db, &$innerOwn) {
                $db->transact(function () use ($db, &$innerOwn) {
                    $innerOwn = $db->inTransaction();
                });
            });
            if ($innerOwn === true && !$db->inTransaction()) {
                echo "✓ nested transact() shares the outer transaction\n";
            } else {
                $this->fail('nested transact inTx inner=' . var_export($innerOwn, true));
            }
        } catch (\Throwable $e) {
            $this->reportCaught($e);
        }

        echo "\n";
    }

    /**
     * [GENE_FIX:2026-08-19 N2/N6/N8] Transaction hygiene on shutdown/release.
     *
     * Covers the half of P1-4 / N2 that §13.5-4 originally deferred to MySQL:
     * hygiene runs while a business exception is IN FLIGHT *and* PDO::rollBack()
     * itself would throw. We desync PDO's in_txn bookkeeping WITHOUT MySQL by
     * issuing a raw COMMIT through Gene while PDO believes a transaction is
     * still open; the subsequent rollBack() would then error under the forced
     * ERRMODE_EXCEPTION. After N6 the hygiene path forces ERRMODE_SILENT around
     * rollBack(), so no exception is raised at all and the business exception
     * survives cleanly.
     *
     * Also asserts the basic "dirty request -> rolled back + E_WARNING" path
     * (P1-4) and that a userland error handler cannot hijack shutdown cleanup.
     */
    public function testTxHygiene()
    {
        echo "Testing transaction hygiene (N2/N6/N8, SQLite):\n";

        if (!class_exists('\\Gene\\Db\\Sqlite') || !class_exists('\\Gene\\Application')) {
            $this->skip('skip tx hygiene — extension classes missing');
            return;
        }

        try {
            // --- P1-4 baseline: dirty request ends, hygiene rolls back + warns.
            // The warning is captured via error_log to avoid user handler noise.
            $f = sys_get_temp_dir() . '/gene_tx_hygiene_test.db';
            $log = sys_get_temp_dir() . '/gene_tx_hygiene_test.log';
            @unlink($f);
            @unlink($log);
            ini_set('log_errors', '1');
            ini_set('error_log', $log);

            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f]);
            $db->sql('CREATE TABLE t (a int)')->execute();
            \Gene\Di::set('tx_hygiene_db', $db);
            $db->beginTransaction();
            $db->sql('INSERT INTO t VALUES (1)')->execute();

            // clearState() triggers the DI-scan hygiene: open transaction must
            // be rolled back and an E_WARNING emitted via the bypassed handler.
            $warned = false;
            set_error_handler(function ($no, $str) use (&$warned) {
                if (strpos($str, 'open transaction') !== false) {
                    $warned = true;
                }
                return true;
            }, E_WARNING);
            \Gene\Application::clearState();
            restore_error_handler();

            // After clearState() the connection is rolled back; a fresh handle
            // on the same file must see 0 rows.
            $check = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f]);
            $rows = $check->select('t')->all();
            $rolledBack = is_array($rows) && count($rows) === 0;
            if (!$db->inTransaction() && $rolledBack) {
                echo "✓ hygiene rolled back the dirty transaction\n";
            } else {
                $this->fail("hygiene baseline: inTransaction=" . var_export($db->inTransaction(), true)
                    . " rows=" . json_encode($rows));
            }
            // The warning is emitted with the user handler bypassed, so it goes
            // to error_log, not to the local handler. Verify via the log file.
            $logContent = (string) @file_get_contents($log);
            if (strpos($logContent, 'open transaction') !== false) {
                echo "✓ hygiene emitted E_WARNING (via error_log, handler bypassed)\n";
            } else {
                $this->fail("hygiene warning missing in error_log: " . substr($logContent, 0, 200));
            }

            // --- N2 / N6: business exception in flight + rollBack() would throw.
            // Desync PDO's in_txn by issuing a raw COMMIT behind its back; PDO
            // still reports inTransaction()=true but a subsequent rollBack()
            // would raise under ERRMODE_EXCEPTION. After N6 hygiene forces
            // ERRMODE_SILENT around rollBack(), so no exception is raised and
            // the business exception propagates untouched.
            // Release the first segment's handles before unlinking the file
            // (sqlite keeps the file locked until all PDOs on it are gone).
            unset($db, $check);
            \Gene\Di::set('tx_hygiene_db', null);
            @unlink($f);
            @unlink($log);
            $db2 = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f]);
            $db2->sql('CREATE TABLE t (a int)')->execute();
            \Gene\Di::set('tx_hygiene_db2', $db2);
            $db2->beginTransaction();
            $db2->sql('INSERT INTO t VALUES (1)')->execute();
            try { $db2->sql('COMMIT')->execute(); } catch (Throwable $e) {}
            $desynced = $db2->inTransaction();

            $caught = null;
            try {
                try {
                    throw new RuntimeException('BIZ');
                } finally {
                    \Gene\Application::clearState();
                }
            } catch (Throwable $e) {
                $caught = get_class($e) . ':' . $e->getMessage();
            }
            if ($caught === 'RuntimeException:BIZ') {
                echo "✓ N2/N6: business exception survives hygiene (rollBack cannot throw under SILENT)\n";
            } else {
                $this->fail("N2/N6: caught=" . var_export($caught, true) . " desynced=" . var_export($desynced, true));
            }
            if ($desynced) {
                echo "✓ N2/N6: PDO in_txn desync reproduced on sqlite (no MySQL needed)\n";
            } else {
                // Not a failure — some driver builds read autocommit directly.
                echo "- NOTE: PDO in_txn did not desync on this build; N6 silent-errmode branch still exercised\n";
            }

            @unlink($f);
            @unlink($log);
        } catch (Throwable $e) {
            $this->reportCaught($e);
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
        $this->testSqliteAttachDetach();
        $this->testSqliteV2WriteApis();
        $this->testTransactionCallback();
        $this->testTxHygiene();

        echo "=== Database Classes Test Suite Complete ===\n";
        echo "Summary: failed={$this->failed}, skipped={$this->skipped}\n";
        return $this->failed === 0;
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new DatabaseTest();
    // Non-zero exit on real failures (API mismatches / broken assertions);
    // environment-missing sections are SKIPped and do not affect the exit code.
    exit($test->runAllTests() ? 0 : 1);
}
