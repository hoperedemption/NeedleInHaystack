/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"
#include "util.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

#define HTTP_PROTOCOL_ID_LENGTH 9
#define HTTP_LINE_DELIM_LENGTH 2
#define HTTP_HDR_END_DELIM_LENGTH 4
#define CONTENT_LENGTH_LENGTH 16
#define CONTENT_LENGTH "Content-Length: "

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);
MK_OUR_ERR(ERR_DEBUG);
MK_OUR_ERR(ERR_RUNTIME);
MK_OUR_ERR(ERR_INVALID_COMMAND);

void* convert_error(int error)
{
    switch (error) {
    case ERR_NONE:
        return &our_ERR_NONE;
    case ERR_RUNTIME:
        return &our_ERR_RUNTIME;
    case ERR_OUT_OF_MEMORY:
        return &our_ERR_OUT_OF_MEMORY;
    case ERR_INVALID_ARGUMENT:
        return &our_ERR_INVALID_ARGUMENT;
    case ERR_INVALID_COMMAND:
        return &our_ERR_INVALID_COMMAND;
    case ERR_IO:
        return &our_ERR_IO;
    default:
        return &our_ERR_DEBUG;
    }
}

/**
* @brief this function is the garbage collector for handle_connection. It frees the pointers and close the connection socket
* before returning a void pointer on the error code passed to the function.
* @param output (char*) : a pointer to a string
* @param error_code (int) : the error code to be returned in the form of a void pointer
* @param socket (int*) : a pointer to the connection socket to be closed
* @param start_body (char*) : a pointer to the string containing the body of the message sent to the server by the client
* @return (void*) : the error corresponding to the error code passed to the fucntion.
*/
void* return_and_garbage_collect_handle_connection(char* output, int error_code, int* socket, char* start_body)
{
    if(output != NULL) free(output);
    if(start_body != NULL) free(start_body);
    if(socket != NULL) {
        if(*socket != -1) close(*socket);
        free(socket);
    }
    return convert_error(error_code);
}

/**
* @brief this function resets all the variable passed (except the connection socket) and calls the callback (if requested)
* to maybe satisfy the request of the client if there are no error in the latter. Indeed it prepares for the next round of tcp
* read. Thus, there are two cases to consider : the headers are completed but there are no body, in which case we call the callback,
* the headers are completed and the body is completed, in which case the callback is also called, and, we are at the beginning
* of handle_connection, in which case we received no preivous requests and we need to initialise the variables.
* @param use_callback (int): a boolean value indicating whether we do a callback or not
* @param output (char**): pointer to the string of the output buffer in handle connection
* @param to_receive (size_t*): a pointer to an size_t, the number of bytes to be received in handle connection
* @param bytes_received (ssize_t*): a pointer ssize_t, the number of bytes received in handle connection
* @param content_length (int*): a pointer to an int, the length of the content in handle connection
* @param started_read (unsigned short int*): a pointer to an int, the boolean value indicating whether or not
* we have started reading the body in handle connection
* @param iter (char**) : a pointer that allows us to fill any buffer in rounds of tcp reads without changing the location of
* the base pointer (the latter being the begginning of the string, thus it must not be modified)
.* @param message (struct http_message*): a pointer to an httpp_message struct in handle conection
* @param socket (int*) : a pointer to the file descriptor of the connection socket.
* @return (int) : returns an error code. ERR_NONE if everything went fine or the error of the callback if an error
* occured there.
*/
int prepare_for_next_round(int use_callback, char** output, size_t* to_receive, ssize_t* bytes_received, int* content_length,
                           unsigned short int* started_read, char** iter, struct http_message* message, int* socket)
{

    if(use_callback) {
        int ret = cb(message, *socket);
        if (ret != ERR_NONE) return ret;
    }

    *to_receive = MAX_HEADER_SIZE;

    *output = realloc(*output, *to_receive * sizeof(char));
    if(*output == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    memset(*output, 0, *to_receive * sizeof(char));

    *iter = *output;
    *started_read = 0;
    *bytes_received = 0;
    *content_length = 0;
    zero_init_var(*message);

    return ERR_NONE;
}


/*******************************************************************
 * Handle connection
 */
static void *handle_connection(void *arg)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return convert_error(ERR_INVALID_ARGUMENT);
    int* socket_ptr = (int*) arg;

    int ret_error = 0;

    ssize_t to_receive = 0;
    char* output = NULL;
    char* body_full = NULL;
    ssize_t length_read = 0;
    ssize_t bytes_received = 0;
    unsigned short int started_read = 0;
    char* iter = NULL;
    char* start_body = NULL;
    struct http_message message;
    int content_length = 0;

    if((ret_error = prepare_for_next_round(0, &output, &to_receive, &bytes_received, &content_length, &started_read, &iter, &message, socket_ptr)) != ERR_NONE)
        return return_and_garbage_collect_handle_connection(output, ret_error, socket_ptr, NULL);

    while((length_read = tcp_read(*socket_ptr, iter, to_receive - bytes_received)) != 0) {
        if(length_read == -1) return return_and_garbage_collect_handle_connection(output, ERR_IO, socket_ptr, start_body);

        bytes_received += length_read;
        iter += length_read;

        if(started_read == 0) {
            ret_error = http_parse_message(output, bytes_received, &message, &content_length);

            if(ret_error < 0) {
                if(ret_error == ERR_RUNTIME) return return_and_garbage_collect_handle_connection(output, ERR_RUNTIME, socket_ptr, start_body);
                else if(ret_error == ERR_OUT_OF_MEMORY) return return_and_garbage_collect_handle_connection(output, ERR_OUT_OF_MEMORY, socket_ptr, start_body);
                else return return_and_garbage_collect_handle_connection(output, ERR_DEBUG, socket_ptr, start_body);
            } else if(ret_error == 0 && (0 < content_length && message.body.len < content_length)) {
                start_body = realloc(start_body, content_length * sizeof(char));
                if(start_body == NULL)
                    return return_and_garbage_collect_handle_connection(output, ERR_OUT_OF_MEMORY, socket_ptr, start_body);

                memset(start_body, 0, content_length * sizeof(char));
                memcpy(start_body, iter - message.body.len, message.body.len);

                iter = start_body + message.body.len;
                to_receive = content_length;
                bytes_received = message.body.len;
                started_read = 1;
            } else if(ret_error > 0) {
                ret_error = prepare_for_next_round(1, &output, &to_receive,
                                                   &bytes_received, &content_length, &started_read, &iter, &message, socket_ptr);
                if(ret_error != ERR_NONE)
                    return return_and_garbage_collect_handle_connection(output, ret_error, socket_ptr, start_body);
            }
        } else if((started_read == 1) && content_length == bytes_received) {
            message.body.val = start_body;
            message.body.len = content_length;

            ret_error = prepare_for_next_round(1, &output, &to_receive, &bytes_received, &content_length, &started_read, &iter, &message, socket_ptr);
            if(ret_error != ERR_NONE)
                return return_and_garbage_collect_handle_connection(output, ret_error, socket_ptr, start_body);
        }
    }
    return return_and_garbage_collect_handle_connection(output, ERR_NONE, socket_ptr, start_body);
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}


/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}


/**
* @brief this function is the garbage collector for http_receive. If the pointer is not null
* it does garbage collecting. If the socket pointer contains a valid socket file desscriptor it closes it.
* After that it frees the pointer. Then it always returns error none as no errors are raised.
* @param active_socket (int*) : a pointer to an int containing the value of a socket file descriptor
* @return (int) : the error code, which is always ERR_NONE.
*/
int garbage_collector_http_receive(int* active_socket)
{
    if(active_socket != NULL) {
        if(*active_socket != -1) close(*active_socket);
        free(active_socket);
    }
    return ERR_NONE;
}
/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
    int* active_socket = calloc(1, sizeof(int));
    if(active_socket == NULL)
        return garbage_collector_http_receive(active_socket);

    *active_socket = tcp_accept(passive_socket);
    if(*active_socket == -1)
        return garbage_collector_http_receive(active_socket);

    pthread_attr_t attr;
    zero_init_var(attr);
    pthread_t thread;
    zero_init_var(thread);

    int ret = 0;
    if( ((ret = pthread_attr_init(&attr)) != 0)
        || ((ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
        || ((ret = pthread_create(&thread, &attr, handle_connection, (void*) active_socket)) != 0))
        return garbage_collector_http_receive(active_socket);

    pthread_attr_destroy(&attr);
    return ERR_NONE;
}
/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

int garbage_collector_of_http_reply(int error_code, char* body_len_string_ptr, char* http_message)
{
    if (http_message != NULL) free(http_message);
    if(body_len_string_ptr != NULL) free(body_len_string_ptr);
    return error_code;
}

/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char *body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    if(body_len != 0) {
        M_REQUIRE_NON_NULL(body);
    } else {
        if(body != NULL && body[0] != '\0') return ERR_INVALID_COMMAND;
    }

    char* body_len_string = calloc(21, sizeof(char));
    if(body_len_string == NULL) return ERR_OUT_OF_MEMORY;

    if(snprintf(body_len_string, 21, "%zu", body_len) == -1) {
        return garbage_collector_of_http_reply(ERR_RUNTIME, body_len_string, NULL);
    }

    size_t size_of_body_len = strlen(body_len_string);
    // we have that the maximum length is 20 for an unsigned long, so to null terminate
    ssize_t total_header_size = HTTP_PROTOCOL_ID_LENGTH + strlen(status) + HTTP_LINE_DELIM_LENGTH + strlen(headers)
                                + CONTENT_LENGTH_LENGTH + size_of_body_len + HTTP_HDR_END_DELIM_LENGTH;
    size_t total_message_size = total_header_size + body_len;

    char* http_message_buffer = calloc(total_message_size + 1, sizeof(char));
    if(http_message_buffer == NULL) {
        return garbage_collector_of_http_reply(ERR_OUT_OF_MEMORY, body_len_string, NULL);
    }

    if(snprintf(http_message_buffer, total_header_size + 1, "%s%s%s%s%s%zu%s",
                HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, CONTENT_LENGTH, body_len, HTTP_HDR_END_DELIM) == -1) {
        return garbage_collector_of_http_reply(ERR_RUNTIME, body_len_string, http_message_buffer);
    }

    if(body_len != 0) {
        memcpy(http_message_buffer + total_header_size, body, body_len);
    }

    size_t bytes_sent = 0;
    char* iter = http_message_buffer;
    while(bytes_sent < total_message_size) {
        size_t ret = tcp_send(connection, iter, total_message_size - bytes_sent);
        if(ret < 0) return garbage_collector_of_http_reply(ERR_IO, body_len_string, http_message_buffer);
        bytes_sent += ret;
        iter += ret;
    }

    return garbage_collector_of_http_reply(ERR_NONE, body_len_string, http_message_buffer);
}