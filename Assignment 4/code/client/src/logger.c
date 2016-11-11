#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>

#include "logger.h"
#include "messages.h"

#define SHMOBJ_PATH "/shmbbfs"

int shm_fd = -1;

struct shared_data {

    char data[1000];
};

struct shared_data *shared_msg;      

void log_shm_init() {
    int shared_seg_size = (1 * sizeof(struct shared_data));   

    /* creating the shared memory object    --  shm_open()  */
    shm_fd = shm_open(SHMOBJ_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_fd < 0) {
        perror("In shm_open()");
        exit(1);
    }

    ftruncate(shm_fd, shared_seg_size);

    shared_msg = (struct shared_data *) mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (shared_msg == NULL) {
        perror("In mmap()");
        exit(1);
    }

    printf("Found %d\n", shared_msg->client_id);

    if (shm_fd == -1) {
        printf("Server not running or full!\n");
        exit(1);
    }
}

void log_shm_msg(char *buff) {
}
