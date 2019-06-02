<?php
namespace Api;

/**
 * User Api Controller
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class User extends \Gene\Controller
{
    
    /**
     * 远程登录
     * 
     * @param string $username 用户名
     * @param string $password 密码
     * @return array
     */
    function remoteLogin()
    {
        return $this->data(\Gene\Request::post());
    }

}
