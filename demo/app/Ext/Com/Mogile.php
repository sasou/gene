<?php
namespace Ext\Com;

class Mogile
{

    protected $config = array('host' => '127.0.0.1', 'port' => 7777, 'domain' => 'attachments', 'class' => 'files', 'device' => 2);
    protected $client = null;
    public $error = '';

    public function __construct($config)
    {
        $this->config = $config + $this->config;
        $this->getConnection();
    }

    /**
     * 设置配置
     * @param array $config
     * @return Mogilefs | array
     */
    public function config(array $config = array())
    {
        if (empty($config)) {
            return $this->config;
        }
        $this->config = $config;
        return $this;
    }

    /**
     *
     * @param array $config
     * @return MogileFs
     */
    public function getConnection(array $config = array())
    {
        if ($this->client === null || !$this->client->isConnected()) {
            $config += $this->config;
            $this->client = new MogileFs();
            $this->client->connect($config['host'], $config['port'], $config['domain']);
        }
        return $this->client;
    }

    /**
     * get time out
     * @return int
     */
    public function getGetTimeout()
    {
        return $this->client->getReadTimeout();
    }

    /**
     * set time out
     * @param int $timeout
     * @return Cola\Com\mogilefs
     */
    public function setGetTimeout($timeout)
    {
        $this->client->setReadTimeout($timeout);
        return $this;
    }

    /**
     * Return a list of domains
     * @return array
     */
    public function getDomains()
    {
        return $this->client->getDomains();
    }

    /**
     * create domain
     * @param string $domain
     * @return \Cola\Com\Mogilefs
     */
    public function addDomain($domain)
    {
        $this->client->createDomain($domain);
        return $this;
    }

    /**
     * delete domain
     * @param string $domain
     * @return string | null
     */
    public function delDomain($domain)
    {
        return $this->client->deleteDomain($domain);
    }

    /**
     * create domain class
     * @param string $className
     * @param array ['domain', 'class', 'mindevcount']
     */
    public function addClass($className)
    {
        $this->config['class'] = $className;
        return $this->client->createClass($this->config['domain'], $className, $this->config['device']);
    }

    /**
     * delete domain class
     * @param type $className
     * @return array ['domain', 'class', 'mindevcount']
     */
    public function delClass($className)
    {
        return $this->client->deleteClass($this->config['domain'], $className);
    }

    /**
     * update domain device
     * @param int $device
     * @return array
     */
    public function updateDevice($device)
    {
        return $this->client->updateClass($this->config['domain'], $this->config['class'], $device);
    }

    /**
     * Checks if key exists at tracker
     * @param string $key
     * @return boolean
     */
    public function exists($key)
    {
        if (empty($key)) {
            return false;
        }
        try {
            $result = $this->client->fileInfo($key);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }

        return isset($result['length']);
    }

    /**
     * Delete a file from system
     * @param string $key
     * @return boolean
     */
    public function delete($key)
    {
        if (empty($key)) {
            return false;
        }
        try {
            $this->client->delete($key);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }
        return true;
    }

    /**
     * Rename a file/key
     * @param string $from
     * @param string $to
     * @return boolean
     */
    public function rename($from, $to)
    {
        if (empty($from) || empty($to)) {
            return false;
        }
        try {
            $this->client->rename($from, $to);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }
        return true;
    }

    /**
     * List a number of keys
     * @param string $prefix
     * @param string $lastKey
     * @param integer $limit
     * @return array
     */
    public function listKeys($prefix = null, $lastKey = null, $limit = null)
    {
        try {
            return $this->client->listKeys($prefix, $lastKey, $limit);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }
    }

    /**
     * Get file from Mogilefs and write to file
     * @return string|false
     */
    function getFile($key, $outputfile)
    {
        if (empty($key)) {
            $this->error = 'key is empty';
            return false;
        }
        try {
            $paths = $this->client->get($key);
            array_pop($paths);

            foreach ($paths as $path) {
                if (file_exists($outputfile)) {
                    unlink($outputfile);
                }
                $out = fopen($outputfile, 'wb');
                if ($out === false) {
                    $this->error = "getFile unable to open {$outputfile} for writing";
                    return false;
                }

                $ch = curl_init();
                curl_setopt($ch, CURLOPT_FILE, $out);
                curl_setopt($ch, CURLOPT_HEADER, 0);
                curl_setopt($ch, CURLOPT_URL, $path);
                curl_exec($ch);
                $error = curl_errno($ch);
                curl_close($ch);
                fclose($out);
                if ($error === 0) {
                    return $outputfile;
                } else {
                    unlink($outputfile);
                }
            }
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }

        return false;
    }

    /**
     * Get infos for a file
     * @param string $key
     * @return array|false
     */
    public function fileinfo($key)
    {
        if (empty($key)) {
            return false;
        }

        try {
            $result = $this->client->fileInfo($key);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }

        return $result;
    }

    /**
     * Get a file from mogstored and send it directly to stdout by way of fpassthru()
     * @param string $key
     */
    function getPassthru($key)
    {
        if (empty($key)) {
            $this->error = 'The key is empty';
            return false;
        }
        try {
            $paths = $this->client->get($key);
            array_pop($paths);
            $path = $paths[array_rand($paths, 1)];

            $fh = fopen($path, 'r');
            if ($fh && fpassthru($fh) === false) {
                $this->error = 'getPassthru failed';
            }
            fclose($fh);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }
        return false;
    }

    /**
     * Stores string into file
     * @param string $key
     * @param string $value
     * @return boolean
     */
    public function set($key, $value)
    {
        if (empty($key)) {
            $this->error = 'key is empty';
            return false;
        }
        return $this->client->put($value, $key, $this->config['class'], false);
    }

    /**
     * Stores a local file in Mogilefs
     * @param string $key
     * @param string $file
     * @return boolean
     */
    public function setFile($key, $file)
    {
        if (empty($key)) {
            $this->error = 'key is empty';
            return false;
        }
        if (!file_exists($file)) {
            $this->error = "{$file} is not exists!";
            return false;
        }
        try {
            return $this->client->put($file, $key, $this->config['class'], true);
        } catch (\Exception $ex) {
            $this->error = $ex->getMessage();
            return false;
        }
        return true;
    }

    /**
     * close
     * @return \Cola_Com_Mogile
     */
    public function close()
    {
        if ($this->client !== null && $this->client->isConnected()) {
            $this->client->close();
        }
        $this->client = null;
        return $this;
    }

}
