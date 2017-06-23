#ifndef __UTF8_H
#define __UTF8_H

#include <stddef.h>

#define UTF8_ERROR_MESSAGE_SIZE 128

extern char utf8_error_message[UTF8_ERROR_MESSAGE_SIZE];

int is_valid_utf8(const char* text, size_t length);

#endif /* __UTF8_H */
