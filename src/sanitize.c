#include "sanitize.h"
#include "markup.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int note(scrub_context *ctx, const char *kind, const char *name) {
    size_t need = strlen(kind) + strlen(name) + 4;
    char *msg = malloc(need);
    if (!msg) return 0;
    snprintf(msg, need, "%s: %s", kind, name);
    if (ctx->message_count == ctx->message_cap) {
        size_t next = ctx->message_cap ? ctx->message_cap * 2 : 16;
        char **messages = realloc(ctx->messages, next * sizeof(*messages));
        if (!messages) {
            free(msg);
            return 0;
        }
        ctx->messages = messages;
        ctx->message_cap = next;
    }
    ctx->messages[ctx->message_count++] = msg;
    return 1;
}

static int note_reason(scrub_context *ctx, const char *kind, const char *name, const char *reason) {
    size_t need = strlen(kind) + strlen(name) + strlen(reason) + strlen(":  (reason: )") + 1;
    char *msg = malloc(need);
    if (!msg) return 0;
    snprintf(msg, need, "%s: %s (reason: %s)", kind, name, reason);
    if (ctx->message_count == ctx->message_cap) {
        size_t next = ctx->message_cap ? ctx->message_cap * 2 : 16;
        char **messages = realloc(ctx->messages, next * sizeof(*messages));
        if (!messages) {
            free(msg);
            return 0;
        }
        ctx->messages = messages;
        ctx->message_cap = next;
    }
    ctx->messages[ctx->message_count++] = msg;
    return 1;
}

static int note_reason_line(scrub_context *ctx, const char *kind, const char *name, size_t line, const char *reason) {
    size_t line_digits = 1;
    for (size_t n = line; n >= 10; n /= 10) line_digits++;
    size_t need = strlen(kind) + strlen(name) + strlen(reason) + strlen(":  (line: , reason: )") + line_digits + 1;
    char *msg = malloc(need);
    if (!msg) return 0;
    snprintf(msg, need, "%s: %s (line: %zu, reason: %s)", kind, name, line, reason);
    if (ctx->message_count == ctx->message_cap) {
        size_t next = ctx->message_cap ? ctx->message_cap * 2 : 16;
        char **messages = realloc(ctx->messages, next * sizeof(*messages));
        if (!messages) {
            free(msg);
            return 0;
        }
        ctx->messages = messages;
        ctx->message_cap = next;
    }
    ctx->messages[ctx->message_count++] = msg;
    return 1;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static int eqi(char a, char b) {
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

const char *scrub_policy_name(scrub_policy policy) {
    switch (policy) {
        case SCRUB_POLICY_CALM:
            return "calm";
        case SCRUB_POLICY_WARY:
            return "wary";
        case SCRUB_POLICY_PARANOID:
            return "paranoid";
    }
    return "wary";
}

int scrub_parse_policy(const char *name, scrub_policy *policy) {
    if (strcmp(name, "calm") == 0) {
        *policy = SCRUB_POLICY_CALM;
        return 1;
    }
    if (strcmp(name, "wary") == 0) {
        *policy = SCRUB_POLICY_WARY;
        return 1;
    }
    if (strcmp(name, "paranoid") == 0) {
        *policy = SCRUB_POLICY_PARANOID;
        return 1;
    }
    return 0;
}

int scrub_parse_report_format(const char *name, scrub_report_format *format) {
    if (strcmp(name, "text") == 0) {
        *format = SCRUB_REPORT_TEXT;
        return 1;
    }
    if (strcmp(name, "json") == 0) {
        *format = SCRUB_REPORT_JSON;
        return 1;
    }
    return 0;
}

static int starts_i(const char *s, const char *prefix) {
    while (*prefix) {
        if (!*s || !eqi(*s, *prefix)) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static int ends_i(const char *s, const char *suffix) {
    size_t a = strlen(s);
    size_t b = strlen(suffix);
    return a >= b && starts_i(s + a - b, suffix);
}

static int is_markup_name(const char *name) {
    return ends_i(name, ".xhtml") || ends_i(name, ".html") || ends_i(name, ".htm") || ends_i(name, ".svg") ||
           ends_i(name, ".xml") || ends_i(name, ".ncx");
}

static int contains_i_len(const char *s, size_t len, const char *needle) {
    size_t n = strlen(needle);
    if (n > len) return 0;
    for (size_t i = 0; i + n <= len; i++) {
        size_t j = 0;
        while (j < n && eqi(s[i + j], needle[j])) j++;
        if (j == n) return 1;
    }
    return 0;
}

static const char *removed_file_reason(const char *name, scrub_policy policy) {
    size_t name_len = strlen(name);
    if (name_len > 0 && name[name_len - 1] == '/') {
        return NULL;
    }
    if (policy == SCRUB_POLICY_PARANOID &&
        (ends_i(name, ".svg") || ends_i(name, ".ttf") || ends_i(name, ".otf") || ends_i(name, ".woff") ||
         ends_i(name, ".woff2") || ends_i(name, ".mp3") || ends_i(name, ".mp4") || ends_i(name, ".m4a") ||
         ends_i(name, ".webm") || ends_i(name, ".ogg") || ends_i(name, ".oga") || ends_i(name, ".ogv"))) {
        return "paranoid policy removes active-capable, font, and media resources to minimize attack surface";
    }
    if (ends_i(name, ".js") || ends_i(name, ".mjs")) {
        return "JavaScript is active content and can execute in EPUB readers";
    }
    if (ends_i(name, ".exe") || ends_i(name, ".dll") || ends_i(name, ".dylib") || ends_i(name, ".so") ||
        ends_i(name, ".jar") || ends_i(name, ".class")) {
        return "executable or loadable binary content is not needed for a safe EPUB";
    }
    if (ends_i(name, ".sh") || ends_i(name, ".bat") || ends_i(name, ".cmd") || ends_i(name, ".ps1") ||
        ends_i(name, ".php") || ends_i(name, ".pl") || ends_i(name, ".py") || ends_i(name, ".rb")) {
        return "script file could be used as an exploit payload";
    }
    if (ends_i(name, ".zip") || ends_i(name, ".rar") || ends_i(name, ".7z") || ends_i(name, ".tar") ||
        ends_i(name, ".gz")) {
        return "nested archive increases unpacking risk and is outside the EPUB reading surface";
    }
    const char *safe[] = {
        "mimetype", ".xml", ".opf", ".ncx", ".xhtml", ".html", ".htm", ".css",
        ".svg", ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif", ".ttf",
        ".otf", ".woff", ".woff2", ".txt", ".xpgt", ".mp3", ".mp4", ".m4a",
        ".webm", ".ogg", ".oga", ".ogv", NULL
    };
    for (int i = 0; safe[i]; i++) {
        if (strcmp(name, safe[i]) == 0 || ends_i(name, safe[i])) return NULL;
    }
    return "file type is not on the EPUB sanitizer allowlist";
}

static char *append_range(char *out, size_t *len, size_t *cap, const char *src, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t next = *cap ? *cap : 256;
        while (*len + n + 1 > next) next *= 2;
        char *tmp = realloc(out, next);
        if (!tmp) {
            free(out);
            return NULL;
        }
        out = tmp;
        *cap = next;
    }
    memcpy(out + *len, src, n);
    *len += n;
    out[*len] = '\0';
    return out;
}

static char *sanitize_css(const char *src, int *changed, scrub_policy policy) {
    size_t len = strlen(src);
    size_t out_len = 0, cap = 0;
    char *out = NULL;
    for (size_t i = 0; i < len;) {
        if (policy != SCRUB_POLICY_CALM && starts_i(src + i, "@import")) {
            while (i < len && src[i] != ';' && src[i] != '\n') i++;
            if (i < len && src[i] == ';') i++;
            *changed = 1;
            continue;
        }
        if (starts_i(src + i, "url(")) {
            size_t j = i + 4;
            while (j < len && src[j] != ')') j++;
            size_t url_len = j < len ? j - i + 1 : len - i;
            if ((policy != SCRUB_POLICY_CALM &&
                 (contains_i_len(src + i, url_len, "http://") || contains_i_len(src + i, url_len, "https://") ||
                  contains_i_len(src + i, url_len, "data:"))) ||
                contains_i_len(src + i, url_len, "javascript:")) {
                out = append_range(out, &out_len, &cap, "none", 4);
                if (!out) return NULL;
                i += url_len;
                *changed = 1;
                continue;
            }
        }
        out = append_range(out, &out_len, &cap, src + i, 1);
        if (!out) return NULL;
        i++;
    }
    return out ? out : xstrdup("");
}

static int removed_name(zip_archive *archive, const char *name) {
    for (size_t i = 0; i < archive->count; i++) {
        if (archive->entries[i].removed && strcmp(archive->entries[i].name, name) == 0) return 1;
    }
    return 0;
}

static char *join_opf_href(const char *opf_name, const char *href) {
    const char *slash = strrchr(opf_name, '/');
    size_t dir_len = slash ? (size_t)(slash - opf_name) + 1 : 0;
    size_t href_len = strcspn(href, "?#\"");
    char *out = malloc(dir_len + href_len + 1);
    if (!out) return NULL;
    memcpy(out, opf_name, dir_len);
    memcpy(out + dir_len, href, href_len);
    out[dir_len + href_len] = '\0';
    return out;
}

static char *sanitize_opf_manifest(const char *src, const char *opf_name, zip_archive *archive, int *changed) {
    size_t len = strlen(src);
    size_t out_len = 0, cap = 0;
    char *out = NULL;
    for (size_t i = 0; i < len;) {
        if (src[i] == '<' && starts_i(src + i, "<item")) {
            char *end = strchr(src + i, '>');
            if (end) {
                size_t tag_len = (size_t)(end - (src + i)) + 1;
                char *href = NULL;
                for (size_t p = i; p + 5 < i + tag_len; p++) {
                    if (starts_i(src + p, "href=")) {
                        char quote = src[p + 5];
                        if (quote == '\'' || quote == '"') {
                            size_t v = p + 6;
                            size_t e = v;
                            while (e < i + tag_len && src[e] != quote) e++;
                            href = malloc(e - v + 1);
                            if (!href) return NULL;
                            memcpy(href, src + v, e - v);
                            href[e - v] = '\0';
                        }
                        break;
                    }
                }
                if (href) {
                    char *joined = join_opf_href(opf_name, href);
                    free(href);
                    if (!joined) return NULL;
                    if (removed_name(archive, joined)) {
                        free(joined);
                        i += tag_len;
                        *changed = 1;
                        continue;
                    }
                    free(joined);
                }
            }
        }
        out = append_range(out, &out_len, &cap, src + i, 1);
        if (!out) return NULL;
        i++;
    }
    return out ? out : xstrdup("");
}

static int replace_entry_data(zip_entry *entry, char *text) {
    size_t n = strlen(text);
    unsigned char *data = malloc(n ? n : 1);
    if (!data) {
        free(text);
        return 0;
    }
    memcpy(data, text, n);
    free(text);
    free(entry->data);
    entry->data = data;
    entry->size = n;
    return 1;
}

static size_t first_changed_line(const char *before, const char *after) {
    size_t i = 0;
    while (before[i] && after[i] && before[i] == after[i]) i++;
    size_t line = 1;
    for (size_t j = 0; j < i; j++) {
        if (before[j] == '\n') line++;
    }
    return line;
}

int scrub_entries(zip_archive *archive, scrub_context *ctx) {
    int has_mimetype = 0;
    ctx->files_scanned = archive->count;
    if (archive->count == 0 || strcmp(archive->entries[0].name, "mimetype") != 0) {
        if (ctx->fix) {
            ctx->changed = 1;
            ctx->warnings++;
            note(ctx, "fix", "mimetype will be written as the first ZIP entry");
        } else {
            ctx->rejected = 1;
            ctx->warnings++;
            note(ctx, "reject", "mimetype is not the first ZIP entry");
        }
    }
    for (size_t i = 0; i < archive->count; i++) {
        zip_entry *entry = &archive->entries[i];
        if (strcmp(entry->name, "mimetype") == 0) {
            has_mimetype = 1;
            if (entry->method != 0) {
                if (ctx->fix) {
                    ctx->changed = 1;
                    ctx->warnings++;
                    note(ctx, "fix", "mimetype will be written uncompressed");
                } else {
                    ctx->rejected = 1;
                    ctx->warnings++;
                    note(ctx, "reject", "mimetype is compressed");
                }
            }
            if (entry->size != strlen("application/epub+zip") ||
                memcmp(entry->data, "application/epub+zip", entry->size) != 0) {
                ctx->rejected = 1;
                ctx->warnings++;
                note(ctx, "reject invalid mimetype", entry->name);
            }
        }
        const char *reason = removed_file_reason(entry->name, ctx->policy);
        if (reason) {
            entry->removed = 1;
            ctx->changed = 1;
            ctx->files_removed++;
            note_reason(ctx, "remove file", entry->name, reason);
        }
    }
    if (!has_mimetype) {
        ctx->rejected = 1;
        ctx->warnings++;
        note(ctx, "reject missing", "mimetype");
    }
    if (ctx->rejected) return 0;

    for (size_t i = 0; i < archive->count; i++) {
        zip_entry *entry = &archive->entries[i];
        if (entry->removed) continue;
        char *src = malloc(entry->size + 1);
        if (!src) return -1;
        memcpy(src, entry->data, entry->size);
        src[entry->size] = '\0';
        int changed = 0;
        char *clean = NULL;
        if (is_markup_name(entry->name)) {
            clean = markup_sanitize(src, ctx->policy, &changed);
        } else if (ends_i(entry->name, ".css")) {
            clean = sanitize_css(src, &changed, ctx->policy);
        } else if (ends_i(entry->name, ".opf")) {
            clean = markup_sanitize(src, ctx->policy, &changed);
            if (clean) {
                char *opf = sanitize_opf_manifest(clean, entry->name, archive, &changed);
                free(clean);
                clean = opf;
            }
        }
        if (!clean && (is_markup_name(entry->name) || ends_i(entry->name, ".css") || ends_i(entry->name, ".opf"))) {
            free(src);
            return -1;
        }
        if (changed) {
            size_t line = first_changed_line(src, clean);
            if (!replace_entry_data(entry, clean)) {
                free(src);
                return -1;
            }
            ctx->changed = 1;
            ctx->files_modified++;
            if (ends_i(entry->name, ".css")) {
                note_reason_line(ctx, "sanitize", entry->name, line,
                                 "removed remote imports or unsafe CSS URLs; local CSS is preserved");
            } else if (ends_i(entry->name, ".opf")) {
                note_reason_line(ctx, "sanitize", entry->name, line,
                                 "removed unsafe metadata or manifest references to removed files");
            } else {
                note_reason_line(ctx, "sanitize", entry->name, line,
                                 "removed active markup, event handlers, or unsafe external/script URLs");
            }
        } else {
            free(clean);
        }
        free(src);
    }
    return 0;
}

static const char *status_name(const scrub_context *ctx) {
    return ctx->rejected ? "rejected" : (ctx->changed ? "sanitized" : "clean");
}

static void json_string(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':
                fputs("\\\"", f);
                break;
            case '\\':
                fputs("\\\\", f);
                break;
            case '\b':
                fputs("\\b", f);
                break;
            case '\f':
                fputs("\\f", f);
                break;
            case '\n':
                fputs("\\n", f);
                break;
            case '\r':
                fputs("\\r", f);
                break;
            case '\t':
                fputs("\\t", f);
                break;
            default:
                if (c < 0x20) {
                    fprintf(f, "\\u%04x", c);
                } else {
                    fputc(c, f);
                }
                break;
        }
    }
    fputc('"', f);
}

int scrub_report(const scrub_context *ctx, const char *input, const char *output) {
    FILE *f = stdout;
    if (ctx->report_path) {
        f = fopen(ctx->report_path, "w");
        if (!f) return -1;
    }
    if (ctx->report_format == SCRUB_REPORT_JSON) {
        fputs("{\n", f);
        fputs("  \"input\": ", f);
        json_string(f, input);
        fputs(",\n", f);
        fputs("  \"output\": ", f);
        if (output) {
            json_string(f, output);
        } else {
            fputs("null", f);
        }
        fputs(",\n", f);
        fprintf(f, "  \"status\": \"%s\",\n", status_name(ctx));
        fprintf(f, "  \"policy\": \"%s\",\n", scrub_policy_name(ctx->policy));
        fputs("  \"summary\": {\n", f);
        fprintf(f, "    \"files_scanned\": %zu,\n", ctx->files_scanned);
        fprintf(f, "    \"files_removed\": %zu,\n", ctx->files_removed);
        fprintf(f, "    \"files_modified\": %zu,\n", ctx->files_modified);
        fprintf(f, "    \"warnings\": %zu\n", ctx->warnings);
        fputs("  },\n", f);
        fputs("  \"events\": [\n", f);
        for (size_t i = 0; i < ctx->message_count; i++) {
            fputs("    ", f);
            json_string(f, ctx->messages[i]);
            fputs(i + 1 == ctx->message_count ? "\n" : ",\n", f);
        }
        fputs("  ]\n", f);
        fputs("}\n", f);
    } else {
        fprintf(f, "input: %s\n", input);
        if (output) fprintf(f, "output: %s\n", output);
        fprintf(f, "status: %s\n", status_name(ctx));
        fprintf(f, "policy: %s\n", scrub_policy_name(ctx->policy));
        fprintf(f, "summary: files_scanned=%zu files_removed=%zu files_modified=%zu warnings=%zu\n",
                ctx->files_scanned, ctx->files_removed, ctx->files_modified, ctx->warnings);
        for (size_t i = 0; i < ctx->message_count; i++) {
            fprintf(f, "%s\n", ctx->messages[i]);
        }
    }
    if (ctx->report_path) fclose(f);
    return 0;
}

void scrub_context_free(scrub_context *ctx) {
    for (size_t i = 0; i < ctx->message_count; i++) {
        free(ctx->messages[i]);
    }
    free(ctx->messages);
    memset(ctx, 0, sizeof(*ctx));
}
