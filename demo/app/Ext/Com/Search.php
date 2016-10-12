<?php
namespace Ext\Com;
/**
 * Search
 */
require_once COLA_DIR . '/Com/Search/lib/XS.php';

class Search
{

    /**
     * $_config array
     *
     * @var string $_file xunSearch configuration ini file path
     * @var string charset DefaultCharset
     */
    private $_config;

    /**
     * XS object
     * @var type object
     */
    private $_search = NULL;
    private $_tokenizer = NULL;

    public function __construct($config = array())
    {
        $this->_config = $config + array('file' => 'dodoedu', 'charset' => 'UTF-8');

        $this->getInstance();
    }

    /**
     * instance search at XS
     * @return object
     */
    private function getInstance()
    {
        if (NULL === $this->_search) {
            $this->_search = new XS($this->_config['file']);
            $this->_search->setDefaultCharset($this->_config['charset']);
        }

        return $this;
    }

    /**
     * return tokenizer
     */
    public function getTokenizer()
    {
        if (NULL === $this->_tokenizer) {
            $this->_tokenizer = new XSTokenizerScws();
        }

        return $this->_tokenizer;
    }

    /**
     * return xun search object
     */
    public function getSearch()
    {
        return $this->_search;
    }

    /**
     * get search result by key
     * @param string $key search by key
     * @param string $type search type eg. blog, group, class, school
     * @return mixed
     */
    public function get($key, $type = NULL)
    {

        NULL !== $type && $key = 'typeStr:' . $type . ' ' . $key;

        return $this->_search->search->search($key);
    }

    /**
     *
     * @param string $key
     * @param string $sendType
     * @return mixed
     */
    public function getBySendType($key, $sendType = NULL)
    {

        NULL !== $sendType && $key = 'sendType:' . $sendType . ' ' . $key;

        return $this->_search->search->search($key);
    }

    /**
     * update to search db
     * @param string $id search id eg. 10001
     * @param string $type app name eg. site, blog, photo, guest, app
     * @param string $title
     * @param string $content
     * @param string $keyword keyword eg. keyword1, keyword2...
     * @param timestamp $time timestamp
     * @param string $sendType send type eg. user, group, class, school, app
     * @param string $sendId send object id
     * @return boolean TRUE
     * @throws Cola_\Exception
     */
    public function set($id, $type, $title, $content, $keyword = NULL, $time = NULL, $sendType = NULL, $sendId = NULL)
    {
        try {
            $data = array(
                'tid' => $type . '_' . $id,
                'typeStr' => $type,
                'sendType' => $sendType,
                'sendId' => $sendId,
                'keyStr' => $keyword,
                'postTitle' => strip_tags($title),
                'postContent' => strip_tags($content),
                'postTime' => (NULL === $time) ? time() : $time
            );

            $doc = new XSDocument;
            $doc->setFields($data);

            $this->_search->index->update($doc);
            return TRUE;
        } catch (\Exception $exc) {

            throw new Cola_\Exception($exc->getMessage(), $exc->getCode());
        }
    }

    /**
     * update to ShoolCms db
     * @param int $obj_id  对象id
     * @param string $obj_type 对象类形 默认 'school_cms'
     * @param string $obj_title 对象标题
     * @param string $obj_content 对象内容
     * @param string $obj_keywords 对象关键字
     * @param int $obj_column_id 对象的栏目id
     * @param string $obj_column_title 对象栏目标题
     * @param string $obj_school_id 对象所属学校
     * @param string $obj_user_id 对象的作者
     * @param int $on_time  上传时间
     * @throws \Exception
     * @return boolean
     */
    public function setShoolCms($obj_id, $obj_type = 'school_cms', $obj_title, $obj_content, $obj_keywords = null, $obj_column_id = null, $obj_column_title = null, $obj_school_id = null, $obj_user_id = null, $on_time = null)
    {
        try {
            $data = array(
                'obj_id' => $obj_type . '_' . $obj_id,
                'obj_type' => $obj_type,
                'obj_title' => $obj_title,
                'obj_content' => $obj_content,
                'obj_keywords' => $obj_keywords,
                'obj_column_id' => $obj_column_id,
                'obj_column_title' => $obj_column_title,
                'obj_school_id' => $obj_school_id,
                'obj_user_id' => $obj_user_id,
                'on_time' => $on_time ? $on_time : time(),
            );
            $doc = new XSDocument;
            $doc->setFields($data);

            $this->_search->index->update($doc);
            return true;
        } catch (\Exception $exc) {
            throw new \Exception($exc->getMessage(), $exc->getCode());
        }
    }

    /*
     * delete data for search
     * @param array $ids
     * @param string $type
     */

    public function delete($ids, $type = NULL)
    {
        try {
            if (NULL !== $type) {
                if (is_array($ids)) {
                    foreach ($ids as $key => $val) {
                        $ids[$key] = $type . '_' . $val;
                    }
                } else {
                    $ids = $type . '_' . $ids;
                }
                $index = $this->_search->index->del($ids);
            } else {
                $index = $this->_search->index->del($ids);
            }
            return TRUE;
        } catch (\Exception $exc) {

            throw new Cola_\Exception($exc->getMessage(), $exc->getCode());
        }
    }

}
