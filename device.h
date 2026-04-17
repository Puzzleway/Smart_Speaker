/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-22 11:40:07
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-22 12:33:14
 * @FilePath: \smart_speaker\device.h
 * @Description: 
 */
#ifndef _DEVICE_H_
#define _DEVICE_H_

// #define DEVICE_NAME "hw:AudioPCI"  //虚拟机ALSA设备名称
#define DEVICE_NAME "hw:audiocodec"  //开发板ALSA设备名称
// #define DEVICE_ELEMENT "Master"   //虚拟机ALSA设备元素名称
#define DEVICE_ELEMENT "lineout volume"   //开发板ALSA设备元素名称


typedef enum{//按键状态枚举
    STATE_IDLE,
    STATE_FIRST_PRESSED,
    STATE_FIRST_RELEASED,
    STATE_SECOND_PRESSED
}BUTTON_STATE;

int device_set_volume(int volume);
int device_get_volume(int *volume);
int init_button(void);
void device_read_button(void);
void device_button_timer_handler(int sig);

#endif