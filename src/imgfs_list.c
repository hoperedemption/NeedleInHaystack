#include "imgfs.h"
#include  "util.h"

#include <stdio.h>
#include <json-c/json.h>
#include <string.h>

typedef struct {
    const struct img_metadata* metadata;
    struct json_object* json_array;
} arguments_for_helper;

typedef void (*func)(arguments_for_helper* arguments);

typedef struct {
    func helper_function;
    struct imgfs_file* imgfs_file;
    struct json_object* json_array;
} arguments;

void do_list_helper_print(arguments_for_helper* arg)
{
    const struct img_metadata* metadata = arg->metadata;
    if(metadata != NULL && metadata->is_valid)
        print_metadata(metadata);
}

/**
*/
void do_list_helper_json_add(arguments_for_helper* arg)
{
    const struct img_metadata* metadata = arg->metadata;
    if(metadata != NULL && metadata->is_valid) {
        struct json_object* json_string_imgid = json_object_new_string(metadata->img_id);
        struct json_object* json_array = arg->json_array;
        json_object_array_add(json_array, json_string_imgid);
    }
}
/**
*/
void do_list_aux(arguments* general_parameters)
{
    func function = general_parameters->helper_function;
    struct imgfs_file* imgfs_file = general_parameters->imgfs_file;
    struct json_object* json_array = general_parameters->json_array;

    arguments_for_helper parameters_bis = {NULL, json_array};

    for(uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
        parameters_bis.metadata= &imgfs_file->metadata[i];
        function(&parameters_bis);
    }
}

int put_json_and_return(struct json_object* json_object, int return_code)
{
    json_object_put(json_object);
    return return_code;
}

/**
 * @brief This function prints the header of the database and the metadata of all the stored images. If one of the parameters is null,
 * then an error occurs. Also, if the database is empty is signals it by a message.
 * @param imgfs_file (const struct imgfs_file*) : the database containg the images
 * @param output_mode (enum do_list_mode) : the mode of display of the format
 * @return the corresponding error code: ERR_NONE if all went fine or if something failed a specific code defined in error.h
*/
int do_list(const struct imgfs_file* imgfs_file, enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(imgfs_file);

    const struct img_metadata* metadata = imgfs_file->metadata;

    switch(output_mode) {
    case STDOUT:
        print_header(&(imgfs_file->header));

        if(metadata == NULL || imgfs_file->header.nb_files == 0) {
            printf("<< empty imgFS >>\n");
        } else {
            arguments params = {do_list_helper_print, imgfs_file, NULL};
            do_list_aux(&params);
        }
        break;
    case JSON:
        M_REQUIRE_NON_NULL(json);

        struct json_object* json_object = json_object_new_object();
        struct json_object* json_array = json_object_new_array();

        if(metadata != NULL && imgfs_file->header.nb_files != 0) {
            arguments params = {do_list_helper_json_add, imgfs_file, json_array};
            do_list_aux(&params);
        }

        const char* images_string = "Images";

        if(json_object_object_add(json_object, images_string, json_array) < 0)
            put_json_and_return(json_object, ERR_RUNTIME);

        const char* string_in_json_format = strdup(json_object_to_json_string(json_object));
        *json = string_in_json_format;

        put_json_and_return(json_object, ERR_NONE);

        break;
    case NB_DO_LIST_MODES:
        break;
    }

    return ERR_NONE;
}
