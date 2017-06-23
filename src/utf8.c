/* Utf8
 *
 * A library to validate utf8 text.
 *
 * This is a code refactored out of opusinfo.c
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2017 Marek Rusinowski
 * Licensed under the GNU GPL, distributed with this program.
 */

#include <stdio.h>
#include <stdarg.h>
#include "utf8.h"

char utf8_error_message[UTF8_ERROR_MESSAGE_SIZE];

void utf8_error(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(utf8_error_message, UTF8_ERROR_MESSAGE_SIZE, format, ap);
    va_end(ap);
}

int is_valid_utf8(const char* text, size_t length) {
    size_t j, remaining;
    unsigned bytes;
    int broken = 0;
    const unsigned char *val = (const unsigned char *) text;

    for (j = 0; j < length; j += bytes) {
        remaining = length - j;
        if ((val[j] & 0x80) == 0)
            bytes = 1;
        else if ((val[j] & 0x40) == 0x40) {
            if((val[j] & 0x20) == 0)
                bytes = 2;
            else if((val[j] & 0x10) == 0)
                bytes = 3;
            else if((val[j] & 0x08) == 0)
                bytes = 4;
            else if((val[j] & 0x04) == 0)
                bytes = 5;
            else if((val[j] & 0x02) == 0)
                bytes = 6;
            else {
                utf8_error("length marker wrong");
                return 0;
            }
        } else {
            utf8_error("length marker wrong");
            return 0;
        }

        if (bytes > remaining) {
            utf8_error("too few bytes");
            return 0;
        }

        switch (bytes) {
            case 1:
                /* No more checks needed */
                break;
            case 2:
                if((val[j+1] & 0xC0) != 0x80)
                    broken = 1;
                if((val[j] & 0xFE) == 0xC0)
                    broken = 1;
                break;
            case 3:
                if(!((val[j] == 0xE0 && val[j+1] >= 0xA0 && val[j+1] <= 0xBF &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] >= 0xE1 && val[j] <= 0xEC &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] == 0xED && val[j+1] >= 0x80 &&
                         val[j+1] <= 0x9F &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] >= 0xEE && val[j] <= 0xEF &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80)))
                     broken = 1;
                 if(val[j] == 0xE0 && (val[j+1] & 0xE0) == 0x80)
                     broken = 1;
                 break;
            case 4:
                 if(!((val[j] == 0xF0 && val[j+1] >= 0x90 &&
                         val[j+1] <= 0xBF &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80) ||
                     (val[j] >= 0xF1 && val[j] <= 0xF3 &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80) ||
                     (val[j] == 0xF4 && val[j+1] >= 0x80 &&
                         val[j+1] <= 0x8F &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80)))
                     broken = 1;
                 if(val[j] == 0xF0 && (val[j+1] & 0xF0) == 0x80)
                     broken = 1;
                 break;
            /* 5 and 6 aren't actually allowed at this point */
            case 5:
                broken = 1;
                break;
            case 6:
                broken = 1;
                break;
        }

        if (broken) {
            char simple[6 + 1];
            char seq[6 * 3 + 1];
            static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            size_t k, c1 = 0, c2 = 0;
            for (k = j; k < j + bytes; k++) {
                seq[c1++] = hex[((unsigned char) text[k]) >> 4];
                seq[c1++] = hex[((unsigned char) text[k]) & 0xf];
                seq[c1++] = ' ';

                if (text[k] < 0x20 || text[k] > 0x7D) {
                    simple[c2++] = '?';
                } else {
                    simple[c2++] = text[k];
                }
            }
            seq[c1] = 0;
            simple[c2] = 0;
            utf8_error("invalid sequence \"%s\": %s", simple, seq);
            return 0;
        }
    }

    return 1;
}
