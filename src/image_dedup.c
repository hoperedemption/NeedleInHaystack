#include "imgfs.h"
#include "image_dedup.h"

#include <string.h>

int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if(index >= imgfs_file->header.max_files || !imgfs_file->metadata[index].is_valid) return ERR_IMAGE_NOT_FOUND;

    struct img_metadata image_to_find = imgfs_file->metadata[index];
    int has_duplicate_content = 0;

    for(size_t i = 0; i < imgfs_file->header.max_files; ++i) {
        struct img_metadata current_image = imgfs_file->metadata[i];
        if(i != index && current_image.is_valid) {
            if(!strncmp(current_image.img_id, image_to_find.img_id, MAX_IMG_ID + 1)) {
                return ERR_DUPLICATE_ID;
            } else if(!strncmp(current_image.SHA, image_to_find.SHA, SHA256_DIGEST_LENGTH)) {
                memcpy(image_to_find.offset, current_image.offset, 3 * sizeof(uint64_t));
                memcpy(image_to_find.size, current_image.size, 3 * sizeof(uint32_t));

                ++has_duplicate_content;
            }
        }
    }

    if (!has_duplicate_content) image_to_find.offset[ORIG_RES] = 0;

    imgfs_file->metadata[index] = image_to_find;

    return ERR_NONE;
}