#define _POSIX_C_SOURCE 200809L

#include "sanitize.h"
#include "zip.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char path[] = "/tmp/epubscrub-fuzz-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 0;

    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(path);
        return 0;
    }
    if (size > 0) {
        (void)fwrite(data, 1, size, f);
    }
    fclose(f);

    char err[256];
    zip_archive archive = {0};
    zip_result zr = zip_read_archive(path, &archive, err, sizeof(err));
    unlink(path);
    if (zr != ZIP_OK) {
        zip_archive_free(&archive);
        return 0;
    }

    for (int p = SCRUB_POLICY_CALM; p <= SCRUB_POLICY_PARANOID; p++) {
        scrub_context ctx = {0};
        ctx.policy = (scrub_policy)p;
        ctx.report_format = SCRUB_REPORT_TEXT;
        (void)scrub_entries(&archive, &ctx);
        scrub_context_free(&ctx);
    }

    zip_archive_free(&archive);
    return 0;
}

#ifdef EPUBSCRUB_STANDALONE_FUZZ
static uint32_t fuzz_rand(uint32_t *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void mutate(unsigned char *buf, size_t len, uint32_t *state) {
    if (len == 0) return;
    size_t edits = 1 + (fuzz_rand(state) % 16);
    for (size_t i = 0; i < edits; i++) {
        size_t pos = fuzz_rand(state) % len;
        switch (fuzz_rand(state) % 4) {
            case 0:
                buf[pos] ^= (unsigned char)(1u << (fuzz_rand(state) % 8));
                break;
            case 1:
                buf[pos] = (unsigned char)fuzz_rand(state);
                break;
            case 2:
                buf[pos] = 0;
                break;
            default:
                buf[pos] = 0xff;
                break;
        }
    }
}

int main(int argc, char **argv) {
    int runs = 1000;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-runs=", 6) == 0) {
            runs = atoi(argv[i] + 6);
        }
    }

    static const unsigned char seeds[][32] = {
        "not an epub",
        "PK\003\004",
        "application/epub+zip",
    };
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        LLVMFuzzerTestOneInput(seeds[i], sizeof(seeds[i]));
    }

    uint32_t state = 0x45505542u;
    for (int i = 0; i < runs; i++) {
        size_t len = fuzz_rand(&state) % 4096;
        unsigned char *buf = malloc(len ? len : 1);
        if (!buf) return 1;
        for (size_t j = 0; j < len; j++) {
            buf[j] = (unsigned char)fuzz_rand(&state);
        }
        if (i % 8 == 0 && len >= 4) {
            memcpy(buf, "PK\003\004", 4);
        }
        mutate(buf, len, &state);
        LLVMFuzzerTestOneInput(buf, len);
        free(buf);
    }

    fprintf(stderr, "standalone fuzz smoke completed %d runs\n", runs);
    return 0;
}
#endif
