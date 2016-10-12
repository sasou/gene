<?php
namespace Ext\Com;
/**
 * Upload
 */
class Upload
{

    /**
     * Upload error message
     *
     * @var array
     */
    protected $_message = array(
        1 => 'upload_file_exceeds_limit',
        2 => 'upload_file_exceeds_form_limit',
        3 => 'upload_file_partial',
        4 => 'upload_no_file_selected',
        6 => 'upload_no_temp_directory',
        7 => 'upload_unable_to_write_file',
        8 => 'upload_stopped_by_extension'
    );

    /**
     * Upload config
     *
     * @var array
     */
    protected $_config = array(
        'savePath' => '/tmp',
        'maxSize' => 0,
        'maxWidth' => 0,
        'maxHeight' => 0,
        'allowedExts' => '*',
        'allowedTypes' => '*',
        'override' => false,
        'mogilefs' => array(),
    );

    /**
     * The num of successfully uploader files
     *
     * @var int
     */
    protected $_num = 0;

    /**
     * Formated $_FILES
     *
     * @var array
     */
    protected $_files = array();

    /**
     * Error
     *
     * @var array
     */
    protected $_error;

    /**
     * mogilefs
     *
     * @var Cola_Com_Mogilefs
     */
    protected $_mogilefs = NULL;

    /**
     * Constructor
     *
     * Construct && formate $_FILES
     * @param array $config
     */
    public function __construct(array $config = array())
    {
        $this->_config = $config + $this->_config;

        $this->_config['savePath'] = rtrim($this->_config['savePath'], DIRECTORY_SEPARATOR);

        $this->_format();
    }

    /**
     * Config
     *
     * Set or get configration
     * @param string $name
     * @param mixed $value
     * @return mixed
     */
    public function config($name = NULL, $value = NULL)
    {
        if (NULL == $name) {
            return $this->_config;
        }

        if (NULL == $value) {
            return isset($this->_config[$name]) ? $this->_config[$name] : NULL;
        }

        $this->_config[$name] = $value;

        return $this;
    }

    /**
     * Format $_FILES
     *
     */
    protected function _format()
    {
        foreach ($_FILES as $field => $file) {

            if (empty($file['name'])) {
                continue;
            }

            if (is_array($file['name'])) {
                $cnt = count($file['name']);

                for ($i = 0; $i < $cnt; $i++) {
                    if (empty($file['name'][$i])) {
                        continue;
                    }
                    $this->_files[] = array(
                        'field' => $field,
                        'name' => $file['name'][$i],
                        'type' => $file['type'][$i],
                        'tmp_name' => $file['tmp_name'][$i],
                        'error' => $file['error'][$i],
                        'size' => $file['size'][$i],
                        'ext' => $this->getExt($file['name'][$i], TRUE)
                    );
                }
            } else {
                $this->_files[] = $file + array('field' => $field, 'ext' => $this->getExt($file['name'], TRUE));
            }
        }
    }

    /**
     * Instance MogileFs
     * @return Cola_Com_Mogilefs
     */
    public function getMogilefsInstance()
    {

        if (NULL === $this->_mogilefs) {
            extract($this->config('mogilefs'));
            $this->_mogilefs = new Cola_Com_Mogilefs($domain, $class, $trackers);
        }

        return $this->_mogilefs;
    }

    /**
     * Save uploaded files
     *
     * @param array $file
     * @param string $name
     * @return boolean
     */
    public function save($file = NULL, $name = NULL)
    {
        if (NULL !== $file) {
            return $this->_move($file, $name);
        }

        $return = TRUE;

        foreach ($this->_files as $file) {
            $return = $return && $this->_move($file);
        }

        return $return;
    }

    /**
     * Move file
     *
     * @param array $file
     * @param string $name
     * @return boolean
     */
    protected function _move($file, $name = NULL)
    {
        if (!$this->check($file)) {
            return false;
        }

        if (NULL === $name) {
            $name = $file['name'];
        }
        $fileFullName = $this->_config['savePath'] . DIRECTORY_SEPARATOR . $name;

        if (file_exists($fileFullName) && !$this->_config['override']) {
            $msg = 'file_already_exits:' . $fileFullName;
            $this->_error[] = $msg;
            return false;
        }

        $dir = dirname($fileFullName);
        is_dir($dir) || Cola_Com_Fs::mkdir($dir);

        if (is_writable($dir) && move_uploaded_file($file['tmp_name'], $fileFullName)) {
            $this->_num++;
            return TRUE;
        }

        $this->_error[] = 'move_uploaded_file_failed:' . $dir . 'may not be writeable.';
        return false;
    }

    /**
     * Save uploaded files to mogilefs
     *
     * @param array $file
     * @param string $name
     * @return boolean
     */
    public function saveToMogilefs($file = NULL, $name = NULL)
    {
        if (NULL !== $file) {
            return $this->_moveToMogilefs($file, $name);
        }

        $return = TRUE;

        foreach ($this->_files as $file) {
            $return = $return && $this->_moveToMogilefs($file);
        }

        return $return;
    }

    /**
     * upload file to mogilefs
     *
     * @param array $file
     * @param string $name
     * @return boolean
     */
    protected function _moveToMogilefs($file, $name = NULL)
    {
        if (!$this->check($file)) {
            return false;
        }

        if (NULL === $name) {
            $name = $file['name'];
        }

        //create mogilefs object
        $this->getMogilefsInstance();

        if ($this->_mogilefs->exists($name) && !$this->_config['override']) {
            $msg = 'file_already_exits:' . $name;
            $this->_error[] = $msg;
            return false;
        }

        if ($this->_mogilefs->setFile($name, $file['tmp_name'])) {
            $this->_num++;
            return TRUE;
        }

        $this->_error[] = 'move_uploaded_file_failed:' . $name . 'may not be writeable.';
        return false;
    }

    /**
     * Check file
     *
     * @param array $file
     * @return string
     */
    public function check($file)
    {
        if (UPLOAD_ERR_OK != $file['error']) {
            $this->_error[] = $this->_message[$file['error']] . ':' . $file['name'];
            return false;
        }

        if (!is_uploaded_file($file['tmp_name'])) {
            $this->_error[] = 'file_upload_failed:' . $file['name'];
            return false;
        }

        if (!$this->checkType($file, $this->_config['allowedTypes'])) {
            $this->_error[] = 'file_type_not_allowed:' . $file['name'];
            return false;
        }

        if (!$this->checkExt($file, $this->_config['allowedExts'])) {
            $this->_error[] = 'file_ext_not_allowed:' . $file['name'];
            return false;
        }

        if (!$this->checkFileSize($file, $this->_config['maxSize'])) {
            $this->_error[] = 'file_size_not_allowed:' . $file['name'];
            return false;
        }

        if ($this->isImage($file) && !$this->checkImageSize($file, array($this->_config['maxWidth'], $this->_config['maxHeight']))) {
            $this->_error[] = 'image_size_not_allowed:' . $file['name'];
            return false;
        }

        return TRUE;
    }

    /**
     * Get image size
     *
     * @param string $name
     * @return array like array(x, y),x is width, y is height
     */
    public function getImageSize($name)
    {
        if (function_exists('getimagesize')) {
            $size = getimagesize($name);
            return array($size[0], $size[1]);
        }

        return false;
    }

    /**
     * Get file extension
     *
     * @param string $fileName
     * @return string
     */
    public function getExt($name, $withdot = false)
    {
        $pathinfo = pathinfo($name);
        if (isset($pathinfo['extension'])) {
            return ($withdot ? '.' : '' ) . $pathinfo['extension'];
        }
        return '';
    }

    /**
     * Check if is image
     *
     * @param string $type
     * @param string $imageTypes
     * @return boolean
     */
    public function isImage($file)
    {
        return 'image' == substr($file['type'], 0, 5);
    }

    /**
     * Check file type
     *
     * @param string $file
     * @param string $allowedTypes
     * @return boolean
     */
    public function checkType($file, $allowedTypes)
    {
        return ('*' == $allowedTypes || false !== stripos($allowedTypes, $file['type'])) ? TRUE : false;
    }

    /**
     * Check file ext
     *
     * @param string $file
     * @param string $allowedExts
     * @return boolean
     */
    public function checkExt($file, $allowedExts)
    {
        return ('*' == $allowedExts || false !== stripos($allowedExts, $this->getExt($file['name']))) ? TRUE : false;
    }

    /**
     * Check file size
     *
     * @param int $file
     * @param int $maxSize
     * @return boolean
     */
    public function checkFileSize($file, $maxSize)
    {
        return 0 === $maxSize || $file['size'] <= $maxSize;
    }

    /**
     * Check image size
     *
     * @param array $file
     * @param array $maxSize
     * @return unknown
     */
    public function checkImageSize($file, $maxSize)
    {
        $size = $this->getImageSize($file['tmp_name']);
        return (0 === $maxSize[0] || $size[0] <= $maxSize[0]) && (0 === $maxSize[1] || $size[1] <= $maxSize[1]);
    }

    /**
     * Get formated files
     *
     * @return array
     */
    public function files()
    {
        return $this->_files;
    }

    /**
     * Get the num of sucessfully uploaded files
     *
     * @return int
     */
    public function num()
    {
        return $this->_num;
    }

    /**
     * Get upload error
     *
     * @return array
     */
    public function error()
    {
        return $this->_error;
    }

    /**
     * 获取图片信息
     * @param type $img
     * @return mixed FALSE Or array
     */
    public function getImageInfo($img)
    {
        $imageInfo = getimagesize($img);
        if ($imageInfo) {
            return array(
                'width' => $imageInfo[0],
                'height' => $imageInfo[1],
                'type' => strtolower(substr(image_type_to_extension($imageInfo[2]), 1)),
                'size' => filesize($img),
                'mime' => $imageInfo['mime']
            );
        }
        return FALSE;
    }

    /**
     * 生成缩略图，针对正方形
     *
     * @param string $image 文件路径
     * @param string $thumbName 缩略图名称
     * @param int $width 生成缩略图宽度大小
     * @return string 生成缩略图文件路径
     */
    public function thumbA($image, $thumbName, $width = 200)
    {
        $info = $this->getImageInfo($image);
        $type = $info['type'];
        $createFun = 'ImageCreateFrom' . ($type == 'jpg' ? 'jpeg' : $type);
        $srcImg = $createFun($image);
        $srcWidth = $info['width'];
        $srcHeight = $info['height'];
        $min = min($srcWidth, $srcHeight);
        $width = ($width > $min) ? $min : $width;
        if ($srcWidth > $srcHeight) {
            //图片宽大于高
            $sx = abs(($srcHeight - $srcWidth) / 2);
            $sy = 0;
            $thumbw = $srcHeight;
            $thumbh = $srcHeight;
        } else {
            //图片高大于等于宽
            //$sy = abs(($srcWidth - $srcHeight) / 2);
            $sy = 0;
            $sx = 0;
            $thumbw = $srcWidth;
            $thumbh = $srcWidth;
        }
        if (function_exists("imagecreateTRUEcolor")) {
            // 创建目标图gd2
            $thumbImg = imagecreateTRUEcolor($width, $width);
        } else {
            // 创建目标图gd1
            $thumbImg = imagecreate($width, $width);
        }
        if (function_exists("ImageCopyResampled")) {
            imagecopyresampled($thumbImg, $srcImg, 0, 0, $sx, $sy, $width, $width, $thumbw, $thumbh);
        } else {
            imagecopyresized($thumbImg, $srcImg, 0, 0, $sx, $sy, $width, $width, $thumbw, $thumbh);
        }
        if ('gif' == $type || 'png' == $type) {
            // 指派一个绿色
            $background_color = imagecolorallocate($thumbImg, 0, 255, 0);
            // 设置为透明色，若注释掉该行则输出绿色的图
            imagecolortransparent($thumbImg, $background_color);
        }
        // 对jpeg图形设置隔行扫描
        if ('jpg' == $type || 'jpeg' == $type) {
            imageinterlace($thumbImg, 1);
        }
        // 生成图片
        $imageFun = 'image' . ($type == 'jpg' ? 'jpeg' : $type);
        $imageFun($thumbImg, $thumbName);
        imagedestroy($thumbImg);
        return $thumbName;
    }

    /**
     * 生成缩略图
     *
     * @param string $srcFile 图片文件路径
     * @param string $thumbFile 缩略图名称
     * @param string $type 类型
     * @param int $maxWidth 图片宽度
     * @param int $maxHeight 图片高度
     * @param bool $interlace
     * @return mixed FALSE Or string
     */
    public function thumb($srcFile, $thumbFile, $type = '', $maxWidth = 200, $maxHeight = 50, $interlace = TRUE)
    {
        // 获取原图信息
        $info = $this->getImageInfo($srcFile);
        if ($info) {
            $srcWidth = $info['width'];
            $srcHeight = $info['height'];
            $type = strtolower(empty($type) ? $info['type'] : $type);
            if ($maxWidth == $maxHeight) {
                return $this->thumbA($srcFile, $thumbFile, $maxWidth);
            }
            $interlace = $interlace ? 1 : 0;
            unset($info);
            // 计算缩放比例
            $scale = min($maxWidth / $srcWidth, $maxHeight / $srcHeight);
            if ($scale >= 1) {
                // 超过原图大小不再缩略
                $width = $srcWidth;
                $height = $srcHeight;
            } else {
                // 缩略图尺寸
                $width = (int) ($srcWidth * $scale);
                $height = (int) ($srcHeight * $scale);
            }
            // 载入原图
            $createFun = 'ImageCreateFrom' . ($type == 'jpg' ? 'jpeg' : $type);
            $srcImg = $createFun($srcFile);

            //创建缩略图
            if ($type != 'gif' && function_exists('imagecreateTRUEcolor')) {
                $thumbImg = imagecreateTRUEcolor($width, $height);
            } else {
                $thumbImg = imagecreate($width, $height);
            }
            // 复制图片
            if (function_exists("ImageCopyResampled")) {
                imagecopyresampled($thumbImg, $srcImg, 0, 0, 0, 0, $width, $height, $srcWidth, $srcHeight);
            } else {
                imagecopyresized($thumbImg, $srcImg, 0, 0, 0, 0, $width, $height, $srcWidth, $srcHeight);
            }
            if ('gif' == $type || 'png' == $type) {
                //imagealphablending($thumbImg, false);//取消默认的混色模式
                //imagesavealpha($thumbImg,TRUE);//设定保存完整的 alpha 通道信息
                // 指派一个绿色
                $background_color = imagecolorallocate($thumbImg, 0, 255, 0);
                // 设置为透明色，若注释掉该行则输出绿色的图
                imagecolortransparent($thumbImg, $background_color);
            }
            // 对jpeg图形设置隔行扫描
            if ('jpg' == $type || 'jpeg' == $type) {
                imageinterlace($thumbImg, 1);
            }
            //$gray=ImageColorAllocate($thumbImg,255,0,0);
            //ImageString($thumbImg,2,5,5,"ThinkPHP",$gray);
            // 生成图片
            $imageFun = 'image' . ($type == 'jpg' ? 'jpeg' : $type);
            $imageFun($thumbImg, $thumbFile);
            imagedestroy($thumbImg);
            imagedestroy($srcImg);
            return $thumbFile;
        }
        return false;
    }

}
