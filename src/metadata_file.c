/* Copyright (C)2017 Marek Rusinowski
   File: metadata_file.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metadata_file.h"
#include "utf8.h"

static inline int push_to_metadata_list(metadata_elem** list,
                                        int *len,
                                        int *cap,
                                        char *tag,
                                        char *val,
                                        const char **error_message) {
   int new_cap;
   if (*len == *cap) {
      if (*cap == 0) {
         new_cap = 10;
      } else {
         new_cap = *cap * 2;
      }
      *list = (metadata_elem *) realloc(*list, new_cap * sizeof(metadata_elem));
      if (*list == NULL) {
         *error_message = "insufficient memory";
         return 0;
      }
      *cap = new_cap;
   }
   (*list)[*len].tag = tag;
   (*list)[*len].val = val;
   ++(*len);
   return 1;
}

static inline int append_to_buf(char **buf,
                                size_t *len,
                                size_t *cap,
                                const char *src,
                                size_t n,
                                const char **error_message) {
   size_t new_cap = *cap == 0 ? n + 1 : *cap;
   while (*len + n >= new_cap) {
      new_cap *= 2;
   }
   if (new_cap != *cap) {
      *buf = (char *) realloc(*buf, new_cap);
      if (*buf == NULL) {
         *error_message = "insufficient memory";
         return 0;
      }
      *cap = new_cap;
   }
   memcpy(*buf + *len, src, n);
   *len += n;
   (*buf)[*len] = '\0';
   return 1;
}

static char* load_metadata_file(const char *file,
                                size_t *buf_size,
                                const char **error_message) {
   FILE *metadata_file = NULL;
   long metadata_file_size;
   char *buf;

   metadata_file = fopen(file, "rb");
   if (metadata_file == NULL) {
      *error_message = "error opening metadata file";
      return NULL;
   }
   if (fseek(metadata_file, 0, SEEK_END) == -1) {
      *error_message = "error moving to the end of metadata file";
      fclose(metadata_file);
      return NULL;
   }
   metadata_file_size = ftell(metadata_file);
   if (metadata_file_size == -1) {
      *error_message = "error getting metadata file size";
      fclose(metadata_file);
      return NULL;
   }
   rewind(metadata_file);
   buf = (char *) malloc(metadata_file_size);
   if (buf == NULL) {
      *error_message = "insufficient memory";
      fclose(metadata_file);
      return NULL;
   }
   if (fread(buf, 1, metadata_file_size, metadata_file)
         != (size_t) metadata_file_size) {
      *error_message = "failed to load metadata file to memory";
      fclose(metadata_file);
      free(buf);
      return NULL;
   }
   fclose(metadata_file);
   *buf_size = (size_t) metadata_file_size;
   return buf;
}

typedef enum {
   TAG,
   VALUE,
   MULTILINE_VALUE,
   MULTILINE_INDENT
} parser_state;

/* The TAG character set as specified in Vorbis comment field and header
   specification: https://xiph.org/vorbis/doc/v-comment.html */
static inline int is_tag_character(char c) {
   return c >= 0x20 && c <= 0x7d && c != '=';
}

metadata_elem* parse_metadata_file(const char *file,
                                   const char **error_message) {
   static char errstr[256];
   size_t buf_size, val_cap, val_len;
   char *buf, *pos, *mark, *tag = NULL, *val = NULL, c;
   metadata_elem* list = NULL;
   int cap = 0, len = 0;
   parser_state state;

   buf = load_metadata_file(file, &buf_size, error_message);
   if (buf == NULL) {
      return NULL;
   }

   state = TAG;
   pos = buf;
   /* Skip UTF-8 BOM if present. */
   if (buf_size >= 3 && memcmp(pos, "\xef\xbb\xbf", 3) == 0) {
      pos += 3;
   }
   if (!is_valid_utf8(pos, (buf + buf_size) - pos)) {
      snprintf(errstr, 256, "invalid utf-8: %s", utf8_error_message);
      *error_message = errstr;
      goto error;
   }
   mark = pos;
   for (; pos < buf + buf_size || state != TAG; ++pos) {
      c = pos < buf + buf_size ? *pos : '\n';
      if (c == '\0') {
         *error_message = "metadata file mustn't contain null bytes";
         goto error;
      }
      switch (state) {
      case TAG:
         /* Ignore empty lines when waiting for a tag. */
         if (pos == mark && c == '\n') {
            mark = pos + 1;
            break;
         }

         if (c != '=') {
            if (!is_tag_character(c)) {
               *error_message = "illegal character used in tag";
               goto error;
            }
            break;
         }

         if (pos == mark) {
            *error_message = "empty tags are not permitted";
            goto error;
         }
         tag = strndup(mark, pos - mark);
         if (tag == NULL) {
            *error_message = "insufficient memory";
            goto error;
         }
         state = VALUE;
         mark = pos + 1;
         break;

      case VALUE:
         if (c != '\n') break;

         if (pos == mark) {
            state = MULTILINE_INDENT;
            val_len = 0;
            val_cap = 0;
            if (!append_to_buf(&val, &val_len, &val_cap, "", 0, error_message)) {
               goto error;
            }
            break;
         }
         val = strndup(mark, pos - mark);
         if (val == NULL) {
            *error_message = "insufficient memory";
            goto error;
         }
         if (!push_to_metadata_list(&list, &len, &cap, tag, val, error_message)) {
            goto error;
         }
         tag = val = NULL;
         state = TAG;
         mark = pos + 1;
         break;

      case MULTILINE_VALUE:
         if (c != '\n') break;

         if (!append_to_buf(&val, &val_len, &val_cap, mark, pos - mark, error_message)) {
            goto error;
         }
         state = MULTILINE_INDENT;
         break;

      case MULTILINE_INDENT:
         if (c == '\t') {
            if (val_len > 0 && !append_to_buf(&val, &val_len, &val_cap, "\n", 1, error_message)) {
               goto error;
            }
            state = MULTILINE_VALUE;
            mark = pos + 1;
            break;
         }

         /* End of multiline value. */
         if (!push_to_metadata_list(&list, &len, &cap, tag, val, error_message)) {
            goto error;
         }
         tag = val = NULL;
         state = TAG;
         mark = pos;
         --pos;  /* epsilon transition */
         break;
      }
   }

   if (!push_to_metadata_list(&list, &len, &cap, NULL, NULL, error_message)) {
      goto error;
   }

   free(buf);
   return list;

error:
   free(buf);
   if (tag != NULL) {
      free(tag);
   }
   if (val != NULL) {
      free(val);
   }
   if (list != NULL) {
      free_metadata_list(list);
   }
   return NULL;
}

void free_metadata_list(metadata_elem* list) {
   int i;
   for (i = 0; list[i].tag != NULL; ++i) {
      free(list[i].tag);
      free(list[i].val);
   }
   free(list);
}
