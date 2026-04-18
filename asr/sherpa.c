#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sherpa-onnx/c-api/c-api.h"

unsigned int target_rate = 16000;
const SherpaOnnxOnlineStream *asr_stream = NULL;
const SherpaOnnxOnlineRecognizer *asr_recognizer = NULL;

extern int asr_fd;

int init_sherpa_asr()
{
	SherpaOnnxOnlineRecognizerConfig config;

	memset(&config, 0, sizeof(config));

	//特征配置
	config.feat_config.sample_rate = target_rate;   //采样频率
	config.feat_config.feature_dim = 80;      //梅尔频谱特征的维度
	
	//模型配置
	config.model_config.transducer.encoder = "/home/sherpa/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16/encoder-epoch-99-avg-1.onnx";
	config.model_config.transducer.decoder = "/home/sherpa/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16/decoder-epoch-99-avg-1.onnx";
	config.model_config.transducer.joiner = "/home/sherpa/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16/joiner-epoch-99-avg-1.onnx";
	config.model_config.tokens = "/home/sherpa/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16/tokens.txt";
	config.model_config.num_threads = 4;       //四个线程
	config.model_config.provider = "cpu";
	config.model_config.debug = 0;

	config.max_active_paths = 4;
	config.decoding_method = "greedy_search";  //贪婪搜索算法

	//端点检测
	config.enable_endpoint = 1;
	config.rule1_min_trailing_silence = 1.0;   //静音触发时间
	config.rule2_min_trailing_silence = 0.6;
	config.rule3_min_utterance_length = 500;   //毫秒 最小语音长度
	
	//创建识别器
	asr_recognizer = SherpaOnnxCreateOnlineRecognizer(&config);
	if (asr_recognizer == NULL)
	{
		fprintf(stderr, "SherpaOnnxCreateOnlineRecognizer error\n");

		return -1;
	}

	//创建音频流
  	asr_stream = SherpaOnnxCreateOnlineStream(asr_recognizer);
  	if (NULL == asr_stream)
  	{
	  	fprintf(stderr, "SherpaOnnxCreateOnlineStream ERROR\n");

	  	return -1;
  	}

  return 0;
}

int sherap_asr(float *float_buffer, int resample_frams)
{
	int ret = 0;

	//把数据提交到音频流
	//哪个音频流 采样频率 数据地址 数据长度
	SherpaOnnxOnlineStreamAcceptWaveform(asr_stream, target_rate, float_buffer, resample_frams);

	//开始识别
	while (SherpaOnnxIsOnlineStreamReady(asr_recognizer, asr_stream)) 
	{
      	SherpaOnnxDecodeOnlineStream(asr_recognizer, asr_stream);
    }

	//读取数据
	const SherpaOnnxOnlineRecognizerResult *r =
       SherpaOnnxGetOnlineStreamResult(asr_recognizer, asr_stream);

	//端点检测
	if (SherpaOnnxOnlineStreamIsEndpoint(asr_recognizer, asr_stream))
	{
		if (r && r->text && strlen(r->text) > 0)
		{
			printf("---> %s\n", r->text);

			//把数据写入管道
			if (write(asr_fd, r->text, strlen(r->text)) == -1)
			{
				perror("write fifo");
			}

			//清空音频流数据
			SherpaOnnxOnlineStreamReset(asr_recognizer, asr_stream);

			ret = 1;
		}
	}

	//清空结果
	SherpaOnnxDestroyOnlineRecognizerResult(r);

	return ret;
}
