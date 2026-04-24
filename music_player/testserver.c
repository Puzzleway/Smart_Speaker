#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <json/json.h>

void server_send_data(int fd, struct json_object *obj)
{
	char buf[1024] = {0};
	int len = 0;

	//把json对象转换成字符串
	const char *data = json_object_to_json_string(obj);
	if (NULL == data)
	{
		printf("NOT JSON OBJECT!\n");
		return;
	}

	len = strlen(data);
	memcpy(buf, &len, 4);
	memcpy(buf + 4, data, len);

	if (send(fd, buf, len + 4, 0) == -1)
	{
		perror("send server");
		return;
	}
}

void *recv_client(void *arg)
{
	int fd = *(int *)arg;
	char buf[1024] = {0};
	int len = 0;
	size_t size = 0;

	while (1)
	{
		while (1)
		{
			size += recv(fd, buf + size, sizeof(int) - size, 0);
			if (size >= sizeof(int))
				break;
		}

		size = 0;
		len = *(int *)buf;
		memset(buf, 0, sizeof(buf));
		printf("收到 %d 字节：", len);

		while (1)
		{
			size += recv(fd, buf + size, len - size, 0);
			if (size >= len)
				break;
		}

		printf("%s\n", buf);

		//把字符串转换成json对象
		struct json_object *obj = json_tokener_parse(buf);
		struct json_object *val = json_object_object_get(obj, "cmd");
		if (!strcmp("get_music_list", json_object_get_string(val)))
		{
			//返回音乐数据
			struct json_object *snd_obj = json_object_new_object();

			json_object_object_add(snd_obj, "cmd", json_object_new_string("reply_music"));

			struct json_object *array = json_object_new_array();
			json_object_array_add(array, json_object_new_string("其他/以后的以后.mp3"));
			json_object_array_add(array, json_object_new_string("其他/倾国倾城.mp3"));
			json_object_array_add(array, json_object_new_string("其他/童话.mp3"));
			json_object_array_add(array, json_object_new_string("其他/那些年.mp3"));
			json_object_array_add(array, json_object_new_string("其他/一直想着他.mp3"));

			/*json_object_array_add(array, json_object_new_string("其他/1.mp3"));
			json_object_array_add(array, json_object_new_string("其他/2.mp3"));
			json_object_array_add(array, json_object_new_string("其他/3.mp3"));
			json_object_array_add(array, json_object_new_string("其他/4.mp3"));
			json_object_array_add(array, json_object_new_string("其他/5.mp3"));*/

			json_object_object_add(snd_obj, "music", array);

			server_send_data(fd, snd_obj);
		}

		memset(buf, 0, sizeof(buf));
		size = 0;
	}
}

int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sockfd)
	{
		perror("socket");
		return -1;
	}

	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in server_info;
	memset(&server_info, 0, sizeof(server_info));

	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(8008);
	server_info.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(sockfd, (struct sockaddr *)&server_info, sizeof(server_info)) == -1)
	{
		perror("bind");
		return -1;
	}

	if (listen(sockfd, 10) == -1)
	{
		perror("listen");
		return -1;
	}

	struct sockaddr_in client_info;
	int len = sizeof(client_info);

	int fd = accept(sockfd, (struct sockaddr *)&client_info, &len);
	if (-1 == fd)
	{
		perror("accept");
		return -1;
	}

	printf("接受客户端的连接 %d\n", fd);

	pthread_t tid;
	pthread_create(&tid, NULL, recv_client, &fd);

	sleep(2);

	/*struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "cmd", json_object_new_string("app_get_music"));
	server_send_data(fd, obj);

	sleep(5);

	json_object_object_add(obj, "cmd", json_object_new_string("app_circle"));
	server_send_data(fd, obj);

	sleep(5);

	json_object_object_add(obj, "cmd", json_object_new_string("app_circle"));
	server_send_data(fd, obj);*/

	while (1)
	{
		sleep(1);
	}

	close(fd);
	close(sockfd);
	
	return 0;
}
