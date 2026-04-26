#include <stdio.h>
#include <jsoncpp/json/json.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

int sockfd;

void *receive(void *arg)
{
	char buf[1024] = {0};
	int len;

	while (1)
	{
		recv(sockfd, &len, 4, 0);
		recv(sockfd, buf, len, 4);
		printf("%s\n", buf);
	}
}

void send_server(int sig)
{
	Json::Value v;
	v["cmd"] = "app_info";
	v["appid"] = "1001";
	v["deviceid"] = "0001";

	char buf[1024] = {0};
	string str = Json::FastWriter().write(v);
	int len = str.size();
	memcpy(buf, &len, 4);
	memcpy(buf + 4, str.c_str(), len);

	send(sockfd, buf, len + 4, 0);

	alarm(2);
}

int main()
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_info;
	int len = sizeof(server_info);
	memset(&server_info, 0, len);
	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(8000);
	// server_info.sin_addr.s_addr = inet_addr("172.29.15.17");
	server_info.sin_addr.s_addr = inet_addr("121.41.167.17");

	if (connect(sockfd, (struct sockaddr *)&server_info, len) == -1)
	{
		perror("connect");
		return -1;
	}

	signal(SIGALRM, send_server);//注册信号处理函数，每隔2秒发送一次APP信息到服务器
	send_server(SIGALRM);// 第一次需要主动发送APP信息到服务器

	pthread_t tid;
	pthread_create(&tid, NULL, receive, NULL);//创建一个线程，接收服务器发来的数据

	getchar();//用于阻塞主线程，测试APP各种操作

	Json::Value v;//创建一个Json::Value对象
	v["cmd"] = "app_offline";//设置命令
	v["appid"] = "111";//设置APPID
	v["deviceid"] = "001";//设置设备ID

	char buf[1024] = {0};
	string str = Json::FastWriter().write(v);//将Json::Value对象转换为字符串
	len = str.size();
	memcpy(buf, &len, 4);
	memcpy(buf + 4, str.c_str(), len);

	send(sockfd, buf, len + 4, 0);//发送APP信息到服务器


	while (1);
	return 0;
}
