#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "alsa.h"
#include "sherpa.h"
#include "kws.h"
#include "sherpa-onnx/c-api/c-api.h"

enum AppState
{
	STATE_KWS,
	STATE_ASR
};

int running = 1;
enum AppState cur_state = STATE_KWS;
int asr_fd;
enum DeviceMode device_mode = ONLINE_MODE;

extern snd_pcm_t *pcmp;
extern snd_pcm_uframes_t frams_per_buffer;
extern unsigned int target_rate;
extern unsigned int sample_rate;
extern const SherpaOnnxOnlineStream *asr_stream;
extern const SherpaOnnxOnlineRecognizer *asr_recognizer;

void quit_handler(int s)
{
	running = 0;
}

void clean_up()
{
	//关闭PCM设备
	if (pcmp)
		snd_pcm_close(pcmp);

	//关闭音频流
	if (asr_stream)
		SherpaOnnxDestroyOnlineStream(asr_stream);

	//关闭识别器
	if (asr_recognizer)
		SherpaOnnxDestroyOnlineRecognizer(asr_recognizer);
}

void offline_handler(int s)
{
	device_mode = OFFLINE_MODE;
}

int main()
{
	//信号处理函数
	signal(SIGINT, quit_handler);
	signal(SIGUSR1, offline_handler);

	//初始化ALSA	
	if (init_alsa() == -1)
	{
		printf("ALSA初始化失败\n");
		clean_up();
		return -1;
	}
	printf("ALSA初始化成功\n");

	//初始化语音识别
	if (init_sherpa_asr() == -1)
	{
		printf("语音识别初始化失败\n");
		clean_up();
		return -1;
	}
	printf("语音识别加载成功\n");

	if (init_sherpa_kws() == -1)
	{
		printf("关键词识别初始化失败\n");
		clean_up();
		return -1;
	}
	printf("关键词加载成功\n");

	// 打开管道（使用 O_RDWR 避免没有读端时 O_WRONLY 阻塞）
	asr_fd = open("/home/fifo/asr_fifo", O_RDWR);
	if (-1 == asr_fd)
	{
		fprintf(stderr, "open fifo error\n");
		clean_up();
		return -1;
	}

	printf("\n\n=====关键词识别模式=====\n");
	printf("请说关键词唤醒...\n");

	//申请内存存放读取的音频数据
	int16_t *buffer = malloc(sizeof(int16_t) * frams_per_buffer);
	if (NULL == buffer)
	{
		fprintf(stderr, "buffer malloc failure\n");
		clean_up();
		return -1;
	}

	while (running)
	{
		//读取数据
		snd_pcm_sframes_t frams_read;
		frams_read = snd_pcm_readi(pcmp, buffer, frams_per_buffer);
		if (-EPIPE == frams_read)
		{
			// 缓冲区欠载
			snd_pcm_prepare(pcmp);
			continue;
		}
		else if (frams_read < 0)
		{
			fprintf(stderr, "%s\n", snd_strerror(frams_read));
			continue;
		}

		//重采样
		//计算重采样后的帧数
		size_t resample_frams = frams_read * target_rate / sample_rate + 0.5;
		int16_t *resample_buffer = malloc(sizeof(int16_t) * resample_frams);
		if (resample_buffer == NULL)
		{
			fprintf(stderr, "resample buffer malloc failure\n");
			break;
		}

		resample_linear(buffer, frams_read, resample_buffer, resample_frams);

		float *float_buffer = malloc(sizeof(float) * resample_frams);
		if (float_buffer == NULL)
		{
			fprintf(stderr, "float buffer malloc failure\n");
			free(resample_buffer);
			break;
		}

		//转换成浮点数并且归一化  -32768-32767
		for (int i = 0; i < resample_frams; i++)
		{
			//保证所有数据在 -1 到 1 范围内
			float_buffer[i] = resample_buffer[i] / 32768.0f;
		}

		if (cur_state == STATE_ASR && device_mode == ONLINE_MODE)
		{
			if (sherap_asr(float_buffer, resample_frams))
			{
				cur_state = STATE_KWS;
				printf("\n=====关键词识别模式=====\n");
				printf("请说关键词唤醒...\n");
			}
		}
		else if (cur_state == STATE_KWS)
		{
			if (sherpa_kws(float_buffer, resample_frams))
			{
				if (device_mode == ONLINE_MODE)
				{
					cur_state = STATE_ASR;
					printf("\n=====语音识别模式=====\n");
					printf("请说话...\n");
	
					//sleep(2);
					usleep(1200000);
					snd_pcm_drop(pcmp);   //清空缓冲区 变成SETUP状态
					snd_pcm_prepare(pcmp);
				}
			}
		}
			
		free(resample_buffer);
		free(float_buffer);

		usleep(1000);
	}

	free(buffer);

	clean_up();

	return 0;
}
