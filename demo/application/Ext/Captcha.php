<?php
namespace Ext;

/**
 * Captcha
 */
class Captcha extends \Gene\Service
{

    private $charset = 'abcdefghkmnprstuvwxyzABCDEFGHKMNPRSTUVWXYZ23456789'; //随机因子  
    private $code; //验证码  
    private $codelen = 4; //验证码长度  
    private $width = 120; //宽度  
    private $height = 36; //高度  
    private $img; //图形资源句柄  
    private $font; //指定的字体  
    private $fontsize = 20; //指定字体大小  
    private $fontcolor; //指定字体颜色  

    //构造方法初始化  

    public function __construct()
    {
        $this->font = dirname(__FILE__) . '/fonts/elephant.ttf'; //注意字体路径要写对，否则显示不了图片  
    }
    
    //生成随机码  
    private function createCode()
    {
        $_len = strlen($this->charset) - 1;
        for ($i = 0; $i < $this->codelen; $i++) {
            $this->code .= $this->charset[mt_rand(0, $_len)];
        }
    }

    //生成背景  
    private function createBg()
    {
        $this->img = imagecreatetruecolor($this->width, $this->height);
        $color = imagecolorallocate($this->img, mt_rand(157, 255), mt_rand(157, 255), mt_rand(157, 255));
        imagefilledrectangle($this->img, 0, $this->height, $this->width, 0, $color);
    }

    //生成文字  
    private function createFont()
    {
        $_x = $this->width / $this->codelen;
        $code = $this->getCode();
        for ($i = 0; $i < $this->codelen; $i++) {
            $this->fontcolor = imagecolorallocate($this->img, mt_rand(0, 156), mt_rand(0, 156), mt_rand(0, 156));
            imagettftext($this->img, $this->fontsize, mt_rand(-30, 30), $_x * $i + mt_rand(1, 5), $this->height / 1.4, $this->fontcolor, $this->font, $code[$i]);
        }
    }

    //生成线条、雪花  
    private function createLine()
    {
        //线条  
        for ($i = 0; $i < 6; $i++) {
            $color = imagecolorallocate($this->img, mt_rand(0, 156), mt_rand(0, 156), mt_rand(0, 156));
            imageline($this->img, mt_rand(0, $this->width), mt_rand(0, $this->height), mt_rand(0, $this->width), mt_rand(0, $this->height), $color);
        }
        //雪花  
        for ($i = 0; $i < 100; $i++) {
            $color = imagecolorallocate($this->img, mt_rand(200, 255), mt_rand(200, 255), mt_rand(200, 255));
            imagestring($this->img, mt_rand(1, 5), mt_rand(0, $this->width), mt_rand(0, $this->height), '*', $color);
        }
    }

    //输出  
    private function outPut()
    {
        $this->response->header('Content-type', "image/png");
        imagepng($this->img);
        imagedestroy($this->img);
    }

    //对外生成  
    public function create($width = null, $height = null)
    {
        $width != null && $this->width = $width;
        $height != null && $this->height = $height;
        $this->createBg();
        $this->createLine();
        $this->createFont();
        $this->outPut();
    }

    //获取验证码  
    public function getCode()
    {
        $this->createCode();
        return strtolower($this->code);
    }

    
    //生成base64图片  
    public function img($code, $htmlColor = "#000")
    {
        $code != null && $this->code = $code;
        $this->fontsize = 14;
        $this->width = strlen($code) * $this->fontsize;
        $this->height = $this->fontsize + 2;

        $hex = str_replace("#", "", $htmlColor);
        if(strlen($hex) == 3) {
           $r = hexdec(substr($hex,0,1).substr($hex,0,1));
           $g = hexdec(substr($hex,1,1).substr($hex,1,1));
           $b = hexdec(substr($hex,2,1).substr($hex,2,1));
        } else {
           $r = hexdec(substr($hex,0,2));
           $g = hexdec(substr($hex,2,2));
           $b = hexdec(substr($hex,4,2));
        }
        $this->img = imagecreatetruecolor($this->width, $this->height);
        $color = imagecolorallocate($this->img, 255, 255, 255);
        imagefilledrectangle($this->img, 0, $this->height, $this->width, 0, $color);
        $this->fontcolor = imagecolorallocate($this->img, $r, $g, $b);
        imagestring($this->img,$this->fontsize,0, 0,$this->code,$this->fontcolor); 
        ob_start();
        imagepng($this->img);
        $image_data = ob_get_contents();
        ob_end_clean();
        $base64_image = 'data:png;base64,' . chunk_split(base64_encode($image_data));
        return $base64_image;
    }
    
}
