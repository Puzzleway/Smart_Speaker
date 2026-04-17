/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:23:16
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-04-12 22:00:46
 * @FilePath: \smart_speaker\select.c
 * @Description: 这是一个智能音箱项目的select监听集合初始化函数实现文件
 */
#include<sys/select.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>  // 添加这个头文件以使用errno和EINTR
#include<unistd.h> // 可能也需要这个头文件
#include<pthread.h>
#include<json-c/json.h>
#include<string.h>

#include"player.h"
#include"socket.h"
#include"device.h"


extern int g_maxfd;
extern int g_socketfd;
extern int g_tid;
extern int g_buttonfd;

fd_set g_readfds;//监听集合
//select()监听集合初始化函数
int init_select()
{
    //监听集合初始化
    
    FD_ZERO(&g_readfds);//清空监听集合
    FD_SET(0,&g_readfds);//添加标准输入为监听对象

    return 0;
}

void show_menu(void)
{ 
    system("clear");
    printf("*******************************\n");
    printf("**********1. 播放音乐**********\n");
    printf("**********2. 结束播放**********\n");
    printf("**********3. 暂停播放**********\n");
    printf("**********4. 继续播放**********\n");
    printf("**********5. 下一首************\n");
    printf("**********6. 上一首************\n");
    printf("**********7. 增加音量**********\n");
    printf("**********8. 减小音量**********\n");
    printf("**********9. 单曲循环**********\n");
    printf("**********0. 列表循环**********\n");
    printf("*******************************\n");
}

void  select_read_stdin(void)
{
    char ch;
    scanf("%c",&ch);   
    //排除回车键
    getchar();
    // if(ch == '\n') {
    //     return; // 直接返回，不处理回车符
    // }
    //printf("[select]read from stdin");
    switch(ch)
    {
        case '1':
            player_start_play();
            printf("[select]play music\n");
            break;
        case '2':
            player_stop_play();
            printf("[select]stop music\n");
            break;
        case '3':
            player_pause_play();
            printf("[select]pause music\n");
            break;
        case '4':
            player_continue_play();
            printf("[select]continue music\n");
            break;
        case '5':
            player_next_play();
            printf("[select]next music\n");
            break;
        case '6':
            player_prev_play();
            printf("[select]previous music\n");
            break;
        case '7':
            player_add_volume();
            printf("[select]add volume\n");
            break;
        case '8':
            player_reduce_volume();
            printf("[select]reduce volume\n");
            break;
        case '9':
            player_set_mode(CIRCLE);
            printf("[select]single loop\n");
            break;
        case '0':
            player_set_mode(SEQUENCE);
            printf("[select]list loop\n");
            break;
        default:
            printf("[select]unknow command\n");
            break;
    }

}

int parse_json_cmd(const char *msg, char *cmd)
{// 解析json命令
    struct json_object *obj = json_tokener_parse(msg);
    if(NULL == obj)
    {// 解析失败
        fprintf(stderr, "parse json cmd failed\n");
        return -1;
    }
    struct json_object *val = NULL;
    if(!json_object_object_get_ex(obj, "cmd", &val))
    {// 命令不存在,没有cmd字段
        fprintf(stderr, "json cmd not found\n");
        json_object_put(obj);// 释放json对象
        return -1;
    }
    strcpy(cmd, json_object_get_string(val));// 获取命令
    json_object_put(obj);// 释放json对象
    return 0;
}

void select_read_socket(void)
{
    char msg[1024] = {0};
    char cmd[32] = {0};
    socket_recv_data(msg);
    parse_json_cmd(msg, cmd);
    if(strcmp(cmd, "app_start") == 0)
    {
        socket_start_play();
    }else if(strcmp(cmd, "app_stop") == 0)
    {
        socket_stop_play();
    }else if(strcmp(cmd, "app_pause") == 0)
    {
        socket_pause_play();
    }else if(strcmp(cmd, "app_continue") == 0)
    {
        socket_continue_play();
    }else if(strcmp(cmd, "app_next") == 0)
    {
        socket_next_play();
    }else if(strcmp(cmd, "app_prev") == 0)
    {
        socket_prev_play();
    }else if(strcmp(cmd, "app_add_volume") == 0)
    {
        socket_add_volume();
    }else if(strcmp(cmd, "app_reduce_volume") == 0)
    {
        socket_reduce_volume();
    }else if(strcmp(cmd, "app_mode_sequence") == 0)
    {
        socket_set_mode(SEQUENCE);
    }else if(strcmp(cmd, "app_mode_circle") == 0)
    {
        socket_set_mode(CIRCLE);
    }

}

void select_read_button(void)
{
    device_read_button();
}

void master_select(void)
{ 
    fd_set TMPSET;

    show_menu();

    while(1)
    { 
        TMPSET = g_readfds;
        int ret = select(g_maxfd+1,&TMPSET,NULL,NULL,NULL);
        if(ret < 0 && errno == EINTR)//select容易被信号干扰，所以要处理
        {
            continue;
        }else if(ret <0 && errno != EINTR)
        { 
            perror("select");
            exit(-1);
        }

        if(FD_ISSET(0,&TMPSET))//标准输入，监听键盘输入
        {
            select_read_stdin();
        }else if(FD_ISSET(g_socketfd,&TMPSET))// 网络监听套接字
        { 
            select_read_socket();
        }
        else if(FD_ISSET(g_buttonfd,&TMPSET))// 按键监听
        {
            select_read_button();
        }
    }
}

