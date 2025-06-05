#include "image_content.h"
#include "imgfs.h"

#include <stdlib.h>
#include <stdio.h>
#include <vips/vips.h>

/**
* @brief handles the different possible garbage collecting cases, called only in the event of a specific error occuring.
* @param error_code (int) : the error code to be returned
* @param to_write (void*): a buffer of the image in new resolution
* @param orig_img (void*): a buffer of the image in its original resolution
* @param in (VipsImage*) : the vips image form of the original byte buffer
* @param out (VipsImage*) : the vips image form of the output byte buffer
* @return (int): the specific code of the image
*/
int error_handler_content(int error_code, void* to_write, void* orig_img, VipsImage* in, VipsImage* out)
{
    if(to_write != NULL) {
        free(to_write);
        to_write = NULL;
    }
    if(orig_img != NULL) {
        free(orig_img);
        orig_img = NULL;
    }
    if(in != NULL) {
        g_object_unref(VIPS_OBJECT(in));
        in = NULL;
    }
    if(out != NULL) {
        g_object_unref(VIPS_OBJECT(out));
        out = NULL;
    }

    return error_code;
}

int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if(!(0 <= resolution && resolution <= 2)) return ERR_INVALID_ARGUMENT;
    if(index >= imgfs_file->header.max_files || !imgfs_file->metadata[index].is_valid) return ERR_INVALID_IMGID;
    if(resolution == ORIG_RES) return ERR_NONE;

    struct img_metadata img = imgfs_file->metadata[index];

    if(!img.offset[resolution] || !img.size[resolution]) {
        uint16_t img_width = imgfs_file->header.resized_res[2 * resolution];
        uint16_t img_height = imgfs_file->header.resized_res[2 * resolution + 1];

        uint64_t orig_img_offset = img.offset[ORIG_RES];
        uint32_t orig_img_size = img.size[ORIG_RES];

        VipsImage* in = NULL;
        VipsImage* out = NULL;

        unsigned long int offset = 0;
        size_t to_write_len = 0;

        void* to_write = NULL;
        void* orig_img = NULL;

        if((orig_img = calloc(1, orig_img_size)) == NULL) return ERR_OUT_OF_MEMORY;

        if(fseek(imgfs_file->file, orig_img_offset, SEEK_SET) == -1
           || fread(orig_img, orig_img_size, 1UL, imgfs_file->file) != 1UL)
            return error_handler_content(ERR_IO, to_write, orig_img, in, out);

        if(vips_jpegload_buffer(orig_img, orig_img_size, &in, NULL) == -1
           || vips_thumbnail_image(in, &out, img_width, "height", img_height, NULL) == -1)
            return error_handler_content(ERR_IMGLIB, to_write, orig_img, in, out);

        if(fseek(imgfs_file->file, 0, SEEK_END) == -1)
            return error_handler_content(ERR_IO, to_write, orig_img, in, out);

        offset = ftell(imgfs_file->file);

        if(vips_jpegsave_buffer(out, &to_write, &to_write_len, NULL) == -1)
            return error_handler_content(ERR_IMGLIB, to_write, orig_img, in, out);

        if(fwrite(to_write, to_write_len, 1UL, imgfs_file->file) != 1UL)
            return error_handler_content(ERR_IO, to_write, orig_img, in, out);

        free(to_write);
        free(orig_img);
        to_write = NULL;
        orig_img = NULL;

        img.offset[resolution] = (uint64_t) offset;
        img.size[resolution] = (uint32_t) to_write_len;

        if(fseek(imgfs_file->file, sizeof(struct imgfs_header) + index * sizeof(struct img_metadata), SEEK_SET) == -1
           || fwrite(&img, sizeof(struct img_metadata), 1UL, imgfs_file->file) != 1UL)
            return error_handler_content(ERR_IO, to_write, orig_img, in, out);

        imgfs_file->metadata[index] = img;

        // free the pointers
        g_object_unref(VIPS_OBJECT(in));
        g_object_unref(VIPS_OBJECT(out));
    }

    return ERR_NONE;
}

int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}