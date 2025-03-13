#include "config.h"
#include "config_base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define IMG_MIN_SIZE_PX 300
#define SHM_IMAGE_MAX_SIZE_BYTES_MIN 2 * 1024 * 1024
#define SLIDESHOW_SLEEP_TIME_SEC_MAX 1000

bool file_is_valid(const char *fpath) {
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

struct AmbienceSvcConfig *ambiencesvc_config_init(const char *fpath) {
  struct AmbienceSvcConfig *cfg = malloc(sizeof(struct AmbienceSvcConfig));
  struct Config *cfgbase = config_init(fpath);
  if (!cfg || !cfgbase) {
    goto err;
  }

#define CFG_GET_SZ(key)                                                        \
  if (!config_get_size_t(cfgbase, #key, &cfg->key)) {                          \
    fprintf(stderr,                                                            \
            "Failed to read config: can't find size value " #key "\n");        \
    goto err;                                                                  \
  }
#define CFG_GET_BOOL(key)                                                      \
  if (!config_get_bool(cfgbase, #key, &cfg->key)) {                            \
    fprintf(stderr,                                                            \
            "Failed to read config: can't find bool value " #key "\n");        \
    goto err;                                                                  \
  }
#define CFG_GET_STR(key)                                                       \
  {                                                                            \
    const char *tmp = NULL;                                                    \
    if (!config_get_string(cfgbase, #key, &tmp)) {                             \
      fprintf(stderr,                                                          \
              "Failed to read config: can't find str value " #key "\n");       \
      goto err;                                                                \
    } else {                                                                   \
      /* Make a copy; the other string belongs to jsonc */                     \
      cfg->key = strdup(tmp);                                                  \
    }                                                                          \
  }
#define CFG_GET_STR_OPT(key)                                                   \
  {                                                                            \
    const char *tmp = NULL;                                                    \
    if (config_get_string(cfgbase, #key, &tmp)) {                              \
      /* Make a copy; the other string belongs to jsonc */                     \
      cfg->key = strdup(tmp);                                                  \
    }                                                                          \
  }

  CFG_GET_SZ(image_target_width);
  CFG_GET_SZ(image_target_height);
  CFG_GET_BOOL(image_embed_qr);
  CFG_GET_BOOL(image_request_standalone_qr);
  CFG_GET_BOOL(image_request_metadata);
  CFG_GET_STR(www_svc_url);
  CFG_GET_STR(www_client_id);
  CFG_GET_STR(shm_image_file_name);
  CFG_GET_SZ(shm_image_max_size_bytes);
  CFG_GET_BOOL(shm_leak_file);
  CFG_GET_STR(shm_leak_image_path);
  CFG_GET_STR(image_render_proc_name);
  CFG_GET_SZ(slideshow_sleep_time_sec);
  CFG_GET_BOOL(eink_mock_display);
  CFG_GET_STR_OPT(eink_save_render_to_png_file);

#undef CFG_GET_SZ
#undef CFG_GET_BOOL
#undef CFG_GET_STR

  // Validate cfg makes sense
  if ((cfg->image_target_width < IMG_MIN_SIZE_PX) ||
      (cfg->image_target_height < IMG_MIN_SIZE_PX)) {
    fprintf(stderr,
            "Config err: target width and height must be at least %dx%d\n",
            IMG_MIN_SIZE_PX, IMG_MIN_SIZE_PX);
    goto err;
  }

  if (strlen(cfg->www_svc_url) == 0) {
    fprintf(stderr, "www_svc_url must not be empty\n");
    goto err;
  }

  if (strlen(cfg->shm_image_file_name) == 0) {
    fprintf(stderr, "shm_image_file_name must not be empty\n");
    goto err;
  }

  if (cfg->shm_image_max_size_bytes < SHM_IMAGE_MAX_SIZE_BYTES_MIN) {
    fprintf(stderr,
            "Config err: shm_image_max_size_bytes of %zu is too small, min "
            "expected is %d\n",
            cfg->slideshow_sleep_time_sec, SHM_IMAGE_MAX_SIZE_BYTES_MIN);
    goto err;
  }

  if (cfg->shm_leak_file && !file_is_valid(cfg->shm_leak_image_path)) {
    fprintf(stderr,
            "Config err: shm_leak_image_path must point to a valid file, can't "
            "open %s\n",
            cfg->shm_leak_image_path);
    goto err;
  }

  if (strlen(cfg->image_render_proc_name) == 0) {
    fprintf(stderr, "image_render_proc_name must not be empty\n");
    goto err;
  }

  if (cfg->slideshow_sleep_time_sec > SLIDESHOW_SLEEP_TIME_SEC_MAX) {
    fprintf(stderr,
            "Config err: slideshow_sleep_time_sec of %zu is too large, max "
            "expected is %d seconds\n",
            cfg->slideshow_sleep_time_sec, SLIDESHOW_SLEEP_TIME_SEC_MAX);
    goto err;
  }

  return cfg;

err:
  if (cfgbase) {
    config_free(cfgbase);
  }
  ambiencesvc_config_free(cfg);
  return NULL;
}

void ambiencesvc_config_free(struct AmbienceSvcConfig *h) {
  if (!h) {
    return;
  }

  free((void *)h->www_svc_url);
  free((void *)h->www_client_id);
  free((void *)h->shm_image_file_name);
  free((void *)h->image_render_proc_name);
  free(h);
}

void ambiencesvc_config_print(struct AmbienceSvcConfig *h) {
  printf("AmbienceSvcConfig {\n");
  printf("\timage_target_width=%zu,\n", h->image_target_width);
  printf("\timage_target_height=%zu,\n", h->image_target_height);
  printf("\timage_embed_qr=%d,\n", h->image_embed_qr);
  printf("\timage_request_standalone_qr=%d,\n", h->image_request_standalone_qr);
  printf("\timage_request_metadata=%d,\n", h->image_request_metadata);
  printf("\twww_svc_url=%s,\n", h->www_svc_url);
  printf("\twww_client_id=%s,\n", h->www_client_id);
  printf("\tshm_image_file_name=%s,\n", h->shm_image_file_name);
  printf("\tshm_image_max_size_bytes=%zu,\n", h->shm_image_max_size_bytes);
  printf("\tshm_leak_file=%d,\n", h->shm_leak_file);
  printf("\tshm_leak_image_path=%s,\n", h->shm_leak_image_path);
  printf("\timage_render_proc_name=%s,\n", h->image_render_proc_name);
  printf("\tslideshow_sleep_time_sec=%zu,\n", h->slideshow_sleep_time_sec);
  printf("\teink_mock_display=%d,\n", h->eink_mock_display);
  printf("\teink_save_render_to_png_file=%s,\n", h->eink_save_render_to_png_file);
  printf("}\n");
}
