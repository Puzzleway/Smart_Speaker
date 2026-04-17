/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-22 11:03:30
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-22 11:32:13
 * @FilePath: \smart_speaker\volume.c
 * @Description: 这是获取音量、设置音量的实现
 */
#include<stdio.h>
#include<alsa/asoundlib.h>

int main()
{
    snd_mixer_t *mixer;
    // 打开混音器
    if(snd_mixer_open(&mixer, 0) < 0) {
        fprintf(stderr, "Error opening mixer\n");
        return -1;
    }
    // 连接混音器
    if(snd_mixer_attach(mixer, "hw:AudioPCI") < 0) {
        fprintf(stderr, "Error attaching mixer\n");
        return -1;
    }
    // 注册混音器
    if(snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
        fprintf(stderr, "Error registering mixer\n");
        return -1;
    }
    // 加载混音器
    if(snd_mixer_load(mixer) < 0) {
        fprintf(stderr, "Error loading mixer\n");
        return -1;
    }
    // 设置混音器元素id
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);// 默认索引0
    snd_mixer_selem_id_set_name(sid, "Master");
    // 获取混音器元素
    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, sid);
    if(elem == NULL) {
        fprintf(stderr, "Error finding mixer element\n");
        return -1;
    }
    // 获取当前音量范围
    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    // 设置当前音量
    long value = 50; // 设置为50%
    value = value *(max - min) / 100 + min; // 将百分比转换为实际音量值
    snd_mixer_selem_set_playback_volume_all(elem, value);
    printf("Volume set to %ld\n", value);

    // 关闭混音器
    snd_mixer_close(mixer);
    return 0;
}

