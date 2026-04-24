#include <stdio.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "socket.h"
#include <pthread.h>
#include "player.h"
#include "main.h"
#include <json/json.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "device.h"
#include "link.h"

int g_sockfd = 0;
int g_maxfd = 0;
pthread_t tid;

extern fd_set READSET;
extern int g_start_flag;
extern int g_suspend_flag;
extern int g_device_mode;
extern Node *g_music_head;

void socket_update_music(int sig)
{
	g_start_flag = 0;

	//回收子进程
	Shm s;
	parent_get_shm(&s);
	int status;
	waitpid(s.child_pid, &status, 0);

	//清空链表
	link_clear_list();

	//请求新的数据
	socket_get_music(s.cur_singer);

	player_start_play();

	//通知APP歌曲列表更新
	socket_upload_music();
}

void socket_send_data(struct json_object *obj)
{
	char buf[1024] = {0};
	int len = 0;

	//把json对象转换成字符串
	const char *data = json_object_to_json_string(obj);
	if (NULL == data)
	{
		printf("NOT JSON OBJECT!\n");
		return;
	}

	len = strlen(data);
	memcpy(buf, &len, 4);
	memcpy(buf + 4, data, len);

	if (send(g_sockfd, buf, len + 4, 0) == -1)
	{
		perror("send server");
		return;
	}
	
}

void *send_server(void *arg)
{
	while (1)
	{
		//创建json对象
		struct json_object *obj = json_object_new_object();

		//往json对象中添加键值对
		json_object_object_add(obj, "cmd", json_object_new_string("info"));
		//父进程读取共享内存获得当前音乐和播放模式
		Shm s;
		memset(&s, 0, sizeof(s));
		parent_get_shm(&s);

		json_object_object_add(obj, "cur_music", json_object_new_string(s.cur_music));
		json_object_object_add(obj, "mode", json_object_new_int(s.cur_mode));

		char status[8] = {0};
		//当前的状态
		if (g_start_flag == 0)
			strcpy(status, "stop");
		else if (g_start_flag == 1 && g_suspend_flag == 0)
			strcpy(status, "start");
		else if (g_start_flag == 1 && g_suspend_flag == 1)
			strcpy(status, "suspend");

		json_object_object_add(obj, "status", json_object_new_string(status));

		json_object_object_add(obj, "deviceid", json_object_new_string(DEVICEID));

		//获取当前音量
		int volume;
		device_get_volume(&volume);
		json_object_object_add(obj, "volume", json_object_new_int(volume));

		//发送给服务器
		socket_send_data(obj);

		json_object_put(obj);

		sleep(2);
	}
}


int init_socket()
{
	int count = 50;

	g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == g_sockfd)
	{
		perror("socket");
		return -1;
	}

	struct sockaddr_in server_info;
	memset(&server_info, 0, sizeof(server_info));

	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(PORT);
	server_info.sin_addr.s_addr = inet_addr(IP);

	int ret;

	while (count--)
	{
		ret = connect(g_sockfd, (struct sockaddr *)&server_info, sizeof(server_info));
		if (-1 == ret)
		{
			perror("CONNECT SERVER FAILURE");
			sleep(1);
			continue;
		}

		//把文件描述符加入集合
		FD_SET(g_sockfd, &READSET);
		g_maxfd = (g_maxfd < g_sockfd) ? g_sockfd : g_maxfd;

		//每隔5秒上报数据（当前歌曲 模式 音量 状态）
		if (pthread_create(&tid, NULL, send_server, NULL) != 0)
		{
			perror("pthread_create");
			return -1;
		}

		//音箱模式切换成在线模式
		g_device_mode = ONLINE_MODE;

		return 0;
	}

	return -1;
}

void socket_get_music(const char *singer)
{
	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "cmd", json_object_new_string("get_music_list"));
	json_object_object_add(obj, "singer", json_object_new_string(singer));

	//向服务器请求音乐数据
	socket_send_data(obj);

	//等待服务器返回
	char msg[1024] = {0};
	socket_recv_data(msg);

	//把音乐数据插入链表
	link_create_list(msg);
		
	//释放json对象
	json_object_put(obj);
}

void socket_recv_data(char *msg)
{
	int len = 0;
	size_t size = 0;        //一定要初始化
	
	while (1)
	{
		size += recv(g_sockfd, msg + size, sizeof(int) - size, 0);
		if (size >= sizeof(int))
			break;
		else if (0 == size)
		{
            FD_CLR(g_sockfd, &READSET);
            g_maxfd = (g_maxfd == g_sockfd) ? g_maxfd - 1 : g_maxfd;
            close(g_sockfd);

            pthread_cancel(tid);

			return;
		}
	}

	len = *(int *)msg;
	size = 0;
	memset(msg, 0, sizeof(int));

	while (1)
	{
		size += recv(g_sockfd, msg + size, len - size, 0);
		if (size >= len)
			break;
	}

	printf("[RECV] len %d msg %s\n", len, msg);
}

void socket_start_play()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_start_reply"));

	//开始播放
	player_start_play();

	//判断结果
	char result[128] = {0};
	FILE *fp = popen("pgrep mplayer", "r");
	fgets(result, 128, fp);

	if (strlen(result) == 0)
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
	}

	//返回结果
	socket_send_data(obj);

	json_object_put(obj);
}

void socket_stop_play()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_stop_reply"));

	//开始播放
	player_stop_play();

	//判断结果
	char result[128] = {0};
	FILE *fp = popen("pgrep mplayer", "r");
	fgets(result, 128, fp);

	if (strlen(result) == 0)
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}

	//返回结果
	socket_send_data(obj);

	json_object_put(obj);
}

void socket_suspend_play()
{
	player_suspend_play();

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_suspend_reply"));
	json_object_object_add(obj, "result", json_object_new_string("success"));

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_continue_play()
{
	player_continue_play();

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_continue_reply"));
	json_object_object_add(obj, "result", json_object_new_string("success"));

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_next_play()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_next_reply"));

	//读取共享内存 获取当前歌曲
	Shm old_info;
	parent_get_shm(&old_info);

	//切歌
	player_next_play();

	//再次获取当前歌曲
	Shm new_info;
	parent_get_shm(&new_info);
	
	//回复
	if (strcmp(old_info.cur_music, new_info.cur_music))
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "music", json_object_new_string(new_info.cur_music));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_prior_play()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_prior_reply"));

	//读取共享内存 获取当前歌曲
	Shm old_info;
	parent_get_shm(&old_info);

	//切歌
	player_prior_play();

	//再次获取当前歌曲
	Shm new_info;
	parent_get_shm(&new_info);
	
	//回复
	if (strcmp(old_info.cur_music, new_info.cur_music))
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "music", json_object_new_string(new_info.cur_music));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_volume_up()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_voice_up_reply"));

	//获取当前音量
	int old;
	device_get_volume(&old); 

	if (old == 100)
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "voice", json_object_new_int(100));
		socket_send_data(obj);

		json_object_put(obj);

		return;
	}

	//调整音量
	player_volume_up();

	//再次获取
	int new;
	device_get_volume(&new);

	//比较 返回结果
	if (new <= old)
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "voice", json_object_new_int(new));
	}

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_volume_down()
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_voice_down_reply"));

	//获取当前音量
	int old;
	device_get_volume(&old); 

	if (old == 0)
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "voice", json_object_new_int(0));
		socket_send_data(obj);

		json_object_put(obj);

		return;
	}

	//调整音量
	player_volume_down();

	//再次获取
	int new;
	device_get_volume(&new);

	//比较 返回结果
	if (new >= old)
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
		json_object_object_add(obj, "voice", json_object_new_int(new));
	}

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_set_mode(int mode)
{
	struct json_object *obj = json_object_new_object();

	if (mode == SEQUENCE)
	{
		json_object_object_add(obj, "cmd", json_object_new_string("app_sequence_reply"));
	}
	else 
	{
		json_object_object_add(obj, "cmd", json_object_new_string("app_circle_reply"));
	}

	//读取共享内存
	Shm old;
	parent_get_shm(&old);

	//如果已经是当前模式 直接返回成功
	if (old.cur_mode == mode)
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));

		socket_send_data(obj);

		json_object_put(obj);

		return;
	}

	//修改播放模式
	player_set_mode(mode);

	//再次读取 判断结果 返回
	Shm new;
	parent_get_shm(&new);

	if (new.cur_mode != old.cur_mode)
	{
		json_object_object_add(obj, "result", json_object_new_string("success"));
	}
	else
	{
		json_object_object_add(obj, "result", json_object_new_string("failure"));
	}

	socket_send_data(obj);

	json_object_put(obj);
}

void socket_upload_music()
{
	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "cmd", json_object_new_string("upload_music"));

	//创建json数组对象
	struct json_object *arr = json_object_new_array();
	Node *p = g_music_head->next;

	while (p)
	{
		json_object_array_add(arr, json_object_new_string(p->music_name));
		p = p->next;
	}

	json_object_object_add(obj, "music", arr);

	socket_send_data(obj);

	json_object_put(obj);
	json_object_put(arr);
}

void socket_disconnect()
{
	//取消线程
	pthread_cancel(tid);

	//把 g_sockfd 从集合中去掉
	FD_CLR(g_sockfd, &READSET);
	g_maxfd = (g_maxfd == g_sockfd) ? g_maxfd - 1 : g_maxfd;

	close(g_sockfd);
}
