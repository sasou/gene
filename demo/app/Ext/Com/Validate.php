<?php
namespace Ext\Com;
/**
 * Validate
 */
class Validate
{

    /**
     * Validate Errors
     *
     * @var array
     */
    public $errors = array();

    /**
     * Check if is not empty
     *
     * @param string $str
     * @return boolean
     */
    public static function notEmpty($str, $trim = TRUE)
    {
        if (is_array($str)) {
            return 0 < count($str);
        }

        return strlen($trim ? trim($str) : $str) ? TRUE : FALSE;
    }

    /**
     * Match regex
     *
     * @param string $value
     * @param string $regex
     * @return boolean
     */
    public static function match($value, $regex)
    {
        return preg_match($regex, $value) ? TRUE : FALSE;
    }

    /**
     * Max
     *
     * @param mixed $value numbernic|string
     * @param number $max
     * @return boolean
     */
    public static function max($value, $max)
    {
        is_string($value) && $value = strlen($value);
        return $value <= $max;
    }

    /**
     * Min
     *
     * @param mixed $value numbernic|string
     * @param number $min
     * @return boolean
     */
    public static function min($value, $min)
    {
        is_string($value) && $value = strlen($value);
        return $value >= $min;
    }

    /**
     * Range
     *
     * @param mixed $value numbernic|string
     * @param array $max
     * @return boolean
     */
    public static function range($value, $range)
    {
        is_string($value) && $value = strlen($value);
        return (($value >= $range[0]) && ($value <= $range[1]));
    }

    /**
     * Check if in array
     *
     * @param mixed $value
     * @param array $list
     * @return boolean
     */
    public static function in($value, $list)
    {
        return in_array($value, $list);
    }

    /**
     * Check if is email
     *
     * @param string $email
     * @return boolean
     */
    public static function email($email)
    {
        return preg_match('/^[a-z0-9_\-]+(\.[_a-z0-9\-]+)*@([_a-z0-9\-]+\.)+([a-z]{2}|aero|arpa|biz|com|coop|edu|gov|info|int|jobs|mil|museum|name|nato|net|org|pro|travel)$/', $email) ? TRUE : FALSE;
    }

    /**
     * Check if is url
     *
     * @param string $url
     * @return boolean
     */
    public static function url($url)
    {
        return preg_match('/^((https?|ftp|news):\/\/)?([a-z]([a-z0-9\-]*\.)+([a-z]{2}|aero|arpa|biz|com|coop|edu|gov|info|int|jobs|mil|museum|name|nato|net|org|pro|travel)|(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]))(\/[a-z0-9_\-\.~]+)*(\/([a-z0-9_\-\.]*)(\?[a-z0-9+_\-\.%=&amp;]*)?)?(#[a-z][a-z0-9_]*)?$/i', $url) ? TRUE : FALSE;
    }

    /**
     * Check if is ip
     *
     * @param string $ip
     * @return boolean
     */
    public static function ip($ip)
    {
        return ((FALSE !== ip2long($ip)) && (long2ip(ip2long($ip)) === $ip));
    }

    /**
     * Check if is date
     *
     * @param string $date
     * @return boolean
     */
    public static function date($date)
    {
        return preg_match('/^\d{4}[\/-]\d{1,2}[\/-]\d{1,2}$/', $date) ? TRUE : FALSE;
    }

    /**
     * Check if is datetime
     *
     * @param string $datetime
     * @return boolean
     */
    public static function datetime($datetime, $format = 'Y-m-d H:i:s')
    {
        return ($time = strtotime($datetime)) && ($datetime == date($format, $time));
    }

    /**
     * Check if is numbers
     *
     * @param mixed $value
     * @return boolean
     */
    public static function number($value)
    {
        return is_numeric($value);
    }

    /**
     * Check if is int
     *
     * @param mixed $value
     * @return boolean
     */
    public static function int($value)
    {
        return is_int($value);
    }

    /**
     * Check if is digit
     *
     * @param mixed $value
     * @return boolean
     */
    public static function digit($value)
    {
        return is_int($value) || ctype_digit($value);
    }

    /**
     * Check if is string
     *
     * @param mixed $value
     * @return boolean
     */
    public static function string($value)
    {
        return is_string($value);
    }

    /**
     * Check
     *
     * $rules = array(
     *     'required' => TRUE if required , FALSE for not
     *     'type'     => var type, should be in ('email', 'url', 'ip', 'date', 'number', 'int', 'string')
     *     'regex'    => regex code to match
     *     'func'     => validate function, use the var as arg
     *     'max'      => max number or max length
     *     'min'      => min number or min length
     *     'range'    => range number or range length
     *     'msg'      => error message,can be as an array
     * )
     *
     * @param array $data
     * @param array $rules
     * @param boolean $ignorNotExists
     * @return boolean
     */
    public function check($data, $rules, $ignorNotExists = FALSE)
    {
        foreach ($rules as $key => $rule) {
            $rule += array('required' => FALSE, 'msg' => 'Unvalidated');

            // deal with not existed
            if (!isset($data[$key])) {
                if ($rule['required'] && !$ignorNotExists) {
                    $this->errors[$key] = $rule['msg'];
                }
                continue;
            }

            if (!self::_check($data[$key], $rule)) {
                $this->errors[$key] = $rule['msg'];
                continue;
            }

            if (isset($rule['rules'])) {
                $tmp = $this->check($data[$key], $rule['rules'], $ignorNotExists);
                if (0 !== $tmp['code']) {
                    $this->errors[$key] = $tmp['msg'];
                }
            }
        }

        return $this->errors ? FALSE : TRUE;
    }

    /**
     * Check value
     *
     * @param mixed $value
     * @param array $rule
     * @return mixed string as error, TRUE for OK
     */
    protected static function _check($value, $rule)
    {
        $flag = TRUE;
        foreach ($rule as $key => $val) {
            switch ($key) {
                case 'required':
                    if ($val) {
                        $flag = self::notEmpty($value);
                    }
                    break;

                case 'func':
                    $flag = call_user_func($val, $value);
                    break;

                case 'regex':
                    $flag = self::match($value, $val);
                    break;

                case 'type':
                    $flag = self::$val($value);
                    break;

                case 'max':
                case 'min':
                case 'max':
                case 'range':
                    $flag = self::$key($value, $val);
                    break;

                case 'each':
                    $val += array('required' => FALSE);
                    foreach ($value as $item) {
                        if (!$flag = self::_check($item, $val)) {
                            break;
                        }
                    }
                    break;
                default:
                    break;
            }
            if (!$flag) {
                return FALSE;
            }
        }

        return TRUE;
    }

    /**
     * 18中国位身份证有效验证
     *
     * @param type $idcard
     * @return boolean
     */
    public static function idcard($idcard)
    {
        if (!isset($idcard{17})) {
            return FALSE;
        }

        // 加权因子
        $factor = array(7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2);

        // 校验码对应值
        $verifyNumberList = array('1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2');

        $checksum = 0;
        $i = 17;
        while ($i--) {
            $checksum += $idcard{$i} * $factor[$i];
        }

        return ($verifyNumberList[$checksum % 11] == strtoupper(substr($idcard, -1)));
    }

    /**
     * 判断是否是手机号码
     * @param $string $phoneNumber 被检测的手机号码
     * @return bool
     */
    public static function isMobileNumber($phoneNumber)
    {
        return (preg_match("/^(13[0-9]|147|15[0-9]|17[0-9]|18[0-9]|19[0-9])\d{8}$/", $phoneNumber)) ? TRUE : FALSE;
    }

    /**
     * Get error
     *
     * @return array
     */
    public function error()
    {
        return $this->errors;
    }

}
