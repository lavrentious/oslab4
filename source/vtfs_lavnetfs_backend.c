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

int vtfs_storage_unlink(vtfs_ino_t parent, const char* name) {
  if (!name) {
    return -EINVAL;
  }

  char parent_buf[32];
  char resp[512];

  snprintf(parent_buf, sizeof(parent_buf), "%lu", parent);

  LOG("unlinking file '%s' under parent=%lu\n", name, parent);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN, "unlink", resp, sizeof(resp), 2, "parent", parent_buf, "name", (char*)name
  );

  if (ret < 0) {
    LOG("unlink HTTP call failed: %lld\n", ret);
    return (int)ret;
  }

  LOG("file unlinked\n");
  return 0;
}

int vtfs_storage_mkdir(
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

  LOG("creating directory '%s' under parent=%lu with mode=0%o\n", name, parent, mode);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN,
      "mkdir",
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
    LOG("mkdir HTTP call failed: %lld\n", ret);
    return (int)ret;
  }

  memcpy(out, resp, sizeof(*out));

  LOG("dir created: ino=%lu name=%s\n", out->ino, name);
  return 0;
}

int vtfs_storage_rmdir(vtfs_ino_t parent, const char* name) {
  if (!name) {
    return -EINVAL;
  }

  char parent_buf[32];
  char resp[512];

  snprintf(parent_buf, sizeof(parent_buf), "%lu", parent);

  LOG("rmdir dir '%s' under parent=%lu\n", name, parent);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN, "rmdir", resp, sizeof(resp), 2, "parent", parent_buf, "name", (char*)name
  );

  if (ret < 0) {
    LOG("rmdir HTTP call failed: %lld\n", ret);
    return (int)ret;
  }

  LOG("dir removed\n");
  return 0;
}

ssize_t vtfs_storage_read_file(vtfs_ino_t ino, loff_t offset, size_t len, char* dst) {
  if (!dst || len == 0)
    return -EINVAL;

  char ino_buf[32], offset_buf[32], len_buf[32];
  snprintf(ino_buf, sizeof(ino_buf), "%lu", ino);
  snprintf(offset_buf, sizeof(offset_buf), "%llu", offset);
  snprintf(len_buf, sizeof(len_buf), "%zu", len);

  char resp[8192];
  int64_t ret = vtfs_http_call(
      VTFS_TOKEN,
      "read",
      resp,
      sizeof(resp),
      3,
      "ino",
      ino_buf,
      "offset",
      offset_buf,
      "length",
      len_buf
  );

  if (ret < 0) {
    return (int)ret;
  }

  uint64_t payload_len = 0;
  memcpy(&payload_len, resp, sizeof(payload_len));

  LOG("payload_len=%llu\n", payload_len);

  if (payload_len == 0) {
    LOG("EOF reached\n");
    return 0;
  }

  if (payload_len > len)
    payload_len = len;

  memcpy(dst, resp + 8, payload_len);
  LOG("read_file ino=%lu offset=%llu read=%llu bytes\n", ino, offset, payload_len);

  return (ssize_t)payload_len;
}

ssize_t vtfs_storage_write_file(
    vtfs_ino_t ino, loff_t offset, const char* src, size_t len, loff_t* new_size
) {
  if (!src || len == 0) {
    return 0;
  }
  LOG("write file ino=%lu, offset=%lld, len=%zu\n", ino, offset, len);

  char ino_buf[32], off_buf[32];
  snprintf(ino_buf, sizeof(ino_buf), "%lu", ino);
  snprintf(off_buf, sizeof(off_buf), "%llu", offset);

  /* body = [data] */
  size_t body_len = len;
  char* body = kmalloc(body_len, GFP_KERNEL);
  if (!body) {
    return -ENOMEM;
  }

  memcpy(body, src, len);

  char resp[32];  // written + new_size
  int64_t ret = vtfs_http_call_with_body(
      VTFS_TOKEN, "write", body, body_len, resp, sizeof(resp), 2, "ino", ino_buf, "offset", off_buf
  );

  LOG("ret=%lld\n", ret);

  kfree(body);

  if (ret < 0) {
    return (ssize_t)ret;
  }

  uint64_t written = 0;
  uint64_t size = 0;

  memcpy(&written, resp, sizeof(uint64_t));
  memcpy(&size, resp + sizeof(uint64_t), sizeof(uint64_t));

  if (new_size) {
    *new_size = (loff_t)size;
  }

  LOG("write_file ino=%lu off=%llu wrote=%llu new_size=%llu\n", ino, offset, written, size);

  return (ssize_t)written;
}

int vtfs_storage_link(
    vtfs_ino_t parent, const char* name, vtfs_ino_t target_ino, struct vtfs_node_meta* out
) {
  if (!name || !out) {
    return -EINVAL;
  }

  char parent_buf[32];
  char ino_buf[32];
  char resp[512];

  snprintf(parent_buf, sizeof(parent_buf), "%lu", parent);
  snprintf(ino_buf, sizeof(ino_buf), "%lu", target_ino);

  LOG("creating a link for ino=%lu under name=%s, parent=%lu\n", target_ino, name, parent);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN,
      "link",
      resp,
      sizeof(resp),
      3,
      "parent",
      parent_buf,
      "name",
      (char*)name,
      "ino",
      ino_buf
  );

  if (ret < 0) {
    return (int)ret;
  }

  memcpy(out, resp, sizeof(*out));

  LOG("link created: ino=%lu name=%s\n", out->ino, name);
  return 0;
}

int vtfs_storage_truncate(vtfs_ino_t ino, loff_t size) {
  char ino_buf[32];
  char size_buf[32];
  char resp[512];

  snprintf(ino_buf, sizeof(ino_buf), "%lu", ino);
  snprintf(size_buf, sizeof(size_buf), "%lld", size);

  LOG("truncating file ino=%lu to size=%lld\n", ino, size);

  int64_t ret = vtfs_http_call(
      VTFS_TOKEN, "truncate", resp, sizeof(resp), 2, "ino", ino_buf, "size", size_buf
  );

  if (ret < 0) {
    return (int)ret;
  }

  LOG("file truncated: ino=%lu size=%lld\n", ino, size);
  return 0;
}