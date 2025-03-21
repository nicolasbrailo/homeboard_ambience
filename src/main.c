#include "config.h"
#include "libeink/cairo_helpers.h"
#include "libeink/eink.h"
#include "libwwwslide/wwwslider.h"
#include "proc_utils.h"
#include "shm.h"

#include <cairo/cairo.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "json.h"
#include <json-c/json.h>

json_object* parse_meta(const char *meta_json) {
  json_object *jobj = meta_json? json_tokener_parse(meta_json) : NULL;
  if (jobj == NULL) {
    fprintf(stderr, "Error parsing JSON string\n");
    return NULL;
  }

  struct json_object *remotepath;
  if (json_object_object_get_ex(jobj, "local_path", &remotepath)) {
    const char *v = json_object_get_string(remotepath);
    printf("Received file %s\n", v);
  } else {
    printf("Received unknown file %s\n", meta_json);
  }

  return jobj;
}

void cairo_render_meta(cairo_t* cr, json_object* jobj, const char** meta_keys, size_t meta_keys_sz) {
  cairo_surface_t *surface = cairo_get_target(cr);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 16);

  if (jobj) {
    size_t y = 1;
    for (size_t i = 0; i < meta_keys_sz; ++i) {
      const char *v = json_get_nested_key(jobj, meta_keys[i]);
      if (v) {
        const size_t rendered_lns = cairo_render_text(cr, v, y);
        y += rendered_lns;
      }
    }
  } else {
    cairo_render_text(cr, "Error: failed to load metadata", 1);
  }

  /* Draw rectangle around box {
    const size_t width = cairo_image_surface_get_width(surface);
    const size_t height = cairo_image_surface_get_height(surface);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_stroke(cr);
  } */

  /* Draw a clock */ {
    time_t tt;
    time(&tt);
    struct tm *ti = localtime(&tt);
    char buff[6];
    snprintf(buff, 6, "%02d:%02d", ti->tm_hour, ti->tm_min);

    // Set clock font
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24);

    // Figure out size
    cairo_text_extents_t extents;
    cairo_text_extents(cr, buff, &extents);

    // I'm sure there is a way to have less magic numbers, but this works for now
    const size_t margin = 5;
    const size_t img_width = cairo_image_surface_get_width(surface);
    const size_t img_height = cairo_image_surface_get_height(surface);
    const size_t clock_x_i = img_width - extents.width - margin;
    const size_t clock_y_i = img_height - extents.height + (extents.height / 2);

    cairo_move_to(cr, clock_x_i, clock_y_i);
    cairo_show_text(cr, buff);

    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, clock_x_i - margin, clock_y_i - extents.height - margin, extents.width + 2*margin, extents.height + 2*margin);
    cairo_stroke(cr);
  }
}

void eink_render_meta(struct EInkDisplay *eink, const char* meta_json, const char** meta_keys, size_t meta_keys_sz) {
  cairo_t *cr = eink_get_cairo(eink);

  // Reset canvas
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);

  json_object *meta = parse_meta(meta_json);
  cairo_render_meta(cr, meta, meta_keys, meta_keys_sz);
  json_object_put(meta); // Free
  eink_render(eink);
}


atomic_bool g_user_intr = false;
int g_img_render_pid = -1;
struct AmbienceSvcConfig *g_cfg = NULL;
struct ShmHandle *g_shm = NULL;
struct EInkDisplay *g_eink = NULL;

void handle_user_intr(int sig) { g_user_intr = true; }

void on_image_received(const void* img_ptr, size_t img_sz,
                       const char* meta_ptr, size_t meta_sz,
                       const void* qr_ptr, size_t qr_sz) {
  if (g_cfg->image_request_metadata) {
    eink_render_meta(g_eink, meta_ptr, g_cfg->image_metadata_keys, g_cfg->image_metadata_keys_count);
  }

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

  const char *cfg_fpath = argc > 1 ? argv[1] : "config.json";
  if (!(g_cfg = ambiencesvc_config_init(cfg_fpath))) {
    goto err;
  }

  printf("Startup ambiencesvc, config:\n");
  ambiencesvc_config_print(g_cfg);

  if (!(g_shm = shm_init(g_cfg->shm_image_file_name,
                         g_cfg->shm_image_max_size_bytes))) {
    fprintf(stderr, "Can't initialize shm\n");
    goto err;
  }

  struct EInkConfig eink_cfg = {
      .mock_display = g_cfg->eink_mock_display,
      .save_render_to_png_file = g_cfg->eink_save_render_to_png_file,
  };
  if (!(g_eink = eink_init(&eink_cfg))) {
    fprintf(stderr, "Can't initialize eInk display\n");
    goto err;
  }

  // The eInk display takes a second to refresh, so displaying a message on
  // startup means the first metadata will be skipped, if it comes up fast
  // enough
  // eink_quick_announce(g_eink, g_cfg->eink_hello_message, 36);

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
    printf("Updating ambience image with %s\n", g_cfg->shm_leak_image_path);
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

  printf("eInk announce: %s\n", g_cfg->eink_goodbye_message);
  eink_quick_announce(g_eink, g_cfg->eink_goodbye_message, 36);

  ambiencesvc_config_free(g_cfg);
  wwwslider_free(wwwslider);
  eink_delete(g_eink);
  return 0;

err:
  fprintf(stderr, "Fail to start ambience service\n");
  wwwslider_free(wwwslider);
  shm_free(g_shm);
  ambiencesvc_config_free(g_cfg);
  eink_delete(g_eink);
  return 1;
}
