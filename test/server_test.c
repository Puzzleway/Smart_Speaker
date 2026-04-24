#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include<pthread.h>  // 添加pthread头文件
#include<json-c/json.h>
#include<stdlib.h>
#include<errno.h>

//测试服务端
//测试socket连接是否成功

void send_to_client(int fd, struct json_object *obj)
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

    printf("send data: %s\n", data);
}

static int recv_exact(int fd, void *buf, size_t len)
{
    size_t recved = 0;
    while (recved < len)
    {
        ssize_t n = recv(fd, (char *)buf + recved, len - recved, 0);
        if (n == 0)
        {
            return 0; // 对端关闭连接
        }
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        recved += (size_t)n;
    }
    return 1;
}

void *recv_from_client(void *arg)
{
    int clientfd = *(int *)arg;
    free(arg);
    char buf[1024] = {0};
    int len = 0;

    // 每次新连接先主动下发一条测试命令
    struct json_object *hello = json_object_new_object();
    json_object_object_add(hello, "cmd", json_object_new_string("app_get_music"));
    send_to_client(clientfd, hello);
    json_object_put(hello);

    while(1)
    {
        int ret = recv_exact(clientfd, &len, sizeof(int));
        if (ret <= 0)
        {
            printf("client disconnected(fd=%d)\n", clientfd);
            break;
        }

        if (len <= 0 || len >= (int)sizeof(buf))
        {
            printf("invalid packet length: %d, close client(fd=%d)\n", len, clientfd);
            break;
        }

        memset(buf, 0, sizeof(buf));
        printf("recv data length: %d\n", len);

        ret = recv_exact(clientfd, buf, (size_t)len);
        if (ret <= 0)
        {
            printf("client disconnected while receiving body(fd=%d)\n", clientfd);
            break;
        }
        printf("recv data: %s\n", buf);

        //把字符串转换成json对象
		struct json_object *obj = json_tokener_parse(buf);
        if (obj == NULL)
        {
            printf("invalid json from client(fd=%d)\n", clientfd);
            continue;
        }

		struct json_object *val = NULL;
        if (!json_object_object_get_ex(obj, "cmd", &val))
        {
            json_object_put(obj);
            continue;
        }

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

			// json_object_array_add(array, json_object_new_string("其他/1.mp3"));
			// json_object_array_add(array, json_object_new_string("其他/2.mp3"));
			// json_object_array_add(array, json_object_new_string("其他/3.mp3"));
			// json_object_array_add(array, json_object_new_string("其他/4.mp3"));
			// json_object_array_add(array, json_object_new_string("其他/5.mp3"));

			json_object_object_add(snd_obj, "music", array);

			send_to_client(clientfd , snd_obj);

            json_object_put(snd_obj);
		}

        json_object_put(obj);
    }
    close(clientfd);
    return NULL;
}
int main()
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);//tcp协议,流式socket
    if(socketfd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));//设置socket选项，允许地址重用

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));//清零

    server_addr.sin_family = AF_INET;//ipv4协议
    server_addr.sin_port = htons(8888);//端口号
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器ip地址,先进行本地回环测试

    if(bind(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)//绑定socket和地址结构
    {//绑定失败，返回-1
        perror("bind");
        return -1;
    }
    if(listen(socketfd, 10) < 0)//监听socket，最大连接数为10
    {//监听失败，返回-1
        perror("listen");
        return -1;
    }
    printf("server is listening...\n");

    while(1)
    {
        struct sockaddr_in client_addr;//客户端地址结构
        socklen_t client_addr_len = sizeof(client_addr);//客户端地址结构长度
        int clientfd = accept(socketfd, (struct sockaddr *)&client_addr, &client_addr_len);//接受客户端连接
        if(clientfd < 0)
        {
            perror("accept");
            continue;
        }

        printf("client connected: %s:%d\n",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        int *pfd = (int *)malloc(sizeof(int));
        if (pfd == NULL)
        {
            perror("malloc");
            close(clientfd);
            continue;
        }
        *pfd = clientfd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, recv_from_client, (void *)pfd) != 0)
        {
            perror("pthread_create");
            close(clientfd);
            free(pfd);
            continue;
        }
        pthread_detach(tid);
    }
    close(socketfd);//关闭socket

    return 0;

}