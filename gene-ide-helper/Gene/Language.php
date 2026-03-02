<?php
namespace Gene;

/**
 * Language
 *
 * 语言包读取与当前语言管理。
 * 通过配置注入为 `$this->language`，结合路由/URL 中的语言前缀使用。
 *
 * 示例：
 *
 *  $language = new \Gene\Language('web', 'en');
 *  $language->web('zh');        // 切换到 Language/Web/Zh.php
 *  echo $language->login_title; // 读取语言键
 *
 * @author  sasou<admin@php-gene.com>
 * @version 3.0.5
 */
class Language
{
    /**
     * __construct
     *
     * @param string $dir         语言目录名（如 web、admin）
     * @param string $defaultLang 默认语言（如 en、zh）
     * @return void
     */
    public function __construct($dir, $defaultLang = 'en')
    {

    }

    /**
     * lang
     *
     * 设置当前语言（如 zh、en、ru），返回当前对象以便链式调用。
     *
     * @param string|null $lang 语言代码
     * @return static
     */
    public function lang($lang = null)
    {

    }

    /**
     * __call
     *
     * `$language->web('zh')` 等价于设置当前语言目录为 web，语言为 zh。
     * 之后通过 `__get` 读取对应语言文件中的键值。
     *
     * @param string $name 方法名，作为语言目录名（如 web、admin）
     * @param array  $args 第一个参数为语言代码（如 zh、en）
     * @return static
     */
    public function __call($name, $args)
    {

    }

    /**
     * __get
     *
     * 读取当前目录/语言对应语言文件中的键值：
     *  Language/{Dir}/{Lang}.php 返回数组，键为 $name。
     *
     * @param string $name 语言键名
     * @return string
     */
    public function __get($name)
    {

    }
}

