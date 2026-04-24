#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/select.h>
#include <sys/mount.h>
#include "socket.h"
#include "link.h"
#include "player.h"
#include "device.h"

int g_shmid = 0;                   //共享内存标识
int g_semid = 0;                   //信号量标识
int g_start_flag = 0;              //是否开始播放
int g_suspend_flag = 0;            //是否暂停
int g_device_mode = ONLINE_MODE;
int g_asrfd = 0;                   //用于语音识别的管道
int g_ttsfd = 0;                   //用于语音合成的管道

extern Node *g_music_head;
extern fd_set READSET;
extern int g_maxfd;

int init_shm()
{
	Shm s;
	memset(&s, 0, sizeof(s));

	g_shmid = shmget(SHMKEY, SHMSIZE, IPC_EXCL | IPC_CREAT);
	if (-1 == g_shmid)
	{
		perror("shmget");
		return -1;
	}

	void *addr = shmat(g_shmid, NULL, 0);
	if ((void *)-1 == addr)
	{
		perror("shmat");
		return -1;
	}

	//初始化共享内存里面的数据
	s.cur_mode = SEQUENCE;
	s.parent_pid = getpid();

	memcpy(addr, &s, sizeof(s));

	shmdt(addr);

	return 0;
}

int init_sem()
{
	g_semid = semget(SEMKEY, 1, IPC_CREAT | IPC_EXCL);
	if(g_semid == -1)
	{
		perror("semget");
		return -1;
	}

	union semun s;
	s.val = 1;
	if (semctl(g_semid, 0, SETVAL, s) == -1)
	{
		perror("semctl");
		return -1;
	}

	return 0;
}

/* "加锁" */
void player_sem_p()
{
	struct sembuf s;

	s.sem_num = 0;
	s.sem_op = -1;
	s.sem_flg = SEM_UNDO;

	if (semop(g_semid, &s, 1) == -1)
	{
		perror("semop");
	}
}

void player_sem_v()
{
	struct sembuf s;

	s.sem_num = 0;
	s.sem_op = 1;
	s.sem_flg = SEM_UNDO;

	if (semop(g_semid, &s, 1) == -1)
	{
		perror("semop");
	}
}

void parent_get_shm(Shm *s)
{
	void *addr = shmat(g_shmid, NULL, 0);
	if ((void *)-1 == addr)
	{
		perror("shmat");
		return;
	}

	memcpy(s, addr, sizeof(Shm));

	shmdt(addr);
}

void parent_set_shm(Shm s)
{
	void *addr = shmat(g_shmid, NULL, 0);
	if ((void *)-1 == addr)
	{
		perror("shmat");
		return;
	}

	memcpy(addr, &s, sizeof(s));

	shmdt(addr);
}

void player_start_play()
{
	if (g_start_flag == 1)
		return;

	char music_name[128] = {0};
	strcpy(music_name, g_music_head->next->music_name);

	g_start_flag = 1;

	player_play_music(music_name);
}

/*用于播放音乐的函数*/
void player_play_music(char *name)
{
	pid_t pid = fork();
	if (-1 == pid)
	{
		perror("PARENT FORK");
		return;
	}
	else if (0 == pid)          //子进程
	{
		signal(SIGUSR1, child_quit_process);
		child_process(name);
		exit(0);
	}
	else                        //父进程
	{
		return;
	}
}

void child_quit_process(int sig)
{
	g_start_flag = 0;     //修改子进程里面的标志位
}

/*子进程*/
void child_process(char *name)
{
	while (g_start_flag)
	{
		pid_t pid = fork();
		if (-1 == pid)
		{
			perror("CHILD FORK");
			break;
		}
		else if (0 == pid)        //孙进程
		{
			Shm s;
			parent_get_shm(&s);

			if (strlen(name) == 0)     //第二次进来
			{
				if (link_find_next(s.cur_mode, s.cur_music, name) == -1)
				{
					if (g_device_mode == OFFLINE_MODE)
					{
						strcpy(name, g_music_head->next->music_name);
					}
					else if (g_device_mode == ONLINE_MODE)
					{
						/*给父子进程发送信号
					 	* 父进程收到信号 请求新的歌曲
					 	* 子进程收到信号 修改标志位*/
						kill(s.parent_pid, SIGUSR1);
						kill(s.child_pid, SIGUSR1);

						usleep(100000);

						exit(0);
					}
				}
			}

			//修改共享内存
			s.child_pid = getppid();
			s.grand_pid = getpid();

			if (g_device_mode == ONLINE_MODE)
			{
				const char *p = name;

				while (*p != '/')
					p++;

				strncpy(s.cur_singer, name, p - name);
				strcpy(s.cur_music, p + 1);
			}
			else if (g_device_mode == OFFLINE_MODE)
			{
				strcpy(s.cur_music, name);
			}

			player_sem_p();

			parent_set_shm(s);

			player_sem_v();

			char music_path[128] = {0};

			if (g_device_mode == ONLINE_MODE)
				strcpy(music_path, ONLINE_URL);
			else if (g_device_mode == OFFLINE_MODE)
				strcpy(music_path, OFFLINE_URL);

			strcat(music_path, name);

			char *arg[7] = {0};

			arg[0] = "mplayer";
			arg[1] = music_path;
			arg[2] = "-slave";
			arg[3] = "-quiet";
			arg[4] = "-input";
			arg[5] = "file=/home/fifo/cmd_fifo";
			arg[6] = NULL;

			if (execv("/usr/bin/mplayer", arg) == -1)
			{
				fprintf(stderr, "[ERROR] MPLAYE启动失败\n");
				
				kill(s.child_pid, SIGUSR1);  //通知子进程修改标志位

				exit(-1);
			}
		}
		else                      //子进程
		{
			//清空子进程的name数组
			memset(name, 0, strlen(name));

			int status;
			wait(&status);
		}
	}
}

int write_fifo(const char *cmd)
{
	int fd = open("/home/fifo/cmd_fifo", O_WRONLY);
	if (-1 == fd)
	{
		perror("OPEN FIFO");
		return -1;
	}

	if (write(fd, cmd, strlen(cmd)) == -1)
	{
		perror("WRITE FIFO");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

void player_stop_play()
{
	if (g_start_flag == 0)
		return;

	//通知子进程结束
	Shm s;
	parent_get_shm(&s);

	kill(s.child_pid, SIGUSR1);

	//结束mplayer
	write_fifo("quit\n");

	//回收子进程资源
	int status;
	waitpid(s.child_pid, &status, 0);

	//修改标志位
	g_start_flag = 0;
	g_suspend_flag = 0;
}

void player_suspend_play()
{
	if (g_start_flag == 0 || g_suspend_flag == 1)
		return;

	write_fifo("pause\n");

	g_suspend_flag = 1;

	printf("-------- 暂停播放 --------\n");
}

void player_continue_play()
{
	if (g_start_flag == 0 || g_suspend_flag == 0)
		return;

	write_fifo("pause\n");

	g_suspend_flag = 0;

	printf("-------- 继续播放 --------\n");
}

void player_next_play()
{
	if (g_start_flag == 0)
		return;

	//读取共享内存，找到当前歌曲
	Shm s;
	parent_get_shm(&s);

	char music[128] = {0};
	//遍历链表，根据当前歌曲找到下一首
	if (link_find_next(SEQUENCE, s.cur_music, music) == -1)
	{
		if (g_device_mode == ONLINE_MODE)
		{
			//结束播放
			player_stop_play();

			//清空链表
        	link_clear_list();

			//请求新的音乐数据
        	socket_get_music(s.cur_singer);

			//开始播放
			player_start_play();

			//通知APP歌曲列表已经更新
			socket_upload_music();

			return;
		}
		else if (g_device_mode == OFFLINE_MODE)
		{
			strcpy(music, g_music_head->next->music_name);
		}
	}

	//更新共享内存
	if (g_device_mode == ONLINE_MODE)
	{
		const char *p = music;

		while (*p != '/')
			p++;

		strcpy(s.cur_music, p + 1);
	}
	else if (g_device_mode == OFFLINE_MODE)
	{
		strcpy(s.cur_music, music);
	}

	player_sem_p();

	parent_set_shm(s);

	player_sem_v();

	//写管道播放新的歌曲
	char music_path[128] = {0};
	char cmd[258] = {0};

	if (g_device_mode == ONLINE_MODE)
	{
		strcpy(music_path, ONLINE_URL);
	}
	else if (g_device_mode == OFFLINE_MODE)
	{
		strcpy(music_path, OFFLINE_URL);
	}
	strcat(music_path, music);

	sprintf(cmd, "loadfile \"%s\"\n", music_path);

	write_fifo(cmd);

	g_start_flag = 1;
	g_suspend_flag = 0;
}


void player_prior_play()
{
	if (g_start_flag == 0)
		return;

	//读取共享内存，找到当前歌曲
	Shm s;
	parent_get_shm(&s);

	char music[128] = {0};
	//遍历链表，根据当前歌曲找到上一首
	link_find_prior(s.cur_music, music);

	//更新共享内存
	if (g_device_mode == ONLINE_MODE)
	{
		const char *p = music;

		while (*p != '/')
			p++;

		strcpy(s.cur_music, p + 1);
	}
	else if (g_device_mode == OFFLINE_MODE)
	{
		strcpy(s.cur_music, music);
	}

	player_sem_p();

	parent_set_shm(s);

	player_sem_v();

	//写管道播放新的歌曲
	char music_path[128] = {0};
	char cmd[258] = {0};

	if (g_device_mode == ONLINE_MODE)
	{
		strcpy(music_path, ONLINE_URL);
	}
	else if (g_device_mode == OFFLINE_MODE)
	{
		strcpy(music_path, OFFLINE_URL);
	}
	strcat(music_path, music);

	sprintf(cmd, "loadfile \"%s\"\n", music_path);

	write_fifo(cmd);

	g_start_flag = 1;
	g_suspend_flag = 0;
}

void player_volume_up()
{
	int volume;

	device_get_volume(&volume);

	if (volume < 90)
		volume += 10;
	else if (volume >= 90 && volume < 100)
		volume = 100;
	else if (volume == 100)
	{
		printf("------- 音量最大 -------\n");
		return;
	}

	device_set_volume(volume);

	printf("------- 音量增加 -------\n");
}

void player_volume_down()
{
	int volume;

	device_get_volume(&volume);

	if (volume > 10)
		volume -= 10;
	else if (volume <= 10 && volume > 0)
		volume = 0;
	else if (volume == 0)
	{
		printf("------- 音量最小 -------\n");
		return;
	}

	device_set_volume(volume);

	printf("------- 音量减小 -------\n");
}

void player_set_mode(int mode)
{
	Shm s;

	parent_get_shm(&s);

	s.cur_mode = mode;

	player_sem_p();

	parent_set_shm(s);

	player_sem_v();

	printf("------- 修改播放模式成功 -------\n");
}

int init_fifo()
{
	g_asrfd = open("/home/fifo/asr_fifo", O_RDONLY);
	if (-1 == g_asrfd)
	{
		perror("open fifo");
		return -1;
	}

	//添加到集合中
	FD_SET(g_asrfd, &READSET);
	g_maxfd = (g_maxfd < g_asrfd) ? g_asrfd : g_maxfd;

	g_ttsfd = open("/home/fifo/tts_fifo", O_WRONLY);
	if (-1 == g_ttsfd)
	{
		perror("open fifo");
		return -1;
	}

	return 0;
}

void player_singer_play(const char *name)
{
	//结束播放
	player_stop_play();

	//清空链表
	link_clear_list();

	//请求新的歌曲
	socket_get_music(name);

	//开始播放
	player_start_play();

	//通知APP歌曲列表更新
	socket_upload_music();
}

void player_tts(const char *msg)
{
	if (write(g_ttsfd, msg, strlen(msg)) == -1)
	{
		perror("write fifo");
	}
}

void player_stop_tts()
{
	//获取tts进程号
	FILE *fp = popen("pgrep tts", "r");
	if (NULL == fp)
	{
		perror("popen");
		return;
	}

	char buf[32] = {0};
	fgets(buf, 32, fp);

	pid_t tts_pid = atoi(buf);

	pclose(fp);

	//发送信号SIGUSR1
	kill(tts_pid, SIGUSR1);
}

void player_change_voice()
{
	//获取tts进程号
	FILE *fp = popen("pgrep tts", "r");
	if (NULL == fp)
	{
		perror("popen");
		return;
	}

	char buf[32] = {0};
	fgets(buf, 32, fp);

	pid_t tts_pid = atoi(buf);

	pclose(fp);

	//发送信号SIGUSR1
	kill(tts_pid, SIGUSR2);

	usleep(100000);

	player_tts("好的，后面我用这个声音跟你交流");
}

void player_switch_asr_mode()
{
	//获取tts进程号
	FILE *fp = popen("pgrep asr", "r");
	if (NULL == fp)
	{
		perror("popen");
		return;
	}

	char buf[32] = {0};
	fgets(buf, 32, fp);

	pid_t asr_pid = atoi(buf);

	pclose(fp);

	//发送信号SIGUSR1
	kill(asr_pid, SIGUSR1);
}

int player_offline_mode()
{
	//判断U盘有没有插上
	char device_name[64] = "/dev/sdb1";

	if (access(device_name, F_OK) != 0)    //文件不存在
	{
		strcpy(device_name, "/dev/sdc1");

		if (access(device_name, F_OK) != 0) 
		{
			strcpy(device_name, "/dev/sda1");

			if (access(device_name, F_OK) != 0) 
			{
				//两个设备文件都不存在
				player_tts("请先插上存储设备");
				return -1;
			}
		}
	}

	//挂载
	//先判断挂载点存不存在
	if (access("/mnt/usb", F_OK) != 0)
	{
		//不存在
		mkdir("/mnt/usb", 0755);
	}

	umount("/mnt/usb");

	if (mount(device_name, "/mnt/usb", "exfat", 0, NULL) != 0)
	{
		player_tts("挂载失败");
		return -1;
	}

	//读取U盘歌曲
	if (link_read_music() == -1)
	{
		player_tts("切换离线模式失败");
		return -1;
	}

	link_traverse_list();
	
	//断开网络连接
	socket_disconnect();

	//通知 asr 进程
	player_switch_asr_mode();

	//回收播放进程
	player_stop_play();

	g_start_flag = 0;
	g_suspend_flag = 0;

	g_device_mode = OFFLINE_MODE;

	player_tts("离线模式切换成功");

	return 0;
}
