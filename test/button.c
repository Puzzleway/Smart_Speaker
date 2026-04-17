#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/time.h>
#include <signal.h>

typedef enum{//按键状态枚举
    STATE_IDLE,
    STATE_FIRST_PRESSED,
    STATE_FIRST_RELEASED,
    STATE_SECOND_PRESSED
}BUTTON_STATE;

BUTTON_STATE g_button_state = STATE_IDLE;//按键状态初始化,默认为空闲状态
struct itimerval g_itimer;//定时器结构体

void timer_handler(int sig)
{
    printf("short press\n");
    g_button_state = STATE_IDLE;
}

int main()
{
    int fd = open("/dev/input/event0", O_RDONLY);
    if(fd == -1)
    {
        perror("open");
        return -1;
    }
    struct input_event event;
    struct timeval time1;//按键第一次按下时间
    struct timeval time2;//按键第二次按下时间
    signal(SIGALRM, timer_handler);//注册定时器信号处理函数
    while(1)
    {
        int ret = read(fd, &event, sizeof(event));
        if(ret == -1)
        {
            perror("read");
            return -1;
        }
        if(event.type != EV_KEY)
        {
            continue;
        }
        if(event.value == 1)
        {
            if(g_button_state == STATE_IDLE)
            {
                //printf("first press\n");
                gettimeofday(&time1, NULL);
                g_button_state = STATE_FIRST_PRESSED;
            }else if(g_button_state == STATE_FIRST_RELEASED)
            {
                printf("double press\n");
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
                gettimeofday(&time2, NULL);
                if((time2.tv_sec*1000 + time2.tv_usec/1000) - (time1.tv_sec*1000 + time1.tv_usec/1000) > 500)//按下持续时间大于500毫秒，认为是长按
                {
                    printf("long press\n");
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
            }
            else if(g_button_state == STATE_SECOND_PRESSED)
            {
                g_button_state = STATE_IDLE;
            }
        }
    }
    close(fd);
    return 0;
}