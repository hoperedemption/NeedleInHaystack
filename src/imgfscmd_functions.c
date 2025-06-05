/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

int check_if_string_is_correct(const char* str, size_t max_length);
static void create_name(const char* img_id, int resolution, char** new_name);
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size);
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size);

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE.
     * **********************************************************************
     */

    const char * output =
    "imgfscmd [COMMAND] [ARGUMENTS]\n"
    "  help: displays this help.\n"
    "  list <imgFS_filename>: list imgFS content.\n"
    "  create <imgFS_filename> [options]: create a new imgFS.\n"
    "      options are:\n"
    "          -max_files <MAX_FILES>: maximum number of files.\n"
    "                                  default value is 128\n"
    "                                  maximum value is 4294967295\n"
    "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
    "                                  default value is 64x64\n"
    "                                  maximum value is 128x128\n"
    "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
    "                                  default value is 256x256\n"
    "                                  maximum value is 512x512\n"
    "  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
    "      read an image from the imgFS and save it to a file.\n"
    "      default resolution is \"original\".\n"
    "  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n"
    "  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n";

    printf("%s", output);
    return ERR_NONE;
}


/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 07: WRITE YOUR CODE HERE.
     * **********************************************************************
     */
    M_REQUIRE_NON_NULL(argv);

    if(argc < 1) return ERR_NOT_ENOUGH_ARGUMENTS;
    else if(argc > 1) return ERR_INVALID_COMMAND;

    M_REQUIRE_NON_NULL(argv[0]);

    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);

    char** json = NULL;
    int code_err = ERR_NONE;
    if((code_err = do_open(argv[0], "rb", &imgfs_file)) == ERR_NONE) {
        code_err = do_list(&imgfs_file, STDOUT, json);
    }

    do_close(&imgfs_file);

    return code_err;
}

/**********************************************************************
 * @brief checks whether the resolution passed is within the correct range
 * @param width (uint16_t): the width to be checked for validity
 * @param int16_t): the height to be checked for validity
 * @param resolution (int): the resolution either SMALL_RES or THUMB_RES
 * @return (int): 1 if the passed parameters are valid and 0 otherwise
********************************************************************** */
int check_resolution(uint16_t width, uint16_t height, int resolution)
{
    uint16_t max_resolution = (resolution == SMALL_RES) ? MAX_SMALL_RES : MAX_THUMB_RES;
    return width && height && width <= max_resolution && height <= max_resolution;
}

/**********************************************************************
* @brief converts the width and height parameters to their uint16_t equivalent, passed by reference.
* @param res_width_string (const char*) : the width parameter of resolution in the form of a string
* @param res_width (uint16_t*) : reference to the width in which we save th
* @param res_height_string (const char*) : the height parameter of resolution in the form of a string
* @param res_height (uint16_t*) : reference to the height in which we save the result
********************************************************************** */
int convert_strings_to_int(const char* res_width_string, const char* res_height_string, uint16_t* res_width, uint16_t* res_height)
{
    M_REQUIRE_NON_NULL(res_width_string);
    *res_width = atouint16(res_width_string);
    M_REQUIRE_NON_NULL(res_height_string);
    *res_height = atouint16(res_height_string);

    return ERR_NONE;
}

/******************************************************************************************************************************************
* @brief In this function, we pass a reference to the argv pointer, which allows us to iterate through the different parameters.
* We are thus us able to change back argv so as to synchronise it with do_create_cmd function (which should not be possible without
* passing it by reference). To modify the uint16_t parameters, we also pass them by reference (to modify them and later copy them to the right imgfs_header).
* Then, we dereference argv to iterate on the passed parameters (width and height) and store the result in "iter", which works
* as the "argv" in the do_create_cmd function. We pass the width and height pointers to convert them from string to uint16_t, thanks to
* the convert_strings_to_int function. Finally, we synchronise argv with the one in the do_create_cmd fucntion, at line 136.
* @param argv (char***) : reference to the array of parameters entered in the terminal
* @param resolution (int) : the resoltuion to consider (either SMALL_RES or THUMB_RES)
* @return int : returns ERR_RESOLUTIONS if the parameters (width or height) passed are invalid. Returns ERR_NONE if both parameters
* are valid.
******************************************************************************************************************************************** */

int convert_and_check(char*** argv, int resolution, uint16_t* res_width, uint16_t* res_height)
{
    int ret = ERR_NONE;

    char** iter = *argv;
    if((ret = convert_strings_to_int(*(iter++), *(iter++), res_width, res_height)) != ERR_NONE) return ret;
    *argv = iter;

    if(!check_resolution(*res_width, *res_height, resolution))
        return ERR_RESOLUTIONS;

    return ERR_NONE;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{

    puts("Create");
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */
    M_REQUIRE_NON_NULL(argv);

    if(argc < 1) return ERR_NOT_ENOUGH_ARGUMENTS;

    argc--;
    char* imgfs_filename = *(argv++);
    M_REQUIRE_NON_NULL(imgfs_filename);

    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);
    uint32_t max_files = default_max_files;
    uint16_t thumb_res_height = default_thumb_res;
    uint16_t thumb_res_width = default_thumb_res;
    uint16_t small_res_height = default_small_res;
    uint16_t small_res_width = default_small_res;

    struct argument {
        char const* const argument_name;
        unsigned int nb_parameters;
    };
    struct argument optional_arguments[3] = {{"-max_files", 1}, {"-thumb_res", 2}, {"-small_res", 2}};

    while(argc--) {
        char* argument = *(argv++);
        M_REQUIRE_NON_NULL(argument);

        int found = 0;
        for(int i = 0; !found && i < 3; ++i) {
            if(!strcmp(optional_arguments[i].argument_name, argument)) {
                unsigned int nb_parameters = optional_arguments[i].nb_parameters;

                if(argc < nb_parameters) return ERR_NOT_ENOUGH_ARGUMENTS;

                if(nb_parameters == 1) {
                    char* max_files_string = *(argv++);
                    M_REQUIRE_NON_NULL(max_files_string);
                    max_files = atouint32(max_files_string);
                    if(!max_files) return ERR_MAX_FILES;
                } else {
                    if(i == 1) {
                        int ret = ERR_NONE;
                        if((ret = convert_and_check(&argv, THUMB_RES, &thumb_res_width, &thumb_res_height)) != ERR_NONE) return ret;
                        /**
                         *
                        */
                    } else {
                        int ret = ERR_NONE;
                        if((ret = convert_and_check(&argv, SMALL_RES, &small_res_width, &small_res_height)) != ERR_NONE) return ret;
                    }
                }

                argc -= nb_parameters;
                found = 1;
            }
        }

        if(!found) return ERR_INVALID_ARGUMENT;
    }

    imgfs_file.header.max_files = max_files;
    uint16_t resized_res[2 * (NB_RES - 1)] = {thumb_res_width, thumb_res_height, small_res_width, small_res_height};
    memcpy(imgfs_file.header.resized_res, resized_res, 2 * (NB_RES - 1) * sizeof(uint16_t));

    int ret = do_create(imgfs_filename, &imgfs_file);
    do_close(&imgfs_file);
    return ret;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    M_REQUIRE_NON_NULL(argv);

    if (argc > 2) return ERR_INVALID_COMMAND;
    else if (argc < 2 ) return ERR_NOT_ENOUGH_ARGUMENTS;

    M_REQUIRE_NON_NULL(argv[0]);
    M_REQUIRE_NON_NULL(argv[1]);

    char* filename = *(argv++);
    if(!check_if_string_is_valid(filename, FILENAME_MAX)) return ERR_INVALID_FILENAME;

    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);

    int ret = ERR_NONE;
    if((ret = do_open(filename, "rb+", &imgfs_file)) != ERR_NONE) return ret;

    char* img_id = *(argv);

    if(!check_if_string_is_valid(img_id, MAX_IMG_ID)) {
        do_close(&imgfs_file);
        return ERR_INVALID_IMGID;
    }

    ret = do_delete(img_id, &imgfs_file);
    do_close(&imgfs_file);
    return ret;
}


int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}

int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

char const* const convert_resolution_to_string(int resolution)
{
    switch(resolution) {
    case THUMB_RES:
        return "_thumb";
    case SMALL_RES:
        return "_small";
    case ORIG_RES:
        return "_orig";
    default:
        return "";
    }
}

static void create_name(const char* img_id, int resolution, char** new_name)
{
    if(img_id != NULL && new_name != NULL && (0 <= resolution && resolution <= 2)) {
        char const* const format = ".jpg";
        char const* const resolution_string = convert_resolution_to_string(resolution);

        int total_length = strlen(img_id) + strlen(resolution_string) + strlen(format);

        char* concatenated_name = calloc(total_length + 1, sizeof(char));
        snprintf(concatenated_name,  total_length + 1, "%s%s%s", img_id, resolution_string, format);

        *new_name = concatenated_name;
    }
}



int return_err(FILE* file, int err)
{
    if(file) fclose(file);
    return err;
}

static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    FILE* file = NULL;
    char* buffer = NULL;

    if((file = fopen(path, "rb")) == NULL || fseek(file, 0, SEEK_END) == -1) return return_err(file, ERR_IO);

    long size_to_read = ftell(file);

    if((buffer = calloc(1UL * size_to_read, sizeof(char))) == NULL) return return_err(file, ERR_OUT_OF_MEMORY);

    if(fseek(file, 0, SEEK_SET) == -1 || fread(buffer, sizeof(char), size_to_read, file) != size_to_read) return return_err(file, ERR_IO);

    *image_buffer = buffer;
    *image_size = (uint32_t) size_to_read;

    return ERR_NONE;
}
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(image_buffer);

    FILE* file = NULL;

    int ret = ERR_NONE;

    if((file = fopen(filename, "wb")) == NULL
       || fwrite(image_buffer, sizeof(char), image_size, file) != image_size) ret =  ERR_IO;

    fclose(file);
    return ret;
}