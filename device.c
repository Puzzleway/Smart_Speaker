#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <linux/input.h>
#include <alsa/asoundlib.h>
#include "device.h"
#include "player.h"

extern int g_start_flag;
extern int g_pause_flag;
extern int g_maxfd;
extern fd_set g_readfds;//监听集合

int g_buttonfd;//按键文件描述符
BUTTON_STATE g_button_state = STATE_IDLE;//按键状态初始化,默认为空闲状态
struct itimerval g_itimer;//定时器结构体
//设置设备音量
int device_set_volume(int volume)
{
    snd_mixer_t *mixer;
    // 打开混音器
    if(snd_mixer_open(&mixer, 0) < 0) {
        fprintf(stderr, "Error opening mixer\n");
        return -1;
    }
    // 连接混音器
    if(snd_mixer_attach(mixer, DEVICE_NAME) < 0) {
        fprintf(stderr, "Error attaching mixer\n");
        return -1;
    }
    // 注册混音器
    if(snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
        fprintf(stderr, "Error registering mixer\n");
        return -1;
    }
    // 加载混音器
    if(snd_mixer_load(mixer) < 0) {
        fprintf(stderr, "Error loading mixer\n");
        return -1;
    }
    // 设置混音器元素id
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);// 默认索引0
    snd_mixer_selem_id_set_name(sid, DEVICE_ELEMENT);
    // 获取混音器元素
    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, sid);
    if(elem == NULL) {
        fprintf(stderr, "Error finding mixer element\n");
        return -1;
    }
    // 获取当前音量范围
    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    // 设置当前音量
    long value = 0 ;
    value = volume *(max - min) / 100 + min; // 将百分比转换为实际音量值
    if(snd_mixer_selem_set_playback_volume_all(elem, value) < 0) {
        fprintf(stderr, "Error setting playback volume\n");
        return -1;
    }
    //printf("Volume set to %ld\n", value);
    // 关闭混音器
    snd_mixer_close(mixer);
    return 0;
}

//获取设备音量
int device_get_volume(int *volume)
{
    snd_mixer_t *mixer;
    // 打开混音器
    if(snd_mixer_open(&mixer, 0) < 0) {
        fprintf(stderr, "Error opening mixer\n");
        return -1;
    }
    // 连接混音器
    if(snd_mixer_attach(mixer, DEVICE_NAME) < 0) {
        fprintf(stderr, "Error attaching mixer\n");
        return -1;
    }
    // 注册混音器
    if(snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
        fprintf(stderr, "Error registering mixer\n");
        return -1;
    }
    // 加载混音器
    if(snd_mixer_load(mixer) < 0) {//数据在这里刷新到缓冲区
        fprintf(stderr, "Error loading mixer\n");
        return -1;
    }
    // 设置混音器元素id
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);// 默认索引0
    snd_mixer_selem_id_set_name(sid, DEVICE_ELEMENT);
    // 获取混音器元素
    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, sid);
    if(elem == NULL) {
        fprintf(stderr, "Error finding mixer element\n");
        return -1;
    }
    // 获取当前音量范围
    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    // 获取当前音量
    long value;
    if(snd_mixer_selem_get_playback_volume(elem, 0, &value) < 0) {
        fprintf(stderr, "Error getting playback volume\n");
        return -1;
    }
    *volume = (value - min) * 100 / (max - min); // 将实际音量值转换为百分比
    *volume = (*volume+5)/10*10;//四舍五入到10的倍数
    //printf("Volume is %ld\n", value);

    // 关闭混音器
    snd_mixer_close(mixer);
    return 0;
}

int init_button()
{   
    g_buttonfd = open("/dev/input/event1", O_RDONLY);
    if(g_buttonfd == -1)
    {
        perror("open");
        return -1;
    }
    // 加入到监听集合
    FD_SET(g_buttonfd, &g_readfds);
    if(g_buttonfd > g_maxfd)
    {
        g_maxfd = g_buttonfd;//更新select监听的最大文件描述符
    }
    return 0;
}

void device_button_timer_handler(int sig)
{
    //printf("short press\n");
    if(g_start_flag == 1 && g_pause_flag == 0)
    {
        player_pause_play();
    }else if(g_start_flag == 1 && g_pause_flag == 1)
    {
        player_continue_play();
    }else if(g_start_flag == 0)
    {
        player_start_play();
    }
    g_button_state = STATE_IDLE;
}

void device_read_button(void)
{
    struct input_event event;
    struct timeval time1;//按键第一次按下时间
    struct timeval time2;//按键第二次按下时间

    int ret = read(g_buttonfd, &event, sizeof(event));
    if(ret == -1)
    {
        perror("read");
        return;
    }
    if(event.type != EV_KEY)
    {
        return;
    }
    if(event.value == 1)
    {
         if(g_button_state == STATE_IDLE)
         {
            gettimeofday(&time1, NULL);//记录按键第一次按下时间
            g_button_state = STATE_FIRST_PRESSED;
         }else if(g_button_state == STATE_FIRST_RELEASED)
         {
            //printf("double press\n");
            player_next_play();//双击下一首
            g_button_state = STATE_SECOND_PRESSED;
            g_itimer.it_value.tv_sec = 0;
            g_itimer.it_value.tv_usec = 0;
            g_itimer.it_interval.tv_sec = 0;
            g_itimer.it_interval.tv_usec = 0;
            setitimer(ITIMER_REAL, &g_itimer, NULL);//设置定时器
         }
    }
    else if(event.value == 0)
    {
         if(g_button_state == STATE_FIRST_PRESSED)
         {
            gettimeofday(&time2, NULL);//记录按键松开时间
            if((time2.tv_sec*1000 + time2.tv_usec/1000) - (time1.tv_sec*1000 + time1.tv_usec/1000) > 500)//按下持续时间大于500毫秒，认为是长按
            {
                //printf("long press\n");
                player_prev_play();//长按上一首
                g_button_state = STATE_IDLE;
            }else if((time2.tv_sec*1000 + time2.tv_usec/1000) - (time1.tv_sec*1000 + time1.tv_usec/1000) <= 500)//按下持续时间小于等于500毫秒，进入短按/双击判定
            {
                g_button_state = STATE_FIRST_RELEASED;
                g_itimer.it_value.tv_sec = 0;
                g_itimer.it_value.tv_usec = 500000;//定时器时间间隔500毫秒
                g_itimer.it_interval.tv_sec = 0;
                g_itimer.it_interval.tv_usec = 0;//定时器不重复
                setitimer(ITIMER_REAL, &g_itimer, NULL);//设置定时器
            }
         }else if(g_button_state == STATE_SECOND_PRESSED)
         {
            g_button_state = STATE_IDLE;
         }
    }
}