#include "messages.h"
#include <string.h>

void queue_init(lqueue *data) {
    int i;
    data->free_idx = 0;
    for (i = 0; i < BUFF_LEN; i++) {
        data->msgs[i].rdy = 0;
    }
}

void queue_push(lqueue *data, char *msg) {
    int idx = __sync_add_and_fetch(&data->free_idx, 1);

    strcpy(data->msgs[idx].msg, msg);
    data->msgs[idx].rdy = 1;
}

void queue_front(lqueue *data, lnode *dest) {
    int idx = data->free_idx;

    while (!data->msgs[idx].rdy) {
        // Wait
    }

    strcpy(dest->msg, data->msgs[idx].msg);
}

void queue_pop(lqueue *data, lnode *dest) {
    int idx = data->free_idx - 1;

    while (!data->msgs[idx].rdy) {
        // Wait
    }

    strcpy(dest->msg, data->msgs[idx].msg);

    // Clear out old message
    memset(data->msgs[idx].msg, 0, MSG_LEN);

    data->msgs[idx].rdy = 0;
    __sync_sub_and_fetch(&data->free_idx, 1);
}

int queue_empty(lqueue *data) {
    return data->free_idx == 0;
}
