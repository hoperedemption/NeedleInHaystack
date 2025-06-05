/* ** NOTE: undocumented in Doxygen
 * @file imgfs_tools.c
 * @brief implementation of several tool functions for imgFS
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "util.h"

#include <inttypes.h>      // for PRIxN macros
#include <openssl/sha.h>   // for SHA256_DIGEST_LENGTH
#include <stdint.h>        // for uint8_t
#include <stdio.h>         // for sprintf
#include <stdlib.h>        // for calloc
#include <string.h>        // for strcmp

#define RNULL 1
/*******************************************************************
 * Human-readable SHA
 */
static void sha_to_string(const unsigned char* SHA,
                          char* sha_string)
{
    if (SHA == NULL) return;

    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(sha_string + (2 * i), "%02x", SHA[i]);
    }

    sha_string[2 * SHA256_DIGEST_LENGTH] = '\0';
}

/*******************************************************************
 * imgFS header display.
 */
void print_header(const struct imgfs_header* header)
{
    printf("*****************************************\n\
********** IMGFS HEADER START ***********\n");
    printf("TYPE: " STR_LENGTH_FMT(MAX_IMGFS_NAME) "\
\nVERSION: %" PRIu32 "\n\
IMAGE COUNT: %" PRIu32 "\t\tMAX IMAGES: %" PRIu32 "\n\
THUMBNAIL: %" PRIu16 " x %" PRIu16 "\tSMALL: %" PRIu16 " x %" PRIu16 "\n",
           header->name, header->version, header->nb_files, header->max_files, header->resized_res[THUMB_RES * 2],
           header->resized_res[THUMB_RES * 2 + 1], header->resized_res[SMALL_RES * 2],
           header->resized_res[SMALL_RES * 2 + 1]);
    printf("*********** IMGFS HEADER END ************\n\
*****************************************\n");
}

/*******************************************************************
 * Metadata display.
 */
void print_metadata (const struct img_metadata* metadata)
{
    char sha_printable[2 * SHA256_DIGEST_LENGTH + 1];
    sha_to_string(metadata->SHA, sha_printable);

    printf("IMAGE ID: %s\nSHA: %s\nVALID: %" PRIu16 "\nUNUSED: %" PRIu16 "\n\
OFFSET ORIG. : %" PRIu64 "\t\tSIZE ORIG. : %" PRIu32 "\n\
OFFSET THUMB.: %" PRIu64 "\t\tSIZE THUMB.: %" PRIu32 "\n\
OFFSET SMALL : %" PRIu64 "\t\tSIZE SMALL : %" PRIu32 "\n\
ORIGINAL: %" PRIu32 " x %" PRIu32 "\n",
           metadata->img_id, sha_printable, metadata->is_valid, metadata->unused_16, metadata->offset[ORIG_RES],
           metadata->size[ORIG_RES], metadata->offset[THUMB_RES], metadata->size[THUMB_RES],
           metadata->offset[SMALL_RES], metadata->size[SMALL_RES], metadata->orig_res[0], metadata->orig_res[1]);
    printf("*****************************************\n");
}

/**
 * @brief This function handles specific errors encountered in the do_open function
 * @param file the file pointer to be closed if non null
 * @param error_code the error_code to be returned
 * @return (int) : The different possible error codes encountered in the do_open function
*/

int error_handler(FILE* file, int error_code)
{
    if(file != NULL) fclose(file);
    return error_code;
}

/**
 * @brief This function opens a file in a certain mode and copies all of its content in a given structure, that it its header, and the metadata of the stored images.
 * If one its parameter is initially NULL, the function returns an error.
 * @param imgfs_filename (const char*) : The name of the file to be opened
 * @param open_mode (const char*) : The opening mode, STDOUT or JSON
 * @param imgfs_file (struct imgfs_file*) : A pointer to the structure to be filled by the data contained in the file
 * @return (int) : There are four possible return values. If the file does not exists, or an error occurs during the opening of the file or during the reading of the
 * header or the metadata, an IO_ERROR is sent. If the database has a maximum number of files of 0, then an ERR_MAX_FILES is returned. If an error occurs during
 * the allocation of the pointer to store the metadata of the images, an ERR_OUT_OF_MEMORY is sent. Otherwise, if everything goes smoothly, an ERR_NONE is returned, meaning
 * that any error has not been detected.
*/
int do_open(const char* imgfs_filename, const char* open_mode, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(open_mode);
    M_REQUIRE_NON_NULL(imgfs_file);

    FILE* open_file = fopen(imgfs_filename, open_mode);
    if(open_file == NULL) return ERR_IO;

    struct imgfs_header header_res;
    zero_init_var(header_res);
    struct img_metadata* metadata_arr_res = NULL;

    if(fread(&header_res, sizeof(struct imgfs_header), 1UL, open_file) == 1UL) {
        if(header_res.max_files != 0 && header_res.nb_files <= header_res.max_files) {
            if((metadata_arr_res = calloc(header_res.max_files, sizeof(struct img_metadata))) == NULL)
                return error_handler(open_file, ERR_OUT_OF_MEMORY);

            if(fread(metadata_arr_res, sizeof(struct img_metadata), 1UL * header_res.max_files, open_file) != 1UL * header_res.max_files) {
                free(metadata_arr_res);
                return error_handler(open_file, ERR_IO);
            }
        } else return error_handler(open_file, ERR_MAX_FILES);
    } else return error_handler(open_file, ERR_IO);

    imgfs_file->file = open_file;
    imgfs_file->header = header_res;
    imgfs_file->metadata = metadata_arr_res;

    return ERR_NONE;
}

/**
 * @brief This function close the database and free all the pointers associated to it (the file containing the infroamtion and the images metadata).
 * If the given pointer is NULL, this function does nothing.
 * @param imgfs_ptr (struct imgfs_file*) : a pointer to the structure containing the information of the database.
*/
void do_close(struct imgfs_file* imgfs_ptr)
{
    if(imgfs_ptr != NULL) {
        FILE* file = imgfs_ptr->file;

        if(file != NULL) fclose(file);
        free(imgfs_ptr->metadata);

        imgfs_ptr->metadata = NULL;
        imgfs_ptr->file = NULL;
    }
}

int resolution_atoi (const char* str)
{
    if (str == NULL) return -1;

    if (!strcmp(str, "thumb") || !strcmp(str, "thumbnail")) {
        return THUMB_RES;
    } else if (!strcmp(str, "small")) {
        return SMALL_RES;
    } else if (!strcmp(str, "orig")  || !strcmp(str, "original")) {
        return ORIG_RES;
    }
    return -1;
}