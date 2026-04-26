#include "player.h"
#include "server.h"

Player::Player()
{
	info = new std::list<PlayerInfo>;
}

Player::~Player()
{
	if (info)
		delete info;
}

//更新链表信息
void Player::player_update_list(struct bufferevent *bev, Json::Value &v, Server *ser)
{
	auto it = info->begin();

	//遍历链表，如果设备存在，更新链表并且转发给APP
	for (; it != info->end(); it++)
	{
		if (it->deviceid == v["deviceid"].asString()) //找到了对应的结点
		{
			debug("设备已经存在，更新链表信息");
			it->cur_music = v["cur_music"].asString();
			it->volume = v["volume"].asInt();
			it->mode = v["mode"].asInt();
			it->d_time = time(NULL);

			if (it->a_bev)
			{
				debug("APP在线，数据转发给APP");
				ser->server_send_data(it->a_bev, v);
			}

			return;
		}
	}

	//如果设备不存在，新建结点
	PlayerInfo p;
	p.deviceid = v["deviceid"].asString();//asString()将json对象中的字符串转换为字符串
	p.cur_music = v["cur_music"].asString();
	p.volume = v["volume"].asInt();//asInt()将json对象中的整数转换为整数
	p.mode = v["mode"].asInt();
	p.d_time = time(NULL);//time(NULL)获取当前时间
	p.d_bev = bev;
	p.a_bev = NULL;//将APP的bufferevent设置为NULL

	info->push_back(p);

	debug("第一次上报数据，结点已经建立");
}

//更新音箱链表信息，APP上线时调用
//输入参数：bev是bufferevent对象，v是Json::Value对象
//输出参数：void
//返回值：void
void Player::player_app_update_list(struct bufferevent *bev, Json::Value &v)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{//遍历链表
		if (it->deviceid == v["deviceid"].asString())//找到对应的结点
		{
			it->a_time = time(NULL);
			it->appid = v["appid"].asString();
			it->a_bev = bev;
		}
	}
}

//遍历链表，检查音箱是否在线
//检查音箱是否在线，如果离线，则删除链表中的结点
//输入参数：void
//输出参数：void
//返回值：void
void Player::player_traverse_list()
{
	debug("定时器事件：遍历链表，检查音箱是否在线");

	for (auto it = info->begin(); it != info->end(); it++)
	{
		if (time(NULL) - it->d_time > 6)  //检查时间差，如果超过6秒，则认为音箱离线
		{// 2秒上报一次，如果6秒没有上报，则认为音箱离线
			info->erase(it);//删除链表中的结点
		}

		if (it->a_bev)//APP在线
		{
			if (time(NULL) - it->a_time > 6)// 检查时间差，如果超过6秒，则认为APP离线
			{
				it->a_bev = NULL;//将APP的bufferevent设置为NULL
			}
		}
	}
}

//打印日志，打印时间
//输入参数：s是字符串
//输出参数：void
//返回值：void
void Player::debug(const char *s)
{
	time_t cur = time(NULL);
	struct tm *t = localtime(&cur);

	std::cout << "[ " << t->tm_hour << " : " << t->tm_min << " : ";
	std::cout << t->tm_sec << " ] " << s << std::endl;
}

// 音箱更新音乐列表之后主动上传音乐列表，服务器转发给APP
// 输入参数：ser是服务器对象，bev是bufferevent对象，v是Json::Value对象
// 输出参数：void
// 返回值：void
void Player::player_upload_music(Server *ser, struct bufferevent *bev, Json::Value &v)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{//遍历链表，找到对应的音箱
		if (it->d_bev == bev)
		{
			if (it->a_bev != NULL)    //APP在线
			{//APP在线，转发音乐信息到APP
				ser->server_send_data(it->a_bev, v);
				debug("APP在线 歌曲名字转发成功");
			}
			else
			{
				debug("APP不在线 歌曲名字不转发");
			}
			break;
		}
	}
}

// APP向音箱下发指令,由服务器转发给音箱
//输入参数：ser是服务器对象，bev是bufferevent对象，v是Json::Value对象
//输出参数：void
//返回值：void
void Player::player_option(Server *ser, struct bufferevent *bev, Json::Value &v)
{
	//判断音箱是否在线
	for (auto it = info->begin(); it != info->end(); it++)
	{//遍历链表，找到对应的音箱
		if (it->a_bev == bev)
		{// 音箱在线，转发指令
			ser->server_send_data(it->d_bev, v);
			debug("音箱在线，指令转发成功");
			return;
		}
	}

	//音箱不在线，由服务器回复APP，告知APP音箱不在线
	Json::Value value;
	std::string cmd = v["cmd"].asString();
	cmd += "_reply";
	value["cmd"] = cmd;
	value["result"] = "offline";

	ser->server_send_data(bev, value);

	debug("音箱不在线，转发失败");
}

//音箱回复APP指令,由服务器转发给APP
//输入参数：ser是服务器对象，bev是bufferevent对象，v是Json::Value对象
//输出参数：void
//返回值：void
void Player::player_reply_option(Server *ser, struct bufferevent *bev, Json::Value &v)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{//遍历链表，找到对应的音箱
		if (it->d_bev == bev)
		{
			if (it->a_bev)
			{// APP在线，转发reply指令
				ser->server_send_data(it->a_bev, v);
				break;
			}
		}
	}
}

//音箱或者APP异常下线处理
//输入参数：bev是bufferevent对象
//输出参数：void
//返回值：void
void Player::player_offline(struct bufferevent *bev)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{
		if (it->d_bev == bev)
		{
			debug("音箱异常下线处理");
			info->erase(it);
			break;
		}

		if (it->a_bev == bev)
		{
			debug("APP异常下线处理");
			it->a_bev = NULL;
			break;
		}
	}
}

//APP正常下线处理
//输入参数：bev是bufferevent对象
//输出参数：void
//返回值：void
void Player::player_app_offline(struct bufferevent *bev)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{
		if (it->a_bev == bev)
		{
			debug("APP正常下线处理");
			it->a_bev = NULL;
			break;
		}
	}
}

// APP指令，第一次检测到音箱在线，主动获取音乐列表
//输入参数：ser是服务器对象，bev是bufferevent对象，v是Json::Value对象
//输出参数：void
//返回值：void
void Player::player_get_music(Server *ser, struct bufferevent *bev, Json::Value &v)
{
	for (auto it = info->begin(); it != info->end(); it++)
	{
		if (it->a_bev == bev)
		{
			ser->server_send_data(it->d_bev, v);
		}
	}
}
