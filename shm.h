#pragma once

#include <stddef.h>

struct ShmHandle;

struct ShmHandle* shm_init(const char* shm_shared_fname, size_t max_sz_bytes);
void shm_free(struct ShmHandle* h);
void shm_free_leak_shm(struct ShmHandle* h);

int shm_update(struct ShmHandle* h, const void* data, size_t sz);
int shm_update_from_file(struct ShmHandle* h, const char* fpath);


