#include "markup.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TAG_LEN 65536u

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} outbuf;

typedef struct {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
    int has_value;
} attr_view;

static int eqi(char a, char b) {
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

static int span_eq_i(const char *s, size_t len, const char *word) {
    size_t n = strlen(word);
    if (len != n) return 0;
    for (size_t i = 0; i < len; i++) {
        if (!eqi(s[i], word[i])) return 0;
    }
    return 1;
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

static int out_append(outbuf *out, const char *src, size_t n) {
    if (out->len + n + 1 > out->cap) {
        size_t next = out->cap ? out->cap : 256;
        while (out->len + n + 1 > next) next *= 2;
        char *tmp = realloc(out->buf, next);
        if (!tmp) {
            free(out->buf);
            out->buf = NULL;
            return 0;
        }
        out->buf = tmp;
        out->cap = next;
    }
    memcpy(out->buf + out->len, src, n);
    out->len += n;
    out->buf[out->len] = '\0';
    return 1;
}

static int out_append_char(outbuf *out, char c) {
    return out_append(out, &c, 1);
}

static char *out_finish(outbuf *out) {
    if (!out->buf) {
        out->buf = malloc(1);
        if (!out->buf) return NULL;
        out->buf[0] = '\0';
    }
    return out->buf;
}

static int name_char(unsigned char c) {
    return isalnum(c) || c == ':' || c == '_' || c == '-' || c == '.';
}

static int skip_ws(const char *s, size_t len, size_t *p) {
    while (*p < len && isspace((unsigned char)s[*p])) (*p)++;
    return *p < len;
}

static size_t find_tag_end(const char *s, size_t len, size_t start, int *malformed) {
    char quote = '\0';
    if (start + MAX_TAG_LEN < len) {
        len = start + MAX_TAG_LEN;
        *malformed = 1;
    }
    for (size_t i = start + 1; i < len; i++) {
        char c = s[i];
        if (quote) {
            if (c == quote) quote = '\0';
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == '>') return i;
    }
    *malformed = 1;
    return SIZE_MAX;
}

static int tag_name(const char *tag, size_t tag_len, const char **name, size_t *name_len, int *is_end) {
    size_t p = 1;
    *is_end = 0;
    if (p < tag_len && tag[p] == '/') {
        *is_end = 1;
        p++;
    }
    while (p < tag_len && isspace((unsigned char)tag[p])) p++;
    size_t start = p;
    while (p < tag_len && name_char((unsigned char)tag[p])) p++;
    if (p == start) return 0;
    *name = tag + start;
    *name_len = p - start;
    return 1;
}

static int tag_is(const char *name, size_t name_len, const char *word) {
    return span_eq_i(name, name_len, word);
}

static int body_drop_tag(const char *name, size_t name_len) {
    const char *tags[] = {"script", "iframe", "object", "embed", "form", "textarea", "select", "button", NULL};
    for (int i = 0; tags[i]; i++) {
        if (tag_is(name, name_len, tags[i])) return 1;
    }
    return 0;
}

static int tag_only_drop_tag(const char *name, size_t name_len) {
    return tag_is(name, name_len, "input");
}

static int remove_attr_name(const char *name, size_t name_len) {
    if (name_len >= 3 && eqi(name[0], 'o') && eqi(name[1], 'n') && isalpha((unsigned char)name[2])) {
        return 1;
    }
    return 0;
}

static int unsafe_scheme(const char *value, size_t value_len) {
    while (value_len > 0 && isspace((unsigned char)*value)) {
        value++;
        value_len--;
    }
    return contains_i_len(value, value_len, "javascript:") || contains_i_len(value, value_len, "vbscript:") ||
           contains_i_len(value, value_len, "data:text/html") || contains_i_len(value, value_len, "data:image/svg");
}

static int external_url(const char *value, size_t value_len) {
    while (value_len > 0 && isspace((unsigned char)*value)) {
        value++;
        value_len--;
    }
    return contains_i_len(value, value_len, "http://") || contains_i_len(value, value_len, "https://");
}

static int attr_is(const attr_view *attr, const char *name) {
    return span_eq_i(attr->name, attr->name_len, name);
}

static int url_attr(const attr_view *attr) {
    return attr_is(attr, "href") || attr_is(attr, "src") || attr_is(attr, "xlink:href") || attr_is(attr, "poster");
}

static int resource_url_attr(const char *tag, size_t tag_len, const attr_view *attr) {
    if (!attr_is(attr, "src") && !attr_is(attr, "href") && !attr_is(attr, "xlink:href")) return 0;
    return tag_is(tag, tag_len, "img") || tag_is(tag, tag_len, "link") || tag_is(tag, tag_len, "iframe");
}

static int remove_attr(const char *tag, size_t tag_len, const attr_view *attr, scrub_policy policy) {
    if (remove_attr_name(attr->name, attr->name_len)) return 1;
    if (!attr->has_value || !url_attr(attr)) return 0;
    if (unsafe_scheme(attr->value, attr->value_len)) return 1;
    if (policy != SCRUB_POLICY_CALM && resource_url_attr(tag, tag_len, attr) && external_url(attr->value, attr->value_len)) {
        return 1;
    }
    if (policy == SCRUB_POLICY_PARANOID && tag_is(tag, tag_len, "a") && attr_is(attr, "href") &&
        external_url(attr->value, attr->value_len)) {
        return 1;
    }
    return 0;
}

static int parse_attr(const char *tag, size_t tag_len, size_t *p, attr_view *attr, size_t *raw_start, size_t *raw_end) {
    skip_ws(tag, tag_len, p);
    if (*p >= tag_len || tag[*p] == '>' || tag[*p] == '/') return 0;
    *raw_start = *p;
    size_t name_start = *p;
    while (*p < tag_len && name_char((unsigned char)tag[*p])) (*p)++;
    if (*p == name_start) return -1;
    attr->name = tag + name_start;
    attr->name_len = *p - name_start;
    attr->has_value = 0;
    attr->value = NULL;
    attr->value_len = 0;
    skip_ws(tag, tag_len, p);
    if (*p < tag_len && tag[*p] == '=') {
        (*p)++;
        skip_ws(tag, tag_len, p);
        attr->has_value = 1;
        if (*p < tag_len && (tag[*p] == '"' || tag[*p] == '\'')) {
            char q = tag[*p];
            (*p)++;
            size_t value_start = *p;
            while (*p < tag_len && tag[*p] != q) (*p)++;
            attr->value = tag + value_start;
            attr->value_len = *p - value_start;
            if (*p < tag_len) (*p)++;
        } else {
            size_t value_start = *p;
            while (*p < tag_len && !isspace((unsigned char)tag[*p]) && tag[*p] != '>' && tag[*p] != '/') (*p)++;
            attr->value = tag + value_start;
            attr->value_len = *p - value_start;
        }
    }
    *raw_end = *p;
    return 1;
}

static int emit_sanitized_tag(outbuf *out, const char *tag, size_t tag_len, scrub_policy policy, int *changed) {
    const char *name;
    size_t name_len;
    int is_end;
    if (!tag_name(tag, tag_len, &name, &name_len, &is_end)) {
        *changed = 1;
        return 1;
    }
    if (is_end) {
        if (body_drop_tag(name, name_len) || tag_only_drop_tag(name, name_len)) {
            *changed = 1;
            return 1;
        }
        return out_append(out, tag, tag_len);
    }
    if (body_drop_tag(name, name_len) || tag_only_drop_tag(name, name_len)) {
        *changed = 1;
        return 1;
    }

    if (!out_append_char(out, '<')) return 0;
    if (!out_append(out, name, name_len)) return 0;

    size_t p = (size_t)(name - tag) + name_len;
    int self_closing = 0;
    while (p < tag_len && tag[p] != '>') {
        if (isspace((unsigned char)tag[p])) {
            p++;
            continue;
        }
        if (tag[p] == '/') {
            self_closing = 1;
            p++;
            continue;
        }
        attr_view attr;
        size_t raw_start, raw_end;
        int parsed = parse_attr(tag, tag_len, &p, &attr, &raw_start, &raw_end);
        if (parsed < 0) {
            *changed = 1;
            while (p < tag_len && tag[p] != '>' && !isspace((unsigned char)tag[p])) p++;
            continue;
        }
        if (parsed == 0) break;
        if (remove_attr(name, name_len, &attr, policy)) {
            *changed = 1;
            continue;
        }
        if (!out_append_char(out, ' ')) return 0;
        if (!out_append(out, tag + raw_start, raw_end - raw_start)) return 0;
    }
    if (self_closing && !out_append(out, " /", 2)) return 0;
    return out_append_char(out, '>');
}

static size_t find_matching_end(const char *s, size_t len, size_t pos, const char *name, size_t name_len) {
    while (pos < len) {
        const char *lt = memchr(s + pos, '<', len - pos);
        if (!lt) return len;
        size_t tag_start = (size_t)(lt - s);
        int malformed = 0;
        size_t tag_end = find_tag_end(s, len, tag_start, &malformed);
        if (tag_end == SIZE_MAX) return len;
        const char *got_name;
        size_t got_len;
        int is_end;
        if (tag_name(s + tag_start, tag_end - tag_start + 1, &got_name, &got_len, &is_end) && is_end &&
            got_len == name_len) {
            int same = 1;
            for (size_t i = 0; i < name_len; i++) {
                if (!eqi(got_name[i], name[i])) {
                    same = 0;
                    break;
                }
            }
            if (same) return tag_end + 1;
        }
        pos = tag_end + 1;
    }
    return len;
}

char *markup_sanitize(const char *src, scrub_policy policy, int *changed) {
    size_t len = strlen(src);
    outbuf out = {0};
    size_t pos = 0;

    while (pos < len) {
        const char *lt = memchr(src + pos, '<', len - pos);
        if (!lt) {
            if (!out_append(&out, src + pos, len - pos)) return NULL;
            break;
        }
        size_t tag_start = (size_t)(lt - src);
        if (!out_append(&out, src + pos, tag_start - pos)) return NULL;

        if (tag_start + 4 <= len && strncmp(src + tag_start, "<!--", 4) == 0) {
            const char *end = strstr(src + tag_start + 4, "-->");
            if (!end) {
                *changed = 1;
                break;
            }
            size_t n = (size_t)(end - (src + tag_start)) + 3;
            if (!out_append(&out, src + tag_start, n)) return NULL;
            pos = tag_start + n;
            continue;
        }
        if (tag_start + 2 <= len && (src[tag_start + 1] == '!' || src[tag_start + 1] == '?')) {
            int malformed = 0;
            size_t end = find_tag_end(src, len, tag_start, &malformed);
            if (end == SIZE_MAX) {
                *changed = 1;
                break;
            }
            if (!out_append(&out, src + tag_start, end - tag_start + 1)) return NULL;
            pos = end + 1;
            continue;
        }

        int malformed = 0;
        size_t tag_end = find_tag_end(src, len, tag_start, &malformed);
        if (tag_end == SIZE_MAX || malformed) {
            *changed = 1;
            break;
        }
        const char *name;
        size_t name_len;
        int is_end;
        if (tag_name(src + tag_start, tag_end - tag_start + 1, &name, &name_len, &is_end) && !is_end &&
            body_drop_tag(name, name_len)) {
            *changed = 1;
            pos = find_matching_end(src, len, tag_end + 1, name, name_len);
            continue;
        }
        if (!emit_sanitized_tag(&out, src + tag_start, tag_end - tag_start + 1, policy, changed)) return NULL;
        pos = tag_end + 1;
    }

    return out_finish(&out);
}
