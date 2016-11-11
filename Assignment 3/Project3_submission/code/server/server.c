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


#define SHMOBJ_PATH "/shmchat"

int shm_fd[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

sem_t * sem_rx_id;
sem_t * sem_tx_id;

sem_t * sem_ready_id;
sem_t * sem_ack_id;
sem_t * sem_done_id;

struct chat_room {
    int live;
    char name[100];
    int clients[10];
};

struct chat_room chat_rooms[10];

struct shared_data {
    int data_available;
    int client_id;
    int code;
    int source;
    int destination;
    char data[1000];
};

struct shared_data *shared_msg[10];      

void signal_callback_handler(int signum) {
    int i;

    printf("Stopping Server\n");

    for(i = 0; i < 10; i++){
        struct shared_data *msg = shared_msg[i];

        if (msg->client_id == -1) {
            continue;
        }

        printf("Stopping Client %d\n", msg->client_id);

        msg->data_available = 1;
        sem_wait(sem_tx_id);
        msg->code = MessageShutdown;
        sem_post(sem_ready_id);

        sem_wait(sem_done_id);
        sem_post(sem_ack_id);
    }

    for (i = 0; i < 10; i++) {
        if (shm_fd[i] > 0) {
            char buffer[12];
            snprintf(buffer, 12, "/shmchat%d", i);

            printf("Closing %d %s\n", i, buffer);

            if (shm_unlink(buffer) < 0) {
                perror("shm_unlink");
            }
        }
    }

    if ( sem_close(sem_rx_id) < 0 ) {
        perror("sem_close");
    }

    if ( sem_unlink("/rxsem") < 0 ) {
        perror("sem_unlink");
    }

    if ( sem_close(sem_tx_id) < 0 ) {
        perror("sem_close");
    }

    if ( sem_unlink("/txsem") < 0 ) {
        perror("sem_unlink");
    }
    if ( sem_close(sem_done_id) < 0 ) {
        perror("sem_close");
    }

    if ( sem_unlink("/tdsem") < 0 ) {
        perror("sem_unlink");
    }

    if ( sem_close(sem_ready_id) < 0 ) {
        perror("sem_close");
    }

    if ( sem_unlink("/rdsem") < 0 ) {
        perror("sem_unlink");
    }

    if ( sem_close(sem_ack_id) < 0 ) {
        perror("sem_close");
    }

    if ( sem_unlink("/rksem") < 0 ) {
        perror("sem_unlink");
    }

    // Terminate program
    exit(signum);
}


int main(int argc, char *argv[]) {
    int shared_seg_size = (1 * sizeof(struct shared_data));   

    int j, k;
    for (j = 0; j < 10; j++) {
        chat_rooms[j].live = 0;
        for (k = 0; k < 10; k++) {
            chat_rooms[j].clients[k] = -1;
        }
    }

    int i;
    for (i = 0; i < 10; i++) {
        char buffer[12];
        snprintf(buffer, 12, "/shmchat%d", i);

        /* creating the shared memory object    --  shm_open()  */
        shm_fd[i] = shm_open(buffer, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_fd[i] < 0) {
            perror("In shm_open()");
            exit(1);
        }

        fprintf(stderr, "Created shared memory object %s\n", buffer);

        ftruncate(shm_fd[i], shared_seg_size);

        shared_msg[i] = (struct shared_data *) mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd[i], 0);

        if (shared_msg[i] == NULL) {
            perror("In mmap()");
            exit(1);
        }

        (*shared_msg[i]).client_id = -1;

        fprintf(stderr, "Shared memory segment allocated correctly (%d bytes).\n", shared_seg_size);

        printf("Shared memory created for %d\n", i);
    }

    sem_tx_id = sem_open("/rxsem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_rx_id = sem_open("/txsem", O_CREAT, S_IRUSR | S_IWUSR, 0);

    sem_ready_id = sem_open("/rdsem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_ack_id = sem_open("/rksem", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_done_id = sem_open("/tdsem", O_CREAT, S_IRUSR | S_IWUSR, 0);

    signal(SIGINT, signal_callback_handler);

    while(1)
    {
        printf("Allowing Clients to Write \n");
        int kk;
        sem_getvalue(sem_rx_id, &kk);
        printf("Done %d\n", kk);
        sem_post(sem_rx_id);

        printf("Waiting for Client to Write \n");
        sem_getvalue(sem_ready_id, &kk);
        printf("Done %d\n", kk);

        sem_wait(sem_ready_id);
        printf("Reading from Client \n");

        int client = -1;

        int i = 0;
        for (i = 0; i < 10; i++) {
            if ((*shared_msg[i]).data_available == 1) {
                client = i;
                break;
            }
        }
        if (client == -1) {
            printf("ERROR: No Client Wrote\n");
            continue;
        }

        //sem_wait(sem_tx_id); // Should never have to wait
        switch (shared_msg[i]->code) {
            case MessageNewClient:
                handleNewClient(i);
                break;
            case MessageClientDisconnect:
                handleClientDisconnect(i);
                break;
            case MessageChatJoin:
                handleChatJoin(client);
                break;
            case MessageBroadcast:
                handleBroadcast(client);
                break;
            case MessagePrivateMessage:
                handlePrivateMessage(client);
                break;
        }
    }

    return 0;
}

void handleNewClient(int client_id) {
    struct shared_data *msg = shared_msg[client_id];
    printf("Handle Client Connected\n");

    if (msg->client_id != -1) {
        printf("ERROR: Client trying to connect already has client id\n");
    }

    msg->client_id = client_id;
    msg->data_available = 1;
    msg->code = MessageClientConnected;

    sem_post(sem_done_id);
    sem_wait(sem_ack_id);
}

void handleChatJoin(int client_post) {
    struct shared_data *msg = shared_msg[client_post];
    printf("Handle Chat Join \n"); 

    int i;
    int chat_room_index = -1;
    int first_empty_room = -1;
    for(i = 0; i < 10; i++) {
        if (chat_rooms[i].live == 0 && first_empty_room == -1) {
            first_empty_room = i;
        }

        if (strcmp(chat_rooms[i].name, msg->data) == 0) {
            printf("Found existing chat room! %s %s\n", chat_rooms[i].name, msg->data);
            chat_room_index = i;
            break;
        }
    }

    if (chat_room_index == -1) {
        msg->code = MessageChatCreated;
        chat_room_index = first_empty_room;

        chat_rooms[chat_room_index].live = 1;
        strcpy(chat_rooms[chat_room_index].name, msg->data);
    } else {
        msg->code = MessageChatJoined;
    }

    int k;
    for(k = 0; k < 10; k++) {
        if (chat_rooms[chat_room_index].clients[k] == -1) {
            chat_rooms[chat_room_index].clients[k] = client_post;
            break;
        }
    }

    printf("Joining Chat Room %d\n", chat_room_index);

    msg->data_available = 1;
    sem_post(sem_done_id);
    sem_wait(sem_ack_id);
}

void handleClientDisconnect(int client_id) {
    struct shared_data *msg = shared_msg[client_id];
    printf("Handle Client Disconnect\n");

    int i, k;
    for(i = 0; i < 10; i++) {
        int empty_rooms = 0;
        for(k = 0; k < 10; k++) {
            if (chat_rooms[i].clients[k] == -1) {
                empty_rooms++;
            } else if (chat_rooms[i].clients[k] == msg->client_id) {
                chat_rooms[i].clients[k] = -1;
                empty_rooms++;
            }
        }

        if (empty_rooms == 10) {
            chat_rooms[i].live = 0;
            strcpy(chat_rooms[i].name, "");
        }
    }

    msg->client_id = -1;
    msg->code = -1;
    msg->data_available = 0;
    memset(msg->data, 0, sizeof(msg->data));

    sem_post(sem_done_id);
    sem_wait(sem_ack_id);
}

int get_group(int client_id) {
    int i, k;
    for(i = 0; i < 10; i++) {
        int empty_rooms = 0;
        for(k = 0; k < 10; k++) {
            if (chat_rooms[i].clients[k] == client_id) {
                return i;
            }
        }
    }
}

void handleBroadcast(int client_id) {
    struct shared_data *msg = shared_msg[client_id];

    sem_post(sem_done_id);
    sem_wait(sem_ack_id);

    int i;
    char broadcast[1000];
    strcpy(broadcast, msg->data);

    int group = get_group(client_id);

    for(i = 0; i < 10; i++){
        if (chat_rooms[group].clients[i] == -1) {
            continue;
        }

        if (i == client_id) {
            continue;
        }

        struct shared_data *msg2 = shared_msg[chat_rooms[group].clients[i]];

        msg2->data_available = 1;
        sem_wait(sem_tx_id);

        msg2->source = client_id;
        msg2->code = MessageBroadcast;

        memset(&(msg2->data[0]), 0, sizeof(msg2->data));
        strcpy(msg2->data, broadcast);

        sem_post(sem_ready_id);

        sem_wait(sem_done_id);
        sem_post(sem_ack_id);
    }
}

void handlePrivateMessage(int client_id) {
    struct shared_data *msg = shared_msg[client_id];

    sem_post(sem_done_id);
    sem_wait(sem_ack_id);

    int i;
    char broadcast[1000];
    strcpy(broadcast, msg->data);

    if (msg->destination > 10 || msg->destination < 0) {
        memset(&(msg->data[0]), 0, sizeof(msg->data));
        strcpy(msg->data, "Invalid user");

        // Echo message back to sender
        msg->data_available = 1;
        sem_wait(sem_tx_id);
        sem_post(sem_ready_id);
        sem_wait(sem_done_id);
        sem_post(sem_ack_id);

        return;
    }

    struct shared_data *msg2 = shared_msg[msg->destination];

    printf("Sending PM to %d %d\n", msg->destination, msg2->client_id);
    if (msg2->client_id == -1) {
        memset(&(msg->data[0]), 0, sizeof(msg->data));
        strcpy(msg->data, "User not online\n");

        // Echo message back to sender
        msg->data_available = 1;
        sem_wait(sem_tx_id);
        sem_post(sem_ready_id);
        sem_wait(sem_done_id);
        sem_post(sem_ack_id);

        return;
    }

    msg2->data_available = 1;
    sem_wait(sem_tx_id);

    msg2->source = client_id;
    msg2->code = MessagePrivateMessage;

    sem_post(sem_ready_id);

    sem_wait(sem_done_id);
    sem_post(sem_ack_id);
}

