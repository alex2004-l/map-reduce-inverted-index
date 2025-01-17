#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <queue>
#include <pthread.h>
#include <map>
#include <unordered_map>
#include <set>

#include "utils.hpp"

#define NUM_ARGS        4
#define ALPHABET_SIZE  26

#define MUTEX_INIT_ERROR "Error while initializing mutex"
#define MUTEX_DEST_ERROR "Error while destroying the mutex"

#define COND_INIT_ERROR "Error while initializing condition"
#define COND_DEST_ERROR "Error while destroying condition"

#define BARRIER_INIT_ERROR "Error while initializing barrier"
#define BARRIER_DEST_ERROR "Error while destroying the barrier"

using namespace std;

/* Data structure shared across all the threads in the program */
struct mapper_reducer {
    vector<pair<string, int>> mapper_results;

    pthread_mutex_t cond_lock;
    pthread_cond_t mappers_finished;
    atomic_int reducer_number;
    atomic_int mapper_number;

    pthread_barrier_t barrier;

    mapper_reducer(int num_mappers, int num_reducers) {
        reducer_number = num_reducers;
        mapper_number = num_mappers;
        if (pthread_mutex_init(&cond_lock, nullptr) != 0)
            cerr << MUTEX_INIT_ERROR << endl;
        if (pthread_cond_init(&mappers_finished, nullptr) != 0)
            cerr << COND_INIT_ERROR << endl;
        if  (pthread_barrier_init(&barrier, nullptr, num_reducers) != 0)
            cerr << BARRIER_INIT_ERROR << endl;
    }

    ~mapper_reducer() {
        if (pthread_mutex_destroy(&cond_lock) != 0)
            cerr << MUTEX_DEST_ERROR << endl;
        if (pthread_cond_destroy(&mappers_finished) != 0)
            cerr << COND_DEST_ERROR << endl;
        if  (pthread_barrier_destroy(&barrier) != 0)
            cerr << BARRIER_DEST_ERROR << endl;
    }
};


/* Data structure shared across all the reducer threads */
struct reducers_shared {
    unordered_map<char, unordered_map<string, set<int>>> aggregated_results;
    pthread_mutex_t letter_lock[ALPHABET_SIZE];

    reducers_shared() {
        for (int i = 0; i < ALPHABET_SIZE; ++i) {
            if (pthread_mutex_init(&letter_lock[i], NULL) != 0)
                cerr << MUTEX_INIT_ERROR << endl;
        }

        /* Initializing each letter in the map to avoid race conditions */
        for (char letter = 'a'; letter <= 'z'; ++letter) {
            aggregated_results[letter] = unordered_map<string, set<int>>();
        }
    }

    ~reducers_shared() {
        for (int i = 0; i < ALPHABET_SIZE; ++i) {
            if (pthread_mutex_destroy(&letter_lock[i]) != 0)
                cerr << MUTEX_DEST_ERROR << endl;
        }
    }
};


/* Data structure shared across all the mapper threads */
struct mappers_shared {
    queue<string> files_ptr;
    pthread_mutex_t results_lock;
    pthread_mutex_t queue_lock;
    int file_idx;

    mappers_shared() {
        file_idx = 1;

        if (pthread_mutex_init(&results_lock, NULL) != 0)
            cerr << MUTEX_INIT_ERROR << endl;
        if (pthread_mutex_init(&queue_lock, NULL) != 0)
            cerr << MUTEX_INIT_ERROR << endl;
    }

    ~mappers_shared() {
        if (pthread_mutex_destroy(&results_lock) != 0)
            cerr << MUTEX_DEST_ERROR << endl;
        if (pthread_mutex_destroy(&queue_lock) != 0)
            cerr << MUTEX_DEST_ERROR << endl;
    }
};


/* Data structure for each reducer thread with data it has access to and id */
struct reducer {
    struct mapper_reducer *mapred;
    struct reducers_shared *red_shared;
    int idx;
};


/* Data structure for each mapper thread with data it has access to and id */
struct mapper {
    struct mapper_reducer *mapred;
    struct mappers_shared *map_shared;
    int idx;
};


/* Routine executed by mapper threads */
void *run_mapper(void *arg) {
    /* Retrieving pointers to the shared data between all threads/ mapper threads */
    mapper_reducer *mr_data = ((mapper *) arg)->mapred;
    mappers_shared *shared_data = ((mapper *) arg)->map_shared;

    string file_path;
    int file_idx;
    set<string> unique_words;
    vector<pair<string, int>> partial_results;

    while (true) {
        /* Getting the next document that needs to be processed */
        pthread_mutex_lock(&shared_data->queue_lock);
        if (shared_data->files_ptr.empty()) {
            pthread_mutex_unlock(&shared_data->queue_lock);
            break;
        }
        file_path = shared_data->files_ptr.front();
        shared_data->files_ptr.pop();
        /* Incrementing the file index */
        file_idx = shared_data->file_idx;
        shared_data->file_idx ++;
        pthread_mutex_unlock(&shared_data->queue_lock);

        /* Parsing the document and adding the words in a vector shared across all the threads */
        unique_words = parse_file(file_path);
        for (auto &elem: unique_words) {
            partial_results.push_back(make_pair(elem, file_idx));
        }
        unique_words.clear();

        pthread_mutex_lock(&shared_data->results_lock);
        mr_data->mapper_results.insert(mr_data->mapper_results.end(),
                            partial_results.begin(), partial_results.end());
        pthread_mutex_unlock(&shared_data->results_lock);
        partial_results.clear();
    }

    /* Decrementing the active mapper thread number */
    mr_data->mapper_number--;
    if (!mr_data->mapper_number)
        /* When the last mapper thread exits,
            announce the reducer threads that are waiting */
        pthread_cond_broadcast(&mr_data->mappers_finished);
    return nullptr;
}

/* Routine executed by reducer threads */
void *run_reducer(void *arg) {
    /* Retrieving pointers to the shared data between all threads/ mapper threads */
    mapper_reducer *mr_data = ((reducer *) arg)->mapred;
    reducers_shared *shared_data = ((reducer *) arg)->red_shared;
    int idx = ((reducer *) arg)->idx;

    int start, end;
    
    /* Waiting for all mapper threads to finish */
    pthread_mutex_lock(&mr_data->cond_lock);
    while (mr_data->mapper_number != 0)
        pthread_cond_wait(&mr_data->mappers_finished, &mr_data->cond_lock);
    pthread_mutex_unlock(&mr_data->cond_lock);

    /* Calculating how many words processed by mapper each 
       reducer thread will process further */
    start = idx * ((double) mr_data->mapper_results.size()) / mr_data->reducer_number;
    end = min((int)((idx + 1) * (((double) mr_data->mapper_results.size()) / mr_data->reducer_number))
                                                                , (int) mr_data->mapper_results.size());

    /* For each {word, file_idx} pair , adding the word in a hashmap based on the first letter */
    for (int i = start; i < end; ++i) {
        string word = mr_data->mapper_results[i].first;
        int file_idx = mr_data->mapper_results[i].second;
        int letter = word[0] - 'a';

        /* Only one reducer thread can modify the word map for a letter at a time */
        /* They can concurently modify values for different keys */
        /* Here happens index aggregation for each word */
        pthread_mutex_lock(&shared_data->letter_lock[letter]);
        shared_data->aggregated_results[word[0]][word].insert(file_idx);
        pthread_mutex_unlock(&shared_data->letter_lock[letter]);
    }

    /* Determinig for how many letters the current thread will write the output files */
    start = 1.0 * idx * ALPHABET_SIZE / mr_data->reducer_number;
    end = min((int) (1.0 * (idx + 1) * ALPHABET_SIZE / mr_data->reducer_number), ALPHABET_SIZE);

    /* Waiting for all the words to be processed */
    pthread_barrier_wait(&mr_data->barrier);

    /* Writing the files */
    for (char i = 'a' + start; i < 'a' + end; ++i) {
        string filename = "";
        filename += i;
        filename += ".txt";
        write_output_file(filename, shared_data->aggregated_results[i]);
    }

    return nullptr;
}


int main(int argc, char * argv[]) {
    /* Checking wheter the arguments received are correct */
    if (argc != NUM_ARGS) {
        cerr << "Incorrect usage." <<
            " Please use: ./tema1 <numar_mapperi> <numar_reduceri> <fisier_intrare>" << endl;
        exit(-1);
    }

    int num_mappers = atoi(argv[1]), num_reducers = atoi(argv[2]);

    if (num_mappers <= 0 || num_reducers <= 0) {
        if (num_mappers <= 0)
            cerr << "<numar_mapperi> must be a positive integer" << endl;
        if (num_reducers <= 0)
            cerr << "<numar_reduceri> must be a positive integer" << endl;
        exit(-1);
    }

    pthread_t threads[num_mappers + num_reducers];
    void *status;

    /* Initializing shared data across threads */
    mapper_reducer  *shared_mapper_reducer = new mapper_reducer(num_mappers, num_reducers);
    reducers_shared *reducers_shared_data  = new reducers_shared();
    mappers_shared  *mappers_shared_data   = new mappers_shared();

    mappers_shared_data->files_ptr = parse_input_file(argv[3]);

    /* Initializing data transmitted to each individual thread */
    vector<reducer> reducer_data(num_reducers);
    vector<mapper>  mapper_data (num_mappers) ;

    for (int i = 0; i < num_reducers; ++i) {
        reducer_data[i].idx = i;
        reducer_data[i].mapred = shared_mapper_reducer;
        reducer_data[i].red_shared = reducers_shared_data;
    }

    for (int i = 0; i < num_mappers; ++i) {
        mapper_data[i].idx = i;
        mapper_data[i].map_shared = mappers_shared_data;
        mapper_data[i].mapred = shared_mapper_reducer;
    }

    /* Creating mapper and reducer threads */
    for (int i = 0; i < num_mappers + num_reducers; ++i)
        if (i < num_mappers)
            pthread_create(&threads[i], nullptr, run_mapper, (void *) &mapper_data[i]);
        else
            pthread_create(&threads[i], nullptr, run_reducer, (void *)&reducer_data[i - num_mappers]);

    /* Joining all the threads */
    for (int i = 0; i < num_mappers + num_reducers; ++i) {
        pthread_join(threads[i], &status);
    }

    /* Deleting allocated data */
    delete shared_mapper_reducer;
    delete reducers_shared_data;
    delete mappers_shared_data;

    return 0;
}