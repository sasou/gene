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
    /**
     * HTML body/attribute escape (ENT_QUOTES|ENT_SUBSTITUTE, UTF-8); null/false/array -> "".
     * Template {$v} / {$a.b} compile to this.
     * @param mixed $value
     * @return string
     */
    public static function e($value) {}

    /**
     * JSON literal safe inside <script> (JSON_HEX_TAG|AMP|APOS|QUOT).
     * @param mixed $value
     * @return string
     */
    public static function js($value) {}

    /**
     * Escaped URL for href/src; schemes other than http/https/mailto/tel (and relative) become "#".
     * @param string $url
     * @return string
     */
    public static function safeUrl($url) {}

    /**
     * Rich text through the registered purifier; escapes when none is set.
     * @param mixed $html
     * @return string
     */
    public static function clean($html) {}

    /**
     * @param callable|null $purifier fn(string $html): string
     * @return void
     */
    public static function setPurifier($purifier) {}

    /**
     * Compile every template under the view directory (deploy step).
     * @return int templates compiled or verified fresh
     */
    public static function compileAll() {}

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
     * __isset
     * isset()/empty() support for magic properties, aligned with __get
     * (class-qualified key + DI component fallback, then assign() variables).
     *
     * @param mixed $name name
     * @return bool
     */
    public function __isset($name) {

    }

    /**
     * render
     * Same template resolution as display(), but the output is captured and
     * returned as a string. Does not clear assigned variables, so one assign
     * set can feed several renders. The second argument is the layout
     * template, not a variable bag.
     *
     * @param string $file template path relative to the view root
     * @param string|null $parent_file layout template; $file is nested into it
     * @return string|false rendered output, or false if the output buffer fails to start
     */
    public function render($file, $parent_file = null) {

    }

    /**
     * clearAssign
     * Clear all previously assigned view variables.
     *
     * @return bool
     */
    public function clearAssign() {

    }

}