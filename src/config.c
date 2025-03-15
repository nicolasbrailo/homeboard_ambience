#include "config.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define IMG_MIN_SIZE_PX 300
#define IMG_MAX_SIZE_PX 6000
#define SHM_IMAGE_MAX_SIZE_BYTES 50 * 1024 * 1024
#define SHM_IMAGE_MIN_SIZE_BYTES 2 * 1024 * 1024
#define SLIDESHOW_SLEEP_TIME_SEC_MIN 5
#define SLIDESHOW_SLEEP_TIME_SEC_MAX 1000

static bool file_is_valid(const char *fpath) {
  FILE *fp = fopen(fpath, "rb");
  if (!fp) {
    return false;
  }

  bool ret = true;
  struct stat file_stat;
  int is_file = fstat(fileno(fp), &file_stat);
  if (is_file < 0 || file_stat.st_size == 0) {
    ret = false;
  }

  fclose(fp);
  return ret;
}

static bool cfg_parse_image_metadata_keys(size_t arr_len, size_t idx,
                                          struct json_object *obj, void *usr) {
  struct AmbienceSvcConfig *cfg = usr;
  if ((cfg->image_metadata_keys_count > 0) &&
      (cfg->image_metadata_keys_count != arr_len)) {
    fprintf(stderr,
            "Config err: image_metadata_keys changed size unexpectedly, found "
            "%zu, expected %zu\n",
            arr_len, cfg->image_metadata_keys_count);
    return false;
  }

  if (cfg->image_metadata_keys_count == 0) {
    if (cfg->image_metadata_keys != NULL) {
      fprintf(stderr, "Config err: bug, image_metadata_keys already alloc?\n");
      return false;
    }

    cfg->image_metadata_keys_count = arr_len;
    const size_t sz = sizeof(const char *) * arr_len;
    cfg->image_metadata_keys = malloc(sz);
    if (!cfg->image_metadata_keys) {
      fprintf(stderr, "Config err: image_metadata_keys bad alloc\n");
      return false;
    }

    memset(cfg->image_metadata_keys, 0, sz);
  }

  return jsonobj_strdup(obj, &cfg->image_metadata_keys[idx]);
}

struct AmbienceSvcConfig *ambiencesvc_config_init(const char *fpath) {
  struct json_object *json = NULL;
  struct AmbienceSvcConfig *cfg = malloc(sizeof(struct AmbienceSvcConfig));
  if (!cfg) {
    goto err;
  }

  cfg->image_metadata_keys = NULL;
  cfg->image_metadata_keys_count = 0;
  cfg->www_svc_url = NULL;
  cfg->www_client_id = NULL;
  cfg->shm_image_file_name = NULL;
  cfg->shm_leak_image_path = NULL;
  cfg->image_render_proc_name = NULL;
  cfg->eink_save_render_to_png_file = NULL;

  json = json_init(fpath);
  if (!json) {
    goto err;
  }

  bool ok = true;
  ok &= json_get_size_t(json, "image_target_width", &cfg->image_target_width,
                        IMG_MIN_SIZE_PX, IMG_MAX_SIZE_PX);
  ok &= json_get_size_t(json, "image_target_height", &cfg->image_target_height,
                        IMG_MIN_SIZE_PX, IMG_MAX_SIZE_PX);
  ok &= json_get_bool(json, "image_embed_qr", &cfg->image_embed_qr);
  ok &= json_get_bool(json, "image_request_standalone_qr",
                      &cfg->image_request_standalone_qr);
  ok &= json_get_bool(json, "image_request_metadata",
                      &cfg->image_request_metadata);
  ok &= json_get_arr(json, "image_metadata_keys", cfg_parse_image_metadata_keys,
                     cfg);
  ok &= json_get_strdup(json, "www_svc_url", &cfg->www_svc_url);
  ok &= json_get_strdup(json, "www_client_id", &cfg->www_client_id);
  ok &= json_get_strdup(json, "shm_image_file_name", &cfg->shm_image_file_name);
  ok &= json_get_size_t(json, "shm_image_max_size_bytes",
                        &cfg->shm_image_max_size_bytes,
                        SHM_IMAGE_MIN_SIZE_BYTES, SHM_IMAGE_MAX_SIZE_BYTES);
  ok &= json_get_bool(json, "shm_leak_file", &cfg->shm_leak_file);
  ok &= json_get_strdup(json, "shm_leak_image_path", &cfg->shm_leak_image_path);
  ok &= json_get_strdup(json, "image_render_proc_name",
                        &cfg->image_render_proc_name);
  ok &= json_get_size_t(
      json, "slideshow_sleep_time_sec", &cfg->slideshow_sleep_time_sec,
      SLIDESHOW_SLEEP_TIME_SEC_MIN, SLIDESHOW_SLEEP_TIME_SEC_MAX);
  ok &= json_get_bool(json, "eink_mock_display", &cfg->eink_mock_display);
  ok &= json_get_optional_strdup(json, "eink_save_render_to_png_file",
                                 &cfg->eink_save_render_to_png_file);
  if (!ok) {
    goto err;
  }

  if (cfg->shm_leak_file && !file_is_valid(cfg->shm_leak_image_path)) {
    fprintf(stderr,
            "Config err: shm_leak_image_path must point to a valid file, can't "
            "open %s\n",
            cfg->shm_leak_image_path);
    goto err;
  }

  if (cfg->image_request_metadata && (cfg->image_metadata_keys_count == 0)) {
    fprintf(stderr, "Config err: image_request_metadata is set, but no "
                    "image_metadata_keys defined\n");
    goto err;
  }

  json_free(json);
  return cfg;

err:
  json_free(json);
  ambiencesvc_config_free(cfg);
  return NULL;
}

void ambiencesvc_config_free(struct AmbienceSvcConfig *h) {
  if (!h) {
    return;
  }

  if (h->image_metadata_keys) {
    for (size_t i = 0; i < h->image_metadata_keys_count; ++i) {
      free((void*)h->image_metadata_keys[i]);
    }
    free(h->image_metadata_keys);
  }
  free((void *)h->www_svc_url);
  free((void *)h->www_client_id);
  free((void *)h->shm_image_file_name);
  free((void *)h->shm_leak_image_path);
  free((void *)h->image_render_proc_name);
  free((void*)h->eink_save_render_to_png_file);
  free(h);
}

void ambiencesvc_config_print(struct AmbienceSvcConfig *h) {
  printf("AmbienceSvcConfig {\n");
  printf("\timage_target_width=%zu,\n", h->image_target_width);
  printf("\timage_target_height=%zu,\n", h->image_target_height);
  printf("\timage_embed_qr=%d,\n", h->image_embed_qr);
  printf("\timage_request_standalone_qr=%d,\n", h->image_request_standalone_qr);
  printf("\timage_request_metadata=%d,\n", h->image_request_metadata);
  printf("\timage_metadata_keys_count=%zu,\n", h->image_metadata_keys_count);
  for (size_t i = 0; i < h->image_metadata_keys_count; ++i) {
    printf("\timage_metadata_keys[%zu]=\"%s\",\n", i,
           h->image_metadata_keys[i]);
  }
  printf("\twww_svc_url=%s,\n", h->www_svc_url);
  printf("\twww_client_id=%s,\n", h->www_client_id);
  printf("\tshm_image_file_name=%s,\n", h->shm_image_file_name);
  printf("\tshm_image_max_size_bytes=%zu,\n", h->shm_image_max_size_bytes);
  printf("\tshm_leak_file=%d,\n", h->shm_leak_file);
  printf("\tshm_leak_image_path=%s,\n", h->shm_leak_image_path);
  printf("\timage_render_proc_name=%s,\n", h->image_render_proc_name);
  printf("\tslideshow_sleep_time_sec=%zu,\n", h->slideshow_sleep_time_sec);
  printf("\teink_mock_display=%d,\n", h->eink_mock_display);
  printf("\teink_save_render_to_png_file=%s,\n",
         h->eink_save_render_to_png_file);
  printf("}\n");
}
