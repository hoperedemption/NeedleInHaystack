#include "imgfs.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_filename);

    FILE* database = NULL;

    struct imgfs_header new_header;
    zero_init_var(new_header);
    strcpy(new_header.name, CAT_TXT);
    new_header.max_files = imgfs_file->header.max_files;
    memcpy(new_header.resized_res, imgfs_file->header.resized_res, (2 * (NB_RES - 1)) * sizeof(uint16_t)); // added uint16_t .

    size_t max_files = 1UL * new_header.max_files;

    struct img_metadata* new_metadata = calloc(max_files, sizeof(struct img_metadata));
    if(new_metadata == NULL) return ERR_OUT_OF_MEMORY;

    if((database = fopen(imgfs_filename, "wb")) == NULL
       || fwrite(&new_header, sizeof(struct imgfs_header), 1UL, database) != 1UL
       || fwrite(new_metadata, sizeof(struct img_metadata), max_files, database) != max_files) {
        free(new_metadata);
        if(database) fclose(database);
        return ERR_IO;
    }

    imgfs_file->file = database;
    imgfs_file->header = new_header;
    imgfs_file->metadata = new_metadata;

    printf("%u items were written.\n", 1 + new_header.max_files);
    return ERR_NONE;
}
