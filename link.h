/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:29:58
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-26 20:47:19
 * @FilePath: \smart_speaker\link.h
 * @Description:
 * */
#ifndef _LINK_H_
#define _LINK_H_

//定义双向链表节点结构体
typedef struct Node {
    char music_name[128];
    struct Node *next;
    struct Node *prev;
} Node, *Link;


int init_link(void);//初始化链表
int link_create_list(const char *s);//创建链表
void link_clear_list(void);//清空链表
void link_traverse_list(void);//遍历链表
int link_insert_elem(const char *name);//插入元素
int link_find_next(int mode, char *cur_name,char *next_name);//查找下一首歌
int link_find_prev(int mode, char *cur_name,char *prev_name);
Node *link_find_tail(Node *p);
Node *link_find_node_by_ref(const char *cur_ref);
int link_full_path_by_basename(const char *basename, char *full_out);

#endif