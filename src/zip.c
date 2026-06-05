#include "zip.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define EOCD_SIG 0x06054b50u
#define CEN_SIG 0x02014b50u
#define LOC_SIG 0x04034b50u
#define MAX_ARCHIVE_SIZE (512u * 1024u * 1024u)
#define MAX_ENTRY_SIZE (64u * 1024u * 1024u)
#define MAX_TOTAL_SIZE (512u * 1024u * 1024u)
#define MAX_RATIO 100u

static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(FILE *f, uint16_t v) {
    fputc(v & 0xff, f);
    fputc((v >> 8) & 0xff, f);
}

static void wr32(FILE *f, uint32_t v) {
    fputc(v & 0xff, f);
    fputc((v >> 8) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >> 24) & 0xff, f);
}

static void set_err(char *err, size_t err_len, const char *msg) {
    if (err_len > 0) {
        snprintf(err, err_len, "%s", msg);
    }
}

static int read_file(const char *path, unsigned char **out, size_t *len, char *err, size_t err_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err_len > 0) snprintf(err, err_len, "could not read input file '%s': %s", path, strerror(errno));
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        if (err_len > 0) snprintf(err, err_len, "could not seek input file '%s': %s", path, strerror(errno));
        fclose(f);
        return 0;
    }
    long n = ftell(f);
    if (n < 0) {
        if (err_len > 0) snprintf(err, err_len, "could not size input file '%s': %s", path, strerror(errno));
        fclose(f);
        return 0;
    }
    rewind(f);
    unsigned char *buf = malloc((size_t)n ? (size_t)n : 1);
    if (!buf) {
        if (err_len > 0) snprintf(err, err_len, "could not allocate memory for input file '%s'", path);
        fclose(f);
        return 0;
    }
    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        if (err_len > 0) snprintf(err, err_len, "could not read input file '%s': %s", path, strerror(errno));
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    *out = buf;
    *len = (size_t)n;
    return 1;
}

static int safe_name(const char *name) {
    if (!name || !*name) return 0;
    if (name[0] == '/' || name[0] == '\\') return 0;
    if (strstr(name, "../") || strstr(name, "..\\") || strcmp(name, "..") == 0) return 0;
    for (const char *p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == '\\') return 0;
    }
    return 1;
}

static int add_entry(zip_archive *archive, zip_entry entry) {
    if (archive->count == archive->cap) {
        size_t next = archive->cap ? archive->cap * 2 : 16;
        zip_entry *entries = realloc(archive->entries, next * sizeof(*entries));
        if (!entries) return 0;
        archive->entries = entries;
        archive->cap = next;
    }
    archive->entries[archive->count++] = entry;
    return 1;
}

static int inflate_raw(const unsigned char *src, size_t src_len, unsigned char *dst, size_t dst_len) {
    z_stream s;
    memset(&s, 0, sizeof(s));
    s.next_in = (Bytef *)src;
    s.avail_in = (uInt)src_len;
    s.next_out = dst;
    s.avail_out = (uInt)dst_len;
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) return 0;
    int z = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    return z == Z_STREAM_END && s.total_out == dst_len;
}

zip_result zip_read_archive(const char *path, zip_archive *archive, char *err, size_t err_len) {
    unsigned char *buf = NULL;
    size_t len = 0;
    if (!read_file(path, &buf, &len, err, err_len)) {
        return ZIP_ERR_IO;
    }
    if (len > MAX_ARCHIVE_SIZE) {
        free(buf);
        set_err(err, err_len, "EPUB archive exceeds the configured size limit");
        return ZIP_ERR_UNSAFE;
    }
    if (len < 22) {
        free(buf);
        set_err(err, err_len, "not a ZIP/EPUB file");
        return ZIP_ERR_FORMAT;
    }

    size_t search = len < 66000 ? len : 66000;
    size_t start = len - 22;
    size_t stop = len > search ? len - search : 0;
    size_t eocd = SIZE_MAX;
    for (size_t i = start;; i--) {
        if (rd32(buf + i) == EOCD_SIG) {
            eocd = i;
            break;
        }
        if (i == stop) break;
    }
    if (eocd == SIZE_MAX) {
        free(buf);
        set_err(err, err_len, "ZIP central directory not found");
        return ZIP_ERR_FORMAT;
    }

    uint16_t entries = rd16(buf + eocd + 10);
    uint32_t cd_size = rd32(buf + eocd + 12);
    uint32_t cd_off = rd32(buf + eocd + 16);
    if ((uint64_t)cd_off + cd_size > len) {
        free(buf);
        set_err(err, err_len, "ZIP central directory is out of bounds");
        return ZIP_ERR_FORMAT;
    }

    size_t pos = cd_off;
    size_t total = 0;
    for (uint16_t i = 0; i < entries; i++) {
        if (pos + 46 > len || rd32(buf + pos) != CEN_SIG) {
            free(buf);
            set_err(err, err_len, "bad ZIP central directory entry");
            return ZIP_ERR_FORMAT;
        }
        uint16_t flags = rd16(buf + pos + 8);
        uint16_t method = rd16(buf + pos + 10);
        uint32_t crc = rd32(buf + pos + 16);
        uint32_t comp_size = rd32(buf + pos + 20);
        uint32_t uncomp_size = rd32(buf + pos + 24);
        uint16_t name_len = rd16(buf + pos + 28);
        uint16_t extra_len = rd16(buf + pos + 30);
        uint16_t comment_len = rd16(buf + pos + 32);
        uint16_t made_by = rd16(buf + pos + 4);
        uint32_t ext_attr = rd32(buf + pos + 38);
        uint32_t local_off = rd32(buf + pos + 42);
        if (pos + 46u + name_len + extra_len + comment_len > len) {
            free(buf);
            set_err(err, err_len, "ZIP entry metadata is out of bounds");
            return ZIP_ERR_FORMAT;
        }
        char *name = malloc((size_t)name_len + 1);
        if (!name) {
            free(buf);
            return ZIP_ERR_MEMORY;
        }
        memcpy(name, buf + pos + 46, name_len);
        name[name_len] = '\0';
        if (!safe_name(name)) {
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry has an unsafe path");
            return ZIP_ERR_UNSAFE;
        }
        if ((made_by >> 8) == 3 && ((ext_attr >> 16) & 0170000) == 0120000) {
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry is a symlink");
            return ZIP_ERR_UNSAFE;
        }
        if (flags & 0x1) {
            free(name);
            free(buf);
            set_err(err, err_len, "encrypted ZIP entries are unsupported");
            return ZIP_ERR_UNSUPPORTED;
        }
        if (method != 0 && method != 8) {
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry uses an unsupported compression method");
            return ZIP_ERR_UNSUPPORTED;
        }
        if (uncomp_size > MAX_ENTRY_SIZE || total + uncomp_size > MAX_TOTAL_SIZE ||
            (comp_size > 0 && uncomp_size / comp_size > MAX_RATIO)) {
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry exceeds size or compression-ratio limits");
            return ZIP_ERR_UNSAFE;
        }
        if (local_off + 30u > len || rd32(buf + local_off) != LOC_SIG) {
            free(name);
            free(buf);
            set_err(err, err_len, "bad ZIP local file header");
            return ZIP_ERR_FORMAT;
        }
        uint16_t local_name_len = rd16(buf + local_off + 26);
        uint16_t local_extra_len = rd16(buf + local_off + 28);
        size_t data_off = (size_t)local_off + 30u + local_name_len + local_extra_len;
        if (data_off + comp_size > len) {
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry data is out of bounds");
            return ZIP_ERR_FORMAT;
        }
        unsigned char *data = malloc(uncomp_size ? uncomp_size : 1);
        if (!data) {
            free(name);
            free(buf);
            return ZIP_ERR_MEMORY;
        }
        if (method == 0) {
            if (comp_size != uncomp_size) {
                free(data);
                free(name);
                free(buf);
                set_err(err, err_len, "stored ZIP entry has inconsistent sizes");
                return ZIP_ERR_FORMAT;
            }
            memcpy(data, buf + data_off, uncomp_size);
        } else if (!inflate_raw(buf + data_off, comp_size, data, uncomp_size)) {
            free(data);
            free(name);
            free(buf);
            set_err(err, err_len, "could not inflate ZIP entry");
            return ZIP_ERR_FORMAT;
        }
        if ((uint32_t)crc32(0, data, (uInt)uncomp_size) != crc) {
            free(data);
            free(name);
            free(buf);
            set_err(err, err_len, "ZIP entry CRC mismatch");
            return ZIP_ERR_FORMAT;
        }
        zip_entry entry = {name, data, uncomp_size, crc, method, 0};
        if (!add_entry(archive, entry)) {
            free(data);
            free(name);
            free(buf);
            return ZIP_ERR_MEMORY;
        }
        total += uncomp_size;
        pos += 46u + name_len + extra_len + comment_len;
    }
    free(buf);
    return ZIP_OK;
}

static int deflate_raw(const unsigned char *src, size_t src_len, unsigned char **dst, size_t *dst_len) {
    uLong bound = compressBound(src_len);
    unsigned char *out = malloc(bound ? bound : 1);
    if (!out) return 0;
    z_stream s;
    memset(&s, 0, sizeof(s));
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(out);
        return 0;
    }
    s.next_in = (Bytef *)src;
    s.avail_in = (uInt)src_len;
    s.next_out = out;
    s.avail_out = (uInt)bound;
    int z = deflate(&s, Z_FINISH);
    if (z != Z_STREAM_END) {
        deflateEnd(&s);
        free(out);
        return 0;
    }
    *dst_len = s.total_out;
    deflateEnd(&s);
    *dst = out;
    return 1;
}

typedef struct {
    uint32_t offset;
    uint32_t comp_size;
    uint16_t method;
    uint32_t crc;
} written_entry;

static int write_one(FILE *f, const zip_entry *entry, written_entry *meta) {
    unsigned char *payload = entry->data;
    size_t payload_len = entry->size;
    uint16_t method = strcmp(entry->name, "mimetype") == 0 ? 0 : 8;
    unsigned char *deflated = NULL;
    if (method == 8) {
        if (!deflate_raw(entry->data, entry->size, &deflated, &payload_len)) return 0;
        payload = deflated;
    }
    long off = ftell(f);
    if (off < 0 || off > UINT32_MAX) {
        free(deflated);
        return 0;
    }
    uint32_t crc = (uint32_t)crc32(0, entry->data, (uInt)entry->size);
    size_t name_len = strlen(entry->name);
    wr32(f, LOC_SIG);
    wr16(f, 20);
    wr16(f, 0);
    wr16(f, method);
    wr16(f, 0);
    wr16(f, 0);
    wr32(f, crc);
    wr32(f, (uint32_t)payload_len);
    wr32(f, (uint32_t)entry->size);
    wr16(f, (uint16_t)name_len);
    wr16(f, 0);
    fwrite(entry->name, 1, name_len, f);
    fwrite(payload, 1, payload_len, f);
    free(deflated);
    meta->offset = (uint32_t)off;
    meta->comp_size = (uint32_t)payload_len;
    meta->method = method;
    meta->crc = crc;
    return !ferror(f);
}

zip_result zip_write_archive(const char *path, const zip_archive *archive, char *err, size_t err_len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_err(err, err_len, "could not open output file");
        return ZIP_ERR_IO;
    }
    written_entry *meta = calloc(archive->count, sizeof(*meta));
    if (!meta) {
        fclose(f);
        return ZIP_ERR_MEMORY;
    }

    size_t written = 0;
    for (size_t pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < archive->count; i++) {
            const zip_entry *entry = &archive->entries[i];
            if (entry->removed) continue;
            int is_mimetype = strcmp(entry->name, "mimetype") == 0;
            if ((pass == 0 && !is_mimetype) || (pass == 1 && is_mimetype)) continue;
            if (!write_one(f, entry, &meta[i])) {
                free(meta);
                fclose(f);
                set_err(err, err_len, "could not write ZIP entry");
                return ZIP_ERR_IO;
            }
            written++;
        }
    }

    long cd_start_l = ftell(f);
    if (cd_start_l < 0 || cd_start_l > UINT32_MAX) {
        free(meta);
        fclose(f);
        return ZIP_ERR_IO;
    }
    uint32_t cd_start = (uint32_t)cd_start_l;
    for (size_t i = 0; i < archive->count; i++) {
        const zip_entry *entry = &archive->entries[i];
        if (entry->removed) continue;
        size_t name_len = strlen(entry->name);
        wr32(f, CEN_SIG);
        wr16(f, 20);
        wr16(f, 20);
        wr16(f, 0);
        wr16(f, meta[i].method);
        wr16(f, 0);
        wr16(f, 0);
        wr32(f, meta[i].crc);
        wr32(f, meta[i].comp_size);
        wr32(f, (uint32_t)entry->size);
        wr16(f, (uint16_t)name_len);
        wr16(f, 0);
        wr16(f, 0);
        wr16(f, 0);
        wr16(f, 0);
        wr32(f, 0);
        wr32(f, meta[i].offset);
        fwrite(entry->name, 1, name_len, f);
    }
    long cd_end_l = ftell(f);
    if (cd_end_l < 0 || cd_end_l > UINT32_MAX) {
        free(meta);
        fclose(f);
        return ZIP_ERR_IO;
    }
    uint32_t cd_size = (uint32_t)cd_end_l - cd_start;
    wr32(f, EOCD_SIG);
    wr16(f, 0);
    wr16(f, 0);
    wr16(f, (uint16_t)written);
    wr16(f, (uint16_t)written);
    wr32(f, cd_size);
    wr32(f, cd_start);
    wr16(f, 0);
    free(meta);
    if (fclose(f) != 0) {
        set_err(err, err_len, "could not finish output file");
        return ZIP_ERR_IO;
    }
    return ZIP_OK;
}

void zip_archive_free(zip_archive *archive) {
    for (size_t i = 0; i < archive->count; i++) {
        free(archive->entries[i].name);
        free(archive->entries[i].data);
    }
    free(archive->entries);
    memset(archive, 0, sizeof(*archive));
}
