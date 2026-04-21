/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:51:17
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-28 20:09:40
 * @FilePath: \smart_speaker\player.h
 * @Description: 
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include<sys/types.h>
#include<unistd.h>
#include<time.h>

#define SHM_KEY 1000//共享内存的key值
#define SEM_KEY 1234//信号量的key值
#define SHM_SIZE 4096//共享内存的大小
#define SEQUENCE 1//顺序播放
#define RANDOM 2//随机播放
#define CIRCLE 3//单曲循环

#define ONLINE_MODE 1
#define OFFLINE_MODE 2

#define ONLINE_URL "http://180.76.142.171/music/"
#define OFFLINE_URL "file:///home/puzzleway/music/"

//
union semun
{
    int val;//信号量的值
    struct semid_ds *buf;//信号量的数据结构
    unsigned short *array;//信号量的数组
    struct seminfo *__buf;//信号量的信息数据结构
};

//共享内存中的数据结构
typedef struct
{
	char music_name[128];//音乐名称
    char singer[128];//歌手
    int mode;//播放模式
    pid_t parent_pid;//父进程的pid
	pid_t child_pid;//子进程的pid
    pid_t grand_pid;//孙进程的pid
    /* 手动上一首离开的那首歌 basename，用于曲终自动接歌时跳过「刚离开的那首」避免弹回 */
    char prev_leave_basename[128];//上一首离开的那首歌的basename
    time_t manual_seek_at;//手动上一首的时间
    int post_seek_retry_used;/* 手动 loadfile 后第一次曲终若为瞬时失败则只重试同一首一次 */
}SHM_DATA;

int init_sem(void);
void player_sem_p(void);
void player_sem_v(void);
int init_shm(void);
void parent_get_shm_data(SHM_DATA *shm_data);
void parent_set_shm_data(SHM_DATA shm_data);
void player_start_play(void);
void player_play_music(char *music_name);
void child_quit_process(int sig);
void child_process(char*music_name);
void player_stop_play(void);
void player_pause_play(void);
void player_continue_play(void);
void player_next_play(void);
void player_prev_play(void);
void player_add_volume(void);
void player_reduce_volume(void);
void player_set_mode(int mode);
int write_fifo(const char* cmd);
int init_fifo(void);
void player_singer_play(char *singer);
void player_tts(const char *text);
#endif