#include <stdio.h>
#include <alsa/asoundlib.h>
#include "alsa.h"

snd_pcm_t *pcmp;
int play_flag = 1;    // 1表示合成 0表示不合成

extern int32_t target_rate;

int init_alsa_playback()
{
	int ret;

	// 1.打开PCM设备,播放设备
	ret = snd_pcm_open(&pcmp, PLAY_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
	if (ret != 0)
	{
		fprintf(stderr, "snd_pcm_open error");
		return -1;
	}

	snd_pcm_hw_params_t *params;
	// 2.初始化硬件参数结构体（申请内存） 宏函数
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcmp, params);

	// 3.设置访问模式：多声道交错存储
	snd_pcm_hw_params_set_access(pcmp, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	// 4.设置采样格式：S16_LE
	ret = snd_pcm_hw_params_set_format(pcmp, params, SND_PCM_FORMAT_S16_LE);
	if (ret != 0)
	{
		fprintf(stderr, "不支持S16_LE\n");
		return -1;
	}

	// 5.设置通道：单声道
	snd_pcm_hw_params_set_channels(pcmp, params, 1);

	// 6.设置采样频率：44100Hz   44098  44106
	unsigned int actual_rate = target_rate; 
	snd_pcm_hw_params_set_rate_near(pcmp, params, &actual_rate, 0);
	if (actual_rate != target_rate)
	{
		printf("实际采样频率 %u\n", actual_rate);
	}

	// 7.设置缓冲区大小
	snd_pcm_uframes_t frams_buffer = 8192;//设置大点，不然会溢出
	snd_pcm_hw_params_set_buffer_size_near(pcmp, params, &frams_buffer);

	// 8.应用参数
	snd_pcm_hw_params(pcmp, params);

	return 0;
}

// 回调函数，用于播放音频数据
int32_t play_callback(const float *samples, int32_t n)
{
	if (!play_flag)
		return 0;

	int16_t *play_buffer = malloc(sizeof(int16_t) * n);

	for (int i = 0; i < n; i++)
	{
		float t = samples[i];

		if (t < -1.0)
			t = -1.0;
		if (t > 1.0)
			t = 1.0;

		play_buffer[i] = (int16_t)(t * 32767);
	}

	int ret = snd_pcm_writei(pcmp, play_buffer, n);
	if (ret == -EPIPE)
	{
		snd_pcm_prepare(pcmp);

		ret = snd_pcm_writei(pcmp, play_buffer, n);
		if (ret < 0)
		{
			printf("缓冲区欠载，重写失败\n");
			return 0;
		}
	}
	else if (ret < 0)
	{
		printf("写入数据出错\n");
		return 0;
	}

	free(play_buffer);

	return play_flag;
}
