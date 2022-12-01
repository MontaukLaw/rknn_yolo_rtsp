#ifndef __MY_RKMEDIA_RKNN_MODEL_H
#define __MY_RKMEDIA_RKNN_MODEL_H

int init_model(int argc, char **argv);

// int predict(void *bufData);
int predict(void *bufData, detect_result_group_t *detect_result_group);

#endif // __MY_RKMEDIA_RKNN_MODEL_H
