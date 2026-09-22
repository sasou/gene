<?php
namespace Gene;

/**
 * 内置请求参数扫描器。
 *
 * 应用入口通常使用 Application::webscan() 完成配置；直接实例化主要用于
 * 需要显式执行 check() 的底层场景。
 *
 * @version 6.2.5
 */
class Webscan
{
    /**
     * @param int $webscan_switch 是否启用
     * @param string|null $webscan_white_directory 目录白名单
     * @param array|null $webscan_white_url URL 白名单
     * @param int $webscan_get 是否扫描 GET
     * @param int $webscan_post 是否扫描 POST
     * @param int $webscan_cookie 是否扫描 Cookie
     * @param int $webscan_referer 是否扫描 Referer
     */
    public function __construct(
        $webscan_switch = 1,
        $webscan_white_directory = null,
        $webscan_white_url = null,
        $webscan_get = 1,
        $webscan_post = 1,
        $webscan_cookie = 1,
        $webscan_referer = 1
    ) {}

    /**
     * 扫描当前请求；检测到攻击特征时返回 true。
     *
     * @return bool
     */
    public function check() {}
}
