#include "markup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    scrub_policy policy = SCRUB_POLICY_WARY;
    if (argc > 1) {
        if (strcmp(argv[1], "calm") == 0) {
            policy = SCRUB_POLICY_CALM;
        } else if (strcmp(argv[1], "wary") == 0) {
            policy = SCRUB_POLICY_WARY;
        } else if (strcmp(argv[1], "paranoid") == 0) {
            policy = SCRUB_POLICY_PARANOID;
        } else {
            return 2;
        }
    }

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return 2;
    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *next = realloc(buf, cap);
            if (!next) {
                free(buf);
                return 2;
            }
            buf = next;
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';

    int changed = 0;
    char *clean = markup_sanitize(buf, policy, &changed);
    free(buf);
    if (!clean) return 2;
    fputs(clean, stdout);
    free(clean);
    return 0;
}
