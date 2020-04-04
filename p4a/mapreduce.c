#include "mapreduce.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Key-Value pair
struct pairs_t {
    char *key;
    char *val;
};
// files_t
struct files_t {
    char *name;
};
// threads_t (mapper)
struct threads_t {
    pthread_t tid;
    int loc;
}
// Global variables
struct pairs_t **partitions;
struct files_t *files_name;
struct threads_t *ts;
int *pair_count;
int *pair_alloc;
int *pair_acces;
pthread_mutex_t lock, file_lock;
Partitioner part;
Reducer redu;
Mapper mapp;
Combiner comb;
int parts_num, file_proc, file_total;
int mappers;

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
// get_next for Combiner
char* next_comb (char *key) {
    int h_idx = 0;
    for (int i = 0; i < mappers; i++) {
        if (pthread_equal(pthread_self(), ts[i].tid))
            h_idx = ts[i].loc;
    }
    int idx = pair_acces[h_idx];
    if (idx < pair_count[h_idx] && strcmp(key, partitions[h_idx][idx].key) == 0) {
        pair_acces[h_idx]++;
        return partitions[h_idx][idx].val;
    } else {
        return NULL;
    }
}
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
        if (comb != NULL) {
            int *h_idx = (int*)arg;
            for (int i = 0; i < pair_count[*h_idx]; i++) {
                if (i == pair_acces[*h_idx]) {
                    comb(partitions[*h_idx][i].key, next_comb);
                }
            }
        }
    }
    return arg;
}
// key-value comparison
int comp_pair (const void *p1, const void *p2) {
    struct pairs_t *kv1 = (struct pairs_t*)p1;
    struct pairs_t *kv2 = (struct pairs_t*)p2;
    if (strcmp(kv1->key, kv2->key) == 0) {
        return strcmp(kv1->val, kv2->val);
    }
    return strcmp(kv1->key, kv2->key);
}
// sorting files_t
int sort_files (const void *p1, const void *p2) {
    struct files_t *f1 = (struct files_t*)p1;
    struct files_t *f2 = (struct files_t*)p2;
    struct stat st1, st2;
    stat(f1->name, &st1);
    stat(f2->name, &st2);
    long int s1 = st1.st_size, s2 = st2.st_size;
    return (s1 - s2);
}
// EmitToCombiner
void MR_EmitToCombiner (char *key, char *value) {
    pthread_mutex_lock(&lock);
    unsigned long h_idx = part_func(key, parts_num);
    pair_count[h_idx]++;
    int cnt = pair_count[h_idx];
    if (cnt > pair_alloc[h_idx]) {
        pair_alloc[h_idx] *= 2;
        partitions[h_idx] = (struct pairs*)realloc(partitions[h_idx], pair_alloc[h_idx] * sizeof(struct pairs));
    }
    partitions[h_idx][cnt - 1].key = (char*)malloc((strlen(key) + 1) * sizeof(char));
    partitions[h_idx][cnt - 1].val = (char*)malloc((strlen(value) + 1) * sizeof(char));
    strcpy(partitions[h_idx][cnt - 1].key, key);
    strcpy(partitions[h_idx][cnt - 1].val, value);
    pthread_mutex_unlock(&lock);
}
// EmitToReducer
void MR_EmitToReducer (char *key, char *value) {
    pthread_mutex_lock(&lock);
    unsigned long hidx = part(key, parts_num);
    pair_count[hidx]++;
    int crr_cnt = pair_count[hidx];
    if (crr_cnt > pair_alloc[hidx]) {  // count exceeds allocated space
        pair_alloc[hidx] *= 2;
        partitions[hidx] = (struct pairs_t*)realloc(partitions[hidx], pair_alloc[hidx] * sizeof(struct pairs_t));
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
    partitions = malloc(num_reducers * sizeof(struct pairs_t*));
    files_name = malloc((argc - 1) * sizeof(struct files_t));
    ts = malloc(num_mappers * sizeof(struct threads_t));
    pair_count = malloc(num_reducers * sizeof(int));
    pair_alloc = malloc(num_reducers * sizeof(int));
    pair_acces = malloc(num_reducers * sizeof(int));
    file_proc = 0;
    file_total = argc - 1;
    mappers = num_mappers;
    int idx_arr[num_reducers];
    // initializing values
    for (int i = 0; i < num_reducers; i++) {
        partitions[i] = malloc(1024 * sizeof(struct pairs_t));
        pair_count[i] = 0;
        pair_alloc[i] = 1024;
        idx_arr[i] = i;
        pair_acces[i] = 0;
    }
    // sorting files_t
    for (int i = 0; i < argc - 1; i++) {
        files_name[i].name = malloc((strlen(argv[i + 1]) + 1) * sizeof(char));
        strcpy(files_name[i].name, argv[i + 1]);
    }
    qsort(&files_name[0], argc - 1, sizeof(struct files_t), sort_files);
    // mapper threads_t
    for (int i = 0; i < num_mappers; i++) {
        pthread_create(&t_mappers[i], NULL, init_map, &i);
        ts[i].loc = i;
        ts[i].tid = t_mappers[i];
    }
    for (int i = 0; i < num_mappers; i++) {
        pthread_join(t_mappers[i], NULL);
    }
    // sorting k-v pairs_t
    for (int i = 0; i < num_reducers; i++) {
        qsort(partitions[i], pair_count[i], sizeof(struct pairs_t), comp_pair);
    }
    // reducer threads_t
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
    free(ts);
    free(pair_count);
    free(pair_alloc);
    free(pair_acces);
}
