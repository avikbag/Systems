#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include "messages.h"

#define BUFF_LEN 1000
#define SHMOBJ_PATH "/shmbbfs"

int shm_fd = -1;

struct lqueue *data;      

struct lnode node;

void signal_callback_handler(int signum) {
    if (shm_fd > 0) {
        if (shm_unlink(SHMOBJ_PATH) < 0) {
            perror("shm_unlink");
        }
    }

    // Terminate program
    exit(signum);
}

int main(int argc, char *argv[]) {
    int shared_seg_size = sizeof(struct lqueue);   

    /* creating the shared memory object    --  shm_open()  */
    shm_fd = shm_open(SHMOBJ_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_fd < 0) {
        perror("In shm_open()");
        exit(1);
    }

    fprintf(stderr, "Created shared memory object %s\n", SHMOBJ_PATH);

    ftruncate(shm_fd, shared_seg_size);

    data = (struct lqueue*) mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (data == NULL) {
        perror("In mmap()");
        exit(1);
    }

    fprintf(stderr, "Shared memory segment allocated correctly (%d bytes).\n", shared_seg_size);

    printf("Shared memory created for \n");

    queue_init(data);

    while (1) {
        if (!queue_empty(data)) {
            queue_pop(data, &node);

            printf("%s", node.msg);
        }
    }

    return 0;
}

