#include "link.h"
#include "player.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <json/json.h>
#include <string.h>

Node *g_music_head = NULL;    //保存歌曲信息的链表

int init_link()
{
	g_music_head = (Node *)malloc(sizeof(Node) * 1);
	if (NULL == g_music_head)
	{
		perror("malloc");
		return -1;
	}

	g_music_head->next = NULL;
	g_music_head->prior = NULL;

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

	struct json_object *val = json_object_object_get(obj, "cmd");
	if (strcmp("reply_music", json_object_get_string(val)))
	{
		fprintf(stderr, "[ERROR] JSON格式不对\n");
		return -1;
	}

	struct json_object *array = json_object_object_get(obj, "music");
	for (int i = 0; i < json_object_array_length(array); i++)
	{
		struct json_object *music = json_object_array_get_idx(array, i);
		//把music对象转换成字符串插入链表
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
	new->prior = p;
	p->next = new;

	return 0;
}

void link_traverse_list()
{
	Node *p = g_music_head->next;

	while (p)
	{
		printf("%s ", p->music_name);
		p = p->next;
	}

	printf("\n");
}

/*   描述：根据当前歌曲、当前模式，找到下一首歌
 *   参数：当前模式  当前歌曲   下一首歌
 *   返回值：如果找不到下一首，返回-1，找到返回0
 * */
int link_find_next(int mode, char *cur, char *next)
{
	if (NULL == cur || NULL == next)
		return -1;

	Node *p = g_music_head->next;
	while (p)
	{
		if (strstr(p->music_name, cur))
			break;

		p = p->next;
	}

	if (mode == CIRCLE)
	{
		strcpy(next, p->music_name);
		return 0;
	}

	if (p->next == NULL)
		return -1;

	strcpy(next, p->next->music_name);

	return 0;
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

/*根据当前歌曲找上一首*/
void link_find_prior(const char *cur, char *music)
{
	if (NULL == cur || NULL == music)
		return;

	Node *p = g_music_head->next;
	if (strstr(p->music_name, cur))
	{
		strcpy(music, p->music_name);
		return;
	}

	while (p)
	{
		if (strstr(p->music_name, cur))
		{
			strcpy(music, p->prior->music_name);
			return;
		}
		p = p->next;
	}
}

//判断歌曲名字是否含有空格
int link_is_space(const char *name)
{
	const char *p = name;

	while (*p != '\0')
	{
		if (*p == ' ')
			return 1;

		p++;
	}

	return 0;
}

int link_read_music()
{
	//清空当前链表
	link_clear_list();

	DIR *dir;
	struct dirent *file;
	//char name[128] = {0};

	//打开文件夹
	dir = opendir("/mnt/usb");
	if (NULL == dir)
	{
		perror("opendir");
		return -1;
	}

	while ((file = readdir(dir)) != NULL)
	{
		if (file->d_type != DT_REG)
			continue;

		if (!strstr(file->d_name, ".mp3"))
			continue;

		/*if (link_is_space(file->d_name))
		{
			//歌曲名字含有空格
			name[0] = '"';
			strcpy(name + 1, file->d_name);
			name[strlen(name)] = '"';
		}
		else 
			strcpy(name, file->d_name);*/
	
		link_insert_elem(file->d_name);

		//memset(name, 0, sizeof(name));
	}

	return 0;
}
