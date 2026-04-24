#include <stdio.h>
#include "select.h"
#include "link.h"
#include "player.h"
#include <stdlib.h>
#include <signal.h>
#include "socket.h"
#include "device.h"
#include "main.h"
#include "link.h"

int main()
{
	//运行初始化脚本
	system("/home/music_player/init.sh");

	signal(SIGUSR1, socket_update_music);
	signal(SIGALRM, button_handler);

	//初始化集合    select
	if (init_select() != 0)
	{
		printf("集合初始化失败\n");
		return -1;
	}
	printf("初始化集合成功\n");

	//初始化链表
	if (init_link() == -1)
	{
		printf("链表初始化失败\n");
		return -1;
	}
	printf("初始化链表成功\n");

	//初始化共享内存
	if (init_shm() == -1)
	{
		printf("初始化共享内存失败\n");
		return -1;
	}
	printf("初始化共享内存成功\n");

	//初始化信号量
	if (init_sem() == -1)
	{
		printf("初始化信号量失败\n");
		return -1;
	}
	printf("初始化信号量成功\n");

	//初始化音量
	device_set_volume(DEL_VOLUME);

	//初始化按键
	if (init_button() == -1)
	{
		printf("初始化按键失败\n");
	}
	else
	{
		printf("初始化按键成功\n");
	}

	//初始化管道
	if (init_fifo() == -1)
	{
		printf("初始化管道失败\n");
		return -1;
	}
	printf("初始化管道成功\n");

	//初始化网络
	if (init_socket() == -1)
	{
		printf("初始化网络失败\n");

		//进入离线模式
		if (player_offline_mode() == -1)
		{
			return -1;
		}

		m_select();
	}
	printf("初始化网络成功\n");
	

	//获取音乐文件（歌手/名字）
	socket_get_music("其他");

	//link_traverse_list();
	
	m_select();               //循环监听

	return 0;
}
