/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 18:44:17
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-04-12 22:18:50
 * @FilePath: \smart_speaker\socket.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 18:44:17
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-22 21:27:22
 * @FilePath: \smart_speaker\socket.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<unistd.h>
#include<sys/select.h>
#include<pthread.h>
#include<sys/ipc.h>         // 添加共享内存相关头文件
#include<sys/shm.h> 
#include<json-c/json.h> 
#include<sys/wait.h>

#include"socket.h"
#include"player.h"
#include"main.h"
#include"device.h"
#include"link.h"

int g_socketfd = 0; //socket的文件描述符
int g_maxfd = 0; //select监听的最大文件描述符
pthread_t g_tid;

extern fd_set g_readfds;//监听集合
extern int g_pause_flag;//暂停标志
extern int g_start_flag;//播放标志
extern int g_device_mode;//设备模式
extern Node *g_music_head;//歌曲链表



void socket_send_json(struct json_object *jobj)
{
    char buf[1024] = {0};
    const char *json_str = json_object_to_json_string(jobj);//将json对象转换为json字符串
    if(json_str == NULL)
    {
        fprintf(stderr, "json_object_to_json_string failed\n");
        return;
    }
    int len = strlen(json_str);
    memcpy(buf, &len, 4);//将json字符串的长度拷贝到buf的前4字节中
    memcpy(buf+4, json_str, len);//将json字符串拷贝到buf的后面

    if(send(g_socketfd, buf, len+4, 0) < 0)//发送数据,输入参数0表示默认发送方式
    {
        perror("send");
        return;
    }
    //printf("send %d bytes data to server\n", len);
}

void *send_to_server(void *arg)
{
    while(1)
    {
        sleep(5);//每5秒发送一次数据
        //json格式的数据：歌曲名称、播放模式、音量、状态
        struct json_object *jobj = json_object_new_object();
        json_object_object_add(jobj, "cmd", json_object_new_string("info"));
        //当前进程通过共享内存获取播放器的状态信息
        SHM_DATA shm_data ;
        memset(&shm_data, 0, sizeof(SHM_DATA));
        parent_get_shm_data(&shm_data);
        json_object_object_add(jobj, "music_name", json_object_new_string(shm_data.music_name));
        json_object_object_add(jobj, "mode", json_object_new_int(shm_data.mode));
        int volume;
        if(device_get_volume(&volume) == 0)
        {
            json_object_object_add(jobj, "volume", json_object_new_int(volume));
        }else
        {
            json_object_object_add(jobj, "volume", json_object_new_int(-1));
        }

        json_object_object_add(jobj, "device_id", json_object_new_string(DEVICE_ID));
        if(g_start_flag==0)
        {
            json_object_object_add(jobj, "status", json_object_new_string("stop"));
        }
        else if(g_start_flag==1 && g_pause_flag==1)
        {
            json_object_object_add(jobj, "status", json_object_new_string("pause"));
        }
        else if(g_start_flag==1 && g_pause_flag==0)
        {
            json_object_object_add(jobj, "status", json_object_new_string("playing"));
        }

        //发送数据
        socket_send_json(jobj);

    }
}
int init_socket(void)
{
    g_socketfd = socket(AF_INET, SOCK_STREAM, 0);//tcp协议,流式socket
    if(g_socketfd < 0)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));//清零
    server_addr.sin_family = AF_INET;//ipv4协议
    server_addr.sin_port = htons(PORT);//端口号
    server_addr.sin_addr.s_addr = inet_addr(MY_IP);//服务器ip地址,先进行本地回环测试
    int ret;
    int count = 30;//连接失败重试次数
    while(count --){
        ret = connect(g_socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if(ret < 0){
            perror("connect");
            sleep(1);//连接失败，等待1秒后重试
            continue;
        }

        FD_SET(g_socketfd, &g_readfds);//将socket文件描述符添加到监听集合中
        if(g_socketfd > g_maxfd)
        {
            g_maxfd = g_socketfd;//更新select监听的最大文件描述符
        }
        printf("connect success\n");
        //创建一个线程，每隔5秒，向服务器发送数据，保持连接
        //数据：歌曲名称、播放模式、音量、状态
        if(pthread_create(&g_tid, NULL, send_to_server, NULL) != 0) {
            perror("pthread_create");
            return -1;
        }
        // 在线模式
        g_device_mode = ONLINE_MODE;
        return 0;//连接成功，返回0
    }
    return -1;//连接失败，进入离线模式，继续执行后续的初始化
}

void socket_recv_data(char *msg)
{
	int len = 0;
	size_t size = 0;        //一定要初始化
	
	while (1)
	{
		size += recv(g_socketfd, msg + size, sizeof(int) - size, 0);
		if (size >=sizeof(int))
			break;
        else if(size == 0)
        {
            // 处理连接异常
            FD_CLR(g_socketfd,&g_readfds);
            g_maxfd = (g_maxfd == g_socketfd)? g_socketfd-1 : g_maxfd;// 删除对套接字的监听
            close(g_socketfd);
            pthread_cancel(g_tid);
            return;
        }
	}

	len = *(int *)msg;
	size = 0;
	memset(msg, 0, sizeof(int));

	while (1)
	{
		size += recv(g_socketfd, msg + size, len - size, 0);
		if (size >= len)
			break;
	}

	printf("[RECV] len %d msg %s\n", len, msg);
}

void socket_get_music(const char *singer)
{
	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "cmd", json_object_new_string("get_music_list"));
	json_object_object_add(obj, "singer", json_object_new_string(singer));

	//向服务器请求音乐数据
	socket_send_json(obj);

	//等待服务器返回
	char msg[1024] = {0};
	socket_recv_data(msg);

	//把音乐数据插入链表
	if(link_create_list(msg)<0)
	{
		printf("create list failed\n");
		return;
	}
    
		
	//释放json对象
	json_object_put(obj);
}

void socket_update_music(int sig)
{
    g_start_flag= 0;

    SHM_DATA shm_data;
    parent_get_shm_data(&shm_data);
    int status;
    waitpid(shm_data.child_pid, &status, 0);//等待子进程结束并获取状态码
    link_clear_list();
    socket_get_music(shm_data.singer);
    player_start_play();
}

// 开始播放，通过app控制，服务器转发
void socket_start_play(void)
{
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_start命令，并且开始播放
    json_object_object_add(jobj, "cmd", json_object_new_string("app_start_reply"));
    player_start_play();//启动播放器
    char result[128] = {0};
    FILE *fp = popen("pgrep mplayer", "r");//执行命令，获取播放器进程的pid
    fgets(result, sizeof(result), fp);//读取命令输出结果
    if(strlen(result) == 0)//没有找到播放器进程
    {
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
    }
    else
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
    }
    socket_send_json(jobj);//发送结果给服务器
    json_object_put(jobj);//释放json对象
    pclose(fp);//关闭命令管道
}

// 停止播放，通过app控制，服务器转发
void socket_stop_play(void)
{
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_stop命令，并且停止播放
    json_object_object_add(jobj, "cmd", json_object_new_string("app_stop_reply"));
    player_stop_play();//停止播放器
    char result[128] = {0};
    FILE *fp = popen("pgrep mplayer", "r");//执行命令，
    fgets(result, sizeof(result), fp);
    if(strlen(result) == 0)//没有找到播放器进程
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
    }
    else
    {
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
    }
    socket_send_json(jobj);//发送结果给服务器
    json_object_put(jobj);//释放json对象
    pclose(fp);
}

// 暂停播放，通过app控制，服务器转发
void socket_pause_play(void)
{
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_pause命令，并且暂停播放
    json_object_object_add(jobj, "cmd", json_object_new_string("app_pause_reply"));
    player_pause_play();//暂停播放器
    json_object_object_add(jobj, "result", json_object_new_string("success"));

    socket_send_json(jobj);//发送结果给服务器
    json_object_put(jobj);//释放json对象
}

// 继续播放，通过app控制，服务器转发
void socket_continue_play(void)
{
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_continue命令，并且继续播放
    json_object_object_add(jobj, "cmd", json_object_new_string("app_continue_reply"));
    player_continue_play();//继续播放
    json_object_object_add(jobj, "result", json_object_new_string("success"));

    socket_send_json(jobj);
    json_object_put(jobj);

}

// 下一首，通过app控制，服务器转发
void socket_next_play(void)
{
    //读取共享内存数据
    SHM_DATA shm_data_old;
    parent_get_shm_data(&shm_data_old);
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_next命令，并且下一首
    json_object_object_add(jobj, "cmd", json_object_new_string("app_next_reply"));
    player_next_play();//下一首
    //再次读取共享内存数据
    SHM_DATA shm_data_new;
    parent_get_shm_data(&shm_data_new);
    if(strcmp(shm_data_old.music_name, shm_data_new.music_name) == 0 && shm_data_new.mode == SEQUENCE)
    {
        //没有下一首了
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
    }
    else
    {
        //成功切换到下一首了
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "music_name", json_object_new_string(shm_data_new.music_name));
    }

    socket_send_json(jobj);
    //释放json对象
    json_object_put(jobj);
}

// 上一首，通过app控制，服务器转发
void socket_prev_play(void)
{
    //读取共享内存数据
    SHM_DATA shm_data_old;
    parent_get_shm_data(&shm_data_old);
    struct json_object *jobj = json_object_new_object();
    //回复服务器，接收到app_prev命令，并且上一首
    json_object_object_add(jobj, "cmd", json_object_new_string("app_prev_reply"));
    player_prev_play();//上一首
    //再次读取共享内存数据
    SHM_DATA shm_data_new;
    parent_get_shm_data(&shm_data_new);
    if(strcmp(shm_data_old.music_name, shm_data_new.music_name) == 0 && shm_data_new.mode == SEQUENCE)
    {
        //没有上一首了
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
    }
    else if(strcmp(shm_data_old.music_name, shm_data_new.music_name) != 0)
    {
        //成功切换到上一首了
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "music_name", json_object_new_string(shm_data_new.music_name));
    }
    socket_send_json(jobj);
    json_object_put(jobj);
}

// 通过app控制，服务器转发，增加音量
void socket_add_volume(void)
{ 
    int volume_old;
    device_get_volume(&volume_old);//获取当前音量
    struct json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "cmd", json_object_new_string("app_add_volume_reply"));
    if(volume_old >= 100)
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_old));
        socket_send_json(jobj);
        json_object_put(jobj);
        return;
    }
    player_add_volume();
    int volume_new;
    device_get_volume(&volume_new);
    if(volume_new > volume_old)
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_new));
        
    }else if(volume_new <= volume_old)
    {
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_old));
    }
    socket_send_json(jobj);
    json_object_put(jobj);
}
// 通过app控制，服务器转发，减小音量
void socket_reduce_volume(void)
{
    int volume_old;
    device_get_volume(&volume_old);
    struct json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "cmd", json_object_new_string("app_reduce_volume_reply"));
    if(volume_old <= 0)
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_old));
        socket_send_json(jobj);
        json_object_put(jobj);
        return;
    }
    player_reduce_volume();
    int volume_new;
    device_get_volume(&volume_new);
    if(volume_new < volume_old)
    {
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_new));
    } 
    else if(volume_new >= volume_old)
    {
        
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
        json_object_object_add(jobj, "volume", json_object_new_int(volume_old));
    }
    socket_send_json(jobj);
    json_object_put(jobj);
}

void socket_set_mode(int mode)
{
    SHM_DATA shm_data;
    struct json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "cmd", json_object_new_string("app_set_mode_reply"));
    parent_get_shm_data(&shm_data);
    if(mode == shm_data.mode){//当前模式就是要切换的，则返回成功
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "mode", json_object_new_int(mode));
        socket_send_json(jobj);
        json_object_put(jobj);
        return;
    }
    player_set_mode(mode);
    parent_get_shm_data(&shm_data);
    if(mode == shm_data.mode){//成功切换播放模式了
        json_object_object_add(jobj, "result", json_object_new_string("success"));
        json_object_object_add(jobj, "mode", json_object_new_int(mode));
    }else{
        json_object_object_add(jobj, "result", json_object_new_string("failure"));
    }
    socket_send_json(jobj);
    json_object_put(jobj);
}