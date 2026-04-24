#include "device.h"
#include "player.h"
#include <alsa/asoundlib.h>
#include <sys/select.h>
#include <linux/input.h>
#include <sys/time.h>

int g_buttonfd = 0;
BUTTON_STATE state = STATE_IDLE;
struct itimerval tv;
struct timeval old, new;

extern fd_set READSET;
extern int g_maxfd;
extern int g_start_flag;
extern int g_suspend_flag;

//调用 alsa 函数设置系统音量 参数是音量百分比
int device_set_volume(int volume)
{
	snd_mixer_t *mixer;

	//打开混音设备
	if (snd_mixer_open(&mixer, 0) != 0)
	{
		fprintf(stderr, "snd_mixer_open error");
		return -1;
	}

	//附加声卡设备
	snd_mixer_attach(mixer, CARD_NAME);

	//注册混音器
	snd_mixer_selem_register(mixer, NULL, NULL);

	//加载混音器
	snd_mixer_load(mixer);

	snd_mixer_selem_id_t *id;

	//设置元素ID
	snd_mixer_selem_id_alloca(&id);
	snd_mixer_selem_id_set_index(id, 0);
	snd_mixer_selem_id_set_name(id, ELEM_NAME);

	snd_mixer_elem_t *elem;
	elem = snd_mixer_find_selem(mixer, id);
	if (NULL == elem)
	{
		fprintf(stderr, "snd_mixer_find_selem error");
		return -1;
	}

	long min, max;
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

	volume = (max - min) * volume / 100 + min;

	//设置当前音量的数值
	snd_mixer_selem_set_playback_volume_all(elem, volume);

	//printf("设置成功 %ld\n", volume);

	snd_mixer_close(mixer);

	return 0;
}

int device_get_volume(int *v)
{
	snd_mixer_t *mixer;

	//打开混音设备
	if (snd_mixer_open(&mixer, 0) != 0)
	{
		fprintf(stderr, "snd_mixer_open error");
		return -1;
	}

	//附加声卡设备
	snd_mixer_attach(mixer, CARD_NAME);

	//注册混音器
	snd_mixer_selem_register(mixer, NULL, NULL);

	//加载混音器
	snd_mixer_load(mixer);

	snd_mixer_selem_id_t *id;

	//设置元素ID
	snd_mixer_selem_id_alloca(&id);
	snd_mixer_selem_id_set_index(id, 0);
	snd_mixer_selem_id_set_name(id, ELEM_NAME);

	snd_mixer_elem_t *elem;
	elem = snd_mixer_find_selem(mixer, id);
	if (NULL == elem)
	{
		fprintf(stderr, "snd_mixer_find_selem error");
		return -1;
	}

	long min, max;
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

	long volume;

	//获取当前音量的数值
	snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);

	*v = (volume - min) * 100 / (max - min);

	*v = (*v + 5) / 10 * 10;

	snd_mixer_close(mixer);

	return 0;
}

int init_button()
{
	g_buttonfd = open("/dev/input/event1", O_RDONLY);
    if (-1 == g_buttonfd) 
    {   
        perror("open button");
        return -1; 
    }

	//加入集合
	FD_SET(g_buttonfd, &READSET);

	g_maxfd = (g_maxfd < g_buttonfd) ? g_buttonfd : g_maxfd;

	return 0;
}

void device_read_button()
{
	struct input_event ev;

	int ret = read(g_buttonfd, &ev, sizeof(ev));
    if (-1 == ret)
    {
    	perror("read");
        return;
    }

   	if (ev.type != EV_KEY)
    {
    	return;
    }

	if (ev.value == 1)
	{
		if (state == STATE_IDLE)
		{
			gettimeofday(&old, NULL);

            state = STATE_FIRST_PRESS;
		}
		else if (state == STATE_FIRST_RELEASE)
		{
			//printf("双击\n");
			player_next_play();

            state = STATE_IDLE;

            tv.it_value.tv_sec = 0;
            tv.it_value.tv_usec = 0;

            tv.it_interval.tv_sec = 0;
            tv.it_interval.tv_usec = 0;

            //取消定时器
           	setitimer(ITIMER_REAL, &tv, NULL);
		}
	}
	else if (ev.value == 0)
	{
		if (state == STATE_FIRST_PRESS)
		{
			gettimeofday(&new, NULL);

			if ((new.tv_sec * 1000 + new.tv_usec / 1000) - (old.tv_sec * 1000 + old.tv_usec / 1000) > 300)
            {
            	//printf("长按\n");

				player_prior_play();

                state = STATE_IDLE;
            }
			else
			{
				state = STATE_FIRST_RELEASE;

               	tv.it_value.tv_sec = 0;
                tv.it_value.tv_usec = 300 * 1000;

                tv.it_interval.tv_sec = 0;
                tv.it_interval.tv_usec = 0;

               	//启动定时器
                setitimer(ITIMER_REAL, &tv, NULL);
			}
		}
	}
}

void button_handler(int s)
{
	if (g_start_flag == 0)
		player_start_play();
	else if (g_start_flag == 1 && g_suspend_flag == 0)
		player_suspend_play();
	else if (g_start_flag == 1 && g_suspend_flag == 1)
		player_continue_play();

    state = STATE_IDLE;
}
