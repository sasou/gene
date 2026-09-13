<?php
/**
 * demo 本地化模式的 sqlite 初始化脚本（幂等，可重复执行）。
 *
 * 用途：GENE_DEMO_LOCAL=1 时 config.ini.php 的 db 组件指向本文件生成的
 * demo/database/gene_demo.db，使 doc 页面（Models\Doc\Mark → app_mark 表）
 * 无需外部 MySQL 即可完成「路由 + ORM + 视图」全链路。
 *
 * 用法：php demo/database/init_sqlite.php [db文件路径]
 * 依赖：pdo_sqlite（无需加载 gene 扩展）。
 */

$dbFile = $argv[1] ?? (__DIR__ . '/gene_demo.db');

$pdo = new PDO('sqlite:' . $dbFile);
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec(<<<'SQL'
CREATE TABLE IF NOT EXISTS app_mark (
    mark_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    mark_type       INTEGER NOT NULL DEFAULT 0,
    mark_title      TEXT    NOT NULL DEFAULT '',
    app_description TEXT    NOT NULL,
    user_id         INTEGER NOT NULL DEFAULT 0,
    sort            INTEGER NOT NULL DEFAULT 0,
    status          INTEGER NOT NULL DEFAULT 0,
    addtime         INTEGER NOT NULL DEFAULT 0,
    updatetime      INTEGER NOT NULL DEFAULT 0
)
SQL);

$count = (int)$pdo->query('SELECT COUNT(*) FROM app_mark')->fetchColumn();
if ($count > 0) {
    echo "app_mark already seeded ({$count} rows): {$dbFile}\n";
    exit(0);
}

$now = time();
$rows = [
    // mark_id, mark_type, mark_title, app_description, sort
    [1, 1, '概述与入门', "# 概述与入门\n\nGene 是一个灵活、强大、简单、高效的 C 扩展框架。\n\n* 入口文件引导 autoload → router → config → run\n* 目录结构：application / public / config\n", 1],
    [2, 1, 'MVC分层开发指南', "# MVC分层开发指南\n\nController / Service / Model 三层约定：\n\n* Controller 只做路由收口与视图渲染\n* Service 承载业务逻辑\n* Model 只做数据访问\n", 2],
    [3, 1, '自动加载', "# 自动加载\n\n`autoload(APP_ROOT)` 注册类名到 `application/` 目录的映射加载。\n", 3],
    [4, 1, '依赖注入(IOC)', "# 依赖注入(IOC)\n\n`config.ini.php` 中 `\$config->set()` 声明组件，Di 惰性实例化并注入。\n", 4],
    [5, 1, 'Swoole常驻模式', "# Swoole常驻模式\n\n`public/swoole.php` 入口：workerStart 内 bootstrap + pools + workerReady，请求走 `handleSwoole` 收口。\n", 5],
    [6, 2, '应用类 Gene\\Application', "# 应用类 Gene\\Application\n\n`getInstance()` 单例；`bootstrap()` 共享装载；`handleSwoole()` Swoole 入口收口。\n", 1],
    [7, 2, '路由类 Gene\\Router', "# 路由类 Gene\\Router\n\n支持 get/post/put/delete 等方法、`:param` 占位、分组、事件钩子与错误页注册。\n", 2],
    [8, 2, '配置类 Gene\\Config', "# 配置类 Gene\\Config\n\n`set/get` 组件定义；`instance` 控制共享单例或按请求/协程隔离。\n", 3],
    [9, 2, 'Model类 Gene\\Model', "# Model类 Gene\\Model\n\n继承后通过 `\$this->db` 访问注入的数据库组件，支持链式查询。\n", 4],
    [10, 2, 'ORM类 Gene\\Orm\\Model', "# ORM类 Gene\\Orm\\Model\n\n`fill/find/save/create` 等主动记录语义；自然主键用 `fill(\$data, false)`。\n", 5],
];

$stmt = $pdo->prepare(
    'INSERT INTO app_mark (mark_id, mark_type, mark_title, app_description, user_id, sort, status, addtime, updatetime)
     VALUES (?, ?, ?, ?, 0, ?, 1, ?, ?)'
);
foreach ($rows as $r) {
    $stmt->execute([$r[0], $r[1], $r[2], $r[3], $r[4], $now, $now]);
}

echo "seeded " . count($rows) . " rows into app_mark: {$dbFile}\n";
