#include "config_base.h"
#include <stddef.h>

struct AmbienceSvcConfig {
  // Target width and height for requested image
  size_t image_target_width;
  size_t image_target_height;

  // Should image have an embeded QR code with more info
  bool image_embed_qr;

  // Should we try to display a standalone QR code somewhere
  bool image_request_standalone_qr;

  // Show image metadata
  bool image_request_metadata;

  // Where to get new pictures from
  const char* www_svc_url;

  // Optional client id when registering to image service
  const char* www_client_id;

  // File name to store the image in /dev/shm (eg /dev/shm/ambience_img)
  const char* shm_image_file_name;

  // Will reject to reserve memory for images bigger than this
  size_t shm_image_max_size_bytes;

  // Remove shm file on shutdown or not
  bool shm_leak_file;

  // Image to leave in the shm area if leaked (before program shutdown, it will copy this file to the shm area, then leak it)
  const char* shm_leak_image_path;

  // Image render process name - will notify when an image is updated with SIGUSR1
  const char* image_render_proc_name;

  // Time between pictures
  size_t slideshow_sleep_time_sec;
};

struct AmbienceSvcConfig* ambiencesvc_config_init(const char *fpath);
void ambiencesvc_config_free(struct AmbienceSvcConfig*);
void ambiencesvc_config_print(struct AmbienceSvcConfig*);

