<?php
namespace Controllers\Admin;

/**
 * Index Controller
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Index extends \Gene\Controller
{

    /**
     * run
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function run()
    {
        $this->title = 'Gene';
        $this->loginInfo = \Services\Admin\Log::getInstance()->getUserLastLoginInfo($this->user['user_id']);
        $this->display("welcome", "parent");
    }
    
    /**
     * login
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function login()
    {
        $this->title = 'Gene';
        $this->display("index/login");
    }
    
    /**
     * loginPost
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function loginPost()
    {
        $data = $this->post();
        if (!isset($data['captcha']) || strlen($data['captcha']) != 4) {
            return $this->error("请输入正确的验证码");
        }

        if (\Gene\Session::get('login-captcha') != $data['captcha']) {
            return $this->error("验证码错误");
        }
        return \Services\Admin\User::getInstance()->checkUser($data['username'], $data['password']);
    }
    
    /**
     * exits
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function exits()
    {
        $this->user;
        \Services\Admin\Log::getInstance()->log("后台退出", "用户名:" . $this->user['user_name'], $this->user['user_id']);
        \gene\session::del('admin');
        return $this->success("退出成功");
    }
    
    /**
     * captcha
     *
     * @param  mixed $params url参数
     * @return mixed
     */
    function captcha()
    {
        $captcha = new \Ext\Captcha();
        \gene\session::set('login-captcha', $captcha->getCode());
        $captcha->create();
    }
    
}
