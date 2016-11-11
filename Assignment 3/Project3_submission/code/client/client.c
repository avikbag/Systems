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
#include "messages.h"

int shm_fd = -1;
int shm_id = -1;

sem_t * sem_rx_id;
sem_t * sem_tx_id;

sem_t * sem_ready_id;
sem_t * sem_ack_id;
sem_t * sem_done_id;

char *chat_room;

/* message structure for messages in the shared segment */

struct shared_data {
    int data_available;
    int client_id;
    int code;
    int source;
    int destination;
    char data[1000];
};

struct shared_data *shared_msg;      

void handleBroadcast();

void disconnect() {
    sem_wait(sem_tx_id);
    printf("Disconnecting \n");
    shared_msg->data_available = 1;
    shared_msg->code = MessageClientDisconnect;
    sem_post(sem_ready_id);

    sem_wait(sem_done_id);
    shared_msg->data_available = 0;
    sem_post(sem_ack_id);
}

void signal_callback_handler(int signum) {
    if (shm_fd > 0) {
        disconnect();
    }

    // Terminate program
    exit(signum);
}

int main(int argc, char *argv[]) {
    int numClients = 0;
    int shared_seg_size = (1 * sizeof(struct shared_data));   

    chat_room = argv[1];

    if (chat_room == NULL) {
        printf("First argument must be chat room\n");
        exit(1);
    }

    int i;
    for (i = 0; i < 10; i++) {
        char buffer[12];
        snprintf(buffer, 12, "/shmchat%d", i);

        /* creating the shared memory object    --  shm_open()  */
        shm_fd = shm_open(buffer, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
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

        if (shared_msg->client_id != -1) {
            printf("Closing %d %s\n", i, buffer);

            shm_fd = -1;
            shm_id = -1;

            continue;
        } else {
            shm_id = i;
            break;
        }
    }

    printf("Joined chat as user %d\n", shm_id);

    if (shm_fd == -1) {
        printf("Server not running or full!\n");
        exit(1);
    }

    sem_tx_id = sem_open("/txsem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_rx_id = sem_open("/rxsem", O_CREAT, S_IRUSR | S_IWUSR, 0);

    sem_ready_id = sem_open("/rdsem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_ack_id = sem_open("/rksem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_done_id = sem_open("/tdsem", O_CREAT, S_IRUSR | S_IWUSR, 0);

    signal(SIGINT, signal_callback_handler);

    sem_wait(sem_tx_id);
    shared_msg->data_available = 1;
    shared_msg->client_id = -1;
    shared_msg->code = MessageNewClient;
    sem_post(sem_ready_id);

    sem_wait(sem_done_id);
    shared_msg->data_available = 0;
    sem_post(sem_ack_id);

    switch(shared_msg->code) {
        case MessageClientConnected:
            break;
        case MessageServerFull:
            printf("Server Full\n");
            exit(1);
            break;
        default:
            printf("Unknown Command\n");
            break;
    }

    sem_wait(sem_tx_id);
    shared_msg->data_available = 1;
    shared_msg->code = MessageChatJoin;
    strcpy(shared_msg->data, chat_room);
    sem_post(sem_ready_id);

    sem_wait(sem_done_id);
    shared_msg->data_available = 0;
    sem_post(sem_ack_id);

    switch(shared_msg->code) {
        case MessageChatJoined:
            printf("Chat Joined\n");
            break;
        case MessageChatCreated:
            printf("Chat Created\n");
            break;
        default:
            printf("Unknown Command\n");
            break;
    }

    char kb_msg[1000];
    printf("\n");

    int running = 1;

    while(running) {
        if (shared_msg->data_available > 0) {
            sem_post(sem_rx_id);
            sem_wait(sem_ready_id);

            switch(shared_msg->code) {
                case MessageBroadcast:
                    handleBroadcast();
                    break;
                case MessagePrivateMessage:
                    handlePrivateMessage();
                    break;
                case MessageShutdown:
                    printf("Server shutting down\n");
                    running = 0;

                    sem_post(sem_done_id);
                    shared_msg->data_available = 0;
                    sem_wait(sem_ack_id);
                    break;
                default:
                    printf("Unknown Command\n");
                    break;
            }
        }

        if (!running) {
            break;
        }

        int chars_available = 0;

        ioctl(0, FIONREAD, &chars_available);

        if (chars_available > 0) {
            memset(&kb_msg[0], 0, sizeof(kb_msg));
            read(0, kb_msg, sizeof(kb_msg));

            if (kb_msg[0] == '@') {
                if (kb_msg[2] == ' ') {
                    shared_msg->code = MessagePrivateMessage;
                    shared_msg->destination = kb_msg[1] - '0';
                    printf("Sending PM to %d\n", shared_msg->destination);
                    memmove(kb_msg, kb_msg + 2, strlen(kb_msg));
                } else {
                    printf("Invalid PM Format. Format is @[user id] message\n");
                    continue;
                }
            } else {
                shared_msg->code = MessageBroadcast;
            }

            sem_wait(sem_tx_id);
            shared_msg->data_available = 1;
            shared_msg->source = shared_msg->client_id;
            strcpy(shared_msg->data, kb_msg);
            sem_post(sem_ready_id);

            sem_wait(sem_done_id);
            shared_msg->data_available = 0;
            sem_post(sem_ack_id);

        }
    }

    return 0;
}

void handleBroadcast() {
    printf("%s - %d: %s", chat_room, shared_msg->source, shared_msg->data);

    sem_post(sem_done_id);
    shared_msg->data_available = 0;
    sem_wait(sem_ack_id);
}

void handlePrivateMessage() {
    printf("PM from @%d: %s", shared_msg->source, shared_msg->data);

    sem_post(sem_done_id);
    shared_msg->data_available = 0;
    sem_wait(sem_ack_id);
}
