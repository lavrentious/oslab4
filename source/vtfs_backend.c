#include "vtfs_backend.h"

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "vtfs.h"

#define VTFS_ROOT_INO 1

struct vtfs_ram_node {
  struct vtfs_node_meta meta;
  char name[NAME_MAX + 1];
  struct vtfs_ram_node* next;
};

static struct vtfs_ram_node* vtfs_nodes_head = NULL;
static vtfs_ino_t vtfs_next_ino = VTFS_ROOT_INO + 1;

static struct vtfs_ram_node* vtfs_find_node_by_ino(vtfs_ino_t ino) {
  struct vtfs_ram_node* cur = vtfs_nodes_head;

  while (cur) {
    if (cur->meta.ino == ino) {
      return cur;
    }
    cur = cur->next;
  }

  return NULL;
}

static struct vtfs_ram_node* vtfs_find_child(vtfs_ino_t parent, const char* name) {
  struct vtfs_ram_node* cur = vtfs_nodes_head;

  while (cur) {
    if (cur->meta.parent_ino == parent && !strcmp(cur->name, name)) {
      return cur;
    }
    cur = cur->next;
  }

  return NULL;
}

static struct vtfs_ram_node* vtfs_alloc_node(void) {
  struct vtfs_ram_node* node = kmalloc(sizeof(*node), GFP_KERNEL);
  if (!node) {
    return NULL;
  }

  node->next = vtfs_nodes_head;
  vtfs_nodes_head = node;
  return node;
}

static void vtfs_free_all_nodes(void) {
  struct vtfs_ram_node* cur = vtfs_nodes_head;
  while (cur) {
    struct vtfs_ram_node* next = cur->next;
    kfree(cur);
    cur = next;
  }
  vtfs_nodes_head = NULL;
  vtfs_next_ino = VTFS_ROOT_INO + 1;
}

int vtfs_storage_init(void) {
  LOG("storage_init\n");

  vtfs_free_all_nodes();

  struct vtfs_ram_node* root = vtfs_alloc_node();
  if (!root) {
    LOG("failed to allocate root\n");
    return -ENOMEM;
  }

  root->meta.ino = VTFS_ROOT_INO;
  root->meta.parent_ino = 0;
  root->meta.type = VTFS_NODE_DIR;
  root->meta.mode = S_IFDIR | 0777;
  root->meta.size = 0;
  root->name[0] = '\0';

  LOG("root created: ino=%lu\n", (unsigned long)root->meta.ino);
  return 0;
}

void vtfs_storage_shutdown(void) {
  vtfs_free_all_nodes();
  LOG("vtfs_storage_shutdown: all nodes freed\n");
}

int vtfs_storage_get_root(struct vtfs_node_meta* out) {
  struct vtfs_ram_node* root = vtfs_find_node_by_ino(VTFS_ROOT_INO);
  if (!root) {
    return -ENOENT;
  }

  *out = root->meta;
  return 0;
}

int vtfs_storage_lookup(vtfs_ino_t parent, const char* name, struct vtfs_node_meta* out) {
  LOG("lookup: parent=%lu name=%s\n", parent, name);

  struct vtfs_ram_node* node = vtfs_find_child(parent, name);
  if (!node) {
    LOG("lookup: not found\n");
    return -ENOENT;
  }

  *out = node->meta;
  LOG("lookup: found ino=%lu\n", (unsigned long)out->ino);
  return 0;
}

int vtfs_storage_iterate_dir(vtfs_ino_t dir_ino, unsigned long* offset, struct vtfs_dirent* out) {
  unsigned long count = 0;
  struct vtfs_ram_node* cur = vtfs_nodes_head;

  LOG("iterate: dir=%lu offset=%lu\n", dir_ino, *offset);

  while (cur) {
    if (cur->meta.parent_ino == dir_ino) {
      if (count == *offset) {
        strncpy(out->name, cur->name, NAME_MAX);
        out->name[NAME_MAX] = '\0';
        out->ino = cur->meta.ino;
        out->type = cur->meta.type;

        (*offset)++;
        LOG("iterate: emit %s (ino=%lu)\n", out->name, (unsigned long)out->ino);
        return 0;
      }
      count++;
    }
    cur = cur->next;
  }

  LOG("iterate: end\n");
  return -ENOENT;
}

int vtfs_storage_create_file(
    vtfs_ino_t parent, const char* name, umode_t mode, struct vtfs_node_meta* out
) {
  LOG("create: parent=%lu name=%s mode=%o\n", parent, name, mode);

  if (vtfs_find_child(parent, name)) {
    LOG("create: already exists\n");
    return -EEXIST;
  }

  struct vtfs_ram_node* parent_node = vtfs_find_node_by_ino(parent);
  if (!parent_node || parent_node->meta.type != VTFS_NODE_DIR) {
    LOG("create: parent is not dir\n");
    return -ENOTDIR;
  }

  struct vtfs_ram_node* node = vtfs_alloc_node();
  if (!node) {
    LOG("create: alloc failed\n");
    return -ENOMEM;
  }

  node->meta.ino = vtfs_next_ino++;
  node->meta.parent_ino = parent;
  node->meta.type = VTFS_NODE_FILE;
  node->meta.mode = S_IFREG | (mode & 0777);
  node->meta.size = 0;

  strncpy(node->name, name, NAME_MAX);
  node->name[NAME_MAX] = '\0';

  *out = node->meta;

  LOG("create: created ino=%lu\n", (unsigned long)out->ino);
  return 0;
}

int vtfs_storage_unlink(vtfs_ino_t parent, const char* name) {
  LOG("unlink: parent=%lu name=%s\n", parent, name);

  struct vtfs_ram_node* prev = NULL;
  struct vtfs_ram_node* cur = vtfs_nodes_head;

  while (cur) {
    if (cur->meta.parent_ino == parent && !strcmp(cur->name, name)) {
      if (cur->meta.type != VTFS_NODE_FILE) {
        LOG("unlink: not a file\n");
        return -EPERM;
      }

      if (prev) {
        prev->next = cur->next;
      } else {
        vtfs_nodes_head = cur->next;
      }

      kfree(cur);
      LOG("unlink: success\n");
      return 0;
    }
    prev = cur;
    cur = cur->next;
  }

  LOG("unlink: not found\n");
  return -ENOENT;
}

// --- dirs ---
int vtfs_storage_mkdir(
    vtfs_ino_t parent, const char* name, umode_t mode, struct vtfs_node_meta* out
) {
  LOG("mkdir: parent=%lu name='%s' mode=%o\n", (unsigned long)parent, name, mode);

  if (vtfs_find_child(parent, name)) {
    LOG("mkdir failed: '%s' already exists in %lu\n", name, (unsigned long)parent);
    return -EEXIST;
  }

  struct vtfs_ram_node* parent_node = vtfs_find_node_by_ino(parent);
  if (!parent_node || parent_node->meta.type != VTFS_NODE_DIR) {
    LOG("mkdir failed: parent %lu is not a directory\n", (unsigned long)parent);
    return -ENOTDIR;
  }

  struct vtfs_ram_node* node = vtfs_alloc_node();
  if (!node) {
    LOG("mkdir failed: out of memory\n");
    return -ENOMEM;
  }

  node->meta.ino = vtfs_next_ino++;
  node->meta.parent_ino = parent;
  node->meta.type = VTFS_NODE_DIR;
  node->meta.mode = S_IFDIR | (mode & 0777);
  node->meta.size = 0;

  strncpy(node->name, name, NAME_MAX);
  node->name[NAME_MAX] = '\0';

  *out = node->meta;

  LOG("mkdir success: '%s' (ino=%lu) under parent=%lu\n",
      name,
      (unsigned long)node->meta.ino,
      (unsigned long)parent);

  return 0;
}

int vtfs_storage_rmdir(vtfs_ino_t parent, const char* name) {
  LOG("rmdir: parent=%lu name='%s'\n", (unsigned long)parent, name);

  struct vtfs_ram_node* prev = NULL;
  struct vtfs_ram_node* cur = vtfs_nodes_head;

  while (cur) {
    if (cur->meta.parent_ino == parent && !strcmp(cur->name, name)) {
      if (cur->meta.type != VTFS_NODE_DIR) {
        LOG("rmdir failed: '%s' is not a directory\n", name);
        return -ENOTDIR;
      }

      struct vtfs_ram_node* scan = vtfs_nodes_head;
      while (scan) {
        if (scan->meta.parent_ino == cur->meta.ino) {
          LOG("rmdir failed: '%s' is not empty\n", name);
          return -ENOTEMPTY;
        }
        scan = scan->next;
      }

      if (prev) {
        prev->next = cur->next;
      } else {
        vtfs_nodes_head = cur->next;
      }

      LOG("rmdir success: '%s' (ino=%lu)\n", name, (unsigned long)cur->meta.ino);

      kfree(cur);
      return 0;
    }

    prev = cur;
    cur = cur->next;
  }

  LOG("rmdir failed: '%s' not found under parent %lu\n", name, (unsigned long)parent);

  return -ENOENT;
}
