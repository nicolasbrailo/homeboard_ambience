#pragma once

#include <stdbool.h>
#include <stddef.h>

struct Config;

struct Config *config_init(const char *fpath);
void config_free(struct Config *h);

bool config_get_string(struct Config *h, const char *k, const char **v);
bool config_get_int(struct Config *h, const char *k, int *v);
bool config_get_size_t(struct Config *h, const char *k, size_t *v);
bool config_get_bool(struct Config *h, const char *k, bool *v);
