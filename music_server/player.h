#ifndef PLAYER_H
#define PLAYER_H

#include <list>
#include <time.h>
#include <iostream>
#include <event.h>
#include <json/json.h>
#include <string.h>

class Server;

struct PlayerInfo
{// 一个对象实例代表一个音响
	std::string deviceid;
	std::string appid;
	std::string cur_music;
	int volume;
	int mode;
	time_t d_time;                    //记录音箱上报的时间
	time_t a_time;                    //记录APP上报的时间

	struct bufferevent *d_bev;        //对应音箱事件
	struct bufferevent *a_bev;        //对应APP事件
};

class Player
{
private:
	std::list<PlayerInfo> *info;
public:
	Player();
	~Player();

	void player_update_list(struct bufferevent *,Json::Value &,Server *);
	void player_app_update_list(struct bufferevent *, Json::Value &);
	void player_traverse_list();
	void debug(const char *);//打印日志
	void player_upload_music(Server *, struct bufferevent *, Json::Value &);
	void player_option(Server *, struct bufferevent *, Json::Value &);
	void player_reply_option(Server *, struct bufferevent *, Json::Value&);
	void player_offline(struct bufferevent *);
	void player_app_offline(struct bufferevent *);
	void player_get_music(Server *, struct bufferevent *, Json::Value &);
};


#endif
