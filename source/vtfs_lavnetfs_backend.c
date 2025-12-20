#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "http.h"
#include "vtfs.h"
#include "vtfs_backend.h"

#define VTFS_TOKEN "devtoken"

/* --- helpers --- */

static int vtfs_call(const char* method, char* resp, size_t resp_size, size_t argc, ...) {
  va_list args;
  va_start(args, argc);
  int64_t ret = vtfs_http_call(VTFS_TOKEN, method, resp, resp_size, argc, args);
  va_end(args);

  if (ret < 0)
    return (int)ret;

  return 0;
}

/* --- lifecycle --- */

int vtfs_storage_init(void) {
  printk(KERN_INFO "vtfs_lavnetfs: init\n");
  return 0;
}

void vtfs_storage_shutdown(void) {
  printk(KERN_INFO "vtfs_lavnetfs: shutdown\n");
}

/* --- root --- */

int vtfs_storage_get_root(struct vtfs_node_meta* out) {
  LOG("getting root...");
  char buf[sizeof(struct vtfs_node_meta)];
  int ret = vtfs_http_call(VTFS_TOKEN, "get_root", buf, sizeof(buf), 0);

  LOG("ret: %d", ret);

  if (ret < 0) {
    return (int)ret;
  }

  memcpy(out, buf, sizeof(*out));
  return 0;
}

/* --- lookup --- */

int vtfs_storage_lookup(vtfs_ino_t parent, const char* name, struct vtfs_node_meta* out) {
  char buf[sizeof(struct vtfs_node_meta)];
  char parent_buf[32];
  char name_enc[256];

  snprintf(parent_buf, sizeof(parent_buf), "%lu", parent);
  encode(name, name_enc);

  int ret = vtfs_http_call(
      VTFS_TOKEN, "lookup", buf, sizeof(buf), 2, "parent", parent_buf, "name", name_enc
  );

  if (ret < 0)
    return (int)ret;

  memcpy(out, buf, sizeof(*out));
  return 0;
}

/* --- stubs for now --- */

int vtfs_storage_iterate_dir(vtfs_ino_t dir_ino, unsigned long* offset, struct vtfs_dirent* out) {
  return -ENOSYS;
}

int vtfs_storage_create_file(
    vtfs_ino_t parent, const char* name, umode_t mode, struct vtfs_node_meta* out
) {
  return -ENOSYS;
}

int vtfs_storage_unlink(vtfs_ino_t parent, const char* name) {
  return -ENOSYS;
}

int vtfs_storage_mkdir(
    vtfs_ino_t parent, const char* name, umode_t mode, struct vtfs_node_meta* out
) {
  return -ENOSYS;
}

int vtfs_storage_rmdir(vtfs_ino_t parent, const char* name) {
  return -ENOSYS;
}

ssize_t vtfs_storage_read_file(vtfs_ino_t ino, loff_t offset, size_t len, char* dst) {
  return -ENOSYS;
}

ssize_t vtfs_storage_write_file(
    vtfs_ino_t ino, loff_t offset, const char* src, size_t len, loff_t* new_size
) {
  return -ENOSYS;
}

int vtfs_storage_link(
    vtfs_ino_t parent, const char* name, vtfs_ino_t target_ino, struct vtfs_node_meta* out
) {
  return -ENOSYS;
}
