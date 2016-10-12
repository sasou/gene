<?php
namespace Ext\Com;
/**
 * Email queue
 */
class Email
{

    /**
     *
     * @var string host
     * @var int port
     * @var string name
     */
    protected static $_config = array();

    /**
     *
     * @var string $_uri
     */
    private static $_uri = null;

    public function __construct($config)
    {
        self::$_config = $config;
        self::$_uri = 'http://' . $config['host'] . ':' . $config['port'];
    }

    /**
     * Config
     *
     * Set or get configration
     * @param string $name
     * @param mixed $value
     * @return mixed
     */
    public static function config($name = NULL, $value = NULL)
    {
        if (NULL === $name) {
            return self::$_config;
        }
        if (NULL === $value) {
            return isset(self::$_config[$name]) ? self::$_config[$name] : NULL;
        }
        self::$_config[$name] = $value;

        return $this;
    }

    /**
     * Put data to queue.
     *
     * @param string $mailTo 邮件接收地址
     * @param string $subject 邮件主题
     * @param string $data 邮件内容
     * @param string $mailType 发送邮件的格式：TXT Or HTML
     * @param string $bodyTyep 0系统通知，1用户注册验证，2用户密码找回，3用户审核通知，4小站审批通知，5班级审批通知，6系统异常通知，7短信网关异常通知
     * @param string $uri 邮件的队列服务器地址
     * @param string $name 邮件队列名
     * @return boolean
     * @throws \Exception
     */
    public static function put($mailTo, $subject, $data, $mailType = 'HTML', $bodyTyep = 0, $uri = NULL, $name = NULL)
    {
        try {
            if (empty($mailTo)) {
                return FALSE;
            }

            NULL === $uri && $uri = self::$_uri;

            NULL === $name && $name = self::config('name');

            $context = array(
                'name' => $name,
                'opt' => 'put',
                'data' => json_encode(
                        array(
                            'email' => $mailTo,
                            'subject' => $subject,
                            'data' => $data,
                            'type' => $mailType,
                            'bodyType' => $bodyTyep,
                        )
                )
            );

            if ('HTTPSQS_PUT_OK' == Cola_Com_Http::get($uri, $context)) {
                return TRUE;
            }

            return FALSE;
        } catch (\Exception $e) {
            throw $e;
        }
    }

}
