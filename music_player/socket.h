#ifndef _SOCKET_H
#define _SOCKET_H

//#define PORT   8008
//#define IP     "127.0.0.1"
#define IP     "47.94.80.54"
#define PORT   8000

int init_socket();
void socket_recv_data(char *msg);
void socket_get_music(const char *singer);
void socket_update_music(int);
void socket_start_play();
void socket_stop_play();
void socket_continue_play();
void socket_suspend_play();
void socket_next_play();
void socket_prior_play();
void socket_volume_down();
void socket_volume_up();
void socket_set_mode(int mode);
void socket_upload_music();
void socket_disconnect();

#endif
