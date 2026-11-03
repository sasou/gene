<?php
namespace Ext;

/**
 * UploadCheck — 上传安全校验参考实现（S11）。
 *
 * 多个应用各自封装过一遍同类逻辑（扩展名白名单 + 图片内容校验最完整的版本），
 * 尚未达到下沉 C 层的重复度阈值，先在 demo 留一份可直接抄走的 PHP 版。
 *
 * 使用：
 *   $r = UploadCheck::check($_FILES['up'], ['ext' => ['jpg','png','webp'], 'max' => 8*1024*1024]);
 *   if (!$r['ok']) return $this->error($r['msg']);
 *
 * 校验链（任一步失败即拒）：
 *   1. 大小上限               —— 内存与落盘的双重防线
 *   2. 扩展名白名单           —— 小写比较，不可伪造大小写绕过
 *   3. finfo 真实 MIME        —— 扩展名说谎时拦截（php 改后缀传 webshell）
 *   4. 图片内容校验           —— 对图片扩展名跑 getimagesize，
 *                              非图内容即使 MIME 蒙混也拿不到 "ok"
 */
class UploadCheck extends \Gene\Service
{
    /**
     * 扩展名 → 允许的 MIME 组（同一名扩展可对应多种合法 MIME）。
     * 只放 demo 需要的常见项；生产按业务裁剪，但保持"白名单"语义。
     */
    private static $mimeMap = [
        'jpg'  => ['image/jpeg'],
        'jpeg' => ['image/jpeg'],
        'png'  => ['image/png'],
        'gif'  => ['image/gif'],
        'webp' => ['image/webp'],
        'bmp'  => ['image/bmp', 'image/x-ms-bmp'],
        'svg'  => ['image/svg+xml', 'text/plain', 'text/xml'],   // svg 是文本载体
        'mp4'  => ['video/mp4'],
        'mp3'  => ['audio/mpeg'],
        'zip'  => ['application/zip'],
        'pdf'  => ['application/pdf'],
        'txt'  => ['text/plain'],
    ];

    /** 需要跑"图片内容真校验"的扩展名（svg 是文本，getimagesize 不适用） */
    private static $imageExts = ['jpg', 'jpeg', 'png', 'gif', 'webp', 'bmp'];

    /**
     * check — 对 $_FILES 行或裸路径做全链校验
     * @param array|string $file  $_FILES['x'] 结构，或临时文件路径字符串
     * @param array $opts ['ext'=>[白名单，必填],'max'=>字节,'allow_svg'=>bool]
     * @return array ['ok'=>bool,'msg'=>string,'ext'=>string,'mime'=>string,'size'=>int]
     */
    public static function check($file, array $opts)
    {
        $r = ['ok' => false, 'msg' => '', 'ext' => '', 'mime' => '', 'size' => 0];

        /* ---- 0. 参数与文件本体 ---- */
        $path = is_array($file) ? ($file['tmp_name'] ?? '') : (string)$file;
        $name = is_array($file) ? ($file['name'] ?? '') : basename($path);
        $size = is_array($file) ? (int)($file['size'] ?? 0) : (is_file($path) ? filesize($path) : 0);
        if (!is_string($path) || $path === '' || !is_file($path)) {
            $r['msg'] = '文件不存在';
            return $r;
        }
        $allow = array_map('strtolower', $opts['ext'] ?? []);
        if (empty($allow)) {
            $r['msg'] = '未配置扩展名白名单';
            return $r;
        }
        $max = (int)($opts['max'] ?? 10 * 1024 * 1024);
        $allowSvg = !empty($opts['allow_svg']);

        /* ---- 1. 大小 ---- */
        $r['size'] = $size;
        if ($size <= 0) {
            $r['msg'] = '空文件';
            return $r;
        }
        if ($size > $max) {
            $r['msg'] = '文件超过大小限制';
            return $r;
        }

        /* ---- 2. 扩展名白名单 ---- */
        $ext = strtolower(pathinfo($name, PATHINFO_EXTENSION));
        $r['ext'] = $ext;
        if ($ext === '' || !in_array($ext, $allow, true)) {
            $r['msg'] = '不支持的文件类型';
            return $r;
        }
        if ($ext === 'svg' && !$allowSvg) {
            /* svg 可携带 <script>：默认拒绝，业务显式 allow_svg 且做内容消毒后再放行 */
            $r['msg'] = 'SVG 上传默认禁用';
            return $r;
        }

        /* ---- 3. finfo 真实 MIME ---- */
        $mime = '';
        if (function_exists('finfo_open')) {
            $fi = finfo_open(FILEINFO_MIME_TYPE);
            if ($fi) {
                $mime = (string)finfo_file($fi, $path);
                finfo_close($fi);
            }
        }
        $r['mime'] = $mime;
        if ($mime !== '' && isset(self::$mimeMap[$ext])
            && !in_array($mime, self::$mimeMap[$ext], true)) {
            /* 扩展名说自己是 jpg，内容却不是图片一族 → 拒绝 */
            $r['msg'] = '文件内容与类型不符';
            return $r;
        }

        /* ---- 4. 图片内容校验 ---- */
        if (in_array($ext, self::$imageExts, true)) {
            $info = @getimagesize($path);
            if (!is_array($info) || ($info[0] ?? 0) <= 0 || ($info[1] ?? 0) <= 0) {
                /* MIME 可以蒙混（如 polyglot），像素头蒙混不了 */
                $r['msg'] = '文件内容不是有效图片';
                return $r;
            }
            /* 极端尺寸/畸形比例的附加防线（像素炸弹防护在解码端才完整，
             * 这里至少挡住 1x1000000 这类明显畸形） */
            $w = (int)$info[0];
            $h = (int)$info[1];
            if ($w > 20000 || $h > 20000) {
                $r['msg'] = '图片尺寸过大';
                return $r;
            }
        }

        $r['ok'] = true;
        $r['msg'] = 'ok';
        return $r;
    }
}
