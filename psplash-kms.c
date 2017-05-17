#include "psplash-kms.h"

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <drm/drm.h>

static void scanout_buffer_destroy(int fd, struct scanout_buffer_t * sob)
{
  if (!sob)
    return;

  if (sob->fb_base)
    if (0 != munmap(sob->fb_base, sob->create_dumb.size))
      perror("munmap");

  if (sob->cmd_dumb.fb_id)
    if (ioctl(fd, DRM_IOCTL_MODE_RMFB, &sob->cmd_dumb.fb_id) == -1)
      perror("ioctl[DRM_IOCTL_MODE_RMFB]");

  if (sob->destroy_dumb.handle)
    if (ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &sob->destroy_dumb) == -1)
      perror("ioctl[DRM_IOCTL_MODE_RMFB]");

  if (sob->saved_connector) {
    sob->saved_crtc.set_connectors_ptr = (uint64_t)&sob->saved_connector;
    sob->saved_crtc.count_connectors = 1;

    if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &sob->saved_crtc) == -1) {
      if (ioctl(fd, DRM_IOCTL_SET_MASTER, 0) != -1) {
        if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &sob->saved_crtc) == -1)
          perror("ioctl[DRM_IOCTL_MODE_SETCRTC]");
        if (ioctl(fd, DRM_IOCTL_DROP_MASTER, 0) == -1)
          perror("ioctl[DRM_IOCTL_DROP_MASTER]");
      } else {
        perror("ioctl[DRM_IOCTL_SET_MASTER]");
      }
    }
  }

  free(sob);
}

static struct drm_scanout_t * add_drm_scanout(
    int fd, struct drm_scanout_t * root, struct scanout_buffer_t * item)
{
  struct drm_scanout_t * ni;

  if (!(ni = calloc(1, sizeof(*ni)))) {
    perror("calloc");
    scanout_buffer_destroy(fd, item);
    return root;
  }

  ni->scanout = item;
  ni->next = root;
  return ni;
}

void free_drm_scanouts(int fd, struct drm_scanout_t * root)
{
  struct drm_scanout_t * next;

  while (root) {
    next = root->next;
    scanout_buffer_destroy(fd, root->scanout);
    free(root);
    root = next;
  }
}

static struct scanout_buffer_t * scanout_buffer_create(int fd, uint32_t conn_id)
{
  struct scanout_buffer_t * sob;
  struct drm_mode_get_connector conn;
  struct drm_mode_get_encoder enc;
  struct drm_mode_crtc crtc;
  struct drm_mode_map_dumb map_dumb;
  struct drm_mode_modeinfo * conn_modes;
  uint64_t * conn_props_ptr;
  uint64_t * conn_prop_values_ptr;
  uint64_t * conn_encoders_ptr;

  memset(&conn, 0, sizeof(conn));
  memset(&enc, 0, sizeof(enc));
  memset(&crtc, 0, sizeof(crtc));
  memset(&map_dumb, 0, sizeof(map_dumb));
  conn_modes = NULL;
  conn_props_ptr = NULL;
  conn_prop_values_ptr = NULL;
  conn_encoders_ptr = NULL;

  if (!(sob = calloc(1, sizeof(*sob)))) {
    perror("calloc");
    goto err;
  }

  conn.connector_id = conn_id;

  if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETCONNECTOR] (sizes)");
    goto err;
  }

  if (!(conn_modes = calloc(conn.count_modes, sizeof(*conn_modes)))) {
    perror("calloc[conn_modes]");
    goto err;
  }

  if (!(conn_props_ptr = calloc(conn.count_props, sizeof(*conn_props_ptr)))) {
    perror("calloc[conn_props_ptr]");
    goto err;
  }

  if (!(conn_prop_values_ptr =
        calloc(conn.count_props, sizeof(*conn_prop_values_ptr)))) {
    perror("calloc[conn_prop_values_ptr]");
    goto err;
  }

  if (!(conn_encoders_ptr =
        calloc(conn.count_encoders, sizeof(*conn_encoders_ptr)))) {
    perror("calloc[conn_encoders_ptr]");
    goto err;
  }

  conn.modes_ptr = (uint64_t)conn_modes;
  conn.props_ptr = (uint64_t)conn_props_ptr;
  conn.prop_values_ptr = (uint64_t)conn_prop_values_ptr;
  conn.encoders_ptr = (uint64_t)conn_encoders_ptr;

  if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETCONNECTOR] (resources)");
    goto err;
  }

  free(conn_props_ptr);
  conn_props_ptr = NULL;
  free(conn_prop_values_ptr);
  conn_prop_values_ptr = NULL;
  free(conn_encoders_ptr);
  conn_encoders_ptr = NULL;

  if (conn.count_encoders < 1 || conn.count_modes < 1 ||
      !conn.encoder_id || !conn.connection)
    goto err;

  enc.encoder_id = conn.encoder_id;

  if (ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETENCODER]");
    goto err;
  }

  crtc.crtc_id = enc.crtc_id;

  if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETCRTC]");
    goto err;
  }

  memcpy(&sob->saved_crtc, &crtc, sizeof(crtc));
  sob->saved_connector = conn_id;

  sob->create_dumb.width = conn_modes[0].hdisplay;
  sob->create_dumb.height = conn_modes[0].vdisplay;
  sob->create_dumb.bpp = 32;

  if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &sob->create_dumb) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_CREATE_DUMB]");
    goto err;
  }

  sob->destroy_dumb.handle = sob->create_dumb.handle;

  sob->cmd_dumb.width = sob->create_dumb.width;
  sob->cmd_dumb.height = sob->create_dumb.height;
  sob->cmd_dumb.bpp = sob->create_dumb.bpp;
  sob->cmd_dumb.pitch = sob->create_dumb.pitch;
  sob->cmd_dumb.depth = 24;
  sob->cmd_dumb.handle = sob->create_dumb.handle;

  if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &sob->cmd_dumb) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_ADDFB]");
    goto err;
  }

  map_dumb.handle = sob->create_dumb.handle;

  if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_MAP_DUMB]");
    goto err;
  }

  sob->fb_base = mmap(0, sob->create_dumb.size, PROT_READ | PROT_WRITE,
      MAP_SHARED, fd, map_dumb.offset);

  if (!sob->fb_base || sob->fb_base == MAP_FAILED) {
    perror("mmap");
    goto err;
  }

  sob->fb_w = sob->create_dumb.width;
  sob->fb_h = sob->create_dumb.height;
  sob->fb_stride = sob->create_dumb.pitch;
  sob->fb_bpp = sob->create_dumb.bpp;
  sob->interface = conn.connector_type;
  sob->instance = conn.connector_type_id;

  crtc.fb_id = sob->cmd_dumb.fb_id;
  crtc.set_connectors_ptr = (uint64_t)&conn_id;
  crtc.count_connectors = 1;
  crtc.mode = conn_modes[0];
  crtc.mode_valid = 1;

  if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_SETCRTC]");
    goto err;
  }

  free(conn_modes);
  return sob;

err:
  scanout_buffer_destroy(fd, sob);
  free(conn_modes);
  free(conn_props_ptr);
  free(conn_prop_values_ptr);
  free(conn_encoders_ptr);
  return NULL;
}

static uint32_t find_rotation_for_interface_instance(
    struct drm_rotation_t * root, uint32_t interface, uint32_t instance)
{
  struct drm_rotation_t * curr = root;
  while (curr) {
    if (curr->interface == interface && curr->instance == instance)
      return curr->rotation;
    curr = curr->next;
  }

  curr = root;
  while (curr) {
    if (curr->interface == DRM_MODE_CONNECTOR_Unknown && curr->instance == 0)
      return curr->rotation;
    curr = curr->next;
  }

  return 0;
}

struct drm_scanout_t * find_drm_scanouts(int fd,
    struct drm_rotation_t * rotations)
{
  struct drm_scanout_t * scanouts;
  int is_master;

  struct drm_mode_card_res res;
  uint32_t * res_fb_ids;
  uint32_t * res_crtc_ids;
  uint32_t * res_connector_ids;
  uint32_t * res_encoder_ids;
  uint32_t i;

  scanouts = NULL;
  is_master = 0;
  memset(&res, 0, sizeof(res));
  res_fb_ids = NULL;
  res_crtc_ids = NULL;
  res_connector_ids = NULL;
  res_encoder_ids = NULL;

  if (ioctl(fd, DRM_IOCTL_SET_MASTER, 0) == -1) {
    perror("ioctl[DRM_IOCTL_SET_MASTER]");
    goto cleanup;
  }
  is_master = 1;

  if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETRESOURCES (sizes)]");
    goto cleanup;
  }

  if (!(res_fb_ids = calloc(res.count_fbs, sizeof(*res_fb_ids)))) {
    perror("calloc[res_fb_ids]");
    goto cleanup;
  }

  if (!(res_crtc_ids = calloc(res.count_crtcs, sizeof(*res_crtc_ids)))) {
    perror("calloc[res_crtc_ids]");
    goto cleanup;
  }

  if (!(res_connector_ids =
        calloc(res.count_connectors, sizeof(*res_connector_ids)))) {
    perror("calloc[res_connector_ids]");
    goto cleanup;
  }

  if (!(res_encoder_ids =
        calloc(res.count_encoders, sizeof(*res_encoder_ids)))) {
    perror("calloc[res_encoder_ids]");
    goto cleanup;
  }

  res.fb_id_ptr = (uint64_t)res_fb_ids;
  res.crtc_id_ptr = (uint64_t)res_crtc_ids;
  res.connector_id_ptr = (uint64_t)res_connector_ids;
  res.encoder_id_ptr = (uint64_t)res_encoder_ids;

  if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == -1) {
    perror("ioctl[DRM_IOCTL_MODE_GETRESOURCES (resources)]");
    goto cleanup;
  }

  free(res_fb_ids);
  res_fb_ids = NULL;
  free(res_crtc_ids);
  res_crtc_ids = NULL;
  free(res_encoder_ids);
  res_encoder_ids = NULL;

  for (i = 0; i < res.count_connectors; ++i) {
    struct scanout_buffer_t * sob;

    if (!(sob = scanout_buffer_create(fd, res_connector_ids[i]))) {
      continue;
    }

    scanouts = add_drm_scanout(fd, scanouts, sob);
  }

cleanup:
  free(res_fb_ids);
  free(res_crtc_ids);
  free(res_connector_ids);
  free(res_encoder_ids);

  if (is_master) {
    if (ioctl(fd, DRM_IOCTL_DROP_MASTER, 0) == -1) {
      perror("ioctl[DRM_IOCTL_DROP_MASTER]");
      free_drm_scanouts(fd, scanouts);
      scanouts = NULL;
    }
  }

  if (scanouts) {
    struct drm_scanout_t * curr = scanouts;

    while (curr) {
      curr->pso.width = curr->scanout->fb_w;
      curr->pso.height = curr->scanout->fb_h;
      curr->pso.bpp = curr->scanout->fb_bpp;
      curr->pso.stride = curr->scanout->fb_stride;
      curr->pso.data = curr->scanout->fb_base;
      curr->pso.angle = find_rotation_for_interface_instance(
          rotations, curr->scanout->interface, curr->scanout->instance);
      curr->pso.rgbmode = RGB888;

      if (!curr->next)
        curr->pso.next = NULL;
      else
        curr->pso.next = &curr->next->pso;

      switch (curr->pso.angle) {
        case 270:
        case 90:
          curr->pso.width = curr->scanout->fb_h;
          curr->pso.height = curr->scanout->fb_w;
          break;
        default:
          break;
      }

      curr = curr->next;
    }
  }

  return scanouts;
}

void free_drm_rotations(struct drm_rotation_t * root)
{
  struct drm_rotation_t * next;

  while (root) {
    next = root->next;
    free(root);
    root = next;
  }
}

static struct drm_rotation_t * add_drm_rotation(
    struct drm_rotation_t * root,
    uint32_t interface, uint32_t instance, uint32_t rotation)
{
  struct drm_rotation_t * ni;

  if (!(ni = calloc(1, sizeof(*ni)))) {
    perror("calloc");
    free_drm_rotations(root);
    return NULL;
  }

  ni->interface = interface;
  ni->instance = instance;
  ni->rotation = rotation;
  ni->next = root;
  return ni;
}

struct drm_connector_map_t {
  char name[20];
  uint32_t id;
};

static const struct drm_connector_map_t drm_connector_map[] = {
  {"Unknown",     DRM_MODE_CONNECTOR_Unknown},
  {"VGA",         DRM_MODE_CONNECTOR_VGA},
  {"DVII",        DRM_MODE_CONNECTOR_DVII},
  {"DVID",        DRM_MODE_CONNECTOR_DVID},
  {"DVIA",        DRM_MODE_CONNECTOR_DVIA},
  {"Composite",   DRM_MODE_CONNECTOR_Composite},
  {"SVIDEO",      DRM_MODE_CONNECTOR_SVIDEO},
  {"LVDS",        DRM_MODE_CONNECTOR_LVDS},
  {"Component",   DRM_MODE_CONNECTOR_Component},
  {"9PinDIN",     DRM_MODE_CONNECTOR_9PinDIN},
  {"DisplayPort", DRM_MODE_CONNECTOR_DisplayPort},
  {"HDMIA",       DRM_MODE_CONNECTOR_HDMIA},
  {"HDMIB",       DRM_MODE_CONNECTOR_HDMIB},
  {"TV",          DRM_MODE_CONNECTOR_TV},
  {"eDP",         DRM_MODE_CONNECTOR_eDP},
  {"VIRTUAL",     DRM_MODE_CONNECTOR_VIRTUAL},
  {"DSI",         DRM_MODE_CONNECTOR_DSI},
#if defined(DRM_MODE_CONNECTOR_DPI)
  {"DPI",         DRM_MODE_CONNECTOR_DPI},
#endif
};
static const size_t drm_connector_map_size =
  sizeof(drm_connector_map) / sizeof(drm_connector_map[0]);

static int drm_connector_name_to_id(const char * name, uint32_t * id)
{
  size_t i;

  for (i = 0; i < drm_connector_map_size; ++i) {
    if (strcmp(name, drm_connector_map[i].name) == 0) {
      if (id)
        *id = drm_connector_map[i].id;
      return 0;
    }
  }

  return -1;
}

struct drm_rotation_t * parse_rotation_string(
    char * arg, uint32_t default_rotation)
{
  struct drm_rotation_t * root;
  int i;
  char * outer;
  char * inner;
  char * sptr1;
  char * spec;
  char * rotation_str;
  char * interface_str;
  char * instance_str;
  char * ep;
  long lval;
  unsigned long ulval;
  uint32_t rotation;
  uint32_t interface;
  uint32_t instance;

  root = NULL;

  for (i = 0, outer = arg, sptr1 = NULL; ; ++i, outer = NULL) {
    if (!(inner = strtok_r(outer, ",", &sptr1)))
      break;

    spec = strtok_r(inner, ":", &rotation_str);
    if (!spec || !rotation_str)
      goto err;

    interface_str = strtok_r(spec, "-", &instance_str);
    if (!interface_str || !instance_str)
      goto err;

    errno = 0;
    lval = strtol(rotation_str, &ep, 0);
    if (rotation_str[0] == '\0' || *ep != '\0')
      goto err;
    if (errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
      goto err;
    if (lval != 0 && lval != 90 && lval != 180 && lval != 270) {
      errno = EINVAL;
      goto err;
    }
    rotation = (uint32_t)lval;

    if (0 != drm_connector_name_to_id(interface_str, &interface))
      goto err;

    errno = 0;
    ulval = strtoul(instance_str, &ep, 0);
    if (instance_str[0] == '\0' || *ep != '\0')
      goto err;
    if (errno == ERANGE && ulval == ULONG_MAX)
      goto err;
    if (ulval > UINT32_MAX) {
      errno = ERANGE;
      goto err;
    }
    instance = (uint32_t)ulval;

    if (!(root = add_drm_rotation(root, interface, instance, rotation)))
      goto err;
  }

  if (!(root = add_drm_rotation(root, DRM_MODE_CONNECTOR_Unknown,
          0, default_rotation)))
    goto err;

  goto cleanup;

err:
  free_drm_rotations(root);
  root = NULL;

cleanup:
  return root;
}

#if 0
static void draw_one_test(struct scanout_buffer_t * sob)
{
  int x;
  int y;
  int col;

  col = (rand() % 0x00ffffff) & 0x00ff00ff;

  for (y = 0; y < sob->fb_h; ++y) {
    for (x = 0; x < sob->fb_w; ++x) {
      int location = y * sob->fb_w + x;
      *(((uint32_t*)sob->fb_base) + location) = col;
    }
  }
}

static void draw_test(struct drm_scanout_t * scanouts)
{
  struct drm_scanout_t * curr;

  srand(time(NULL));
  curr = scanouts;

  while (curr) {
    draw_one_test(curr->scanout);
    curr = curr->next;
  }
}

int main(int argc, char ** argv)
{
  int ret;
  int fd;
  struct drm_scanout_t * scanouts;

  ret = 1;
  fd = -1;
  scanouts = NULL;

  if ((fd = open("/dev/dri/card0", O_RDWR)) < 0) {
    perror("open[/dev/dri/card0]");
    goto cleanup;
  }

  if (!(scanouts = find_drm_scanouts(fd)))
    goto cleanup;

  draw_test(scanouts);
  sleep(10);

cleanup:
  if (fd != -1) {
    free_drm_scanouts(fd, scanouts);
    close(fd);
  }

  return ret;
}
#endif
