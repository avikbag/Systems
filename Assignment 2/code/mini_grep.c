/* Reference code that searches files for a user-specified string. 
 * 
 * Author: Naga Kandasamy kandasamy
 * Date: 7/19/2015
 *
 * Compile the code as follows: gcc -o mini_grep min_grep.c queue_utils.c -std=c99 -lpthread - Wall
 *
 */

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"

int serialSearch(char **);
int parallelSearchStatic(char **);
int parallelSearchDynamic(char **);

int debug = 0;
int wait_count = 0;
int total_threads;
pthread_mutex_t lock;


struct ThreadArgument {
    int lock;
    char *search_ptr;
    queue_t *queue;
    int thread_num;
    int result;
};


int main(int argc, char** argv) {
    if(argc < 5){
        printf("\n %s <search string> <path> <num threads> static (or)\
                %s <search string> <path> <num threads> dynamic \n", argv[0], argv[0]);
        exit(EXIT_SUCCESS);
    }

    int num_occurrences;
    int first_num_occurrences;

    struct timeval start, stop;                                           /* Start the timer. */
    struct timeval start2, stop2;                                           /* Start the timer. */
    /* Perform a multi-threaded search of the file system. */
    gettimeofday(&start2, NULL);

    if(strcmp(argv[4], "static") == 0){
        printf("\nPerforming multi-threaded search using static load balancing.\n");
        num_occurrences = parallelSearchStatic(argv);
        printf("\nStatic: The string %s was found %d times within the file system.", argv[1], num_occurrences);

    }
    else if(strcmp(argv[4], "dynamic") == 0){
        printf("\nPerforming multi-threaded search using dynamic load balancing.\n");
        num_occurrences = parallelSearchDynamic(argv);
        printf("\nDynamic: The string %s was found %d times within the file system.", argv[1], num_occurrences);

    }
    else{
        printf("\nUnknown load balancing option provided. Defaulting to static load balancing.\n");
        num_occurrences = parallelSearchStatic(argv);
        printf("\nThe string %s was found %d times within the \file system.", argv[1], num_occurrences);

    }

    gettimeofday(&stop2, NULL);

    printf("\n Overall execution time = %fs.", (float)(stop2.tv_sec - start2.tv_sec + (stop2.tv_usec - start2.tv_usec)/(float)1000000));

    printf("\n");
    exit(EXIT_SUCCESS);
}

int processQueue(char *search_ptr, queue_t *queue, int use_lock, int thread_num) {
    int num_occurrences = 0;
    queue_element_t *new_element;
    struct stat file_stats;
    int status; 
    DIR *directory = NULL;
    struct dirent *result = NULL;
    struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH);
    int thread_status = 0;
    while(1){                                   /* While there is work in the queue, process it. */
        if (use_lock) {
            pthread_mutex_lock(&lock); 
        }

        if (queue->head == NULL) {
            if (use_lock) {
                if (!thread_status){
                  wait_count++;
                }
                thread_status = 1;

                if (wait_count == total_threads)
                {
                    pthread_mutex_unlock(&lock); 
                    break;
                }
                else
                {
                  pthread_mutex_unlock(&lock); 
                  continue;
                }
            }
            break;
        }
        if (thread_status)
        {
          wait_count--;
          thread_status = 0;
        }

        queue_element_t *element = removeElement(queue);

        if (use_lock) {
            pthread_mutex_unlock(&lock); 
        }

        if (debug) printf("Processing %s\n", element->path_name);

        status = lstat(element->path_name, &file_stats); /* Obtain information about the file. */
        if(status == -1){
            printf("Error obtaining stats for %s \n", element->path_name);

            free((void *)element);
            continue;
        }

        if(S_ISLNK(file_stats.st_mode)){ /* Ignore symbolic links. */
            printf("SymLink for %s \n", element->path_name);
        } 
        else if(S_ISDIR(file_stats.st_mode)){ /* If directory, descend in and post work to queue. */
            if (debug) printf("%s is a directory. \n", element->path_name);

            directory = opendir(element->path_name);
            if(directory == NULL){
                if (debug) printf("Unable to open directory %s \n", element->path_name);
                continue;
            }
            while(1){
                status = readdir_r(directory, entry, &result); /* Read directory entry. */
                if(status != 0){
                    printf("Unable to read directory %s \n", element->path_name);
                    break;
                }
                if(result == NULL)                /* End of directory. */
                    break;                                           

                if(strcmp(entry->d_name, ".") == 0)   /* Ignore the "." and ".." entries. */
                    continue;
                if(strcmp(entry->d_name, "..") == 0)
                    continue;

                /* Insert this directory entry in the queue. */
                new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
                if(new_element == NULL){
                    printf("Error allocating memory. Exiting. \n");
                    exit(-1);
                }
                /* Construct the full path name for the directory item stored in entry. */

                strcpy(new_element->path_name, element->path_name);
                strcat(new_element->path_name, "/");
                strcat(new_element->path_name, entry->d_name);

                if (use_lock) {
                    pthread_mutex_lock(&lock); 
                }

                insertElement(queue, new_element);

                if (use_lock) {
                    pthread_mutex_unlock(&lock); 
                }
            }
            closedir(directory);
        } 
        else if(S_ISREG(file_stats.st_mode)){      /* Directory entry is a regular file. */
            if (debug) printf("%s is a regular file. \n", element->path_name);

            FILE *file_to_search;
            char buffer[MAX_LENGTH];
            char *bufptr, *searchptr, *tokenptr;

            /* Search the file for the search string provided as the command-line argument. */ 
            file_to_search = fopen(element->path_name, "r");
            if(file_to_search == NULL){
                printf("Unable to open file %s \n", element->path_name);
                continue;
            } 
            else{
                while(1){
                    bufptr = fgets(buffer, sizeof(buffer), file_to_search);        /* Read in a line from the file. */
                    if(bufptr == NULL){
                        if(feof(file_to_search)) {
                            break;
                        }
                        if(ferror(file_to_search)){
                            printf("Error reading file %s \n", element->path_name);
                            break;
                        }
                    }

                    char *saveptr;

                    /* Break up line into tokens and search each token. */ 
                    tokenptr = strtok_r(buffer, " ,.-", &saveptr);
                    while(tokenptr != NULL){

                        searchptr = strstr(tokenptr, search_ptr);
                        if(searchptr != NULL){
                            if (debug) printf("Thread %d Found string %s within file %s. \n", thread_num, search_ptr, element->path_name);
                            num_occurrences++;
                        }

                        tokenptr = strtok_r(NULL, " ,.-", &saveptr);                        /* Get next token from the line. */
                    }
                }
            }
            fclose(file_to_search);
        }
        else{
            printf("%s is of type other. \n", element->path_name);
        }
        free((void *)element);
    }

    return num_occurrences;
}

int serialSearch2(char *search_ptr, char *path_name) {
    queue_element_t *element;

    queue_t *queue = createQueue();               /* Create and initialize the queue data structure. */
    element = (queue_element_t *)malloc(sizeof(queue_element_t));
    if(element == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(element->path_name, path_name);
    element->next = NULL;
    insertElement(queue, element);                            /* Insert the initial path name into the queue. */

    return processQueue(search_ptr, queue, 0, 0);
}


int serialSearch(char **argv) {
    return serialSearch2(argv[1], argv[2]);
}

queue_element_t* create_element(char *file_path) {
    queue_element_t *element;
    element = (queue_element_t *)malloc(sizeof(queue_element_t));
    if(element == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(element->path_name, file_path);
    element->next = NULL;

    return element;
}

void *processQueueT(void *argp) {
    struct ThreadArgument *args = argp;
    int res = processQueue(args->search_ptr, args->queue, args->lock, args->thread_num);

    printf("Thread Ended %d %d \n", args->thread_num, res);

    args->result = res;
}

int parallelSearchStatic(char **argv) {
    int num_threads = atoi(argv[3]);

    DIR *d;
    struct dirent *dir;
    d = opendir(argv[2]);
    queue_element_t *element;

    queue_t *queues[num_threads];
    int i;
    for (i = 0; i < num_threads; i++) {
        queues[i] = createQueue();
    }

    int count = 0;

    if (d) {
        while ((dir = readdir(d)) != NULL) {

            if(strcmp(dir->d_name, ".") == 0)   /* Ignore the "." and ".." entries. */
                continue;
            if(strcmp(dir->d_name, "..") == 0)
                continue;

            int queue_num = count % num_threads;

            element = create_element(dir->d_name);

            strcpy(element->path_name, argv[2]);
            strcat(element->path_name, "/");
            strcat(element->path_name, dir->d_name);

            printf("Adding %d %s\n", queue_num, element->path_name);

            insertElement(queues[count % num_threads], element);

            count++;
        }

        closedir(d);
    }

    int num_occurrences = 0;

    struct ThreadArgument threadArgs[num_threads];

    pthread_t t_gold[num_threads];

    for (i = 0; i < num_threads; i++) {
        threadArgs[i].queue = queues[i];
        threadArgs[i].search_ptr = argv[1];
        threadArgs[i].lock = 0;
        threadArgs[i].thread_num = i;
        pthread_create(&t_gold[i], NULL, processQueueT, &threadArgs[i]);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(t_gold[i], NULL);

        printf("Got result from thread %d of %d\n", i, threadArgs[i].result);

        num_occurrences += threadArgs[i].result;
    }

    return num_occurrences;
}


int parallelSearchDynamic(char **argv) {
    int num_occurrences = 0;
    char *path_name = argv[2];
    int num_threads = atoi(argv[3]);
    total_threads = num_threads;
    DIR *d;
    struct dirent *dir;
    d = opendir(argv[2]);

    queue_element_t *element;

    pthread_mutex_init(&lock, NULL);

    queue_t *queue = createQueue();               /* Create and initialize the queue data structure. */

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(strcmp(dir->d_name, ".") == 0)   /* Ignore the "." and ".." entries. */
                continue;
            if(strcmp(dir->d_name, "..") == 0)
                continue;

            element = create_element(dir->d_name);

            strcpy(element->path_name, argv[2]);
            strcat(element->path_name, "/");
            strcat(element->path_name, dir->d_name);

            insertElement(queue, element);
        }

        closedir(d);
    }

    struct ThreadArgument threadArgs[num_threads];

    pthread_t t_gold[num_threads];

    int i;
    for (i = 0; i < num_threads; i++) {
        threadArgs[i].lock = 1;
        threadArgs[i].queue = queue;
        threadArgs[i].search_ptr = argv[1];
        threadArgs[i].thread_num = i;
        pthread_create(&t_gold[i], NULL, processQueueT, &threadArgs[i]);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(t_gold[i], NULL);

        printf("Got result from thread %d of %d\n", i, threadArgs[i].result);

        num_occurrences += threadArgs[i].result;
    }

    printQueue(queue);

    pthread_mutex_destroy(&lock);

    return num_occurrences;
}

