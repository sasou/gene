-- Migration: 2026-09-23 sys_user.user_pass → varchar(255)
--
-- 背景：6.2.5 起 demo 登录新口令改用 password_hash()（输出最长 255 字节）。
-- 早期 gene_demo.sql 里 user_pass 为 varchar(50)，写入 bcrypt/argon 摘要会
-- 被截断，导致新口令永远验证失败。对【已存在】的 demo 库执行本文件；
-- 全新导入直接用 gene_demo.sql 即可（已含 varchar(255)）。
--
-- 兼容说明：
--  * 旧 md5 摘要（50 字符）不受影响——Services\Admin\User::verifyPassword
--    对非 '$' 开头的存储值仍走 legacy 比对；
--  * 旧摘要登录成功一次后自动升级为 password_hash（见 checkUser），
--    无需批量重置。
--
-- 用法：mysql -u root -p gene_demo < migration_2026_09_23_user_pass.sql

ALTER TABLE `sys_user`
    MODIFY COLUMN `user_pass` varchar(255) NOT NULL DEFAULT '' COMMENT '用户密码';
