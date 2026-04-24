#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <json-c/json.h>
#include <fcntl.h>

//解析json对象
void parse_json(const char *json, char *content)
{
	//字符串转换成json对象
	struct json_object *obj = json_tokener_parse(json);
	if (NULL == obj)
	{
		fprintf(stderr, "不是一个json对象\n");
		return;
	}

	//1.根据 choice 解析得到数组
	struct json_object *arr;
	arr = json_object_object_get(obj, "choices");
	if (NULL == arr || json_object_get_type(arr) != json_type_array)
	{
		fprintf(stderr, "不是一个数组对象\n");
		return;
	}

	struct json_object *first_choice;//从数组中获取第一个元素
	first_choice = json_object_array_get_idx(arr, 0);
	
	struct json_object *message;//从第一个元素中获取消息对象
	message = json_object_object_get(first_choice, "message");

	struct json_object *cont;//从消息对象中获取内容对象
	cont = json_object_object_get(message, "content");

	strcpy(content, json_object_get_string(cont));//将内容拷贝到content中

	json_object_put(obj);
}

int main(int argc, char *argv[])
{
	if (argc != 2)
		return -1;

	char command[1024] = {0};
	//将执行.sh脚本的命令保存到command中
	sprintf(command, "/home/qwen/qwen.sh %s 2>/dev/null", argv[1]);

	FILE *fp = popen(command, "r");//打开文件读取，即执行.sh脚本，返回文件指针
	if (NULL == fp)
	{
		perror("popen");
		return -1;
	}

	char buf[2048] = {0};
	fgets(buf, sizeof(buf), fp);//读取fp中的数据到buf中

	//printf("--> %s\n", buf);

	pclose(fp);//关闭文件流

	char content[1024] = {0};
	parse_json(buf, content);//解析json对象，将buf中的json对象解析为字符串，保存到content中

	if (strlen(content) > 0)
	{
		printf("-->%s\n", content);

		//打开管道
		int tts_fd = open("/home/fifo/tts_fifo", O_WRONLY);
		if (-1 == tts_fd)
		{
			perror("open");
			return -1;
		}

		//写入数据，供tts合成语音并播放
		if (write(tts_fd, content, strlen(content)) == -1)
		{
			perror("write");
		}

		close(tts_fd);//关闭管道
	}

	return 0;
}
