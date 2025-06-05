#include "imgfs.h"
#include <string.h>
#include <stdio.h>

int do_delete(const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    int found = 0;
    for(int i = 0; !found && i < imgfs_file->header.max_files; ++i) {
        if(!strcmp(imgfs_file->metadata[i].img_id, img_id)) {
            if(imgfs_file->metadata[i].is_valid) {
                struct img_metadata img = imgfs_file->metadata[i];
                struct imgfs_header header = imgfs_file->header;

                img.is_valid = EMPTY;

                if(fseek(imgfs_file->file, 1L * sizeof(struct imgfs_header) + i * sizeof(struct img_metadata), SEEK_SET) == -1
                   || fwrite(&img, sizeof(struct img_metadata), 1UL, imgfs_file->file) != 1UL)
                    return ERR_IO;

                header.version++;
                header.nb_files--;

                if(fseek(imgfs_file->file, 0L, SEEK_SET) == -1
                   || fwrite(&header, sizeof(struct imgfs_header), 1UL, imgfs_file->file) != 1UL)
                    return ERR_IO;

                imgfs_file->metadata[i] = img;
                imgfs_file->header = header;
            } else return ERR_IMAGE_NOT_FOUND;
            found = 1;
        }
    }

    return found ? ERR_NONE : ERR_IMAGE_NOT_FOUND;
}
