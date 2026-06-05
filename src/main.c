#include "sanitize.h"
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out) {
    fputs("Usage: epubscrub [--check] [--policy calm|wary|paranoid] "
          "[--report FILE] [--report-format text|json] INPUT.epub [-o OUTPUT.epub]\n",
          out);
}

int main(int argc, char **argv) {
    const char *input = NULL;
    const char *output = NULL;
    scrub_context ctx = {0};
    ctx.policy = SCRUB_POLICY_WARY;
    ctx.report_format = SCRUB_REPORT_TEXT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--check") == 0) {
            ctx.check_only = 1;
        } else if (strcmp(argv[i], "--report") == 0) {
            if (++i >= argc) {
                usage(stderr);
                return 3;
            }
            ctx.report_path = argv[i];
        } else if (strcmp(argv[i], "--report-format") == 0) {
            if (++i >= argc || !scrub_parse_report_format(argv[i], &ctx.report_format)) {
                usage(stderr);
                return 3;
            }
        } else if (strcmp(argv[i], "--policy") == 0) {
            if (++i >= argc || !scrub_parse_policy(argv[i], &ctx.policy)) {
                usage(stderr);
                return 3;
            }
        } else if (strcmp(argv[i], "--calm") == 0) {
            ctx.policy = SCRUB_POLICY_CALM;
        } else if (strcmp(argv[i], "--wary") == 0) {
            ctx.policy = SCRUB_POLICY_WARY;
        } else if (strcmp(argv[i], "--paranoid") == 0) {
            ctx.policy = SCRUB_POLICY_PARANOID;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) {
                usage(stderr);
                return 3;
            }
            output = argv[i];
        } else if (!input) {
            input = argv[i];
        } else {
            usage(stderr);
            return 3;
        }
    }

    if (!input || (!ctx.check_only && !output)) {
        usage(stderr);
        return 3;
    }

    char err[512];
    zip_archive archive = {0};
    zip_result zr = zip_read_archive(input, &archive, err, sizeof(err));
    if (zr != ZIP_OK) {
        fprintf(stderr, "epubscrub: %s\n", err);
        return zr == ZIP_ERR_UNSAFE || zr == ZIP_ERR_FORMAT ? 2 : 3;
    }

    int rc = scrub_entries(&archive, &ctx);
    if (rc != 0 || ctx.rejected) {
        scrub_report(&ctx, input, output);
        zip_archive_free(&archive);
        scrub_context_free(&ctx);
        return 2;
    }

    if (!ctx.check_only && ctx.changed) {
        zr = zip_write_archive(output, &archive, err, sizeof(err));
        if (zr != ZIP_OK) {
            fprintf(stderr, "epubscrub: %s\n", err);
            zip_archive_free(&archive);
            scrub_context_free(&ctx);
            return 3;
        }
    }

    scrub_report(&ctx, input, output);
    int exit_code = ctx.changed ? 1 : 0;
    zip_archive_free(&archive);
    scrub_context_free(&ctx);
    return exit_code;
}
