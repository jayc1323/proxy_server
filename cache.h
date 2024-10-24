
/*
 Header file for the Cache ADT
 Jay Choudhary




*/
#ifndef CACHE
#define CACHE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>


typedef struct Cache *Cache;

Cache Cache_new(int num_items);
void Cache_put(Cache ch,char* file_name,int age);
void Cache_get(Cache ch,char* file_name);
bool exists_in_cache(Cache ch, char *file_name);
bool isStale(Cache ch,char *file_name);
void Cache_free(Cache* ch);
char* return_file_content(Cache ch,char *file_name);
int get_file_age(Cache ch,char *file_name);
void process_commands_file(Cache ch,const char* command_file);
void update_LRU(Cache ch,char *file_name);


#endif 