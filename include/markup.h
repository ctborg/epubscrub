#ifndef EPUBSCRUB_MARKUP_H
#define EPUBSCRUB_MARKUP_H

#include "sanitize.h"

char *markup_sanitize(const char *src, scrub_policy policy, int *changed);

#endif
