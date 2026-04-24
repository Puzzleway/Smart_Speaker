#ifndef _PLAYER_H
#define _PLAYER_H

#include <sys/types.h>
#include <unistd.h>

#define SHMKEY   1000
#define SHMSIZE  4096

//信号量key
#define SEMKEY   1234

//定义播放模式
#define SEQUENCE   1
#define CIRCLE     2

#define ONLINE_MODE    1
#define OFFLINE_MODE   2

#define ONLINE_URL   "http://47.94.80.54/music/"
#define OFFLINE_URL  "/mnt/usb/"

union semun {
	int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET*/
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO(Linux-specific) */
};

//共享内存的数据（结构体）
typedef struct Shm
{
	char cur_music[128];
	char cur_singer[128];
	int cur_mode;
	pid_t parent_pid;
	pid_t child_pid;
	pid_t grand_pid;
}Shm;

int init_shm();

int init_sem();

void player_start_play();

void player_play_music(char *name);

void child_process(char *name);

void parent_get_shm(Shm *s);

void child_quit_process(int sig);

void player_stop_play();

void player_continue_play();

void player_suspend_play();

void player_next_play();

void player_prior_play();

void player_volume_down();

void player_volume_up();

void player_set_mode(int mode);

void player_sem_v();

void player_sem_p();

int init_fifo();

void player_singer_play(const char *name);

void player_tts(const char *msg);

void player_stop_tts();

void player_change_voice();

int player_offline_mode();

#endif
