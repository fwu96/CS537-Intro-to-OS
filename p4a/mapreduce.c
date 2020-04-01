#include "mapreduce.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Key-Value pair
struct pairs {
    char *key;
    char *val;
};
// files
struct files {
    char *name;
};
// Global variables
struct pairs **partitions;
struct files *files_name;
int *pair_count;
int *pair_alloc;
int *pair_acces;
pthread_mutex_t lock, file_lock;
Partitioner part;
Reducer redu;
Mapper mapp;
Combiner comb;
int parts_num, file_proc, file_total;

// Helper function for mapper when pthread_create
void* init_map (void *arg) {
    while (file_proc < file_total) {
        pthread_mutex_lock(&file_lock);
        char *filename = NULL;
        if (file_proc < file_total) {
            filename = files_name[file_proc].name;
            file_proc++;
        }
        pthread_mutex_unlock(&file_lock);
        if (filename != NULL)
            mapp(filename);
    }
    return arg;
}
// get_next for Reducers
char* next_redu (char *key, int part_num) {
    int access_num = pair_acces[part_num];
    if (access_num < pair_count[part_num] && strcmp(key, partitions[part_num][access_num].key) == 0) {
        pair_acces[part_num]++;
        return partitions[part_num][access_num].val;
    } else {
        return NULL;
    }
}
// Helper function for Reducer when pthread_create
void* init_redu (void *arg) {
    int *part_num = (int*)arg;
    for (int i = 0; i < pair_count[*part_num]; i++) {
        if (i == pair_acces[*part_num]) {
            redu(partitions[*part_num][i].key, NULL, next_redu, *part_num);
        }
    }
    return arg;
}
// key-value comparison
int comp_pair (const void *p1, const void *p2) {
    struct pairs *kv1 = (struct pairs*)p1;
    struct pairs *kv2 = (struct pairs*)p2;
    if (strcmp(kv1->key, kv2->key) == 0) {
        return strcmp(kv1->val, kv2->val);
    }
    return strcmp(kv1->key, kv2->key);
}
// sorting files
int sort_files (const void *p1, const void *p2) {
    struct files *f1 = (struct files*)p1;
    struct files *f2 = (struct files*)p2;
    struct stat st1, st2;
    stat(f1->name, &st1);
    stat(f2->name, &st2);
    long int s1 = st1.st_size, s2 = st2.st_size;
    return (s1 - s2);
}
// EmitToReducer
void MR_EmitToReducer (char *key, char *value) {
    pthread_mutex_lock(&lock);
    unsigned long hidx = part(key, parts_num);
    pair_count[hidx]++;
    int crr_cnt = pair_count[hidx];
    if (crr_cnt > pair_alloc[hidx]) {  // count exceeds allocated space
        pair_alloc[hidx] *= 2;
        partitions[hidx] = (struct pairs*)realloc(partitions[hidx], pair_alloc[hidx] * sizeof(struct pairs));
    }
    partitions[hidx][crr_cnt - 1].key = (char*)malloc((strlen(key) + 1) * sizeof(char));
    strcpy(partitions[hidx][crr_cnt - 1].key, key);
    partitions[hidx][crr_cnt - 1].val = (char*)malloc((strlen(value) + 1) * sizeof(char));
    strcpy(partitions[hidx][crr_cnt - 1].val, value);
    pthread_mutex_unlock(&lock);
}
// Defalue Emit
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0') 
        hash = hash * 33 + c;
    return hash % num_partitions;
}
// MR_Run
void MR_Run (int argc, char *argv[],
             Mapper map, int num_mappers,
             Reducer reduce, int num_reducers,
             Combiner combine,
             Partitioner partition) {
    pthread_t t_mappers[num_mappers], t_reducers[num_reducers];
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&file_lock, NULL);
    part = partition;
    mapp = map;
    redu = reduce;
    comb = combine;
    parts_num = num_reducers;
    partitions = malloc(num_reducers * sizeof(struct pairs*));
    files_name = malloc((argc - 1) * sizeof(struct files));
    pair_count = malloc(num_reducers * sizeof(int));
    pair_alloc = malloc(num_reducers * sizeof(int));
    pair_acces = malloc(num_reducers * sizeof(int));
    file_proc = 0;
    file_total = argc - 1;
    int idx_arr[num_reducers];
    // initializing values
    for (int i = 0; i < num_reducers; i++) {
        partitions[i] = malloc(1024 * sizeof(struct pairs));
        pair_count[i] = 0;
        pair_alloc[i] = 1024;
        idx_arr[i] = i;
        pair_acces[i] = 0;
    }
    // sorting files
    for (int i = 0; i < argc - 1; i++) {
        files_name[i].name = malloc(strlen(argv[i + 1] + 1) * sizeof(char));
        strcpy(files_name[i].name, argv[i + 1]);
    }
    qsort(&files_name[0], argc - 1, sizeof(struct files), sort_files);
    // mapper threads
    for (int i = 0; i < num_mappers; i++) {
        pthread_create(&t_mappers[i], NULL, init_map, NULL);
    }
    for (int i = 0; i < num_mappers; i++) {
        pthread_join(t_mappers[i], NULL);
    }
    // sorting k-v pairs
    for (int i = 0; i < num_reducers; i++) {
        qsort(partitions[i], pair_count[i], sizeof(struct pairs), comp_pair);
    }
    // reducer threads
    for (int i = 0; i < num_reducers; i++) {
        if (pthread_create(&t_reducers[i], NULL, init_redu, &idx_arr[i])) {
            printf("Error\n");
        }
    }
    for (int i = 0; i < num_reducers; i++) {
        pthread_join(t_reducers[i], NULL);
    }
    // free memories
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&file_lock);
    for (int i = 0; i < num_reducers; i++) {
        for (int j = 0; j < pair_count[i]; j++) {
            if (partitions[i][j].key != NULL && partitions[i][j].val != NULL) {
                free(partitions[i][j].key);
                free(partitions[i][j].val);
            }
        }
        free(partitions[i]);
    }
    for (int i = 0; i < argc - 1; i++) {
        free(files_name[i].name);
    }
    free(partitions);
    free(files_name);
    free(pair_count);
    free(pair_alloc);
    free(pair_acces);
}
