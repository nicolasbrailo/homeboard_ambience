#include "shm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// shm sz can't ever be 0 for mmap
#define MIN_SHM_SZ 1

struct ShmHandle {
  const char* fname;
  int fd;
  void* ptr;
  size_t sz;
  size_t max_sz;
  bool should_leak_shm;
};


void shm_free(struct ShmHandle* h) {
  if (!h) {
    return;
  }

  if (h->ptr) {
    munmap(h->ptr, h->sz);
  }

  if (!h->should_leak_shm) {
    shm_unlink(h->fname);
  }

  free((void*)h->fname);
  close(h->fd);
}

void shm_free_leak_shm(struct ShmHandle* h) {
  h->should_leak_shm = true;
  shm_free(h);
}

struct ShmHandle* shm_init(const char* shm_shared_fname, size_t max_sz_bytes) {
  struct ShmHandle *h = malloc(sizeof(struct ShmHandle));
  if (!h) {
    perror("shm: handle, bad alloc");
    goto err;
  }

  h->fname = strdup(shm_shared_fname);
  if (!h->fname) {
    perror("shm: fname, bad alloc");
    goto err;
  }

  h->fd = 0;
  h->ptr = NULL;
  h->sz = MIN_SHM_SZ;
  h->max_sz = max_sz_bytes;
  h->should_leak_shm = false;

  h->fd = shm_open(shm_shared_fname, O_CREAT | O_RDWR, 0666);
  if (h->fd < 0) {
    perror("shm: can't open");
    goto err;
  }

  if (ftruncate(h->fd, h->sz) < 0) {
    perror("shm: can't resize");
    goto err;
  }

  h->ptr = mmap(NULL, h->sz, PROT_READ | PROT_WRITE, MAP_SHARED, h->fd, 0);
  if (h->ptr == MAP_FAILED) {
    perror("shm: can't mmap");
    goto err;
  }

  return h;

err:
  shm_free(h);
  return NULL;
}

int shm_update(struct ShmHandle* h, const void* data, size_t sz) {
  if (sz > h->max_sz) {
    fprintf(stderr, "Requested shm data of %zu is bigger than max size %zu\n", sz, h->max_sz);
    return -ENOMEM;
  }

  if (sz < MIN_SHM_SZ) {
    sz = MIN_SHM_SZ;
  }

  if (h->sz < sz) {
    h->sz = sz;
    if (ftruncate(h->fd, h->sz) < 0) {
      perror("shm: can't resize");
      return -ENOMEM;
    }

    h->ptr = mmap(h->ptr, h->sz, PROT_READ | PROT_WRITE, MAP_SHARED, h->fd, 0);
    if (h->ptr == MAP_FAILED) {
      perror("shm: can't remmap");
      return -ENOMEM;
    }
  }

  if (data) {
    memcpy(h->ptr, data, sz);
  }

  return 0;
}

int shm_update_from_file(struct ShmHandle* h, const char* fpath) {
  FILE *fp = fopen(fpath, "rb");
  if (!fp) {
    perror("shm update: can't open file");
    return -ENOENT;
  }

  struct stat file_stat;
  int is_file = fstat(fileno(fp), &file_stat);
  if (is_file < 0) {
    fprintf(stderr, "shm update: not a file %s\n", fpath);
    fclose(fp);
    return is_file;
  }

  if (file_stat.st_size == 0) {
    fprintf(stderr, "shm update: empty file %s\n", fpath);
    fclose(fp);
    return -ENOENT;
  }

  const int sz_update_ret = shm_update(h, NULL, file_stat.st_size);
  if (sz_update_ret != 0) {
    fclose(fp);
    return sz_update_ret;
  }

  const long read_sz = fread(h->ptr, 1, file_stat.st_size, fp);
  if (read_sz != file_stat.st_size) {
    fclose(fp);
    perror("shm update: can't read src file");
    return -errno;
  }

  fclose(fp);
  return file_stat.st_size;
}

