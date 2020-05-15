#ifndef FISH_SAFE_STRERROR_H
#define FISH_SAFE_STRERROR_H

void errno_list_init();
void errno_list_free();
const char *safe_strerror(int err);

#endif
