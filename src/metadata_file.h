#ifndef __METADATA_FILE_H
#define __METADATA_FILE_H

typedef struct {
    char *tag, *val;
} metadata_elem;

metadata_elem* parse_metadata_file(const char *file,
                                   const char **error_message);

void free_metadata_list(metadata_elem* list);

#endif /* __METADATA_FILE_H */
