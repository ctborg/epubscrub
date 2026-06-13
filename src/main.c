#include "sanitize.h"
#include "version.h"
#include "zip.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(FILE *out) {
    fputs("Usage: epubscrub [--dryrun] [--fix] [--policy calm|wary|paranoid] "
          "[--report FILE] [--report-format text|json] INPUT.epub [-o OUTPUT.epub]\n",
          out);
}

static int same_existing_file(const char *a, const char *b) {
    struct stat sa;
    struct stat sb;
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0) return 0;
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static int same_path_target(const char *input, const char *output) {
    if (same_existing_file(input, output)) return 1;

    char input_real[PATH_MAX];
    if (!realpath(input, input_real)) return 0;

    char output_copy[PATH_MAX];
    if (snprintf(output_copy, sizeof(output_copy), "%s", output) >= (int)sizeof(output_copy)) return 0;

    char *slash = strrchr(output_copy, '/');
    const char *base = output_copy;
    const char *parent = ".";
    char parent_buf[PATH_MAX];
    if (slash) {
        base = slash + 1;
        if (slash == output_copy) {
            parent = "/";
        } else {
            *slash = '\0';
            parent = output_copy;
        }
    }
    if (*base == '\0') return 0;

    char parent_real[PATH_MAX];
    if (!realpath(parent, parent_real)) return 0;
    if (snprintf(parent_buf, sizeof(parent_buf), "%s/%s", parent_real, base) >= (int)sizeof(parent_buf)) return 0;
    return strcmp(input_real, parent_buf) == 0;
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
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("epubscrub %s\n", EPUBSCRUB_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--dryrun") == 0 || strcmp(argv[i], "--check") == 0) {
            ctx.check_only = 1;
        } else if (strcmp(argv[i], "--fix") == 0) {
            ctx.fix = 1;
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

    if (!input) {
        usage(stderr);
        return 3;
    }
    if (!ctx.check_only && !output && ctx.report_path) {
        ctx.check_only = 1;
    }
    if (!ctx.check_only && !output) {
        usage(stderr);
        return 3;
    }
    if (!ctx.check_only && output && same_path_target(input, output)) {
        fprintf(stderr, "epubscrub: output path must not be the same as input path\n");
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

    if (!ctx.check_only) {
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
