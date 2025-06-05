#include "imgfs.h"
#include "image_content.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if(!(0 <= resolution && resolution <= 2)) return ERR_INVALID_ARGUMENT;

    for(size_t i = 0; i < imgfs_file->header.max_files; ++i) {
        if(!strcmp(imgfs_file->metadata[i].img_id, img_id)) {
            int ret = ERR_NONE;
            if((ret = lazily_resize(resolution, imgfs_file, i)) != ERR_NONE) {
                return ret;
            }

            uint32_t size = imgfs_file->metadata[i].size[resolution];
            uint64_t offset = imgfs_file->metadata[i].offset[resolution];

            char* buffer = NULL;

            if((buffer = calloc(size, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;

            if(fseek(imgfs_file->file, offset, SEEK_SET) == -1
               || fread(buffer, sizeof(char), size, imgfs_file->file) != size) {
                free(buffer);
                return ERR_IO;
            }

            *image_size = size;
            *image_buffer = buffer;

            return ERR_NONE;
        }
    }

    return ERR_IMAGE_NOT_FOUND;
}