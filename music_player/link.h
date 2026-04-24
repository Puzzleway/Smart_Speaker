#ifndef _LINK_H
#define _LINK_H

//定义双向链表节点
typedef struct Node
{
	char music_name[128];
	struct Node *next;
	struct Node *prior;
}Node;

int init_link();
int link_create_list(const char *s);
int link_insert_elem(const char *name);
void link_traverse_list();
int link_find_next(int mode, char *cur, char *next);
void link_clear_list();
void link_find_prior(const char *cur, char *music);
int link_read_music();

#endif
