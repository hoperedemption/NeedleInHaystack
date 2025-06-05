#include "socket_layer.h"
#include "util.h"

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WHERE_ERROR "Error in tcp server:"
#define SMALL_RESPONSE "Small length"
#define LONG_RESPONSE "Long length "
#define DELIMITER "|\r"
#define END_OF_FILE_DELIMITER "\r\n"


int error_handler_for_server(const char* error_message, FILE* file, char* ptr, int error_code, int socket_server, int connection_socket)
{
    if(ptr != NULL) free(ptr);
    if(file != NULL) fclose(file);
    if(socket_server >= 0) close(socket_server);
    if (connection_socket >= 0) {
        shutdown(connection_socket, SHUT_RDWR);
        close(connection_socket);
    }
    if(error_message) perror(error_message);
    return error_code;
}

int wait_for_tcp(int waiting_socket, size_t bytes_expected, char* buffer_to_store)
{
    M_REQUIRE_NON_NULL(buffer_to_store);

    char* buffer = NULL;
    char* output = NULL;

    if((buffer = calloc(bytes_expected, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;
    if((output = calloc(bytes_expected, sizeof(char))) == NULL) return ERR_OUT_OF_MEMORY;

    char* iter = output;
    int n = bytes_expected;

    while(n > 0) {
        memset(buffer, 0, bytes_expected);

        int bytes_received = tcp_read(waiting_socket, buffer, n);
        if(bytes_received == -1) {
            free(buffer);
            free(output);
            return ERR_IO;
        }

        strncpy(iter, buffer, bytes_received);
        iter += bytes_received;

        n -= bytes_received;
    }

    strncpy(buffer_to_store, output, bytes_expected);
    free(buffer);
    free(output);
    return ERR_NONE;
}

int main(int argc, char** argv)
{
    argc--;
    argv++;
    // skip the first argument

    if(argc < 1) return error_handler_for_server(WHERE_ERROR " not enough arguments\n", NULL, NULL, ERR_NOT_ENOUGH_ARGUMENTS, -1, -1);
    if(argc > 1) return error_handler_for_server(WHERE_ERROR " invalid command\n", NULL, NULL, ERR_INVALID_COMMAND, -1, -1);

    M_REQUIRE_NON_NULL(argv);
    M_REQUIRE_NON_NULL(argv[0]);

    uint16_t port = atouint16(*(argv));

    int ret = ERR_NONE;
    if((ret = tcp_server_init(port)) < 0) return error_handler_for_server(WHERE_ERROR "Error: could not init server\n", NULL, NULL, ret, -1, -1);
    else printf("Server started on port %d\n", port);

    int socket_server = ret;

    while(1) {
        int connection_socket = 0;
        if((connection_socket = tcp_accept(socket_server)) == -1) return error_handler_for_server(WHERE_ERROR "Error: could not accept connection\n", NULL, NULL, ERR_IO, socket_server, -1);
        else printf("Server connected to socket %d\n", connection_socket);

        printf("Waiting for a size...\n");

        char first_buffer[13] = {0};
        if((ret = wait_for_tcp(connection_socket, sizeof(first_buffer), first_buffer)) != ERR_NONE)
            return error_handler_for_server(WHERE_ERROR "Error: in receiving first buffer\n", NULL, NULL, ret, socket_server, connection_socket);
        else printf("Received a size: ");

        size_t size_of_size_str = strlen(first_buffer);

        printf("Size of size str: %d\n", size_of_size_str);
        printf("first_buffer %s\n", first_buffer);

        if(first_buffer[size_of_size_str - 2] != DELIMITER[0] || first_buffer[size_of_size_str - 1] != DELIMITER[1])
            return error_handler_for_server(WHERE_ERROR "Error: first_buffer is not correct\n", NULL, NULL, ERR_IO, socket_server, connection_socket);

        first_buffer[size_of_size_str - 2] = 0;
        first_buffer[size_of_size_str - 1] = 0;

        uint32_t size = atouint32(first_buffer);

        if(size < 1024) {
            printf("--> accepted\n");
            printf("About to receive file of %d bytes\n", size);

            const char* small_length = "Small length";
            if(tcp_send(connection_socket, small_length, 13) < 0)
                return error_handler_for_server(WHERE_ERROR "Error: could not send ack\n", NULL, NULL, ERR_IO, socket_server, connection_socket);


            char filename[FILENAME_MAX+3] = {0};
            if((ret = wait_for_tcp(connection_socket, FILENAME_MAX + 2, filename))  != ERR_NONE)
                return error_handler_for_server(WHERE_ERROR "Error: could not read\n", NULL, NULL, ret, socket_server, connection_socket);

            size_t filename_length = strlen(filename);

            printf("Filename that we receive: %s\n", filename);
            if(filename[filename_length - 2] != END_OF_FILE_DELIMITER[0] || filename[filename_length - 1] != END_OF_FILE_DELIMITER[1])
                return error_handler_for_server(WHERE_ERROR "Error: filename delimiter is wrong\n", NULL, NULL, ERR_IO, socket_server, connection_socket);

            filename[filename_length - 2] = 0;
            filename[filename_length - 1] = 0;

            const char* server_ack = "Filename received";
            if(tcp_send(connection_socket, server_ack, 18) < 0)
                return error_handler_for_server(WHERE_ERROR "Error: could not send filename receive\n", NULL, NULL, ERR_IO, socket_server, connection_socket);


            char* file_buffer = calloc(size, sizeof(char));
            if(file_buffer == NULL)
                return error_handler_for_server(WHERE_ERROR "Error: could not allocate memory\n", NULL, NULL, ERR_OUT_OF_MEMORY, socket_server, connection_socket);

            FILE* file = NULL;
            if((file = fopen(filename, "rb")) == NULL
               || fread(file_buffer, sizeof(char), size, file) != size)
                return error_handler_for_server(WHERE_ERROR "Error: could not fopen the file correctly\n", file, file_buffer, ERR_IO, socket_server, connection_socket);

            printf("File content is:\n %s\n", file_buffer);

            free(file_buffer);
            fclose(file);
        } else {
            printf("--> rejected\n");

            if(tcp_send(connection_socket, LONG_RESPONSE, 13) < 0)
                return error_handler_for_server(WHERE_ERROR "Error: could not send not ack\n", NULL, NULL, ERR_IO, socket_server, connection_socket);
        }

        close(connection_socket);
    }

    close(socket_server);

    return ERR_NONE;
}