<?php
namespace Ext\Com;
/**
 * Sms
 */
class Sms
{

    /**
     *
     * @var string host
     * @var string name
     * @var int port
     * @var int $appId
     */
    protected static $_config = array();
    private static $_uri = NULL;

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
    }

    /**
     * Put data to queue.
     * @deprecated since version 20141130
     * @param number $phoneNumber mobile number
     * @param string $data 短信内容
     * @param string $uri queue uri
     * @param string $name queue name
     * @param int $appId app id
     * @param string $userId 接收者用户ID
     * @param int $statusReview 短信发送状态反馈，如果需要反馈绑定内容的反馈ID
     * @param string $moduleTag 业务模块反馈，需要用户反馈绑定相关模块业务代码标签
     * @return Boolean
     */
    public static function put($phoneNumber, $data, $uri = NULL, $name = NULL, $appId = NULL, $userId = NULL, $statusReview = NULL, $moduleTag = NULL)
    {
        try {
            if (empty($phoneNumber)) {
                return FALSE;
            }

            NULL === $uri && $uri = self::$_uri;

            NULL === $name && $name = self::config('name');

            NULL === $appId && $appId = self::config('appId');

            NULL !== $moduleTag && $data .= "回复短信请编辑{$moduleTag}#内容";

            NULL !== $statusReview && $appId .= ",{$statusReview}";

            $json = json_encode(array(
                'app_id' => (string) $appId,
                'content' => str_replace(array('【', '】'), array('[', ']'), $data),
                'phone' => $phoneNumber,
                'user_id' => (string) $userId
            ));

            $context = array(
                'opt' => 'put',
                'name' => $name,
                'data' => $json
            );
            $data = Cola_Com_Http::get($uri, $context);

            if ('HTTPSQS_PUT_OK' == $data) {
                return TRUE;
            }

            return FALSE;
        } catch (\Exception $e) {
            throw new Cola_\Exception($e->getMessage(), $e->getCode());
        }
    }

    /**
     * Put data to queue.
     *
     * @param number $phoneNumber mobile number
     * @param string $data 短信内容
     * @param string $uri queue uri
     * @param string $name queue name
     * @param int $appId app id
     * @param string $receiveUserId 接收者用户ID
     * @param int $statusReview 短信发送状态反馈，如果需要反馈绑定内容的反馈ID
     * @param string $moduleTag 业务模块反馈，需要用户反馈绑定相关模块业务代码标签
     * @param string $sendUserId 发送者用户Id，非用户发送而为空
     * @return Boolean
     */
    public static function send($phoneNumber, $data, $uri = NULL, $name = NULL, $appId = NULL, $receiveUserId = NULL, $statusReview = NULL, $moduleTag = NULL, $sendUserId = '')
    {
        try {
            if (empty($phoneNumber)) {
                return FALSE;
            }

            NULL === $uri && $uri = self::$_uri;

            NULL === $name && $name = self::config('name');

            NULL === $appId && $appId = self::config('appId');

            NULL !== $moduleTag && $data .= "回复短信请编辑{$moduleTag}#内容";

            $json = json_encode(array(
                'app_id' => $appId,
                'content' => $data,
                'content_id' => NULL !== $statusReview ? (int) $statusReview : 0,
                'phone' => $phoneNumber,
                'receive_user_id' => (string) $receiveUserId,
                'send_user_id' => (string) $sendUserId,
            ));

            $context = array(
                'opt' => 'put',
                'name' => $name,
                'data' => $json
            );
            $data = Cola_Com_Http::get($uri, $context);

            if ('HTTPSQS_PUT_OK' == $data) {
                return TRUE;
            }

            return FALSE;
        } catch (\Exception $e) {
            throw new Cola_\Exception($e->getMessage(), $e->getCode());
        }
    }

}
