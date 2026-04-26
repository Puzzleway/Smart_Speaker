#include <iostream>
#include "server.h"
#include <event2/util.h>

//构造函数
Server::Server()
{
	//初始化事件集合
	m_base = event_base_new();

	//初始化数据库（创建数据库对象 创建表）
	m_database = new DataBase;
	if (!m_database->database_init_table())
	{
		std::cout << "数据库初始化失败" << std::endl;
		exit(1);
	}

	//创建player对象，用于处理APP和音箱之间的交互
	m_p = new Player;
}
//析构函数
Server::~Server()
{
	if (m_database)
		delete m_database;

	if (m_p)
		delete m_p;
}
//监听函数
void Server::listen(const char *ip, int port)
{
	struct sockaddr_in server_info;
	int len = sizeof(server_info);
	memset(&server_info, 0, len);
	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(port);
	server_info.sin_addr.s_addr = inet_addr(ip);
	
	//创建监听对象
	struct evconnlistener *listener = evconnlistener_new_bind(m_base,//m_base是事件循环的基类，负责管理事件，listener_cb是回调函数，this是服务器对象，LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE是选项，5是最大连接数，(struct sockaddr *)&server_info是服务器地址，len是服务器地址长度
		listener_cb, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,//LEV_OPT_CLOSE_ON_FREE表示关闭连接时释放资源，LEV_OPT_REUSEABLE表示允许重用地址
		5, (struct sockaddr *)&server_info, len);//5表示最大连接数，(struct sockaddr *)&server_info表示服务器地址，len表示服务器地址长度
	if (NULL == listener)//如果监听对象为空，则表示监听失败
	{
		// 原有代码（保留）：
		// std::cout << "bind error" << std::endl;
		// return;

		int err = EVUTIL_SOCKET_ERROR();
		std::cout << "bind error(" << ip << ":" << port << "): "
			  << evutil_socket_error_to_string(err) << std::endl;
		// 若绑定指定IP失败，则回退到监听所有网卡地址，适配云服务器/网卡变更场景
		if (strcmp(ip, "0.0.0.0") != 0)
		{
			std::cout << "fallback to 0.0.0.0" << std::endl;
			server_info.sin_addr.s_addr = htonl(INADDR_ANY);
			listener = evconnlistener_new_bind(m_base,
				listener_cb, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
				5, (struct sockaddr *)&server_info, len);
		}
		if (NULL == listener)
		{
			err = EVUTIL_SOCKET_ERROR();
			std::cout << "bind error(0.0.0.0:" << port << "): "
				  << evutil_socket_error_to_string(err) << std::endl;
			return;
		}
	}

	//启动定时器并监听
	server_start_timer();

	//event_base_dispatch(m_base);

	//释放对象
	evconnlistener_free(listener);
	event_base_free(m_base);
}

//一旦有客户端发起连接请求，就会触发该函数
void Server::listener_cb(struct evconnlistener *l, evutil_socket_t fd, struct sockaddr *c, int socklen, void *arg)
{
	struct sockaddr_in *client_info = (struct sockaddr_in *)c;
	//struct event_base *base = (struct event_base *)arg;
	Server *s = (Server *)arg;//将arg转换为Server对象
	//获取事件循环的基类
	struct event_base *base = s->server_get_base();

	std::cout << "[new client connection] ";
	std::cout << inet_ntoa(client_info->sin_addr) << ":";//inet_ntoa将客户端的ip地址转换为字符串
	std::cout << client_info->sin_port << std::endl;//打印客户端的端口号

	struct bufferevent *bev = bufferevent_socket_new(base, //base是事件循环的基类，fd是文件描述符，BEV_OPT_CLOSE_ON_FREE是选项
								fd, BEV_OPT_CLOSE_ON_FREE);//创建一个bufferevent对象，用于读取和写入数据
	if (NULL == bev)
	{
		std::cout << "bufferevent_socket_new error" << std::endl;
		return;
	}
//read_cb是读取数据回调函数，NULL是写入数据回调函数，event_cb是事件回调函数，s是服务器对象
	bufferevent_setcb(bev, read_cb, NULL, event_cb, s);//设置bufferevent的回调函数,read_cb是读取数据回调函数，NULL是写入数据回调函数，event_cb是事件回调函数，s是服务器对象的指针
	bufferevent_enable(bev, EV_READ);//启用bufferevent的读取事件
}

//有客户端发数据过来，会触发该函数
// *bev是bufferevent对象，*ctx是服务器对象的指针
void Server::read_cb(struct bufferevent *bev, void *ctx)
{
	Server *s = (Server *)ctx;
	char buf[1024] = {0};
	s->server_read_data(bev, buf);//读取数据

	//解析json
	Json::Reader reader;      //用于解析的对象
	Json::Value value;        //存放解析的结果

	if (!reader.parse(buf, value))
	{
		std::cout << "[JSON PARSE ERROR]" << std::endl;
		return;
	}
	
	if (value["cmd"] == "get_music_list")      //获取音乐数据
	{//根据歌手获取音乐列表
		s->server_get_music(bev, value["singer"].asString());
	}
	else if (value["cmd"] == "app_register")
	{// APP注册
		s->server_app_register(bev, value);
	}
	else if (value["cmd"] == "app_login")
	{// APP登录
		s->server_app_login(bev, value);
	}
	else if (value["cmd"] == "app_bind")
	{// APP绑定
		s->server_app_bind(bev, value);
	}
	else if (value["cmd"] == "app_offline")
	{// APP下线
		s->server_app_offline(bev);
	}
	else
	{// 处理音箱与APP之间的交互，服务器主要负责转发指令
		s->server_player_handler(bev, value);//处理音箱发过来的指令
	}
	
}

// app下线函数
//输入参数：bev是bufferevent对象
//输出参数：void
//返回值：void
void Server::server_app_offline(struct bufferevent *bev)
{
	m_p->player_app_offline(bev);
}

//读取数据函数
void Server::server_read_data(struct bufferevent *bev, char *msg)
{
	char buf[8] = {0};
	size_t size = 0;

	while (true)
	{//读取数据长度
		size += bufferevent_read(bev, buf + size, 4 - size);
		if (size >= 4)
			break;
	}

	int len = *(int *)buf;
	size = 0;
	while (true)
	{//读取数据
		size += bufferevent_read(bev, msg + size, len - size);
		if (size >= len)
			break;
	}

	std::cout << "-- LEN " << len << " MSG " << msg << std::endl;
}

//事件回调函数
//输入参数：bev是bufferevent对象，what是事件类型，ctx是服务器对象的指针
//输出参数：void
//返回值：void
void Server::event_cb(struct bufferevent *bev, short what, void *ctx)
{
	Server *ser = (Server *)ctx;

	if (what & BEV_EVENT_EOF)//如果事件类型为BEV_EVENT_EOF，则表示客户端断开连接
	{
		ser->server_client_offline(bev);
	}
}

//客户端下线函数
//输入参数：bev是bufferevent对象
//输出参数：void
//返回值：void
void Server::server_client_offline(struct bufferevent *bev)
{
	m_p->player_offline(bev);
}

struct event_base *Server::server_get_base()
{//获取事件循环的基类，基类是一个私有成员变量
	return m_base;
}

//获取音乐列表函数
void Server::server_get_music(struct bufferevent *bev, std::string s)
{
	Json::Value val;
	Json::Value arr;
	std::list<std::string> l;//音乐列表，list是双向链表，可以快速插入和删除元素，但是不能随机访问元素
	char path[128] = {0};
	
	//拼接路径
	//S.c_str()将字符串转换为字符串常量
	//sprintf(path, "/var/www/html/music/%s", s.c_str());
	sprintf(path, "/music/%s", s.c_str());

	DIR *dir = opendir(path);
	if (NULL == dir)
	{
		perror("opendir");
		return;
	}

	struct dirent *d;
	while ((d = readdir(dir)) != NULL)
	{
		//如果不是普通文件，继续循环
		if (d->d_type != DT_REG)
			continue;

		//如果不是mp3文件，继续循环
		if (!strstr(d->d_name, ".mp3"))
			continue;

		char name[512] = {0};
		//拼接路径，歌手名/音乐名
		sprintf(name, "%s/%s", s.c_str(), d->d_name);
		l.push_back(name);//将音乐名称添加到列表中
	}//遍历目录

	//随机选取 5 首歌
	//std::list<std::string>::iterator it = l.begin();
	auto it = l.begin();
	srand(time(NULL));
	int count = rand() % (l.size() - 4);// 随机选取5首歌
	for (int i = 0; i < count; i++)
	{
		it++;
	}

	for (int i = 0; i < 5 && it != l.end(); i++, it++)
	{
		arr.append(*it);//将音乐名称添加到json对象中
	}
	//键值对，将命令和音乐列表添加到json对象中
	val["cmd"] = "reply_music";
	val["music"] = arr;

	server_send_data(bev, val);//发送音乐列表
}
// 向客户端发送数据
void Server::server_send_data(struct bufferevent *bev, Json::Value &v)
{
	char msg[1024] = {0};
	std::string SendStr = Json::FastWriter().write(v);
	int len = SendStr.size();
	memcpy(msg, &len, sizeof(int));//将长度拷贝到msg的前4字节中
	memcpy(msg + sizeof(int), SendStr.c_str(), len);//将json字符串拷贝到msg的后面

	if (bufferevent_write(bev, msg, len + sizeof(int)) == -1)
	{
		std::cout << "bufferevent_write error" << std::endl;
	}
}

//处理音箱发过来的指令，转发给APP
//输入参数：bev是bufferevent对象，v是Json::Value对象
//输出参数：void
//返回值：void
void Server::server_player_handler(struct bufferevent *bev, Json::Value &v)
{
	if (v["cmd"] == "info")
	{// 音箱上报信息，更新链表
		m_p->player_update_list(bev, v, this);
	}
	else if (v["cmd"] == "app_info")
	{// 音箱上报信息，更新链表
		m_p->player_app_update_list(bev, v);
	}
	else if (v["cmd"] == "upload_music")
	{// 音箱上传当前音乐信息
		m_p->player_upload_music(this, bev, v);
	}
	else if (v["cmd"] == "app_get_music")
	{// 向APP发送音乐列表
		m_p->player_get_music(this, bev, v);
	}
	else if (v["cmd"] == "app_start" || v["cmd"] == "app_stop" || 
			 v["cmd"] == "app_suspend" || v["cmd"] == "app_continue" ||
			 v["cmd"] == "app_prior" || v["cmd"] == "app_next" || 
			 v["cmd"] == "app_voice_up" || v["cmd"] == "app_voice_down" ||
			 v["cmd"] == "app_circle" || v["cmd"] == "app_sequence") 
	{//APP下发指令，转发给音箱
		m_p->player_option(this, bev, v);
	}
	/* 命令回复函数 */
/* 这里主要考虑到音箱自主上报周期是2秒，ui显示回延迟*/
/* 所以每次执行完一个命令之后，音箱额外上报命令结果给服务器*/
	else if (v["cmd"] == "app_start_reply" ||
			 v["cmd"] == "app_stop_reply" ||
			 v["cmd"] == "app_suspend_reply" ||
			 v["cmd"] == "app_continue_reply" ||
			 v["cmd"] == "app_prior_reply" ||
			 v["cmd"] == "app_next_reply" ||
			 v["cmd"] == "app_voice_up_reply" || 
			 v["cmd"] == "app_voice_down_reply" || 
			 v["cmd"] == "app_circle_reply" ||
			 v["cmd"] == "app_sequence_reply")
	{//音箱回复APP指令，转发给APP
		m_p->player_reply_option(this, bev, v);
	}
}

//启动定时器
void Server::server_start_timer()
{
	struct event tv;      //定时器事件
	struct timeval t;     //定时器时间

	if (event_assign(&tv, m_base, -1, EV_PERSIST, timeout_cb, m_p) == -1)
	{//event_assign是事件分配函数，m_base是事件循环的基类，-1不绑定具体文件描述符，而是通过回调函数来处理事件
	// EV_PERSIST是持久化事件，timeout_cb是回调函数，m_p是player对象的指针
		std::cout << "event_assign" << std::endl;
		return;
	}

	evutil_timerclear(&t);     //清空定时器时间
	t.tv_sec = 2;                 //2秒执行一次
	event_add(&tv, &t);//添加定时器事件到m_base中

	event_base_dispatch(m_base);  //监听
}


//定时器回调函数
//输入参数：fd是文件描述符，s是事件类型，arg是player对象的指针
//输出参数：void
//返回值：void
//因为libevent是C语言编写，所以需要C风格的回调函数，不能用this指针
void Server::timeout_cb(evutil_socket_t fd, short s, void *arg)
{
	Player *p = (Player *)arg;

	p->player_traverse_list();//遍历链表，检查音箱是否在线	
}

//APP注册函数
//输入参数：bev是bufferevent对象，v是Json::Value对象
//输出参数：val是Json::Value对象
//返回值：void
void Server::server_app_register(struct bufferevent *bev, Json::Value &v)
{
	Json::Value val;

	//连接数据库
	m_database->database_connect();

	std::string appid = v["appid"].asString();
	std::string password = v["password"].asString();

	//判断用户是否存在
	if (m_database->database_user_exist(appid))   //用户存在
	{
		val["result"] = "failure";
		std::cout << "[用户已存在 注册失败]" << std::endl;
	}
	else                   //如果不存在，修改数据库
	{
		m_database->database_add_user(appid, password);

		val["result"] = "success";
		std::cout << "[用户注册成功]" << std::endl;
	}

	//断开数据库
	m_database->database_disconnect();

	//回复
	val["cmd"] = "app_register_reply";

	server_send_data(bev, val);
}


//APP登录函数，用于登录APP，验证APP的账号和密码
//输入参数：bev是bufferevent对象，v是Json::Value对象
//输出参数：val是Json::Value对象
//返回值：void
void Server::server_app_login(struct bufferevent *bev, Json::Value &v)
{
	Json::Value val;
	std::string appid = v["appid"].asString();
	std::string password = v["password"].asString();
	std::string deviceid;

	//连接数据库
	m_database->database_connect();

	do
	{
		//判断用户是否存在
		if (!m_database->database_user_exist(appid))
		{
			val["result"] = "not_exist";
			break;
		}

		//判断密码是否正确
		if (!m_database->database_password_correct(appid, password))
		{
			val["result"] = "password_error";
			break;
		}

		//是否绑定
		if (m_database->database_user_bind(appid, deviceid))
		{
			val["result"] = "bind";
			val["deviceid"] = deviceid;
		}
		else
		{
			val["result"] = "not_bind";
		}

	}while (0);

	//断开数据库
	m_database->database_disconnect();

	//返回APP
	val["cmd"] = "app_login_reply";

	server_send_data(bev, val);
}



/* 这里是puzzleway的代码 */
//APP绑定函数
//输入参数：bev是bufferevent对象，v是Json::Value对象
//输出参数：val是Json::Value对象
//返回值：void
void Server::server_app_bind(struct bufferevent *bev, Json::Value &v)
{
	std::string appid = v["appid"].asString();
	std::string deviceid = v["deviceid"].asString();

	//连接数据库
	m_database->database_connect();

	//判断用户是否已经绑定
	if (m_database->database_device_bind(deviceid))
	{
		Json::Value val;
		val["cmd"] = "app_bind_reply";
		val["result"] = "device_already_bind";
		server_send_data(bev, val);
		m_database->database_disconnect();
		return;
	}
	// 这里有个问题，应该设置用户可以绑定多个设备，但是设备只能绑定一个用户
	// 应该判断要绑定的设备是否已经绑定，如果已经绑定，则返回错误
	// 如果未绑定，则绑定

	//修改数据库
	m_database->database_bind_user(appid, deviceid);

	//关闭数据库
	m_database->database_disconnect();

	//返回数据
	Json::Value val;
	val["cmd"] = "app_bind_reply";
	val["result"] = "success";

	server_send_data(bev, val);
}
