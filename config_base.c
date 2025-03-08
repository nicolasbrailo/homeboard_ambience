#include "config_base.h"

#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>

struct Config {
  struct json_object *json;
};

struct Config *config_init(const char *fpath) {
  struct Config *h = malloc(sizeof(struct Config));
  if (!h) {
    perror("config: malloc");
    goto err;
  }

  h->json = json_object_from_file(fpath);
  if (!h->json) {
    fprintf(stderr, "Config fail: can't parse JSON %s\n", fpath);
    goto err;
  }

  return h;

err:
  config_free(h);
  return NULL;
}

void config_free(struct Config *h) {
  if (!h) {
    return;
  }

  json_object_put(h->json);
  free(h);
}

bool config_get_string(struct Config *h, const char *k, const char **v) {
  struct json_object *n;
  if (json_object_object_get_ex(h->json, k, &n)) {
    *v = json_object_get_string(n);
    return true;
  }

  return false;
}

bool config_get_int(struct Config *h, const char *k, int* v) {
  struct json_object *n;
  if (json_object_object_get_ex(h->json, k, &n)) {
    *v = json_object_get_int(n);
    return true;
  }

  return false;
}

bool config_get_size_t(struct Config *h, const char *k, size_t* v) {
  int iv;
  if (!config_get_int(h, k, &iv)) {
    return false;
  }

  if (iv < 0) {
    return false;
  }

  *v = (size_t)iv;
  return true;
}

bool config_get_bool(struct Config *h, const char *k, bool* v) {
  struct json_object *n;
  if (json_object_object_get_ex(h->json, k, &n)) {
    *v = json_object_get_boolean(n);
    return true;
  }

  return false;
}

