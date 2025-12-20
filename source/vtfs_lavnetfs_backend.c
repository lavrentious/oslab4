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

  if (ret < 0) {
    return (int)ret;
  }

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

  LOG("got root\n");
  LOG("ino=%d\n", out->ino);
  LOG("parent_ino=%d\n", out->parent_ino);
  LOG("type=%d\n", out->type);
  LOG("mode=%d\n", out->mode);
  LOG("size=%d\n", out->size);
  LOG("nlink=%d\n", out->nlink);

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

  if (ret < 0) {
    return (int)ret;
  }

  memcpy(out, buf, sizeof(*out));
  return 0;
}

int vtfs_storage_iterate_dir(vtfs_ino_t dir_ino, unsigned long* offset, struct vtfs_dirent* out) {
  if (!offset || !out)
    return -EINVAL;

  LOG("iterating dir_ino=%lu, offset=%lu...\n", dir_ino, *offset);

  char dir_ino_buf[32], offset_buf[32];
  snprintf(dir_ino_buf, sizeof(dir_ino_buf), "%lu", dir_ino);
  snprintf(offset_buf, sizeof(offset_buf), "%lu", *offset);

  char resp[512];
  int64_t ret = vtfs_http_call(
      VTFS_TOKEN, "iterate_dir", resp, sizeof(resp), 2, "dir_ino", dir_ino_buf, "offset", offset_buf
  );

  if (ret < 0) {
    return (int)ret;
  }

  // payload
  char* payload = resp;

  // if payload is empty
  int empty = 1;
  for (size_t i = 0; i < sizeof(*out); i++) {
    if (payload[i] != 0) {
      empty = 0;
      break;
    }
  }

  if (empty) {
    LOG("directory ended\n");
    return 1;  // end of dir
  }

  memcpy(out, payload, sizeof(*out));
  (*offset)++;

  LOG("got dirent ino=%u name=%s type=%d\n", out->ino, out->name, out->type);
  return 0;
}

int vtfs_storage_create_file(
    vtfs_ino_t parent, const char* name, umode_t mode, struct vtfs_node_meta* out
) {
  if (!name || !out) {
    return -EINVAL;
  }

  char parent_buf[32];
  char mode_buf[32];
  char resp[512];

  snprintf(parent_buf, sizeof(parent_buf), "%lu", parent);
  snprintf(mode_buf, sizeof(mode_buf), "%u", mode);

  LOG("creating file '%s' under parent=%lu with mode=0%o\n", name, parent, mode);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN,
      "create",
      resp,
      sizeof(resp),
      3,
      "parent",
      parent_buf,
      "name",
      (char*)name,
      "mode",
      mode_buf
  );

  if (ret < 0) {
    LOG("create_file HTTP call failed: %ld\n", ret);
    return (int)ret;
  }

  memcpy(out, resp, sizeof(*out));

  LOG("file created: ino=%u name=%s\n", out->ino, name);
  return 0;
}

/* --- stubs for now --- */
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
