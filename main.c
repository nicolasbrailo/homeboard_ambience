#include "libeink/eink.h"
#include "libwwwslide/wwwslider.h"
#include "shm.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

atomic_bool g_user_intr = false;
struct ShmHandle* gShm = NULL;

void handle_user_intr(int sig) { g_user_intr = true; }

void on_image_received(const void *img_ptr, size_t img_sz, const char *meta_ptr,
                       size_t meta_sz, const void *qr_ptr, size_t qr_sz) {
  printf("Received new image%s%s\n", meta_ptr ? ": " : "",
         meta_ptr ? meta_ptr : "");

  if (shm_update(gShm, img_ptr, img_sz) <= 0) {
    fprintf(stderr, "Failed to update shm with received image\n");
  }
}


int main(int argc, const char** argv) {
  if (signal(SIGINT, handle_user_intr) == SIG_ERR) {
    fprintf(stderr, "Error setting up signal handler\n");
    return 1;
  }

  gShm = shm_init("ambience_img", /*max_sz_bytes=*/20 * 1024 * 1024);
  if (!gShm) {
    fprintf(stderr, "Failed to init image storage, check shm settings\n");
    return 1;
  }

  struct WwwSliderConfig cfg = {
      .target_width = 640,
      .target_height = 480,
      .embed_qr = false,
      .request_standalone_qr = true,
      .request_metadata = true,
      .client_id = "ambience_test",
      .on_image_available = on_image_received,
  };
  struct WwwSlider *slider = wwwslider_init("http://bati.casa:5000", cfg);
  if (!slider || !wwwslider_wait_registered(slider)) {
    fprintf(stderr, "Fail to register with image service\n");
    return 1;
  }

  while (!g_user_intr) {
    printf("Requesting next image\n");
    wwwslider_get_next_image(slider);
    // TODO wwwslider_get_prev_image(slider);
    sleep(10);
  }

  shm_free(gShm);
  wwwslider_free(slider);

  return 0;
}
