#ifndef SERVER_H
#define SERVER_H

#include <event.h>
#include <event2/listener.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "database.h"
#include <list>
#include <time.h>
#include <json/json.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "player.h"

//#define IP      "172.24.6.180" // 原服务器
//#define IP      "127.0.0.1"    //本地
//#define IP      "121.41.167.17" // 新服务器（原有代码，已注释保留）

#define IP      "0.0.0.0"       //监听所有网卡地址，避免固定IP导致bind失败
#define PORT    8000

#define SEQUENCE      1
#define CIRCLE        2

class Server
{
private:
	struct event_base *m_base;        //事件集合
	DataBase *m_database;             //数据库对象
	Player *m_p;
public:
	Server();
	~Server();
	void listen(const char *ip, int port);
	struct event_base *server_get_base();
	void server_read_data(struct bufferevent *, char *);
	void server_get_music(struct bufferevent *, std::string);
	void server_send_data(struct bufferevent *, Json::Value &);
	void server_player_handler(struct bufferevent *, Json::Value &);
	void server_start_timer();
	void server_app_register(struct bufferevent *, Json::Value &);
	void server_app_login(struct bufferevent *, Json::Value &);
	void server_app_bind(struct bufferevent *, Json::Value &);
	void server_client_offline(struct bufferevent *);
	void server_app_offline(struct bufferevent *);
	
	static void listener_cb(struct evconnlistener *, evutil_socket_t , struct sockaddr *, int socklen, void *);
	static void read_cb(struct bufferevent *, void *);
	static void event_cb(struct bufferevent *, short, void *);
	static void timeout_cb(evutil_socket_t, short, void *);
};


#endif
