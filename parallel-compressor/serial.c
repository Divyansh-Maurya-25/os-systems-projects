// Project 2 — Parallel Text Compressor
// Group Members:  Divyansh Maurya  Arjan Subedi  Ayush Poudel  Aidan Lauser
// Description: Multithreaded text file compressor using pthreads and zlib
// Techniques included: thread pool (≤19 workers), per-thread reusable z_stream
// with deflateReset, pre-allocated per-thread buffers, job counter with light
// locking, lexicographic ordering, bounded memory and thread count checks.

#include <dirent.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1048576
#define MAX_WORKERS 19

// Compare function for sorting filenames
static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(char * const *)a, *(char * const *)b);
}

// Structure to hold file information
typedef struct {
    char *fullpath;
} Job;

// Structure to store compression results
typedef struct {
    int in_bytes;
    int out_bytes;
    unsigned char *outbuf;
} Result;

// Global variables for work distribution
static Job *jobs;
static Result *results;
static int num_jobs;

static int next_job = 0;
static pthread_mutex_t job_lock;

// Thread-local buffers and compression stream
typedef struct {
    unsigned char *input_buffer;
    unsigned char *output_buffer;
    z_stream stream;
} ThreadData;

// Worker thread function
static void *worker_thread(void *arg) {
    ThreadData *data = (ThreadData*)arg;

    // Initialize compression stream once per thread
    memset(&data->stream, 0, sizeof(z_stream));
    int ret = deflateInit(&data->stream, Z_BEST_COMPRESSION);
    assert(ret == Z_OK);

    while (1) {
        // Get next job index
        pthread_mutex_lock(&job_lock);
        int job_idx = next_job;
        if (job_idx < num_jobs)
            next_job++;
        pthread_mutex_unlock(&job_lock);

        if (job_idx >= num_jobs)
            break;

        Job *current_job = &jobs[job_idx];
        Result *current_result = &results[job_idx];

        // Read input file (single-chunk up to BUFFER_SIZE, as in starter)
        FILE *file = fopen(current_job->fullpath, "rb");
        if (!file) {
            current_result->in_bytes = 0;
            current_result->out_bytes = 0;
            continue;
        }

        int bytes_read = (int)fread(data->input_buffer, 1, BUFFER_SIZE, file);
        // If fread fails, bytes_read will be < 0; guard with clamp
        if (bytes_read < 0) bytes_read = 0;
        fclose(file);

        // Reset stream for reuse first, then set IO pointers
        int r2 = deflateReset(&data->stream);
        assert(r2 == Z_OK);

        data->stream.avail_in = (uInt)bytes_read;
        data->stream.next_in = data->input_buffer;
        data->stream.avail_out = BUFFER_SIZE;
        data->stream.next_out = data->output_buffer;

        // Compress data
        ret = deflate(&data->stream, Z_FINISH);
        assert(ret == Z_STREAM_END);

        int compressed_size = (int)(BUFFER_SIZE - data->stream.avail_out);
        if (compressed_size < 0) compressed_size = 0;
        if (compressed_size > BUFFER_SIZE) compressed_size = BUFFER_SIZE;

        // Store results
        current_result->in_bytes = bytes_read;
        current_result->out_bytes = compressed_size;
        if (compressed_size > 0)
            memcpy(current_result->outbuf, data->output_buffer, (size_t)compressed_size);
    }

    deflateEnd(&data->stream);
    return NULL;
}

// Main compression function
void compress_directory(char *directory_name) {

    // Open directory
    DIR *dir = opendir(directory_name);
    if (!dir) {
        printf("An error has occurred\n");
        return;
    }

    // Collect all .txt files
    char **filenames = NULL;
    int file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        int len = (int)strlen(entry->d_name);
        if (len >= 4 &&
            entry->d_name[len-4] == '.' &&
            entry->d_name[len-3] == 't' &&
            entry->d_name[len-2] == 'x' &&
            entry->d_name[len-1] == 't')
        {
            char **tmp = realloc(filenames, (size_t)(file_count+1) * sizeof(char*));
            if (!tmp) { // graceful out-of-memory path
                closedir(dir);
                // Cleanup so far
                for (int k = 0; k < file_count; k++) free(filenames[k]);
                free(filenames);
                printf("An error has occurred\n");
                return;
            }
            filenames = tmp;
            filenames[file_count] = strdup(entry->d_name);
            if (!filenames[file_count]) {
                closedir(dir);
                for (int k = 0; k < file_count; k++) free(filenames[k]);
                free(filenames);
                printf("An error has occurred\n");
                return;
            }
            file_count++;
        }
    }
    closedir(dir);

    // Handle empty directory
    if (file_count == 0) {
        FILE *empty = fopen("text.tzip", "wb");
        if (empty) fclose(empty);
        printf("Compression rate: 0.00%%\n");
        free(filenames);
        return;
    }

    // Sort filenames lexicographically
    qsort(filenames, file_count, sizeof(char*), cmp_strptr);

    // Allocate job and result arrays
    num_jobs = file_count;
    jobs = (Job*)calloc((size_t)file_count, sizeof(Job));
    results = (Result*)calloc((size_t)file_count, sizeof(Result));
    if (!jobs || !results) {
        free(jobs);
        free(results);
        for (int i = 0; i < file_count; i++) free(filenames[i]);
        free(filenames);
        printf("An error has occurred\n");
        return;
    }

    // Build full paths and allocate output buffers
    for (int i = 0; i < file_count; i++) {
        size_t path_size = strlen(directory_name) + 1 + strlen(filenames[i]) + 1;
        jobs[i].fullpath = (char*)malloc(path_size);
        if (!jobs[i].fullpath) {
            // Cleanup partial allocations
            for (int k = 0; k <= i; k++) {
                if (k < i) free(results[k].outbuf);
                free(jobs[k].fullpath);
            }
            free(jobs);
            free(results);
            for (int k = 0; k < file_count; k++) free(filenames[k]);
            free(filenames);
            printf("An error has occurred\n");
            return;
        }
        sprintf(jobs[i].fullpath, "%s/%s", directory_name, filenames[i]);

        results[i].outbuf = (unsigned char*)malloc(BUFFER_SIZE);
        if (!results[i].outbuf) {
            for (int k = 0; k <= i; k++) {
                free(results[k].outbuf);
                free(jobs[k].fullpath);
            }
            free(jobs);
            free(results);
            for (int k = 0; k < file_count; k++) free(filenames[k]);
            free(filenames);
            printf("An error has occurred\n");
            return;
        }
    }

    // Initialize mutex and counter
    pthread_mutex_init(&job_lock, NULL);
    next_job = 0;

    // Determine number of worker threads
    int num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) num_cores = 4;

    int num_workers = num_cores * 2;
    if (num_workers > MAX_WORKERS) num_workers = MAX_WORKERS;
    if (num_workers > file_count) num_workers = file_count;
    if (num_workers < 1) num_workers = 1; // at least one thread when files exist

    // Create worker threads
    pthread_t threads[MAX_WORKERS];
    ThreadData thread_data[MAX_WORKERS];

    for (int i = 0; i < num_workers; i++) {
        thread_data[i].input_buffer = (unsigned char*)malloc(BUFFER_SIZE);
        thread_data[i].output_buffer = (unsigned char*)malloc(BUFFER_SIZE);
        if (!thread_data[i].input_buffer || !thread_data[i].output_buffer) {
            if (thread_data[i].input_buffer) free(thread_data[i].input_buffer);
            if (thread_data[i].output_buffer) free(thread_data[i].output_buffer);
            // Join previously launched threads
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            pthread_mutex_destroy(&job_lock);
            for (int k = 0; k < file_count; k++) {
                free(jobs[k].fullpath);
                free(results[k].outbuf);
            }
            for (int k = 0; k < file_count; k++) free(filenames[k]);
            free(filenames);
            free(jobs);
            free(results);
            printf("An error has occurred\n");
            return;
        }
        int rc = pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
        if (rc != 0) {
            // If thread creation fails, free buffers and continue with fewer workers
            free(thread_data[i].input_buffer);
            free(thread_data[i].output_buffer);
            num_workers = i; // only the ones already created will run
            break;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_workers; i++) {
        pthread_join(threads[i], NULL);
        free(thread_data[i].input_buffer);
        free(thread_data[i].output_buffer);
    }

    pthread_mutex_destroy(&job_lock);

    // Write compressed archive
    long long total_input = 0, total_output = 0;

    FILE *archive = fopen("text.tzip", "wb");
    if (!archive) {
        // Cleanup and return (don’t crash)
        for (int i = 0; i < file_count; i++) {
            free(jobs[i].fullpath);
            free(results[i].outbuf);
            free(filenames[i]);
        }
        free(filenames);
        free(jobs);
        free(results);
        printf("An error has occurred\n");
        return;
    }

    for (int i = 0; i < file_count; i++) {
        int size = results[i].out_bytes;
        if (size < 0) size = 0;
        fwrite(&size, sizeof(int), 1, archive);
        if (size > 0) fwrite(results[i].outbuf, 1, (size_t)size, archive);

        total_input += results[i].in_bytes;
        total_output += results[i].out_bytes;
    }
    fclose(archive);

    // Calculate and print compression rate
    double compression_rate = (total_input == 0 ? 0.0 :
        100.0 * (double)(total_input - total_output) / (double)total_input);

    printf("Compression rate: %.2lf%%\n", compression_rate);

    // Cleanup
    for (int i = 0; i < file_count; i++) {
        free(jobs[i].fullpath);
        free(results[i].outbuf);
        free(filenames[i]);
    }

    free(filenames);
    free(jobs);
    free(results);
}
