#include "database.h"

//构造函数
DataBase::DataBase()
{

}

//析构函数
DataBase::~DataBase()
{

}

//连接数据库
//输入参数：无
//输出参数：是否成功
//返回值：true表示连接成功，false表示连接失败
bool DataBase::database_connect()
{
	//初始化数据库句柄
	mysql = mysql_init(NULL);

	//向数据库发起连接
	//输入：数据库句柄、数据库地址、数据库用户名、数据库密码、数据库名称、数据库端口、数据库连接标志、数据库连接标志
	//输出：数据库句柄
	mysql = mysql_real_connect(mysql, "localhost", "root", "root", 
			"musicplayer", 0, NULL, 0);
	if (mysql == NULL)
	{
		std::cout << "[DATABASE CONNECT FAILURE] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	/*if (mysql_query(mysql, "set names utf8;") != 0)
	{
		//设置数据库编码为utf8，解决中文乱码问题，但是目前没有中文，所以注释掉
	}*/

	return true;
}

//断开数据库
//输入参数：无
//输出参数：void
//返回值：void
void DataBase::database_disconnect()
{
	mysql_close(mysql);//关闭数据库连接
}
//初始化数据库表
//输入参数：无
//输出参数：是否成功
//返回值：true表示初始化成功，false表示初始化失败
//数据表
// 第一列appid char(11) 用户ID
// 第二列password varchar(16) 密码
// 第三列deviceid varchar(8) 设备ID
bool DataBase::database_init_table()
{
	if (!this->database_connect())//连接数据库
		return false;
	//创建数据库表的SQL语句
	const char *sql = "create table if not exists account(appid char(11),password varchar(16), deviceid varchar(8))charset utf8;";
	//创建数据库表
	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	this->database_disconnect();

	return true;
}

//判断用户是否存在
//输入参数：appid是APPID
//输出参数：是否成功
//返回值：true表示用户存在，false表示用户不存在
bool DataBase::database_user_exist(std::string appid)
{
	char sql[256] = {0};
	//查询用户是否存在的SQL语句
	sprintf(sql, "select * from account where appid = '%s';", appid.c_str());
	//输入参数：mysql是数据库句柄，sql是SQL语句
	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return true;
	}

	MYSQL_RES *res = mysql_store_result(mysql);//获取结果集
	if (NULL == res)
	{
		std::cout << "[MYSQL STORE RESULT ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return true;
	}

	MYSQL_ROW row = mysql_fetch_row(res);//获取结果集
	if (row == NULL)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//添加用户
//输入参数：a是APPID，p是密码
//输出参数：void
//返回值：void
void DataBase::database_add_user(std::string a, std::string p)
{
	char sql[128] = {0};
	//添加用户到数据库的SQL语句
	sprintf(sql, "insert into account (appid, password) values ('%s', '%s');", a.c_str(), p.c_str());

	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
	}
}

//判断密码是否正确
//输入参数：a是APPID，p是密码
//输出参数：是否成功
//返回值：true表示密码正确，false表示密码不正确
bool DataBase::database_password_correct(std::string a, std::string p)
{
	char sql[256] = {0};
	//查询密码是否正确的SQL语句
	sprintf(sql, "select password from account where appid = '%s';", a.c_str());

	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		std::cout << "[MYSQL STORE RESULT ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);//获取结果集
	if (NULL == row)
	{
		std::cout << "[MYSQL FETCH ROW ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	if (!strcmp(p.c_str(), row[0]))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//判断用户是否绑定
//输入参数：a是APPID，d是设备ID
//输出参数：是否成功
//返回值：true表示用户绑定，false表示用户未绑定
//数据表
// 第一列appid char(11) 用户ID
// 第二列password varchar(16) 密码
// 第三列deviceid varchar(8) 设备ID
bool DataBase::database_user_bind(std::string a, std::string &d)
{
	char sql[256] = {0};
	//查询用户是否绑定的SQL语句
	// 用appid查询设备ID
	sprintf(sql, "select deviceid from account where appid = '%s';", a.c_str());

	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_RES *res = mysql_store_result(mysql);//获取结果集
	if (NULL == res)
	{
		std::cout << "[MYSQL STORE RESULT ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);//获取结果集
	if (NULL == row)// 对应appid的行不存在
	{
		std::cout << "[MYSQL FETCH ROW ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	if (NULL == row[0])// 对应appid的行的第一列不存在
	{
		return false;
	}
	else
	{
		d = std::string(row[0]);//
		return true;
	}
}

//绑定用户
//输入参数：a是APPID，d是设备ID
//输出参数：void
//返回值：void
void DataBase::database_bind_user(std::string a, std::string d)
{
	char sql[256] = {0};
	//更新用户绑定的SQL语句
	sprintf(sql, "update account set deviceid = '%s' where appid = '%s';", d.c_str(), a.c_str());

	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
	}
}


/* 这里是puzzleway的代码 */
//判断设备是否被绑定
//输入参数：d是设备ID
//输出参数：是否成功
//返回值：true表示设备被绑定，false表示设备未被绑定
//数据表
// 第一列appid char(11) 用户ID
// 第二列password varchar(16) 密码
// 第三列deviceid varchar(8) 设备ID
bool DataBase::database_device_bind(std::string d)
{
	char sql[256] = {0};
	//查询设备是否被绑定的SQL语句
	sprintf(sql, "select appid from account where deviceid = '%s';", d.c_str());
	if (mysql_query(mysql, sql) != 0)//执行SQL语句
	{
		std::cout << "[MYSQL QUERY ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_RES *res = mysql_store_result(mysql);//获取结果集
	if (NULL == res)
	{
		std::cout << "[MYSQL STORE RESULT ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);//获取结果集
	if (NULL == row)// 对应设备ID的行不存在
	{
		std::cout << "[MYSQL FETCH ROW ERROR] ";
		std::cout << mysql_error(mysql) << std::endl;
		return false;
	}

	if (NULL == row[0])// 对应设备ID的行的第一列不存在，即设备未被绑定
	{
		return false;//设备未被绑定
	}
	else// 对应设备ID的行的第一列存在，即设备被绑定
	{
		return true;//设备被绑定，返回true
	}
}