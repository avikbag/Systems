#ifndef MESSAGES_H
#define MESSAGES_H

#define MSG_LEN 1000
#define BUFF_LEN 1000

typedef struct lnode {
    int rdy;
    char msg[MSG_LEN];
} lnode;

typedef struct lqueue {
    int free_idx;
    struct lnode msgs[BUFF_LEN];
} lqueue;

void queue_init(lqueue *data);

void queue_push(lqueue *data, char *msg);
void queue_front(lqueue *data, lnode *dest);
void queue_pop(lqueue *data, lnode *dest);

int queue_empty(lqueue *data);

#endif // MESSAGES_H
