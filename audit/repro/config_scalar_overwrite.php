<?php
// 验证 memory.c 新增的“数组目录不被标量叶子覆盖”守卫对 Config::set 的副作用
$c = new \Gene\Config();
$c->set('svc.db.host', '127.0.0.1');
var_dump($c->get('svc.db.host'));

// 现在把 svc.db 整个改成一个标量（合法的用户意图）
$c->set('svc.db', 'disabled');
var_dump($c->get('svc.db'));   // 期望 'disabled'，守卫生效则仍是数组
