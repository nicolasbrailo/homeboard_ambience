#pragma once

#include <stddef.h>

struct ShmHandle;

struct ShmHandle *shm_init(const char *shm_shared_fname, size_t max_sz_bytes);
void shm_free(struct ShmHandle *h);
void shm_free_leak_shm(struct ShmHandle *h);

// Copy sz bytes from data to the shm area. Will grow the shm area if needed, up
// to the configured max_sz_bytes.
// Returns 0 on success, an error code in any other case
int shm_update(struct ShmHandle *h, const void *data, size_t sz);

// Copy the contents of fpath to the shm area.
// Returns the number of bytes copied on success, 0 if nothing
// was copied, an error code in any other case
int shm_update_from_file(struct ShmHandle *h, const char *fpath);
