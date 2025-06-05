/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>


typedef int (*command)(int argc, char** argv);

struct command_mapping {
    char const* const command_name;
    const command command;
};

#define NB_COMMANDS 5
/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    VIPS_INIT(argv[0]);

    int ret = ERR_NONE;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        /* **********************************************************************
         * TODO WEEK 07: THIS PART SHALL BE EXTENDED.
         * **********************************************************************
         */


        struct command_mapping list_cmd = {"list", do_list_cmd};
        struct command_mapping create_cmd = {"create", do_create_cmd};
        struct command_mapping help_cmd = {"help", help};
        struct command_mapping delete_cmd = {"delete", do_delete_cmd};
        struct command_mapping insert_cmd = {"insert", do_insert_cmd};
        struct command_mapping read_cmd = {"read", do_read_cmd};
        struct command_mapping null_cmd = {"null", NULL};

        struct command_mapping commands[] = {list_cmd, create_cmd, help_cmd, delete_cmd, insert_cmd, read_cmd, null_cmd};

        argc--; argv++; // skips command call name

        char const* const cmd_name = argv[0];

        int found = 0;

        struct command_mapping* iter;
        for(iter = commands; !found && iter->command != NULL; ++iter) {
            if(!strcmp(iter->command_name, cmd_name)) {
                argc--; argv++; // skip command
                ret = iter->command(argc, argv);
                found = 1;
            }
        }

        ret = (found) ? ret : ERR_INVALID_COMMAND;
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }

    vips_shutdown();

    return ret;
}
