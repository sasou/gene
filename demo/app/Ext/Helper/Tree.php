<?php
namespace Ext\Helper;
/*********************************************************************************
 * sasou   扩展类库-无限极分类
***********************************************************************************/
class Tree{
	
	private $parentid = 'parentid';
	private $id       = 'id';
	private $name     = 'name';
	
    protected static $_instance = array();
    
    /**
     * init (add by sasou)
     * 
     * @return self
     */
    public static function init()
    {
        $className = get_called_class();

        if (isset(self::$_instance[$className]) && is_object(self::$_instance[$className])) {
            return self::$_instance[$className];
        }
        return self::$_instance[$className] = new static();
    }
    
	/**
	 * 无限级分类树-初始化配置
	 * @param  array $config array('parentid'=>'', 'id' => '', 'name' =>'name')
	 * @return string|array
	 */
	public function config($config = array()) {
		if (!is_array($config)) return false;
		$this->parentid = (isset($config['parentid'])) ? $config['parentid'] : $this->parentid;
		$this->id = (isset($config['id'])) ? $config['id'] : $this->id;
		$this->name = (isset($config['name'])) ? $config['name'] : $this->name;
		return true;
	}
	
	/**
	 * 无限级分类树-获取树
	 * @param  array $tree 树的数组
	 * @param  int   $mid  初始化树时候，代表ID下的所有子集
	 * @param  int   $selectid  选中的ID值
	 * @param  string  $code  代码
	 * @param  string  $prefix  前缀
	 * @param  string  $selected  选中
	 * @return string|array
	 */
	public function get_tree($tree, $mid = 0, $selectid = 5, $code = "<option value='\$id' \$selecteds>\$prefix\$name</option>", $prefix = '|-', $selected = 'selected') {
		if (!is_array($tree)) return '';
		$temp = array();
		$string = '';
		foreach ($tree as $k => $v) {
			if ($v[$this->parentid] == $mid) {
				$id = $v[$this->id];
				$name = $v[$this->name];
				$selecteds = ($id == $selectid) ? $selected : '';
				eval("\$temp_code = \"$code\";");//转化
				$string .=  $temp_code;
				$string .=  $this->get_tree($tree, $v[$this->id], $selectid, $code, '&nbsp;&nbsp;' . $prefix);
			}
		}
		return $string ;
	}
	
	/**
	 * 无限级分类树-获取树
	 * @param  array $tree 树的数组
	 * @param  int   $mid  初始化树时候，代表ID下的所有子集
	 * @param  int   $selectid  选中的ID值
	 * @param  string  $code  代码
	 * @param  string  $prefix  前缀
	 * @param  string  $selected  选中
	 * @return string|array
	 */
	public function get_trees($tree,&$data, $mid = 0,$deep = 0,$prefix = '|-') {
		if (!is_array($tree)) return '';
		$deep++;
		foreach ($tree as $k => $v) {
			if ($v[$this->parentid] == $mid) {
				$id = $v[$this->id];
				$name = $v[$this->name];
				$v["deep"] = $deep-1;
				$v["prefix"] = $prefix;
				$data[] = $v;
				$this->get_trees($tree,$data,$v[$this->id],$deep,str_repeat("&nbsp;", $deep * 4).$prefix);
			}
		}
	}
	
	/**
	 * 无限级分类树-获取子类
	 * @param  array $tree 树的数组
	 * @param  int   $id   父类ID
	 * @return string|array
	 */
	public function get_child($tree, $id) {
		if (!is_array($tree)) return array();
		$temp = array();
		foreach ($tree as $k => $v) {
			if ($v[$this->parentid] == $id) {
				$temp[] = $v;
			}
		}
		return $temp;
	}
	
	/**
	 * 无限级分类树-获取父类
	 * @param  array $tree 树的数组
	 * @param  int   $id   子类ID
	 * @return string|array
	 */
	public function get_parent($tree, $id) {
		if (!is_array($tree)) return array();
		$temp = array();
		foreach ($tree as $k => $v) {
			$temp[$v[$this->id]] = $v;
		}
		$parentid = $temp[$id][$this->parentid];
		return $temp[$parentid];
	}
}
