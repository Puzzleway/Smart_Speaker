一些数据库操作

首先在服务器上安装部署mysql
sudo apt install mysql-server mysql-client libmysqlclient-dev

进入mysql
mysql -u root -p

创建数据库musicplayer
create database musicplayer;
create database if not exists musicplayer;

删除数据库
drop database musicplayer

进入创建的数据库
use musicplayer;

创建账号绑定表
create table if not exists account(
    appid char(11),
    password varchar(16),
    deviceid varchar(8)
    )charset utf8;

在 mysql> 提示符下退出，用任意一个都可以：
exit;
quit;
\q