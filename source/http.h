#ifndef VTFS_HTTP_H
#define VTFS_HTTP_H

#include <linux/inet.h>

int64_t vtfs_http_call(
    const char* token,
    const char* method,
    char* response_buffer,
    size_t buffer_size,
    size_t arg_size,
    ...
);

int64_t vtfs_http_call_with_body(
    const char* token,
    const char* method,
    const void* body,
    size_t body_len,
    char* response_buffer,
    size_t response_size,
    size_t arg_size,
    ...
);

void encode(const char*, char*);

#endif  // VTFS_HTTP_H
