<?php
namespace Gene;

/**
 * View
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */

class View
{

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }
    
    /**
     * assign
     * 
     * @param mixed $name
     * @param mixed $value
     * @return mixed
     */
    public function assign($name, $value) {

    }

    /**
     * display
     * 
     * @param mixed $file file
     * @param mixed $parent_file parent_file
     * @return mixed
     */
    public function display($file, $parent_file = null) {

    }

    /**
     * displayExt
     * 
     * @param mixed $file file
     * @param mixed $parent_file parent_file
     * @param mixed $isCompile isCompile
     * @return mixed
     */
    public function displayExt($file, $parent_file = null, $isCompile = null) {

    }

    /**
     * contains
     * 
     * @return mixed
     */
    public function contains() {

    }

    /**
     * containsExt
     * 
     * @return mixed
     */
    public function containsExt() {

    }

    /**
     * url
     * 返回带语言前缀的 URL，如 url("login.html") => "/en/login.html"
     * 传入 "/" 也会加上语言前缀，如 url("/") => "/en/"
     * $lang 未传时使用当前请求语言，指定 $lang 则返回该语言的 URL；
     * 传空串 "" 则返回不含语言前缀的 URL，如 url("login.html", "") => "/login.html"。
     *
     * @param string $path 路径，如 login.html 或 "/"
     * @param string $lang 指定语言；未传时使用当前请求语言，传 "" 则不加语言前缀
     * @return string
     */
    public function url($path, $lang = null) {

    }

    /**
     * getPath
     * 返回当前请求路径。$withoutLang=true 时去除语言前缀。
     *
     * @param bool $withoutLang 是否去除语言前缀
     * @return string|null
     */
    public function getPath($withoutLang = false) {

    }

    /**
     * getRouterUri
     * 返回当前路由 URI（:m/:c/:a 替换后，小写）。
     *
     * @return string|null
     */
    public function getRouterUri() {

    }

    /**
     * scope
     *
     * @param int $num
     * @return bool
     */
    public function scope($num = 0) {

    }

    /**
     * __get
     * 
     * @param mixed $name name
     * @return mixed
     */
    public function __get($name) {

    }

    /**
     * __set
     * 
     * @param mixed $name name
     * @param mixed $value value
     * @return mixed
     */
    public function __set($name, $value) {

    }

    /**
     * render
     * Render a template and return the output as a string without sending
     * it to the response. Variables assigned via assign() are available.
     *
     * @param string $template template file path relative to view root
     * @param array $vars optional extra variables for this render only
     * @return string rendered template output
     */
    public function render(string $template, array $vars = []) {

    }

    /**
     * clearAssign
     * Clear all previously assigned view variables.
     *
     * @return $this
     */
    public function clearAssign() {

    }

}