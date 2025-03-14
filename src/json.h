#pragma once

#include <stdbool.h>
#include <stddef.h>

struct json_object;

struct json_object *json_init(const char *fpath);
void json_free(struct json_object *h);
bool json_get_strdup(struct json_object *h, const char *k, const char **v);
bool json_get_optional_strdup(struct json_object *h, const char *k,
                              const char **v);
bool json_get_int(struct json_object *h, const char *k, int *v);
bool json_get_size_t(struct json_object *h, const char *k, size_t *v,
                     size_t min, size_t max);
bool json_get_bool(struct json_object *h, const char *k, bool *v);

// Invoke a callback for each element of an array
typedef bool (*arr_parse_cb)(size_t arr_len, size_t idx, struct json_object *,
                             void *usr);
bool json_get_arr(struct json_object *h, const char *k, arr_parse_cb cb,
                  void *usr);

// Retrieve a string key from a nested path, eg "foo.bar.baz" will return "baz"
// as a string Ownership retained by this module
const char *json_get_nested_key(struct json_object *obj, const char *key);
