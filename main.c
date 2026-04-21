/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:18:46
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-24 22:40:47
 * @FilePath: \smart_speaker\main.c
 * @Description: 这是智能音箱主程序的入口，负责初始化各个模块，并进入主循环。
 */

#include<stdio.h>
#include<unistd.h>
#include<signal.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>
#include "select.h"
#include "link.h"
#include "player.h"
#include "socket.h"
#include "device.h"
#include "main.h"

static void run_init_script(void)
{
    char exe_path[PATH_MAX] = {0};
    char script_path[PATH_MAX] = {0};
    const char *suffix = "/init.sh";
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0)
    {
        perror("readlink /proc/self/exe");
        return;
    }

    exe_path[len] = '\0';
    char *last_sep = strrchr(exe_path, '/');
    if (last_sep == NULL)
    {
        fprintf(stderr, "invalid executable path: %s\n", exe_path);
        return;
    }
    *last_sep = '\0';

    if ((size_t)len + strlen(suffix) + 1 > sizeof(script_path))
    {
        fprintf(stderr, "init script path too long\n");
        return;
    }

    if (snprintf(script_path, sizeof(script_path), "%s%s", exe_path, suffix) < 0)
    {
        perror("snprintf init.sh path");
        return;
    }

    if (access(script_path, X_OK) != 0)
    {
        fprintf(stderr, "warning: init script not executable or missing: %s\n", script_path);
        return;
    }

    if (system(script_path) == -1)
    {
        perror("system init.sh");
    }
}

int main()
{
    //一个初始化脚本，在启动后执行，主要功能是清理共享内存，以确保播放器能够正常运行。
    run_init_script();
    //注册信号处理函数,一旦收到SIGUSR1信号，则执行socket_update_music函数
    signal(SIGUSR1,socket_update_music);
    //注册定时器信号处理函数,一旦收到SIGALRM信号，则执行device_button_timer_handler函数
    signal(SIGALRM, device_button_timer_handler);
    //初始化监听集合
    if(init_select()==-1)
    {
        perror("init_select");
        return -1;
    }else {
        printf("init_select success\n");
    }
    //初始化音乐播放列表
    if(init_link()==-1)
    {
        perror("init_link");
        return -1;
    }else {
        printf("init_link success\n");
    }
    //初始化共享内存
    if(init_shm()==-1)
    {
        perror("init_shm");
        return -1;
    }else {
        printf("init_shm success\n");
    }
    //初始化信号量
    if(init_sem()==-1)
    {
        perror("init_sem");
        return -1;
    }else {
        printf("init_sem success\n");
    }
     //初始化播放音量
    if(device_set_volume(DEFAULT_VOLUME)==-1)
    {
        perror("device_set_volume");
        return -1;
    }else {
        printf("device_set_volume success\n");
    }
    //初始化网络
    if(init_socket()==-1)
    {
        perror("init_socket");
        //网络初始化失败，进入离线模式，继续执行后续的初始化
        /* TODO: 离线模式*/
    }else {
        printf("init_socket success\n");
    }
    //初始化按键
   if(init_button()==-1)
   {
        perror("init_button");
        return -1;
   }else {
        printf("init_button success\n");
   }
   if(init_fifo()==-1)
   {
       perror("init_fifo");
       return -1;
   }else {
       printf("init_fifo success\n");
   }
    // 测试获取服务器上歌曲、歌手
    socket_get_music("其他");
    // 遍历音乐列表
    // link_traverse_list();
    // 清空音乐列表
    // link_clear_list();


    master_select();//持续监听
}