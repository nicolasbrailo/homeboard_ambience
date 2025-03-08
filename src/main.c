#include "config.h"
#include "libeink/eink.h"
#include "libwwwslide/wwwslider.h"
#include "proc_utils.h"
#include "shm.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

atomic_bool g_user_intr = false;
int g_img_render_pid = -1;
struct AmbienceSvcConfig *g_cfg = NULL;
struct ShmHandle *g_shm = NULL;

void handle_user_intr(int sig) { g_user_intr = true; }

void on_image_received(const void *img_ptr, size_t img_sz, const char *meta_ptr,
                       size_t meta_sz, const void *qr_ptr, size_t qr_sz) {
  printf("Received new image%s%s\n", meta_ptr ? ": " : "",
         meta_ptr ? meta_ptr : "");

  if (shm_update(g_shm, img_ptr, img_sz) < 0) {
    fprintf(stderr, "Failed to update shm with received image\n");
  }

  g_img_render_pid = signal_single_kill_old(
      SIGUSR1, g_cfg->image_render_proc_name, g_img_render_pid);
  if (g_img_render_pid > 0) {
    printf("Notified %s (pid %d) of new shm image\n",
           g_cfg->image_render_proc_name, g_img_render_pid);
  } else {
    printf("Failed to notify %s of new shm image, can't find process\n",
           g_cfg->image_render_proc_name);
  }
}

int main(int argc, const char **argv) {
  struct WwwSlider *wwwslider = NULL;

  const char* cfg_fpath = argc > 1? argv[1] : "config.json";
  if (!(g_cfg = ambiencesvc_config_init(cfg_fpath))) {
    goto err;
  }

  printf("Startup ambiencesvc, config:\n");
  ambiencesvc_config_print(g_cfg);

  if (!(g_shm = shm_init(g_cfg->shm_image_file_name,
                         g_cfg->shm_image_max_size_bytes))) {
    goto err;
  }

  struct WwwSliderConfig wcfg = {
      .target_width = g_cfg->image_target_width,
      .target_height = g_cfg->image_target_height,
      .embed_qr = g_cfg->image_embed_qr,
      .request_standalone_qr = g_cfg->image_request_standalone_qr,
      .request_metadata = g_cfg->image_request_metadata,
      .client_id = "uninitialized_client_id",
      .on_image_available = on_image_received,
  };
  if (strlen(g_cfg->www_client_id) >= sizeof(wcfg.client_id)) {
    fprintf(stderr, "Invalid client id %s, max len must be %zu\n",
            g_cfg->www_client_id, sizeof(wcfg.client_id));
    goto err;
  }
  strncpy(wcfg.client_id, g_cfg->www_client_id, sizeof(wcfg.client_id));
  wwwslider = wwwslider_init(g_cfg->www_svc_url, wcfg);
  if (!wwwslider || !wwwslider_wait_registered(wwwslider)) {
    fprintf(stderr, "Fail to register with image service\n");
    goto err;
  }

  // Start main loop, register signal handler now to let user stop
  if (signal(SIGINT, handle_user_intr) == SIG_ERR) {
    fprintf(stderr, "Error setting up signal handler\n");
    goto err;
  }

  while (!g_user_intr) {
    printf("Requesting next image\n");
    wwwslider_get_next_image(wwwslider);
    // TODO wwwslider_get_prev_image(wwwslider);
    sleep(g_cfg->slideshow_sleep_time_sec);
  }

  printf("Shutting down ambiencesvc...\n");
  if (g_cfg->shm_leak_file) {
    if (shm_update_from_file(g_shm, g_cfg->shm_leak_image_path) <= 0) {
      fprintf(stderr,
              "Failed to update shm file with %s pre-shutdown, contents not "
              "defined\n",
              g_cfg->shm_leak_image_path);
    }
    shm_free_leak_shm(g_shm);
  } else {
    shm_free(g_shm);
  }

  ambiencesvc_config_free(g_cfg);
  wwwslider_free(wwwslider);
  return 0;

err:
  fprintf(stderr, "Fail to start ambience service\n");
  wwwslider_free(wwwslider);
  shm_free(g_shm);
  ambiencesvc_config_free(g_cfg);
  return 1;
}
