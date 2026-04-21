/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:48:59
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-04-11 23:30:15
 * @FilePath: \smart_speaker\player.c
 * @Description: 
 */
#include<stdio.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/shm.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<signal.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/sem.h>
#include<errno.h>
#include<time.h>


#include "player.h"
#include "link.h"
#include "socket.h"
#include "device.h"

int g_shmid = 0; //共享内存的id
int g_semid ; //信号量的id
int g_start_flag = 0; //播放标志，0表示未开始播放，1表示正在播放
int g_volume = 50; //音量，范围0-100
int g_pause_flag = 0; //暂停标志，0表示未暂停，1表示已暂停
int g_device_mode = ONLINE_MODE;
int g_asrfd = 0; //asr_fifo的文件描述符
int g_ttsfd = 0; //tts_fifo的文件描述符

extern Node *g_music_head;
extern fd_set g_readfds;
extern int g_maxfd;

int init_sem()
{
    g_semid = semget(SEM_KEY, 1, IPC_CREAT|IPC_EXCL|0666);//创建信号量
    if(g_semid == -1)
    {
        if (errno == EEXIST)
        {
            g_semid = semget(SEM_KEY, 1, 0666);
            if (g_semid == -1)
            {
                perror("semget attach");
                return -1;
            }
            return 0;
        }
        perror("semget create");
        return -1;
    }
    union semun sem_union;//定义一个联合体，用于初始化信号量
    sem_union.val = 1;
    if(semctl(g_semid, 0, SETVAL, sem_union) == -1)//初始化,对第0个信号量的值为1
    {
        perror("semctl");
        return -1;
    }
    return 0;
}
/**
 * @brief 对播放信号量执行P操作（等待操作）
 * 
 * 该函数通过系统V信号量机制实现同步控制，对全局信号量g_semid的第0个信号量
 * 执行P操作，将信号量值减1。如果信号量当前值为0，则进程会被阻塞直到信号量
 * 值大于0为止。进程结束时会自动释放该操作。
 * 
 * @return 无返回值，若操作失败则程序异常退出
 */
void player_sem_p()
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;//对第0个信号量操作
    sem_b.sem_op = -1;//P操作,即将信号量的值减1，如果信号量的值为0，则阻塞等待
    sem_b.sem_flg = SEM_UNDO;//进程结束时自动释放
    if(semop(g_semid, &sem_b, 1) == -1)
    {
        perror("semop P");
        exit(-1);
    }
}
/**
 * @brief 对播放信号量执行V操作（释放操作）
 * 
 * 该函数通过系统V信号量机制实现同步控制，对全局信号量g_semid的第0个信号量
 * 执行V操作，将信号量值加1。如果信号量的值为0，则唤醒一个等待该信号量的进程。
 * 
 * @return 无返回值，若操作失败则程序异常退出
 */
void player_sem_v()
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;//对第0个信号量操作
    sem_b.sem_op = 1;//V操作,即将信号量的值加1，如果有进程在等待该信号量，则唤醒一个等待的进程
    sem_b.sem_flg = SEM_UNDO;//进程结束时自动释放
    if(semop(g_semid, &sem_b, 1) == -1)
    {
        perror("semop V");
        exit(-1);
    }
}
//初始化共享内存,与播放有关
int init_shm()
{
    SHM_DATA shm_data; //共享内存中的数据结构
    memset(&shm_data, 0, sizeof(SHM_DATA)); //初始化共享内存中的数据结构
    g_shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT|IPC_EXCL|0666);//创建共享内存
    if(g_shmid == -1)
    {
        if (errno == EEXIST)
        {
            g_shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
            if (g_shmid == -1)
            {
                perror("shmget attach");
                return -1;
            }
            void *addr = shmat(g_shmid, NULL, 0);
            if (addr != (void *)-1)
            {
                SHM_DATA *ps = (SHM_DATA *)addr;
                ps->prev_leave_basename[0] = '\0';
                ps->manual_seek_at = 0;
                ps->post_seek_retry_used = 0;
                shmdt(addr);
            }
            return 0;
        }
        perror("shmget create");
        return -1;
    }

    void *addr = shmat(g_shmid, NULL, 0); //将共享内存映射到当前进程的地址空间
    if(addr == (void *)-1)
    {
        perror("shmat");
        return -1;
    }
    shm_data.parent_pid = getpid();//获取进程号
    shm_data.mode = SEQUENCE; //默认播放模式为顺序播放
    memcpy(addr, &shm_data, sizeof(SHM_DATA)); //将数据结构写入共享内存
    shmdt(addr); //解除共享内存的映射
    return 0;
}

void parent_get_shm_data(SHM_DATA *shm_data)
{
    void *addr = shmat(g_shmid, NULL, 0); //将共享内存映射到当前进程的地址空间
    if(addr == (void *)-1)
    {
        perror("shmat");
        return;
    }
    memcpy(shm_data, addr, sizeof(SHM_DATA)); //从共享内存中读取数据结构
    shmdt(addr); //解除共享内存的映射
}

void parent_set_shm_data(SHM_DATA shm_data)
{
    void *addr = shmat(g_shmid, NULL, 0); //将共享内存映射到当前进程的地址空间
    if(addr == (void *)-1)
    {
        perror("shmat");
        return;
    }
    memcpy(addr, &shm_data, sizeof(SHM_DATA)); //将数据结构写入共享内存
    shmdt(addr); //解除共享内存的映射

}

//开始播放
void player_start_play(void)
{
    if(g_start_flag == 1)return;
    SHM_DATA sd;
    parent_get_shm_data(&sd);
    sd.prev_leave_basename[0] = '\0';
    sd.manual_seek_at = 0;
    sd.post_seek_retry_used = 0;
    player_sem_p();
    parent_set_shm_data(sd);
    player_sem_v();

    char music_name[128] = {0};
    strcpy(music_name,g_music_head->next->music_name);
    g_start_flag = 1;
    player_play_music(music_name);
    
}

// 播放音乐的函数
void player_play_music(char *music_name)
{
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        signal(SIGUSR1,child_quit_process);
        child_process(music_name);
        exit(0);
    } else {
        // 父进程：
        return;
    }
}

void child_quit_process(int sig)
{
    g_start_flag = 0; //修改标志位
}

void child_process(char*music_name)
{
    while(g_start_flag)//继承自父进程
    {
        pid_t pid = fork();
        if(pid == -1)
        {
            perror("CHILD FORK");
            break;
        }else if(pid == 0)//孙进程
        {
            //修改共享内存
            SHM_DATA s;
            parent_get_shm_data(&s);
            if(strlen(music_name) == 0)//当第二次创建孙进程并进入
            {
                time_t now = time(NULL);
                int fast_after_manual = (s.manual_seek_at != 0
                    && (now - s.manual_seek_at) <= 2);

                if (fast_after_manual && !s.post_seek_retry_used
                    && link_full_path_by_basename(s.music_name, music_name) == 0)
                {
                    s.post_seek_retry_used = 1;
                    player_sem_p();
                    parent_set_shm_data(s);
                    player_sem_v();
                }
                else
                {
                    s.post_seek_retry_used = 0;
                    if (link_find_next(s.mode, s.music_name, music_name) == -1)
                    {
                        printf("全部歌曲播放完毕······\n");
                        kill(s.parent_pid,SIGUSR1);
                        kill(s.child_pid,SIGUSR1);
                        usleep(100000);
                        exit(0);
                    }

                    parent_get_shm_data(&s);
                    if (s.prev_leave_basename[0])
                    {
                        const char *sl = strrchr(music_name, '/');
                        char cand_bn[128];
                        strncpy(cand_bn, sl ? sl + 1 : music_name, sizeof(cand_bn) - 1);
                        cand_bn[sizeof(cand_bn) - 1] = '\0';
                        if (strcmp(cand_bn, s.prev_leave_basename) == 0)
                        {
                            Node *nb = link_find_node_by_ref(music_name);
                            if (nb && nb->next != NULL)
                                strcpy(music_name, nb->next->music_name);
                        }
                        s.prev_leave_basename[0] = '\0';
                        player_sem_p();
                        parent_set_shm_data(s);
                        player_sem_v();
                    }
                }
            }

            s.child_pid = getppid();//获取父进程号
            s.grand_pid = getpid(); //获取当前进程号
            
            if(g_device_mode == ONLINE_MODE)
            {
                const char *p = music_name;
                while(*p != '/')
                {
                    p++;
                }
                strncpy(s.singer,music_name,p-music_name);
                strcpy(s.music_name,p+1);//
                
            }
            player_sem_p();//P操作，进入临界区
            parent_set_shm_data(s);
            player_sem_v();//V操作，退出临界区
            char music_path[128]={0};
            if (g_device_mode == ONLINE_MODE)
				strcpy(music_path, ONLINE_URL);
			else if (g_device_mode == OFFLINE_MODE)
				strcpy(music_path, OFFLINE_URL);
                
			strcat(music_path, music_name);

			char *arg[7] = {0};

			arg[0] = "mplayer";
			arg[1] = music_path;
			arg[2] = "-slave";
			arg[3] = "-quiet";
			arg[4] = "-input";
			arg[5] = "file=/home/fifo/cmd_fifo";
			arg[6] = NULL;

            if(execv("/usr/bin/mplayer",arg)== -1)
            {
                fprintf(stderr,"[ERROR]MPLAYER failed to start\n");
                kill(s.child_pid,SIGUSR1);
                exit(-1);
            }
        }else
        {//孙进程结束，进入子进程
            //注意不同进程有不同的地址空间
            memset(music_name,0,strlen(music_name));//清空，防止新的孙进程继承旧的歌曲名
            int status;
            wait(&status);
        }
    }
}

void player_stop_play(void)
{
    SHM_DATA shm_data;
    parent_get_shm_data(&shm_data);
    kill(shm_data.child_pid,SIGUSR1);

    write_fifo("quit\n");

    int status;

    waitpid(shm_data.child_pid,&status,0);
    g_start_flag = 0;
    g_pause_flag = 0;
}

void player_pause_play(void)
{
    if(g_start_flag == 0 || g_pause_flag == 1)return;
    write_fifo("pause\n");
    g_pause_flag = 1;
    printf("------暂停播放------\n");
}

void player_continue_play(void)
{
    if(g_start_flag == 0 || g_pause_flag == 0)return;
    write_fifo("pause\n");
    g_pause_flag = 0;
    printf("------继续播放------\n");
}

void player_next_play(void)
{
    if(g_start_flag == 0)return;
    SHM_DATA shm_data;
    parent_get_shm_data(&shm_data);
    char music_name[128] = {0};
    if(link_find_next(shm_data.mode,shm_data.music_name,music_name) == -1)
    {
        if(g_device_mode == ONLINE_MODE)
        {
            player_stop_play();
            link_clear_list();
            socket_get_music(shm_data.singer);
            player_start_play();
            return;
        }else if(g_device_mode == OFFLINE_MODE)
        {
            printf("全部歌曲播放完毕······\n");
            player_stop_play();
            return;
        }
    }
    if(g_device_mode == ONLINE_MODE)
    {
        const char *p = music_name;
        while(*p != '/')
        {
            p++;
        }
        strcpy(shm_data.music_name,p+1);
    }else if(g_device_mode == OFFLINE_MODE)
    {
        strcpy(shm_data.music_name,music_name);
    }
    shm_data.manual_seek_at = time(NULL);
    shm_data.post_seek_retry_used = 0;
    shm_data.prev_leave_basename[0] = '\0';
    player_sem_p();//P操作，进入临界区
    parent_set_shm_data(shm_data);//更新修改共享内存
    player_sem_v();//V操作，离开临界区
    char music_path[128]={0};
    char cmd[256]={0};
    if (g_device_mode == ONLINE_MODE)
		strcpy(music_path, ONLINE_URL);//播放在线歌曲
	else if (g_device_mode == OFFLINE_MODE)
		strcpy(music_path, OFFLINE_URL);
    strcat(music_path, music_name);
    sprintf(cmd,"loadfile %s\n",music_path);
    write_fifo(cmd);
    g_start_flag = 1;
    g_pause_flag = 0;
}

void player_prev_play(void)
{
    if(g_start_flag == 0)return;// 没有播放
    SHM_DATA shm_data;
    parent_get_shm_data(&shm_data);
    char music_name[128] = {0};
    if(link_find_prev(shm_data.mode,shm_data.music_name,music_name) == -1)// 没有上一首
    {
        if(g_device_mode == ONLINE_MODE)// 在线模式
        {
            player_stop_play();// 停止播放
            link_clear_list();// 清空链表
            socket_get_music(shm_data.singer);// 获取歌手音乐列表
            player_start_play();// 开始播放
            return;
        }else if(g_device_mode == OFFLINE_MODE)
        {
            printf("没有上一首······\n");// 没有上一首
            player_stop_play();// 停止播放
            return;
        }
    }
    {
        const char *ls = strrchr(shm_data.music_name, '/');
        strncpy(shm_data.prev_leave_basename,
            ls ? ls + 1 : shm_data.music_name,
            sizeof(shm_data.prev_leave_basename) - 1);
        shm_data.prev_leave_basename[sizeof(shm_data.prev_leave_basename) - 1] = '\0';
    }
    if(g_device_mode == ONLINE_MODE)
    {
        const char *p = music_name;// 获取音乐名称
        while(*p != '/')
        {
            p++;
        }
        strcpy(shm_data.music_name,p+1);// 更新共享内存中的音乐名称
    }
    else if(g_device_mode == OFFLINE_MODE)
    {
        strcpy(shm_data.music_name,music_name);
    }
    shm_data.manual_seek_at = time(NULL);
    shm_data.post_seek_retry_used = 0;
    player_sem_p();//P操作，进入临界区
    parent_set_shm_data(shm_data);
    player_sem_v();//V操作，离开临界区
    char music_path[128]={0};
    char cmd[256]={0};
    if (g_device_mode == ONLINE_MODE)
		strcpy(music_path, ONLINE_URL);
    else if (g_device_mode == OFFLINE_MODE)
		strcpy(music_path, OFFLINE_URL);
    strcat(music_path, music_name);
    sprintf(cmd,"loadfile %s\n",music_path);
    write_fifo(cmd);
    g_start_flag = 1;
    g_pause_flag = 0;
}

void player_add_volume()
{
    int volume;
    device_get_volume(&volume);
    if(volume < 90)
    {
        volume = volume+10;
    }
    if(volume >= 90)
    {
        volume = 100;
        printf("音量已最大\n");
    }
    device_set_volume(volume);
    printf("当前音量：%d\n",volume);    
}

 void player_reduce_volume()
{
    int volume;
    device_get_volume(&volume);
    if(volume > 10)
    {
        volume = volume-10;
    }
    if(volume <= 10)
    {
        volume = 0;
        printf("音量已最小\n");
    }
    device_set_volume(volume);
    printf("当前音量：%d\n",volume);    
}

void player_set_mode(int mode)
{
    SHM_DATA shm_data;
    parent_get_shm_data(&shm_data);
    shm_data.mode = mode;
    player_sem_p();//P操作，进入临界区
    parent_set_shm_data(shm_data);
    player_sem_v();//V操作，离开临界区
}
int write_fifo(const char* cmd)
{
    int fd = open("/home/fifo/cmd_fifo",O_WRONLY);
    if(fd == -1)
    {
        perror("OPEN FIFO\n");
        return -1;
    }
    if(write(fd,cmd,strlen(cmd))==-1)
    {
        perror("WRITE FIFO\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int init_fifo()
{
    g_asrfd = open("/home/fifo/asr_fifo",O_RDONLY);
    g_ttsfd = open("/home/fifo/tts_fifo",O_WRONLY);
    if(g_asrfd == -1 || g_ttsfd == -1)
    {
        perror("open /home/fifo/asr_fifo or /home/fifo/tts_fifo");
        return -1;
    }
    FD_SET(g_asrfd, &g_readfds);
    // 语音识别是读端，tts是写端，写端不需要监听
    if(g_asrfd > g_maxfd)
    {
        g_maxfd = g_asrfd;
    }
    return 0;
}

void player_singer_play(char *singer)
{
    // 结束当前播放，清空链表，获取歌手音乐列表
    // 开始播放
    player_stop_play();
    link_clear_list();
    socket_get_music(singer);
    player_start_play();
    return;
}