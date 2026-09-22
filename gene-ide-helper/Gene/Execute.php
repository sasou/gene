<?php
namespace Gene;

/**
 * PHP 代码编译与执行辅助类。
 *
 * 该 API 会编译或执行调用方提供的 PHP 源码，只应处理可信代码，
 * 不得传入请求参数或其他外部输入。
 *
 * @version 6.2.5
 */
class Execute
{
    /**
     * @param int $debug 非零时在 opcode 项中附加调试信息
     */
    public function __construct($debug = 0) {}

    /**
     * 编译 PHP 源码并返回 opcode 信息，不执行源码。
     *
     * @param string $php_script PHP 源码
     * @return array
     */
    public function GetOpcodes($php_script) {}

    /**
     * 在当前进程中执行 PHP 源码。
     *
     * @param string $php_script 仅限可信 PHP 源码
     * @return bool
     */
    public function StringRun($php_script) {}
}
