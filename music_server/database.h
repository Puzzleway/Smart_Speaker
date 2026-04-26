#ifndef DATABASE_H
#define DATABASE_H

#include <iostream>
#include <mysql/mysql.h>
#include <string.h>

class DataBase
{
private:
	MYSQL *mysql;             //数据库句柄
public:
	DataBase();
	~DataBase();
	bool database_connect();
	void database_disconnect();
	bool database_init_table();
	bool database_user_exist(std::string);
	void database_add_user(std::string, std::string);
	bool database_password_correct(std::string, std::string);
	bool database_user_bind(std::string, std::string &);
	void database_bind_user(std::string, std::string);
	bool database_device_bind(std::string);
};

#endif
