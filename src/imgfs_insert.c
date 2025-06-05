#include "imgfs.h"
#include "util.h"
#include "image_dedup.h"
#include "image_content.h"

#include <stdio.h>
#include <string.h>


int garbage_collect_and_error(struct img_metadata* backup, struct img_metadata* original, int error_code)
{
    *original = *backup;
    return error_code;
}

int do_insert(const char* image_buffer, size_t image_size, const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if(imgfs_file->header.nb_files >= imgfs_file->header.max_files) return ERR_IMGFS_FULL;

    for(int i = 0; i < imgfs_file->header.max_files; ++i) {
        if(imgfs_file->metadata[i].is_valid == EMPTY) {
            // store a backup to change the metadata in case of error
            struct img_metadata backup = imgfs_file->metadata[i];
            struct imgfs_header header = imgfs_file->header;

            zero_init_var(imgfs_file->metadata[i]);

            unsigned char image_hash_value[SHA256_DIGEST_LENGTH] = {0};
            SHA256((const unsigned char *)image_buffer, image_size, image_hash_value);
            memcpy(imgfs_file->metadata[i].SHA, image_hash_value, SHA256_DIGEST_LENGTH);

            // could check img_id in some way tbh, would define file with macro functions
            strcpy(imgfs_file->metadata[i].img_id, img_id);

            imgfs_file->metadata[i].size[ORIG_RES] = (uint32_t) image_size;

            uint32_t height = 0;
            uint32_t width = 0;
            int ret = ERR_NONE;
            if((ret = get_resolution(&height, &width, image_buffer, image_size)) != ERR_NONE)
                return garbage_collect_and_error(&backup, &imgfs_file->metadata[i], ret);

            imgfs_file->metadata[i].orig_res[0] = width;
            imgfs_file->metadata[i].orig_res[1] = height;

            imgfs_file->metadata[i].is_valid = NON_EMPTY;

            ret = ERR_NONE;
            if((ret = do_name_and_content_dedup(imgfs_file, i)) != ERR_NONE)
                return garbage_collect_and_error(&backup, &imgfs_file->metadata[i], ret);

            if(!imgfs_file->metadata[i].offset[ORIG_RES]) {
                if(fseek(imgfs_file->file, 0, SEEK_END) == -1) return garbage_collect_and_error(&backup, &imgfs_file->metadata[i], ERR_IO);

                unsigned long int offset = ftell(imgfs_file->file);
                imgfs_file->metadata[i].offset[ORIG_RES] = (uint64_t) offset;

                if(fwrite(image_buffer, sizeof(char), image_size, imgfs_file->file) != image_size) return garbage_collect_and_error(&backup, &imgfs_file->metadata[i], ERR_IO);
            }

            header.nb_files++;
            header.version++;

            if(fseek(imgfs_file->file, sizeof(struct imgfs_header) + i * sizeof(struct img_metadata), SEEK_SET) == -1
               || fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1UL, imgfs_file->file) != 1UL
               || fseek(imgfs_file->file, 0, SEEK_SET) == -1
               || fwrite(&header, sizeof(struct imgfs_header), 1UL, imgfs_file->file) != 1UL)
                return garbage_collect_and_error(&backup, &imgfs_file->metadata[i], ERR_IO);

            imgfs_file->header = header;

            return ERR_NONE;
        }
    }

    return ERR_IMGFS_FULL;
}