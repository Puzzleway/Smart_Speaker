/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 18:48:20
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-04-12 21:31:14
 * @FilePath: \smart_speaker\socket.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef _SOCKET_H_
#define _SOCKET_H_ 

#define PORT 8888//服务器监听端口号,大于1024，小于65535
#define MY_IP "127.0.0.1"
#define SERVER_IP "192.168.1.100"

int init_socket(void);
void socket_recv_data(char *msg);
void socket_get_music(const char *singer);
void socket_update_music(int sig);
void socket_start_play(void);
void socket_stop_play(void);
void socket_pause_play(void);
void socket_continue_play(void);
void socket_next_play(void);
void socket_prev_play(void);
void socket_add_volume(void);
void socket_reduce_volume(void);
void socket_set_mode(int mode);
void socket_upload_music(void);
void socket_disconnect(void);

#endif