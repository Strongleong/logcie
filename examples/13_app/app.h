#ifndef APP_H
#define APP_H

#include <stddef.h>

int  storage_open(const char *path);
void storage_write(const char *key, size_t bytes);
void storage_close(void);

void api_serve(int requests);

#endif
