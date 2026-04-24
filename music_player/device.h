#ifndef _DEVICE_H
#define _DEVICE_H

//#define CARD_NAME "hw:AudioPCI"
#define CARD_NAME "hw:audiocodec"
#define ELEM_NAME "lineout volume"
//#define ELEM_NAME "Master"

typedef enum{
    STATE_IDLE,
    STATE_FIRST_PRESS,
    STATE_FIRST_RELEASE,
    STATE_SECOND_PRESS
}BUTTON_STATE;

int device_set_volume(int volume);
int device_get_volume(int *v);
int init_button();
void button_handler(int s);
void device_read_button();


#endif

