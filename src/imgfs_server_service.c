/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <vips/vips.h>
#include <pthread.h>

#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port;

#define URI_ROOT "/imgfs"

pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup (int argc, char **argv)
{
    if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS;
    M_REQUIRE_NON_NULL(argv);
    M_REQUIRE_NON_NULL(argv[0]);
    M_REQUIRE_NON_NULL(argv[1]);

    VIPS_INIT(argv[0]);

    ++argv; --argc;

    if(pthread_mutex_init(&global_lock, NULL) != 0) return ERR_RUNTIME;

    server_port = DEFAULT_LISTENING_PORT;

    int ret = ERR_NONE;
    if((ret = do_open(*(argv++), "rb+", &fs_file)) != ERR_NONE) return ret;
    --argc;

    print_header(&fs_file.header);

    while(argc--) {
        char* i = *(argv++);
        if(i == NULL) {
            return ERR_INVALID_COMMAND;
        } else {
            server_port = atouint16(i);
            if(!server_port) return ERR_INVALID_ARGUMENT;
        }
    }

    if((ret = http_init(server_port, handle_http_message)) <= 0) return ERR_RUNTIME;
    printf("ImgFS server started on http://localhost: %u", server_port);

    return ERR_NONE;
}

/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    http_close();
    do_close(&fs_file);
    pthread_mutex_destroy(&global_lock);
    vips_shutdown();
}

/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

/**********************************************************************
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 connection,
                 (int) msg->uri.len, msg->uri.val);


    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    } else if(http_match_uri(msg, URI_ROOT "/list")) {
        return handle_list_call(connection);
    } else if(http_match_uri(msg, URI_ROOT "/insert") && http_match_verb(&msg->method, "POST")) {
        return handle_insert_call(msg, connection);
    } else if(http_match_uri(msg, URI_ROOT "/read")) {
        return handle_read_call(&(msg->uri), connection);
    } else if(http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call(msg, connection);
    } else
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
}

/**
 *
* @brief A global garbage collector helper function for all other functions in this file
* that allows us to simultaneously garabge collect, i.e free the buffer
* and fclose the files, and return the correct error code.
 * @param connection (int) : the file descriptor of the socket
 * @param buffer (char*) : a pointer to the string containing a aprameter comming from the client uri.
 * @param headers(char*) : a pointer to the string containing the headers to be sent to the client
 * @param error_code (int) : the error code to be sent
 * @param file (file*) : a file to be closed
 * @return (int) returns the error code after having freed evry pointer and having sent an error message to the client
*/
int return_and_garbage_collect_call(int connection, char* buffer, char* headers, int error_code, FILE* file)
{
    if(buffer != NULL) free(buffer);
    if(headers != NULL) free(headers);
    if(file != NULL) fclose(file);
    int ret = reply_error_msg(connection, error_code);
    if(ret != ERR_NONE) return ret;
    else return error_code;
}


/**
 *
* @brief A nice way of returning a pair of values of different types was defining a struct.
* This struct is here just as a return value for our return correct header function below.
* It returns a specific error code as weel as the string the value of the header
* @param (int) error_code, the error code internal to the return correct header function
* @param (char*) string_of_return, the string that the function returns
*/
struct return_value_of_return_correct_header {
    int error_code;
    char* string_of_return;
};


/**
 *
* @brief A helper function to return the correct headers needed by callers that send specific requests.
* These callers might need to insert specific parameters to their headers. As a side note, we never use
* snprintf or strlen or any string function for the values here to be consistent with all other networking
* functions of this project: as stated in the handout we are handling http_strings!
 * @param key (char*) : the first part of the header to be sent to the client
 * @param len_key (size_t) : the length of the key string
 * @param specific_parameter (char*) : the the second part of the strnig to be sent to the cleint
 * @param len_parameter (size_t) : the length of the specific parameter
 * @return (struct return_value_of_return_correct_header) : the struct containing an error code if something bad happened and the complete
 * string to be sent to the client
*/
struct return_value_of_return_correct_header return_correct_header(char* key, size_t len_key, char* specific_parameter, size_t len_parameter)
{
    size_t size = len_key + len_parameter + 3;
    char* headers = calloc(size, sizeof(char));
    if (headers == NULL) {
        struct return_value_of_return_correct_header return_value = {.error_code = ERR_OUT_OF_MEMORY, .string_of_return = NULL};
        return return_value;
    }

    char* iter = headers;
    memcpy(iter, key, len_key);
    iter += len_key;
    memcpy(iter, specific_parameter, len_parameter);
    iter += len_parameter;
    memcpy(iter, HTTP_LINE_DELIM, 2);

    struct return_value_of_return_correct_header return_value = {.error_code = ERR_NONE, .string_of_return = headers};
    return return_value;
};

int handle_list_call(int connection)
{
    if(connection <= 0) return ERR_INVALID_ARGUMENT;

    char* json = NULL;

    if(pthread_mutex_lock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, NULL, NULL, ERR_RUNTIME, NULL);
    int ret = do_list(&fs_file, JSON, &json);
    if(pthread_mutex_unlock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, NULL, NULL, ERR_RUNTIME, NULL);

    if(ret < 0) return return_and_garbage_collect_call(connection, json, NULL, ret, NULL);

    struct return_value_of_return_correct_header headers = return_correct_header("Content-Type: ", 14, "application/json", 16);
    if(headers.error_code < 0)
        return return_and_garbage_collect_call(connection, json, headers.string_of_return, ERR_RUNTIME, NULL);

    ret = http_reply(connection, HTTP_OK, headers.string_of_return, json, strlen(json));
    if(ret != ERR_NONE)
        return return_and_garbage_collect_call(connection, json, headers.string_of_return, ret, NULL);

    free(json);
    free(headers.string_of_return);

    return ERR_NONE;
}

int handle_read_call(struct http_string* uri, int connection)
{
    M_REQUIRE_NON_NULL(uri);
    if(connection <= 0) return return_and_garbage_collect_call(connection, NULL, NULL, ERR_INVALID_ARGUMENT, NULL);

    int ret = ERR_NONE;

    size_t out_len = 11;
    char* buffer = calloc(out_len, sizeof(char));
    if(buffer == NULL)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_OUT_OF_MEMORY, NULL);

    ret = http_get_var(uri, "res", buffer, out_len - 1);
    if(ret <= 0)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_NOT_ENOUGH_ARGUMENTS, NULL);

    int resolution = resolution_atoi(buffer);
    if(resolution == -1)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_RESOLUTIONS, NULL);

    out_len = MAX_IMG_ID + 1;
    buffer = realloc(buffer, out_len * sizeof(char));
    if(buffer == NULL) return return_and_garbage_collect_call(connection, buffer, NULL, ERR_OUT_OF_MEMORY, NULL);
    memset(buffer, 0, out_len * sizeof(char));
    ret = http_get_var(uri, "img_id", buffer, out_len - 1);
    if(ret <= 0) return return_and_garbage_collect_call(connection, buffer, NULL, ERR_NOT_ENOUGH_ARGUMENTS, NULL);

    char* image_buffer = NULL;
    uint32_t image_size = 0;

    if(pthread_mutex_lock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_RUNTIME, NULL);
    ret = do_read(buffer, resolution, &image_buffer, &image_size, &fs_file);
    if(pthread_mutex_unlock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_RUNTIME, NULL);

    if(ret != ERR_NONE) return return_and_garbage_collect_call(connection, buffer, NULL, ret, NULL);

    free(buffer);
    buffer = NULL;

    struct return_value_of_return_correct_header headers = return_correct_header("Content-Type: ", 14, "image/jpeg", 10);
    if(headers.error_code < 0)
        return return_and_garbage_collect_call(connection, image_buffer, headers.string_of_return, ERR_RUNTIME, NULL);

    ret = http_reply(connection, HTTP_OK, headers.string_of_return, image_buffer, image_size);

    if(ret != ERR_NONE) return return_and_garbage_collect_call(connection, image_buffer, headers.string_of_return, ret, NULL);

    free(headers.string_of_return);
    free(image_buffer);

    return ERR_NONE;
}


/**
 *
* @brief A helper function common to delete and insert. This a function that first finds and copies
* the value corresponding to the header key in the key value pairs stored in msg->headers.
* It then completes an allocated partial header field and copies the correct response to be sent
* to the client, so that the latter reloads the web page. Once the header is completed, we send it
* using a http reply back to the client. All errors are treated accoring to the explanations in the return
* section.
 * @param connection (int) : the file decriptor of the socket where to send the message
 * @param msg (struct http_message*) : the message received by the server.
 * @return (int) : the error code sent. ERR_NONE if everything went fine. ERR_OUT_OF_MEMORY in case of an allocation error
 * ERR_INVALID_ARGUMENT if msg is NULL or the error code of http_reply or return_correct_header if something went wrong.
*/
int handle_reply_with_headers_delete_and_insert(int connection, struct http_message* msg)
{
    M_REQUIRE_NON_NULL(msg);
    struct http_string* uri = &(msg->uri);

    const char* host_name = NULL;
    size_t len_of_host_name = 0;
    unsigned short int found_header = 0;
    for(int i = 0; !found_header && i < msg->num_headers; ++i) {
        if(msg->headers[i].key.len == 4 && !strncmp(msg->headers[i].key.val, "Host", msg->headers[i].key.len)) {
            len_of_host_name = msg->headers[i].value.len;
            host_name = calloc(len_of_host_name, sizeof(char));
            if(host_name == NULL) return return_and_garbage_collect_call(connection, NULL, NULL, ERR_OUT_OF_MEMORY, NULL);

            memcpy(host_name, msg->headers[i].value.val, len_of_host_name);

            found_header = 1;
        }
    }

    size_t partial_headers_length = len_of_host_name + 7 + 11 + 1;
    const char* partial_headers = calloc(partial_headers_length, sizeof(char));
    if(partial_headers == NULL)
        return return_and_garbage_collect_call(connection, host_name, partial_headers, ERR_OUT_OF_MEMORY, NULL);

    char* iter = partial_headers;

    char* http_part = "http://";
    size_t http_len = 7;
    memcpy(iter, http_part, http_len);
    iter += http_len;
    memcpy(iter, host_name, len_of_host_name);
    iter += len_of_host_name;
    char* index_part = "/index.html";
    size_t index_len = 11;
    memcpy(iter, index_part, index_len);

    free(host_name);
    host_name = NULL;

    struct return_value_of_return_correct_header header_ret = return_correct_header("Location: ", 10, partial_headers, partial_headers_length - 1);
    if(header_ret.error_code != ERR_NONE)
        return return_and_garbage_collect_call(connection, NULL, partial_headers, header_ret.error_code, NULL);

    char* headers = header_ret.string_of_return;

    int ret = http_reply(connection, "302 Found", headers, NULL, 0);
    if(ret != ERR_NONE) return return_and_garbage_collect_call(connection, partial_headers, headers, ret, NULL);

    free(headers);
    free(partial_headers);
    return ERR_NONE;
}

int handle_delete_call(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    struct http_string uri = msg->uri;

    if(connection <= 0) return return_and_garbage_collect_call(connection, NULL, NULL, ERR_INVALID_ARGUMENT, NULL);

    size_t out_len = MAX_IMG_ID;
    char* buffer = calloc(out_len + 1, sizeof(char));
    if(buffer == NULL) return return_and_garbage_collect_call(connection, buffer, NULL, ERR_OUT_OF_MEMORY, NULL);
    int ret = http_get_var(&uri, "img_id", buffer, out_len);
    if(ret <= 0) return return_and_garbage_collect_call(connection, buffer, NULL, ERR_NOT_ENOUGH_ARGUMENTS, NULL);

    if(pthread_mutex_lock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_RUNTIME, NULL);
    ret = do_delete(buffer, &fs_file);
    if(pthread_mutex_unlock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, buffer, NULL, ERR_RUNTIME, NULL);

    free(buffer);
    buffer = NULL;

    if (ret != ERR_NONE) return return_and_garbage_collect_call(connection, buffer, NULL, ret, NULL);

    return handle_reply_with_headers_delete_and_insert(connection, msg);
}

int handle_insert_call(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    struct http_string uri = msg->uri;

    if(connection <= 0) return return_and_garbage_collect_call(connection, NULL, NULL, ERR_INVALID_ARGUMENT, NULL);
    else if(msg->body.len <= 0) return return_and_garbage_collect_call(connection, NULL, NULL, ERR_INVALID_ARGUMENT, NULL);

    size_t out_len = MAX_IMG_ID;
    char* img_id = calloc(out_len + 1, sizeof(char));
    if(img_id == NULL) return return_and_garbage_collect_call(connection, img_id, NULL, ERR_OUT_OF_MEMORY, NULL);
    int ret = http_get_var(&uri, "name", img_id, out_len);
    if(ret <= 0) return return_and_garbage_collect_call(connection, img_id, NULL, ERR_NOT_ENOUGH_ARGUMENTS, NULL);

    char* image = calloc(msg->body.len, sizeof(char));
    if(image == NULL)
        return return_and_garbage_collect_call(connection, img_id, NULL, ERR_OUT_OF_MEMORY, NULL);
    memcpy(image, msg->body.val, msg->body.len);

    if(pthread_mutex_lock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, img_id, NULL, ERR_RUNTIME, NULL);
    ret = do_insert(image, msg->body.len, img_id, &fs_file);
    if(pthread_mutex_unlock(&global_lock) != 0)
        return return_and_garbage_collect_call(connection, img_id, NULL, ERR_RUNTIME, NULL);

    if(ret != ERR_NONE) return return_and_garbage_collect_call(connection, img_id, image, ret, NULL);

    free(image);
    image = NULL;
    free(img_id);
    img_id = NULL;

    return handle_reply_with_headers_delete_and_insert(connection, msg);
}