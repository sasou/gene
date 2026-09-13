<?php
namespace Gene;

/**
 * Service
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcache $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Gene\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  6.1.0
 */
 
class Application
{
    protected static $instance;

    /**
     * __construct
     * 
     * @param mixed $safe safe
     * @return mixed
     */
    public function __construct($safe = null) {

    }

    /**
     * getInstance
     *
     * 获取应用单例实例（由 C 扩展静态属性维护）。
     * 首次调用创建并注册，后续调用返回同一实例。
     *
     * @param mixed $safe 隔离命名空间 key（可选）
     * @return static|null
     */
    public static function getInstance($safe = null) {
        return null;
    }

    /**
     * load
     * 
     * @param string $file 配置文件名
     * @param string|null $path 文件所在目录（默认使用 app_root）
     * @param int $validity 文件变更检测缓存时间（秒，默认10）
     * @return static
     */
    public function load($file, $path = null, $validity = null) {

    }

    /**
     * autoload
     * 
     * @param string|null $app_root 应用根目录
     * @param callable|null $auto_function 自定义自动加载函数
     * @return static
     */
    public function autoload($app_root = null, $auto_function = null) {

    }

    /**
     * setMode
     * 
     * @param int|null $error_type 错误处理类型：0=内置HTML，1=自定义
     * @param int|null $exception_type 异常处理类型：0=内置HTML，1=自定义
     * @param callable|null $ex_callback 自定义异常处理回调
     * @param callable|null $error_callback 自定义错误处理回调
     * @return static
     */
    public function setMode($error_type = null, $exception_type = null, $ex_callback = null, $error_callback = null) {

    }

    /**
     * setView
     * 
     * @param mixed $view view
     * @param mixed $tpl_ext tpl_ext
     * @return mixed
     */
    public function setView($view = null, $tpl_ext = null) {

    }

    /**
     * error
     * 
     * @param mixed $type type
     * @param mixed $callback callback
     * @param mixed $error_type error_type
     * @return mixed
     */
    public function error($type, $callback, $error_type = null) {

    }

    /**
     * exception
     * 
     * @param mixed $type type
     * @param mixed $callback callback
     * @return mixed
     */
    public function exception($type, $callback = null) {

    }


    /**
     * webscan
     * 
     * @param int|null $webscan_switch
     * @param string|null $webscan_white_directory
     * @param callable|null $callback
     * @param array|null $webscan_white_url
     * @param int|null $webscan_get
     * @param int|null $webscan_post
     * @param int|null $webscan_cookie
     * @param int|null $webscan_referer
     * @return static
     */
    public function webscan(
        $webscan_switch = null,
        $webscan_white_directory = null,
        $callback = null,
        $webscan_white_url = null,
        $webscan_get = null,
        $webscan_post = null,
        $webscan_cookie = null,
        $webscan_referer = null
    ) {

    }
    /**
     * run
     * 
     * FPM模式：自动从 $_SERVER 读取 REQUEST_METHOD 和 REQUEST_URI
     * Swoole模式：自动从 Request::init() 初始化的 server 数据中读取，无需手动传参
     * 也可手动传入 method/uri 覆盖自动检测
     *
     * @param string|null $method HTTP 请求方法（可选，默认自动检测）
     * @param string|null $uri 请求路径（可选，默认自动检测）
     * @return static
     */
    public function run($method = null, $uri = null) {

    }

    /**
     * Enable the request-id policy, or disable it by passing null.
     * @param array|null $config header, bytes, trust and max_length options
     * @return static
     */
    public function requestId(?array $config = null) {

    }

    /**
     * waitWorkerReady
     *
     * Swoole/常驻模式下等待 workerStart 初始化结束（workerReady 标记）。
     * FPM 模式下无阻塞，直接返回。
     *
     * @return static|bool
     */
    public static function waitWorkerReady() {

    }

    /**
     * workerReady
     *
     * Swoole/常驻模式下标记 Worker 已就绪，冻结进程级 Memory，
     * 并根据需要预热请求上下文池。
     *
     * @return static|bool
     */
    public static function workerReady() {

    }

    /**
     * prewarmCtxPool
     *
     * Swoole/常驻模式下预热请求上下文池。
     * `$count = -1` 表示填充到 `gene.ctx_pool_max`，返回实际新增的上下文数。
     * FPM 模式下返回 0。
     *
     * @param int $count 预热的上下文数量，-1 表示填充到上限
     * @return int
     */
    public static function prewarmCtxPool($count = -1) {

    }

    /**
     * getMethod
     * 
     * @return mixed
     */
    public static function getMethod() {

    }

    /**
     * getPath
     *
     * 返回当前请求路径。$withoutLang=true 时去除语言前缀。
     *
     * @param bool $withoutLang 是否去除语言前缀
     * @return string|null
     */
    public static function getPath($withoutLang = false) {

    }

    /**
     * getRouterUri
     *
     * 返回当前路由 URI（:m/:c/:a 替换后，小写）。
     *
     * @return string|null
     */
    public static function getRouterUri() {

    }

    /**
     * url
     *
     * 返回带语言前缀的 URL。$lang 未传时使用当前请求语言；传空串则不加语言前缀。
     *
     * @param string $path 路径
     * @param string $lang 指定语言；未传时使用当前请求语言，传 "" 则返回不含语言前缀的 URL
     * @return string
     */
    public static function url($path, $lang = null) {

    }

    /**
     * getLang
     * 获取当前语言
     *
     * @return string|null
     */
    public static function getLang() {

    }

    /**
     * getModule
     * 
     * @return mixed
     */
    public static function getModule() {

    }

    /**
     * getController
     * 
     * @return mixed
     */
    public static function getController() {

    }

    /**
     * getAction
     * 
     * @return mixed
     */
    public static function getAction() {

    }

    /**
     * setEnvironment
     * 
     * @param mixed $type type(int): 0-dev, 1-test, 2-prod, 3-gray
     * @return mixed
     */
    public static function setEnvironment($type) {

    }

    /**
     * getEnvironment
     * 
     * @return mixed
     */
    public static function getEnvironment() {

    }

    /**
     * getEnvironmentName
     * 
     * @return string
     */
    public static function getEnvironmentName() {

    }

    /**
     * setRuntimeType
     * 
     * @param mixed $type type(int|string): 1/fpm, 2/swoole, 3/coroutine
     * @return mixed
     */
    public static function setRuntimeType($type) {

    }

    /**
     * getRuntimeType
     * 
     * @return mixed
     */
    public static function getRuntimeType() {

    }

    /**
     * getRuntimeTypeName
     * 
     * @return string
     */
    public static function getRuntimeTypeName() {

    }

    /**
     * cleanup
     * 
     * Swoole模式下的合并清理方法，等价于依次调用 clearState() + destroyContext()。
     * Phase 1: 重置视图变量，软重置请求上下文（释放用户数据，触发对象析构）。
     * Phase 2: 从协程上下文哈希表中移除已清空的上下文结构体。
     * FPM模式下：行为与 clearState() 相同。
     *
     * @param bool $gc 是否执行垃圾回收
     * @return bool
     */
    public static function cleanup($gc = false) {

    }

    /**
     * clearState
     * 
     * 软重置当前请求上下文（释放用户数据但保留上下文结构体）。
     * Swoole模式下建议使用 cleanup() 代替。
     *
     * @return static|bool
     */
    public static function clearState() {

    }

    /**
     * destroyContext
     * 
     * 销毁当前协程的请求上下文结构体。
     * Swoole模式下建议使用 cleanup() 代替。
     *
     * @return bool
     */
    public static function destroyContext() {

    }

    /**
     * bootstrap
     *
     * FPM/Swoole 共享的应用装载收口，等价于：
     *   autoload($appRoot)
     *   ->load($options['router'], $confDir)    // 可含 {env} 占位符
     *   ->load($options['config'], $confDir)   // {env} 展开为 getEnvironmentName()
     *   ->setMode($options['mode'] ?? 1, $debug, $options['ex_callback'], $options['error_callback'])
     *
     * $debug = options['debug'] ?? (getEnvironmentName() ∈ options['debug_envs'])。
     * 注意 debug 即 setMode 的 exception_type：为 0 时不注册异常处理器（未捕获
     * 异常直接 fatal）。旧式 setMode(1,1) 恒开语义等价于 'debug' => 1 或列出
     * 全部非 prod 环境：['dev', 'test', 'gray']。
     * ex_callback/error_callback 分别透传为 setMode 的第 3/4 参（如
     * 'doException' 字符串或闭包）；非 callable 抛 ValueError。
     * 不承载 request-id/webscan/时区/池名等业务策略，这些由应用显式配置。
     *
     * @param string $appRoot 应用目录（autoload 目标）
     * @param string $confDir 配置目录
     * @param array|null $options ['router' => string|null, 'config' => string|null, 'mode' => int(默认1), 'debug_envs' => array(内置 env：dev/test/gray/prod), 'debug' => int|bool(优先于 debug_envs), 'ex_callback' => callable|null, 'error_callback' => callable|null]
     * @return static
     */
    public function bootstrap($appRoot, $confDir, $options = []) {

    }

    /**
     * pools
     *
     * 登记连接池声明（workerStart 中与 startPools 配套使用）：
     *   ['dbPool'    => ['driver' => 'db',    'component' => 'db',    'params' => [...]],
     *    'redisPool' => ['driver' => 'redis', 'component' => 'redis']]
     *
     * driver 仅接受 'db'（Gene\Pool）或 'redis'（Gene\Cache\RedisPool）；
     * component 为 config 键名；params 为可选池参数（min/max/idleTimeout/
     * waitTimeout）。已启动的池不可重声明（ValueError）。
     *
     * @param array $decls 池名 => 声明数组
     * @return static
     */
    public static function pools($decls) {

    }

    /**
     * startPools
     *
     * 按 pools() 声明创建全部未启动的池（workerStart 中调用）。
     * FPM（runtime_type < 2）下明确拒绝返回 false；重复调用幂等；
     * 某池创建失败时异常向外传播且该声明保持未启动（可重试）。
     *
     * @return bool
     */
    public static function startPools() {

    }

    /**
     * stopPoolTimers
     *
     * workerExit 用：仅为已通过 startPools 启动的驱动族调用 stopTimers()，
     * 不影响未声明/手动创建的池。幂等。
     *
     * @return bool
     */
    public static function stopPoolTimers() {

    }

    /**
     * closePools
     *
     * workerStop 用：为已启动的驱动族调用 closeAll()，随后复位 started
     * 标记使进程可安全重入。幂等。
     *
     * @return bool
     */
    public static function closePools() {

    }

    /**
     * handleSwoole
     *
     * 一站式 Swoole onRequest 生命周期适配器，封装：
     *   waitWorkerReady → Request::initSwoole → setResponse → 输出缓冲
     *   → run() → Throwable 边界 → 收敛本层输出 → 响应可写时 end(output)
     *   → cleanup()
     *
     * 选项：
     *   'cleanup_gc' => bool  请求结束后是否执行 GC（默认 false）
     *   'catch'      => callable|null  形如 function (\Throwable $e, object $request, object $response): void
     *
     * 未配置 catch 时框架记录 Gene\Log::exception()，并在响应仍可写时补
     * status(500) + end(已缓冲输出)。不设置默认 Content-Type，不覆盖业务
     * status/header；dispatch 内已 end/json/sendFile/redirect 的请求不会
     * 被二次 end()。catch 再抛出的异常向外传播，cleanup 照常执行。
     *
     * @param object $request Swoole\Http\Request 或 duck-type 兼容对象
     * @param object $response Swoole\Http\Response 或 duck-type 兼容对象
     * @param array|null $options ['cleanup_gc' => bool, 'catch' => callable|null]
     * @return bool
     */
    public function handleSwoole($request, $response, $options = []) {

    }

    /**
     * setResponse
     * 
     * 设置当前请求的 Swoole Response 对象到请求上下文中。
     *
     * @param mixed $response Swoole\Http\Response 对象
     * @return bool
     */
    public static function setResponse($response) {

    }

    /**
     * config
     * 
     * @param mixed $key key
     * @return mixed
     */
    public static function config($key) {

    }

    /**
     * params
     * 
     * @param string|null $name 路径参数名，不传则返回全部参数数组 
     * @return mixed
     */
    public static function params($name = null) {

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
     * stop
     * Signal the router to stop dispatching the current request. After a
     * before-hook or controller action calls stop(), the router skips the
     * remaining action and after-hooks. The flag is per-request.
     *
     * @return $this
     */
    public function stop() {

    }

    /**
     * isStopped
     * Returns true if stop() has been called during the current request.
     *
     * @return bool
     */
    public static function isStopped() {

    }

}
