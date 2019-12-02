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
        $data = $this->request->post;
        $data['captcha_system'] = $this->session->get('login-captcha');
        $this->validate->init($data)
             ->name('username')->required()->msg("请输入用户名")
             ->name('password')->required()->msg("请输入密码")
             ->name('captcha')->required()->length(4, 4)->msg("验证码格式不正确")
             ->equal("captcha_system")->msg("验证码校验错误");
        
        if (!$this->validate->valid()) {
            return $this->error($this->validate->error());
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
        $this->session->del('admin');
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
        $captcha = \Ext\Captcha::getInstance();
        $this->session->set('login-captcha', $captcha->getCode());
        $captcha->create();
    }
    
}
