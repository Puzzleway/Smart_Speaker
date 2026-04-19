#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include<pthread.h>  // 添加pthread头文件
#include<json-c/json.h>

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

void *recv_from_client(void *arg)
{
    int clientfd = *(int *)arg;
    char buf[1024] = {0};
    int len = 0;
    size_t size = 0;
    while(1)
    {
        while(1)
        {
            size += recv(clientfd, buf+size, 4-size, 0);//头4字节表示数据长度,第二个参数是接收数据的缓冲区指针,第三个参数是接收数据的长度,第四个参数是接收数据的方式,输入参数0表示默认接收方式
            if(size >= 4)
            {
                break;//接收到了完整的头部，跳出循环
            }
        }
        size = 0;//重置size，准备接收数据
        
        len = *(int *)buf;//从头部获取数据长度,注意buf是char类型的指针，需要强制转换为int类型的指针，然后解引用获取数据长度
        memset(buf, 0, sizeof(buf));//清空缓冲区,准备接收数据
        printf("recv data length: %d\n", len);
        while(1)
        {
            size += recv(clientfd, buf+size, len-size, 0);//接收数据，直到接收完整
            if(size >= len)
            {
                break;//接收到了完整的数据，跳出循环
            }
        }
        printf("recv data: %s\n", buf);

        //把字符串转换成json对象
		struct json_object *obj = json_tokener_parse(buf);
		struct json_object *val ;
        json_object_object_get_ex(obj, "cmd",&val);
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

		memset(buf, 0, sizeof(buf));
		size = 0;
	
    }
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

    struct sockaddr_in client_addr;//客户端地址结构
        socklen_t client_addr_len = sizeof(client_addr);//客户端地址结构长度
        int clientfd = accept(socketfd, (struct sockaddr *)&client_addr, &client_addr_len);//接受客户端连接
        if(clientfd < 0)
        {//接受连接失败，返回-1
            perror("accept");
            return -1;
        }
    printf("client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    pthread_t tid;
    //接收客户端发送的数据
    pthread_create(&tid, NULL, recv_from_client, (void *)&clientfd);
    // sleep(2);// 等待2秒
    //测试客户端对app命令的处理
    // struct json_object *obj = json_object_new_object();
    // json_object_object_add(obj, "cmd", json_object_new_string("app_start"));
    // send_to_client(clientfd, obj);//向客户端发送数据，通知客户端开始播放
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_mode_circle"));
    // send_to_client(clientfd, obj);
    // sleep(5);
    // json_object_object_add(obj, "cmd", json_object_new_string("app_next"));
    // send_to_client(clientfd, obj);//向客户端发送数据，通知客户端下一首
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_mode_sequence"));
    // send_to_client(clientfd, obj);
    // sleep(5);
    // json_object_object_add(obj, "cmd", json_object_new_string("app_add_volume"));
    // send_to_client(clientfd, obj);
    // json_object_object_add(obj, "cmd", json_object_new_string("app_add_volume"));
    // send_to_client(clientfd, obj);
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_next"));
    // send_to_client(clientfd, obj);//向客户端发送数据，通知客户端下一首
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_pause"));
    // send_to_client(clientfd, obj);//向客户端发送数据，通知客户端暂停播放
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_continue"));
    // send_to_client(clientfd, obj);
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_reduce_volume"));
    // send_to_client(clientfd, obj);
    // json_object_object_add(obj, "cmd", json_object_new_string("app_reduce_volume"));
    // send_to_client(clientfd, obj);
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_prev"));
    // send_to_client(clientfd, obj);
    // sleep(5);// 等待
    // json_object_object_add(obj, "cmd", json_object_new_string("app_stop"));
    // send_to_client(clientfd, obj);//向客户端发送数据，通知客户端停止播放
    // json_object_put(obj);//释放json对象


    while(1)
    {
        
        
    }

    close(clientfd);//关闭客户端连接
    close(socketfd);//关闭socket

    return 0;

}