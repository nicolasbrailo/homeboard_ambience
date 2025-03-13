#include "json.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_object* json_init(const char *fpath) {
  struct json_object* json = json_object_from_file(fpath);
  if (!json) {
    fprintf(stderr, "Config fail: can't parse JSON %s\n", fpath);
    goto err;
  }

  return json;

err:
  json_free(json);
  return NULL;
}

void json_free(struct json_object *h) {
  if (!h) {
    return;
  }

  json_object_put(h);
}

static bool json_get_strdup_impl(struct json_object *h, const char *k, const char **v, bool is_optional) {
  struct json_object *n;
  if (!json_object_object_get_ex(h, k, &n)) {
    if (!is_optional) {
      fprintf(stderr, "Failed to read config: can't find str value %s\n", k);
    }
    return false;
  }

  const char* json_v = json_object_get_string(n);
  if (!json_v) {
    if (!is_optional) {
      fprintf(stderr, "Failed to read key %s, not a string\n", k);
    }
    return false;
  }

  *v = strdup(json_v);
  if (!*v) {
    if (!is_optional) {
      fprintf(stderr, "Failed to read config: can't alloc for value %s\n", k);
    }
    return false;
  }

  if (strlen(*v) == 0) {
    if (!is_optional) {
      fprintf(stderr, "Failed to read config: %s is empty\n", k);
    }
    return false;
  }

  return true;
}

bool json_get_optional_strdup(struct json_object *h, const char *k, const char **v) {
  return json_get_strdup_impl(h, k, v, true);
}

bool json_get_strdup(struct json_object *h, const char *k, const char **v) {
  return json_get_strdup_impl(h, k, v, false);
}

bool json_get_int(struct json_object *h, const char *k, int *v) {
  struct json_object *n;
  if (json_object_object_get_ex(h, k, &n)) {
    *v = json_object_get_int(n);
    return true;
  }

  return false;
}

bool json_get_size_t(struct json_object *h, const char *k, size_t *v, size_t min, size_t max) {
  int iv;
  if (!json_get_int(h, k, &iv)) {
    fprintf(stderr, "Failed to read config: can't find value %s of type size_t\n", k);
    return false;
  }

  if ((iv < 0) || ((size_t)iv < min) || ((size_t)iv > max)) {
    fprintf(stderr, "Bad config value: invalid value %d for %s, expected interval is [%zu, %zu]\n",
            iv, k, min, max);
    return false;
  }

  *v = (size_t)iv;
  return true;
}

bool json_get_bool(struct json_object *h, const char *k, bool *v) {
  struct json_object *n;
  if (json_object_object_get_ex(h, k, &n)) {
    *v = json_object_get_boolean(n);
    return true;
  }

  fprintf(stderr, "Failed to read config: can't find bool value %s\n", k);
  return false;
}

bool json_get_arr(struct json_object *h, const char *k, arr_parse_cb cb, void *usr) {
  struct json_object *arr;
  if (!json_object_object_get_ex(h, k, &arr) || !json_object_is_type(arr, json_type_array)) {
    fprintf(stderr, "Failed to read config: can't find array %s\n", k);
    return false;
  }

  const size_t arr_len = json_object_array_length(arr);
  for (size_t i = 0; i < arr_len; i++) {
    struct json_object* tmp = json_object_array_get_idx(arr, i);
    if (!cb(arr_len, i, tmp, usr)) {
      fprintf(stderr, "Failed to read config: invalid value at %s[%zu]\n", k, i);
      return false;
    }
  }

  return true;
}

const char* json_get_nested_key(struct json_object* obj, const char* key) {
  const size_t max_depth = 10;
  char subkey[32];
  size_t subkey_i = 0;
  size_t subkey_f = 0;

  for (size_t lvl = 0; lvl < max_depth; lvl++) {
    while ((key[subkey_f] != '\0') && (key[subkey_f] != '.')) {
      subkey_f++;
    }

    if (subkey_f == subkey_i) {
      fprintf(stderr, "Error retrieving metadata: requested metadata key '%s' can't be parsed\n", key);
      return NULL;

    } else if (subkey_f - subkey_i > sizeof(subkey)) {
      fprintf(stderr, "Error retrieving metadata: requested metadata key '%s' is too large to handle\n", key);
      return NULL;

    } else {
      size_t subkey_sz = subkey_f - subkey_i;
      strncpy(subkey, &key[subkey_i], subkey_sz);
      subkey[subkey_sz] = '\0';

      // Traverse json tree
      struct json_object* tmp;
      if (!json_object_object_get_ex(obj, subkey, &tmp)) {
        fprintf(stderr, "Error retrieving metadata: requested key '%s' doesn't exist\n", key);
        return NULL;
      }
      obj = tmp;

      if (key[subkey_f] == '.') {
        // We're still traversing, do nothing
      } else {
        // Found a leaf
        return json_object_get_string(obj);
      }

      subkey_f = subkey_i = subkey_f+1;
    }
  }

  fprintf(stderr, "Error retrieving metadata: requested metadata key '%s' too deeply nested, expected max %zu levels\n", key, max_depth);
  return NULL;
}

