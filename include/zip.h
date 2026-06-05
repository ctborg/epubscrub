#ifndef EPUBSCRUB_ZIP_H
#define EPUBSCRUB_ZIP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *name;
    unsigned char *data;
    size_t size;
    uint32_t crc32;
    uint16_t method;
    int removed;
} zip_entry;

typedef struct {
    zip_entry *entries;
    size_t count;
    size_t cap;
} zip_archive;

typedef enum {
    ZIP_OK = 0,
    ZIP_ERR_IO,
    ZIP_ERR_FORMAT,
    ZIP_ERR_UNSUPPORTED,
    ZIP_ERR_MEMORY,
    ZIP_ERR_UNSAFE
} zip_result;

zip_result zip_read_archive(const char *path, zip_archive *archive, char *err, size_t err_len);
zip_result zip_write_archive(const char *path, const zip_archive *archive, char *err, size_t err_len);
void zip_archive_free(zip_archive *archive);

#endif
