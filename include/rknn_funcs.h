#ifndef __RKNN_FUNCS_H__
#define __RKNN_FUNCS_H__

#include "postprocess.h"

typedef struct node {
  long timeval;
  detect_result_group_t detect_result_group;
  struct node* next;
} Node;

typedef struct my_stack {
  int size;
  Node* top;
} rknn_list;

void rknn_list_push(rknn_list* s, long timeval, detect_result_group_t detect_result_group);

int detect_by_buf(void* data, detect_result_group_t* detect_result_group);

void rknn_list_drop(rknn_list* s);
int rknn_list_size(rknn_list* s);

void create_rknn_list(rknn_list** s);

void destory_rknn_list(rknn_list** s);

int init_model(const char* modelPath);

void rknn_list_pop(rknn_list* s, long* timeval, detect_result_group_t* detect_result_group);


#endif
