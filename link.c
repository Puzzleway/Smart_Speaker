/*
 * @Author: puzzleway 17773994382@163.com
 * @Date: 2026-03-21 17:29:09
 * @LastEditors: puzzleway 17773994382@163.com
 * @LastEditTime: 2026-03-26 23:28:55
 * @FilePath: \smart_speaker\link.c
 * @Description: 这是初始化双向音乐播放链表的函数实现文件
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <dirent.h>
#include "link.h"
#include "player.h"


Node *g_music_head = NULL;//音乐播放链表头节点指针

/* 与共享内存中 music_name（多为 basename）对齐：按最后一段文件名比较 */
static void link_copy_basename(const char *path, char *out, size_t outsz)
{
	const char *slash;
	if (!path || !out || outsz == 0)
	{
		if (out && outsz)
			out[0] = '\0';
		return;
	}
	slash = strrchr(path, '/');
	if (slash)
		strncpy(out, slash + 1, outsz - 1);
	else
		strncpy(out, path, outsz - 1);
	out[outsz - 1] = '\0';
}

static int link_refs_equal(const char *stored_path, const char *cur_ref)
{
	char bn_stored[128], bn_ref[128];
	if (!cur_ref || !cur_ref[0])
		return 0;
	link_copy_basename(stored_path, bn_stored, sizeof(bn_stored));
	if (strchr(cur_ref, '/'))
	{
		link_copy_basename(cur_ref, bn_ref, sizeof(bn_ref));
		return strcmp(bn_stored, bn_ref) == 0;
	}
	return strcmp(bn_stored, cur_ref) == 0;
}

Node *link_find_node_by_ref(const char *cur_ref)
{
	Node *p = g_music_head->next;
	while (p)
	{
		if (link_refs_equal(p->music_name, cur_ref))
			return p;
		p = p->next;
	}
	return NULL;
}

int link_full_path_by_basename(const char *basename, char *full_out)
{
	Node *p = g_music_head->next;
	if (!basename || !basename[0] || !full_out)
		return -1;
	while (p)
	{
		if (link_refs_equal(p->music_name, basename))
		{
			strcpy(full_out, p->music_name);
			return 0;
		}
		p = p->next;
	}
	return -1;
}
//链表初始化函数
int init_link()
{
    //链表初始化
    g_music_head = (Node *)malloc(sizeof(Node));//创建头节点
    if(g_music_head == NULL){
        perror("malloc");
        return -1;
    }
    g_music_head->next = NULL;
    g_music_head->prev = NULL;
    return 0;
}

//解析json字符串，把音乐数据放到链表里面
int link_create_list(const char *s)
{
    struct json_object *obj = json_tokener_parse(s);
    if (NULL == obj)
    {
        fprintf(stderr, "[ERROR] 不是JSON对象\n");
        return -1;
    }

    struct json_object *val = NULL;
    if (!json_object_object_get_ex(obj, "cmd", &val))
    {//此处原有json库的api：json_object_object_get已停用
        fprintf(stderr, "[ERROR] JSON中没有cmd字段\n");
        json_object_put(obj);
        return -1;
    }
    
    if (strcmp("reply_music", json_object_get_string(val)))
    {
        fprintf(stderr, "[ERROR] JSON格式不对\n");
        json_object_put(obj);
        return -1;
    }

    struct json_object *array = NULL;
    if (!json_object_object_get_ex(obj, "music", &array))
    {
        fprintf(stderr, "[ERROR] JSON中没有music字段\n");
        json_object_put(obj);
        return -1;
    }
    
    for (int i = 0; i < json_object_array_length(array); i++)
    {
        struct json_object *music = json_object_array_get_idx(array, i);
        if (link_insert_elem(json_object_get_string(music)) == -1)
        {
            fprintf(stderr, "[ERROR] 元素插入失败\n");
            continue;
        }
    }

    json_object_put(obj);

    return 0;
}

int link_insert_elem(const char *name)
{
	Node *p = g_music_head;

	while (p->next)
		p = p->next;

	Node *new = malloc(sizeof(Node));
	if (NULL == new)
	{
		return -1;
	}

	strcpy(new->music_name, name);
	new->next = NULL;
	new->prev = p;
	p->next = new;

	return 0;
}
//遍历链表
void link_traverse_list()
{
	Node *p = g_music_head->next;

	while (p)
	{
		printf("%s ", p->music_name);
		p = p->next;
	}
	printf("\n");//不加换行，不刷新
}

void link_clear_list()
{
	Node *p = g_music_head->next;

	while (p)
	{
		Node *q = p->next;
		free(p);
		p = q;
	}

	g_music_head->next = NULL;
}

int link_find_next(int mode, char *cur_name,char *next_name)
{
    if(NULL == cur_name||NULL == next_name)return -1;
    if (!cur_name[0])
        return -1;

    Node *p = link_find_node_by_ref(cur_name);
    if(p == NULL)
        return -1;
    if(mode == CIRCLE){
        strcpy(next_name,p->music_name);
        return 0;
    }
    if(p->next == NULL)return -1;// 链表末尾,最后一首歌

    strcpy(next_name,p->next->music_name);//下一首歌
    return 0;
}

int link_find_prev(int mode, char *cur_name,char *pre_name)
{//查找上一首歌：单曲循环仍为当前曲；顺序/随机在第一首时环绕到最后一首
    if(NULL == cur_name||NULL == pre_name)return -1;
    if (!cur_name[0])
        return -1;

    Node *p = link_find_node_by_ref(cur_name);
    if(p == NULL)
        return -1;

    if(mode == CIRCLE)
    {
        strcpy(pre_name, p->music_name);
        return 0;
    }

    if(p->prev == g_music_head || p->prev == NULL)
    {
        Node *tail = link_find_tail(g_music_head->next);
        if(tail == NULL)
            return -1;
        strcpy(pre_name, tail->music_name);
        return 0;
    }

    strcpy(pre_name, p->prev->music_name);
    return 0;
}

Node *link_find_tail(Node *p)//查找链表末尾
{ 
    if(p == NULL)return NULL;
    while(p->next)
    {
        p = p->next;
    }
    return p;
}

int link_is_space(const char *name)
{
    const char *p = name;
    while(*p != '\0')
    {
        if(*p == ' ')
        {
            return 1;
        }
        p++;
    }
    return 0;
}

//离线模式，读取U盘中的音乐文件，并插入链表
int link_read_offline_music(void)
{
    link_clear_list();//清空链表
    char music_name[128] = {0};
    DIR *dir = opendir("/mnt/usb");
    if(dir == NULL)
    {
        perror("opendir");
        return -1;
    }
    struct dirent *file;//dirent结构体，用于读取目录中的文件
    while((file = readdir(dir)) != NULL)
    {
        if(file->d_type != DT_REG)//只读取普通文件
        {
            continue;
        }
        if(strstr(file->d_name, ".mp3") == NULL)//只读取mp3文件
        {
            continue;
        }
        if(link_is_space(file->d_name))//文件名中包含空格
        {
            //给带空格的文件名加上引号
            strcpy(music_name, "\"");
            strcat(music_name, file->d_name);
            strcat(music_name, "\"");
        }else//文件名中不包含空格
        {
            strcpy(music_name, file->d_name);
        }
        link_insert_elem(music_name);//插入链表
        memset(music_name, 0, sizeof(music_name));//清空音乐名称，防止下一次插入时被覆盖
    }
    closedir(dir);
    return 0;
}
