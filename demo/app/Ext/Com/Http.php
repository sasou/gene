<?php
namespace Ext\Com;
/**
 * Http
 */
class Http
{

    /**
     * Default params
     *
     * @var array
     */
    public static $defaultParams = array(
        'headers' => array(),
        'timeout' => 15,
        'ssl' => false,
        'opts' => array(),
        'debug' => false,
    );

    /**
     * HTTP GET
     *
     * @param string $url
     * @param array $data
     * @param array $params
     * @return string
     */
    public static function get($url, $data = array(), $params = array())
    {
        if ($data) {
            $queryStr = http_build_query($data);
            $url .= "?{$queryStr}";
        }

        return self::request($url, $params);
    }

    /**
     * HTTP POST
     *
     * @param string $url
     * @param array $data
     * @param array $params
     * @return string
     */
    public static function post($url, $data, $params = array())
    {
        $params['opts'][CURLOPT_POST] = true;
        $params['opts'][CURLOPT_POSTFIELDS] = http_build_query($data);
        return self::request($url, $params);
    }

    /**
     * HTTP request
     *
     * @param string $url
     * @param array $params
     * @return string or throw \Exception
     */
    public static function request($url, $params)
    {
        if (!function_exists('curl_init')) {
            throw new Cola_\Exception('Can not find curl extension');
        }

        $curl = curl_init();
        $params = self::initOpts($url, $params);
        curl_setopt_array($curl, $params['opts']);
        $response = curl_exec($curl);

        $errno = curl_errno($curl);
        $error = curl_error($curl);

        if ($params['debug']) {
            return array(
                'url' => $url,
                'httpInfo' => curl_getinfo($curl),
                'response' => $response,
                'error' => $error,
                'errno' => $errno
            );
        }

        if (0 !== $errno) {
            $error .= " ~ {$url} ~ " . json_encode($params);
            throw new Cola_\Exception($error, $errno);
        }

        curl_close($curl);
        return $response;
    }

    /**
     * Init curl opts
     *
     * @param string $url
     * @param array $params
     * @return array
     */
    public static function initOpts($url, $params)
    {
        $params += self::$defaultParams;
        $params['opts'] += array(
            CURLOPT_URL => $url,
            CURLOPT_TIMEOUT => $params['timeout'],
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_SSL_VERIFYPEER => $params['ssl'],
            CURLOPT_DNS_CACHE_TIMEOUT => 60,
        );

        if ($params['headers']) {
            $params['opts'][CURLOPT_HTTPHEADER] = $params['headers'];
        }

        return $params;
    }

}
