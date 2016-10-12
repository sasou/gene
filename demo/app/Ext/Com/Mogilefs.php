<?php
namespace Ext\Com;
/* Mogilefs.class.php - Class for accessing the Mogile File System
 * Copyright (C) 2007 Interactive Path, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* File Authors:
 *   Erik Osterman <eosterman@interactivepath.com>
 *   Jon Skarpeteig <jon.skarpeteig@gmail.com>
 *
 * Thanks to the Mogilefs mailing list and the creator of the MediaWiki
 * Mogilefs client.
 */

/* Changelog
 *
 * 31.07.2011
 *
 * - Fixed issue with exists() introduced with new \Exception
 * - Fixed issue with getFile()
 *
 * 10.06.2011
 *
 * - Removed failiure on filesize = 0, which is now supported by Mogilefs
 * - Removed debug options
 * - Added more comments, and changed some syntax
 * - getFile implemented to avoid hogging ram
 * - \Exceptions for file not found and empty file added (Thanks Jan Kantert)
 * - FILE_INFO implemented (Patch by Jan Kantert)
 *
 * 21.01.2010
 *
 * - setResource with key == null now close file handle before throwing \Exception
 * - unknown key, and empty file now produce a warning, and not fatal error in doRequest
 * - setFile now tries upload 10 times before failing - example; tracker dies during upload
 * - 0byte check now before upload, and not relying on tracker
 * - added pathcount and noverify options to getPaths (copied from perl client 1.08)
 */

class MogileFsNotFoundException extends \Exception
{

}

class MogileFsFileEmptyException extends \Exception
{

}

class Mogilefs
{

    const DELETE = 'DELETE';
    const GET_DOMAINS = 'GET_DOMAINS';
    const GET_PATHS = 'GET_PATHS';
    const RENAME = 'RENAME';
    const LIST_KEYS = 'LIST_KEYS';
    const CREATE_OPEN = 'CREATE_OPEN';
    const CREATE_CLOSE = 'CREATE_CLOSE';
    const FILE_INFO = 'FILE_INFO';
    const SUCCESS = 'OK';    // Tracker success code
    const ERROR = 'ERR';   // Tracker error code
    const DEFAULT_PORT = 7001;    // Tracker port

    protected $retrycount = 3;
    protected $domain;
    protected $class;
    protected $trackers = array();
    protected $socket;
    protected $requestTimeout;
    protected $putTimeout;
    protected $getTimeout;

    public function __construct($domain, $class, $trackers)
    {
        $this->setDomain($domain);
        $this->setClass($class);
        $this->setHosts($trackers);
        $this->setRequestTimeout(1800);
        $this->setPutTimeout(6);
        $this->setGetTimeout(10);
    }

    public function getRequestTimeout()
    {
        return $this->requestTimeout;
    }

    public function setRequestTimeout($timeout)
    {
        if ($timeout > 0) {
            return $this->requestTimeout = $timeout;
        } else {
            throw new \Exception(get_class($this) . "::setRequestTimeout expects a positive integer");
        }
    }

    public function getPutTimeout()
    {
        return $this->putTimeout;
    }

    public function setPutTimeout($timeout)
    {
        if ($timeout > 0) {
            return $this->putTimeout = $timeout;
        } else {
            throw new \Exception(get_class($this) . "::setPutTimeout expects a positive integer");
        }
    }

    public function getGetTimeout()
    {
        return $this->getTimeout;
    }

    public function setGetTimeout($timeout)
    {
        if ($timeout > 0) {
            return $this->getTimeout = $timeout;
        } else {
            throw new \Exception(get_class($this) . "::setGetTimeout expects a positive integer");
        }
    }

    public function getHosts()
    {
        return $this->trackers;
    }

    public function setHosts($trackers)
    {
        if (is_scalar($trackers)) {
            $this->trackers = Array($trackers);
        } elseif (is_array($trackers)) {
            $this->trackers = $trackers;
        } else {
            throw new \Exception(get_class($this) . "::setHosts unrecognized host argument");
        }
    }

    public function getDomain()
    {
        return $this->domain;
    }

    public function setDomain($domain)
    {
        if (is_scalar($domain)) {
            return $this->domain = $domain;
        } else {
            throw new \Exception(get_class($this) . "::setDomain unrecognized domain argument");
        }
    }

    public function getClass()
    {
        return $this->class;
    }

    public function setClass($class)
    {
        if (is_scalar($class)) {
            return $this->class = $class;
        } else {
            throw new \Exception(get_class($this) . "::setClass unrecognized class argument");
        }
    }

    /**
     * Connect to a mogilefsd; scans through the list of daemons and tries to connect one.
     * @return resource
     */
    public function getConnection()
    {
        if ($this->socket && is_resource($this->socket) && !feof($this->socket)) {
            return $this->socket;
        }

        foreach ($this->trackers as $host) {
            $parts = parse_url($host);
            if (!isset($parts['port'])) {
                $parts['port'] = self::DEFAULT_PORT;
            }

            $errno = null;
            $errstr = null;
            $this->socket = fsockopen($parts['host'], $parts['port'], $errno, $errstr, $this->requestTimeout);
            if ($this->socket) {
                break;
            }
        }

        if (!is_resource($this->socket) || feof($this->socket)) {
            throw new \Exception(get_class($this) . "::doConnection failed to obtain connection");
        } else {
            return $this->socket;
        }
    }

    /**
     * Send a request to mogilefsd and parse the result.
     * @param string $cmd
     * @param array $args
     */
    protected function doRequest($cmd, $args = Array())
    {
        try {
            $args['domain'] = $this->domain;
            $args['class'] = $this->class;
            $params = '';
            foreach ($args as $key => $value) {
                $params .= '&' . urlencode($key) . '=' . urlencode($value);
            }

            $socket = $this->getConnection();

            $result = fwrite($socket, $cmd . $params . "\n");
            if ($result === false) {
                throw new \Exception(get_class($this) . "::doRequest write failed");
            }
            $line = fgets($socket);
            if ($line === false) {
                throw new \Exception(get_class($this) . "::doRequest read failed");
            }

            //print "[$line]\n";
            $words = explode(' ', $line);
            if ($words[0] == self::SUCCESS) {
                parse_str(trim($words[1]), $result);
            } else {
                if (isset($words[1])) {
                    switch ($words[1]) {
                        case 'unknown_key':
                            if (isset($args['from_key'])) {
                                trigger_error(get_class($this) . "::doRequest unknown_key {$args['from_key']}", E_USER_WARNING);
                            } else {
                                throw (new MogileFsNotFound\Exception($args['key']));
                            }
                            break;
                        case 'empty_file':
                            throw (new MogileFsFileEmpty\Exception($args['key']));
                            break;
                        default:
                            throw new \Exception(get_class($this) . "::doRequest " . trim(urldecode($line)));
                    }
                }
            }
            return $result;
        } catch (\Exception $e) {
            // Clean up
            if (isset($socket)) {
                fclose($socket);
            }
            // Recast the \Exception
            throw $e;
        }
    }

    /**
     * Return a list of domains
     * @return array
     */
    public function getDomains()
    {
        $res = $this->doRequest(self::GET_DOMAINS);

        $domains = array();
        for ($i = 1; $i <= $res['domains']; $i++) {
            $dom = 'domain' . $i;
            $classes = array();
            for ($j = 1; $j <= $res[$dom . 'classes']; $j++) {
                $classes[$res[$dom . 'class' . $j . 'name']] = $res[$dom . 'class' . $j . 'mindevcount'];
            }
            $domains[] = array('name' => $res[$dom], 'classes' => $classes);
        }
        return $domains;
    }

    /**
     * Checks if key exists at tracker
     * @param string $key
     * @return boolean
     */
    public function exists($key)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::exists key cannot be null");
        }

        try {
            $result = @$this->getPaths($key, 1);
        } catch (MogileFsNotFound\Exception $e) {
            return false;
        }

        return isset($result['path1']);
    }

    /**
     * Get an array of paths
     * @param string $key
     * @param string $pathcount
     * @param string $noverify
     * @return array
     */
    public function getPaths($key, $pathcount = 2, $noverify = 1)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::getPaths key cannot be null");
        }

        $result = $this->doRequest(self::GET_PATHS, Array(
            'key' => $key,
            'pathcount' => $pathcount,
            'noverify' => $noverify
                )
        );
        if (!isset($result['path1'])) {
            throw (new MogileFsNotFound\Exception($args['key']));
        }
        unset($result['paths']);
        return $result;
    }

    /**
     * Delete a file from system
     * @param string $key
     * @return boolean
     */
    public function delete($key)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::delete key cannot be null");
        }
        for ($i = 0; $i < $this->retrycount + 1; $i++) {
            try {
                $status = $this->doRequest(self::DELETE, Array('key' => $key));
                if (!$this->exists($key)) {
                    return $status;
                } else {
                    fclose($this->socket); // In case tracker failed, try another if possible
                }
            } catch (\Exception $e) {
                if ($i >= $this->retrycount) {
                    throw $e;
                }
            }
        }
        return false;
    }

    /**
     * Rename a file/key
     * @param string $from
     * @param string $to
     * @return boolean
     */
    public function rename($from, $to)
    {
        if ($from === null) {
            throw new \Exception(get_class($this) . "::rename from key cannot be null");
        } elseif ($to === null) {
            throw new \Exception(get_class($this) . "::rename to key cannot be null");
        }
        $this->doRequest(self::RENAME, array('from_key' => $from, 'to_key' => $to));
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
            return $this->doRequest(self::LIST_KEYS, array('prefix' => $prefix, 'after' => $lastKey, 'limit' => $limit));
        } catch (\Exception $e) {
            if (strstr($e->getMessage(), 'ERR none_match')) {
                return array();
            } else {
                throw $e;
            }
        }
    }

    /**
     * Get a file from mogstored and return it as a string
     * @param string $key
     * @return string
     */
    public function get($key)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::get key cannot be null");
        }
        $paths = $this->getPaths($key);
        foreach ($paths as $path) {
            $contents = '';
            $ch = curl_init();
            curl_setopt($ch, CURLOPT_VERBOSE, false);
            curl_setopt($ch, CURLOPT_TIMEOUT, $this->requestTimeout);
            curl_setopt($ch, CURLOPT_URL, $path);
            curl_setopt($ch, CURLOPT_FAILONERROR, true);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            $response = curl_exec($ch);
            curl_close($ch);
            if ($response !== false) {
                return $response;
            }
            // Try next source
        }
        throw new \Exception(get_class($this) . "::get unable to retrieve {$key}");
    }

    /**
     * Get file from Mogilefs and write to file
     * @return string|false
     */
    function getFile($key, $outputfile)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::get key cannot be null");
        }

        $paths = $this->getPaths($key);
        foreach ($paths as $path) {
            if (file_exists($outputfile)) {
                unlink($outputfile);
            }
            $out = fopen($outputfile, 'wb');
            if ($out === false) {
                throw new \Exception(get_class($this) . "::getFile unable to open $outputfile for writing");
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
        return false;
    }

    /**
     * Get infos for a file
     * @param string $key
     * @return array|false
     */
    public function fileinfo($key)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::fileinfo key cannot be null");
        }
        for ($i = 0; $i < $this->retrycount + 1; $i++) {
            try {
                $result = $this->doRequest(self::FILE_INFO, array('key' => $key));
                fclose($this->socket); // In case tracker failed, try another if possible
                return $result;
            } catch (\Exception $e) {
                if ($i >= $this->retrycount) {
                    throw $e;
                }
            }
        }
        return false;
    }

    /**
     * Get a file from mogstored and send it directly to stdout by way of fpassthru()
     * @param string $key
     */
    function getPassthru($key)
    {
        if ($key === null)
            throw new \Exception(get_class($this) . "::getPassthru key cannot be null");
        $paths = $this->getPaths($key);
        foreach ($paths as $path) {
            $fh = fopen($path, 'r');
            if ($fh) {
                if (fpassthru($fh) === false)
                    throw new \Exception(get_class($this) . "::getPassthru failed");
                fclose($fh);
            }
            return $success;
        }
        throw new \Exception(get_class($this) . "::getPassthru unable to retrieve {$key}");
    }

    /**
     * Stores an open file handle to Mogilefs
     * @param string $key
     * @param resource $fh
     * @param int $length
     * @return boolean
     */
    public function setResource($key, $fh, $length)
    {
        if ($key === null) {
            if (is_resource($fh)) {
                fclose($fh);
            }
            throw new \Exception(get_class($this) . "::setResource key cannot be null");
        }

        $location = $this->doRequest(self::CREATE_OPEN, array('key' => $key));
        $uri = $location['path'];
        $parts = parse_url($uri);
        $host = $parts['host'];
        $port = $parts['port'];
        $path = $parts['path'];
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_VERBOSE, false);
        curl_setopt($ch, CURLOPT_INFILE, $fh);
        curl_setopt($ch, CURLOPT_INFILESIZE, $length);
        curl_setopt($ch, CURLOPT_TIMEOUT, $this->requestTimeout);
        curl_setopt($ch, CURLOPT_PUT, $this->putTimeout);
        curl_setopt($ch, CURLOPT_URL, $uri);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_HTTPHEADER, array('Expect: '));
        $response = curl_exec($ch);
        fclose($fh);
        if ($response === false) {
            $error = curl_error($ch);
            curl_close($ch);
            throw new \Exception(get_class($this) . "::set $error for $uri");
        }
        curl_close($ch);
        $this->doRequest(self::CREATE_CLOSE, array('key' => $key,
            'devid' => $location['devid'],
            'fid' => $location['fid'],
            'path' => urldecode($uri)
        ));
        return true;
    }

    /**
     * Stores string into file
     * @param string $key
     * @param string $value
     * @return boolean
     */
    public function set($key, $value)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::set key cannot be null");
        }
        $fh = fopen('php://memory', 'rw');
        if ($fh === false) {
            throw new \Exception(get_class($this) . "::set failed to open memory stream");
        }
        fwrite($fh, $value);
        rewind($fh);
        return $this->setResource($key, $fh, strlen($value));
    }

    /**
     * Stores a local file in Mogilefs
     * @param string $key
     * @param string $file
     * @return boolean
     */
    public function setFile($key, $file)
    {
        if ($key === null) {
            throw new \Exception(get_class($this) . "::setFile key cannot be null");
        }
        if (!file_exists($file)) {
            throw new \Exception("No such file: $file");
        }
        $filesize = filesize($file);
        for ($i = 0; $i < $this->retrycount + 1; $i++) {
            try {
                $fh = fopen($file, 'r');
                if ($fh === false) {
                    throw new \Exception(get_class($this) . "::setFile failed to open $file for reading");
                }
                $status = $this->setResource($key, $fh, $filesize);
                if ($this->exists($key)) {
                    return $status;
                } else {
                    fclose($this->socket); // In case tracker died
                }
            } catch (\Exception $e) {
                if (is_resource($fh)) {
                    fclose($fh);
                }
                if ($i >= $this->retrycount) {
                    throw $e;
                }
            }
        }
        return false;
    }

}
