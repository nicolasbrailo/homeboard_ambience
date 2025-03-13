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
struct EInkDisplay *g_eink = NULL;

void handle_user_intr(int sig) { g_user_intr = true; }


#include <json-c/json.h>

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


#include <cairo/cairo.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 200 // Max length of text before wrapping, you can adjust this.

size_t render_text_to_surface(cairo_t *cr, const char *text, double x, double y, double max_width) {
    cairo_text_extents_t extents;
    double current_x = x;
    double current_y = y;

    // Split the input text into words and handle line breaks manually
    const char *start = text;
    const char *word = start;
    char line[MAX_LINE_LENGTH];
    line[0] = '\0'; // Initialize line buffer
    size_t rendered_lines = 0;

    while (*word != '\0') {
        // Move to the next word (skip leading spaces)
        while (*word && isspace(*word)) word++;

        // Find the end of the current word
        const char *end = word;
        while (*end && !isspace(*end)) end++;

        // Copy the word to line
        size_t word_len = end - word;
        strncat(line, word, word_len);
        line[strlen(line)] = '\0';

        // Measure the line's width with Cairo
        cairo_text_extents(cr, line, &extents);

        // Check if the line fits within the max width
        if (extents.width > max_width) {
            // Line doesn't fit, so render the current line and reset it for the next line
            cairo_move_to(cr, current_x, current_y);
            cairo_show_text(cr, line);
            rendered_lines++;
            current_y += extents.height + 2;  // Adjust vertical spacing between lines

            // Start a new line with the current word
            strcpy(line, "");
            strncat(line, word, word_len);
        }

        // Move to the next word
        word = end;
    }

    // Render the last line
    if (strlen(line) > 0) {
        cairo_move_to(cr, current_x, current_y);
        cairo_show_text(cr, line);
        rendered_lines++;
    }

    return rendered_lines;
}

void foo(const char* meta_json, const void *qr_ptr, size_t qr_sz) {
  json_object *jobj = json_tokener_parse(meta_json);
  if (jobj == NULL) {
    fprintf(stderr, "Error parsing JSON string\n");
    return;
  }

  const char *meta_keys[] = {"EXIF DateTimeOriginal", "albumname", "reverse_geo.revgeo"};
  const size_t meta_keys_sz = 3;

  struct json_object *remotepath;
  if (json_object_object_get_ex(jobj, "local_path", &remotepath)) {
    const char* v = json_object_get_string(remotepath);
    printf("Received file %s\n", v);
  } else {
    printf("Received unknown file %s\n", meta_json);
  }

  cairo_t *cr = eink_get_cairo(g_eink);
  cairo_surface_t *surface = cairo_get_target(cr);

  const size_t width = cairo_image_surface_get_width(surface);
  const size_t height = cairo_image_surface_get_height(surface);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  size_t y = 24;
  for (size_t i = 0; i < meta_keys_sz; ++i) {
    const char* v = json_get_nested_key(jobj, meta_keys[i]);
    if (v) {
      const size_t rendered_lns = render_text_to_surface(cr, v, 5, y, width);
      y += rendered_lns * 24;
    }
  }

  // Draw rectangle around box
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_stroke(cr);

  eink_render(g_eink);

  exit(0);
}


void on_image_received(const void *img_ptr, size_t img_sz, const char *meta_ptr,
                       size_t meta_sz, const void *qr_ptr, size_t qr_sz) {
  printf("Received new image%s%s\n", meta_ptr ? ": " : "",
         meta_ptr ? meta_ptr : "");
  foo(meta_ptr, qr_ptr, qr_sz);

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

  struct EInkConfig eink_cfg = {
    .mock_display = true,
    .save_render_to_png_file = "eink.png",
  };
  if (!(g_eink = eink_init(&eink_cfg))) {
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
