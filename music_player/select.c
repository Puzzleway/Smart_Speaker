#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <json/json.h>
#include <string.h>
#include "player.h"
#include "socket.h"
#include "device.h"

fd_set READSET;                 //集合

extern int g_maxfd;
extern int g_sockfd;
extern pthread_t tid;
extern int g_buttonfd;
extern int g_asrfd;

int init_select()
{
	FD_ZERO(&READSET);         //清空集合
	
	FD_SET(0, &READSET);       //把标准输入加入集合

	return 0;
}

void show_menu()
{
	system("clear");

	printf("\t1.开始播放            2.结束播放\n");
	printf("\t3.暂停播放            4.继续播放\n");
	printf("\t5.下一首              6.上一首\n");
	printf("\t7.增加音量            8.减小音量\n");
	printf("\t9.单曲循环            a.顺序播放\n");
}

void select_read_stdio()
{
	char ch;

	scanf("%c", &ch);

	printf("[SELECT] 读到键盘数据 [%c]\n", ch);

	getchar();

	switch(ch)
	{
		case '1':
			player_start_play();
			break;
		case '2':
			player_stop_play();
			break;
		case '3':
			player_suspend_play();
			break;
		case '4':
			player_continue_play();
			break;
		case '5':
			player_next_play();
			break;
		case '6':
			player_prior_play();
			break;
		case '7':
			player_volume_up();
			break;
		case '8':
			player_volume_down();
			break;
		case '9':
			player_set_mode(CIRCLE);
			break;
		case 'a':
			player_set_mode(SEQUENCE);
			break;
	}
}

void parse_message(const char *buf, char *cmd)
{
	struct json_object *obj = json_tokener_parse(buf);
	if (NULL == obj)
	{
		fprintf(stderr, "[ERROR] 不是一个 JSON 格式\n");
		return;
	}

	struct json_object *value;
	value = json_object_object_get(obj, "cmd");
	if (NULL == value)
	{
		fprintf(stderr, "[ERROR] 没有包含 CMD 字段\n");
		json_object_put(obj);
		return;
	}

	strcpy(cmd, json_object_get_string(value));

	json_object_put(obj);
}

void select_read_socket()
{
	char buf[1024] = {0};
	char cmd[32] = {0};

	socket_recv_data(buf);

	parse_message(buf, cmd);

	if (!strcmp(cmd, "app_start"))
	{
		socket_start_play();
	}
	else if (!strcmp(cmd, "app_stop"))
	{
		socket_stop_play();
	}
	else if (!strcmp(cmd, "app_suspend"))
	{
		socket_suspend_play();
	}
	else if (!strcmp(cmd, "app_continue"))
	{
		socket_continue_play();
	}
	else if (!strcmp(cmd, "app_next"))
	{
		socket_next_play();
	}
	else if (!strcmp(cmd, "app_prior"))
	{
		socket_prior_play();
	}
	else if (!strcmp(cmd, "app_voice_up"))
	{
		socket_volume_up();
	}
	else if (!strcmp(cmd, "app_voice_down"))
	{
		socket_volume_down();
	}
	else if (!strcmp(cmd, "app_circle"))
	{
		socket_set_mode(CIRCLE);
	}
	else if (!strcmp(cmd, "app_sequence"))
	{
		socket_set_mode(SEQUENCE);
	}
	else if (!strcmp(cmd, "app_get_music"))
	{
		socket_upload_music();
	}
}

void select_read_button()
{
	device_read_button();
}

void select_read_fifo()
{
	char buf[256] = {0};

	size_t ret = read(g_asrfd, buf, sizeof(buf));
	if (-1 == ret)
	{
		perror("read fifo");
		return;
	}
	else if (0 == ret)
	{
		//管道另一端被关了
		printf("管道异常结束\n");
		FD_CLR(g_asrfd, &READSET);
		g_maxfd = (g_maxfd == g_asrfd) ? g_maxfd - 1 : g_maxfd;
		return;
	}
	
	if (strstr(buf, "你好小胖"))
	{
		//暂停正在播放的歌曲
		player_suspend_play();

		//结束正在合成的语音
		player_stop_tts();

		//回应
		player_tts("我在呢");
	}
	else if (strstr(buf, "我想听歌") || 
	   (strstr(buf, "首") && strstr(buf, "听听")))
	{
		player_start_play();
	}
	else if (strstr(buf, "暂停") || strstr(buf, "停一下"))
	{
		player_suspend_play();
	}
	else if (strstr(buf, "继续"))
	{
		player_continue_play();
	}
	else if (strstr(buf, "下一首") || strstr(buf, "换一首"))
	{
		player_next_play();
	}
	else if (strstr(buf, "上一首") ||
			(strstr(buf, "刚才") && strstr(buf, "歌")))
	{
		player_prior_play();
	}
	else if ((strstr(buf, "声音") || strstr(buf, "音量")) &&
			  strstr(buf, "大"))
	{
		player_volume_up();

		player_continue_play();
	}
	else if ((strstr(buf, "声音") || strstr(buf, "音量")) &&
			  strstr(buf, "小"))
	{
		player_volume_down();

		player_continue_play();
	}
	else if (strstr(buf, "单曲循环"))
	{
		player_set_mode(CIRCLE);

		player_continue_play();
	}
	else if (strstr(buf, "顺序播放"))
	{
		player_set_mode(SEQUENCE);

		player_continue_play();
	}
	else if (strstr(buf, "周杰伦"))
	{
		player_singer_play("周杰伦");
	}
	else if (strstr(buf, "陈奕迅"))
	{
		player_singer_play("陈奕迅");
	}
	else if (strstr(buf, "五月天"))
	{
		player_singer_play("五月天");
	}
	else if (strstr(buf, "许嵩"))
	{
		player_singer_play("许嵩");
	}
	else if (strstr(buf, "不想听了"))
	{
		player_stop_play();
	}
	else if (strstr(buf, "换") && strstr(buf, "声音"))
	{
		player_change_voice();
	}
	else if (strstr(buf, "离线模式"))
	{
		player_offline_mode();
	}
	else           //聊天             
	{
		char cmd[1024] = {0};

		sprintf(cmd, "/home/qwen/qwen %s", buf);

		//启动qwen进程 调用sh脚本 发送数据 返回数据 解析 写管道
		system(cmd);            //阻塞函数
	}
}

void m_select()
{
	fd_set TMPSET;

	show_menu();

	while (1)
	{
		TMPSET = READSET;

		int ret = select(g_maxfd + 1, &TMPSET, NULL, NULL, NULL);
		if (-1 == ret && errno == EINTR)
		{
			continue;
		}
		else if (-1 == ret && errno != EINTR)
		{
			perror("select");
			continue;
		}

		if (FD_ISSET(0, &TMPSET))        //键盘有数据可读
		{
			select_read_stdio();
		}
		else if (FD_ISSET(g_sockfd, &TMPSET))   //网络可读
		{
			select_read_socket();
		}
		else if (FD_ISSET(g_buttonfd, &TMPSET)) //按键可读
		{
			select_read_button();
		}
		else if (FD_ISSET(g_asrfd, &TMPSET))    //管道可读
		{
			select_read_fifo();
		}
	}
}
