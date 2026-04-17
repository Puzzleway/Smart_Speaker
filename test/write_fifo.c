/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:11:23
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-21 17:15:52
 * @FilePath: \smart_speaker\write_fifdo.c
 * @Description: 管道写入命令控制mplayer播放音乐的原型函数
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(){
    int fd = open("cmd_fifo", O_WRONLY);
    if(fd == -1){
        perror("open");
        return -1;
    }
    char buf[1024];
    char ch = '\n';
    while(1){
        scanf("%s", buf,sizeof(buf));
        strncat(buf, &ch, 1);
        if(write(fd, buf, strlen(buf)) == -1){
            perror("write");
            return -1;
        }
    }
    close(fd);
    return 0;
}