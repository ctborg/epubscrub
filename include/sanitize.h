#ifndef EPUBSCRUB_SANITIZE_H
#define EPUBSCRUB_SANITIZE_H

#include "zip.h"

typedef enum {
    SCRUB_POLICY_CALM = 0,
    SCRUB_POLICY_WARY,
    SCRUB_POLICY_PARANOID
} scrub_policy;

typedef enum {
    SCRUB_REPORT_TEXT = 0,
    SCRUB_REPORT_JSON
} scrub_report_format;

typedef struct {
    int check_only;
    const char *report_path;
    scrub_policy policy;
    scrub_report_format report_format;
    char **messages;
    size_t message_count;
    size_t message_cap;
    size_t files_scanned;
    size_t files_removed;
    size_t files_modified;
    size_t warnings;
    int changed;
    int rejected;
} scrub_context;

const char *scrub_policy_name(scrub_policy policy);
int scrub_parse_policy(const char *name, scrub_policy *policy);
int scrub_parse_report_format(const char *name, scrub_report_format *format);
int scrub_entries(zip_archive *archive, scrub_context *ctx);
void scrub_context_free(scrub_context *ctx);
int scrub_report(const scrub_context *ctx, const char *input, const char *output);

#endif
